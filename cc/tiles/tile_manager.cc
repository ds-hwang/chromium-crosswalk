// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/histograms.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/task_category.h"
#include "cc/raster/tile_task_runner.h"
#include "cc/tiles/tile.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {
namespace {

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
    ScopedRasterTaskTimer,
    "Compositing.%s.RasterTask.RasterUs",
    "Compositing.%s.RasterTask.RasterPixelsPerMs");

class RasterTaskImpl : public RasterTask {
 public:
  RasterTaskImpl(const Resource* resource,
                 DisplayListRasterSource* raster_source,
                 const gfx::Rect& content_rect,
                 const gfx::Rect& invalid_content_rect,
                 float contents_scale,
                 TileResolution tile_resolution,
                 int layer_id,
                 uint64_t source_prepare_tiles_id,
                 const void* tile,
                 uint64_t new_content_id,
                 uint64_t previous_content_id,
                 uint64_t resource_content_id,
                 int source_frame_number,
                 const base::Callback<void(bool)>& reply,
                 ImageDecodeTask::Vector* dependencies)
      : RasterTask(dependencies),
        resource_(resource),
        raster_source_(raster_source),
        content_rect_(content_rect),
        invalid_content_rect_(invalid_content_rect),
        contents_scale_(contents_scale),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        source_prepare_tiles_id_(source_prepare_tiles_id),
        tile_(tile),
        new_content_id_(new_content_id),
        previous_content_id_(previous_content_id),
        resource_content_id_(resource_content_id),
        source_frame_number_(source_frame_number),
        reply_(reply) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT1("cc", "RasterizerTaskImpl::RunOnWorkerThread",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);

    DCHECK(raster_source_.get());
    DCHECK(raster_buffer_);

    Raster(raster_source_.get());
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {
    DCHECK(!raster_buffer_);
    raster_buffer_ = client->AcquireBufferForRaster(
        resource_, resource_content_id_, previous_content_id_);
  }
  void CompleteOnOriginThread(TileTaskClient* client) override {
    client->ReleaseBufferForRaster(std::move(raster_buffer_));
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  ~RasterTaskImpl() override { DCHECK(!raster_buffer_); }

 private:
  void Raster(const DisplayListRasterSource* raster_source) {
    frame_viewer_instrumentation::ScopedRasterTask raster_task(
        tile_, tile_resolution_, source_frame_number_, layer_id_);
    ScopedRasterTaskTimer timer;
    timer.SetArea(content_rect_.size().GetArea());

    DCHECK(raster_source);

    bool include_images = tile_resolution_ != LOW_RESOLUTION;
    raster_buffer_->Playback(raster_source_.get(), content_rect_,
                             invalid_content_rect_, new_content_id_,
                             contents_scale_, include_images);
  }

  const Resource* resource_;
  scoped_refptr<DisplayListRasterSource> raster_source_;
  gfx::Rect content_rect_;
  gfx::Rect invalid_content_rect_;
  float contents_scale_;
  TileResolution tile_resolution_;
  int layer_id_;
  uint64_t source_prepare_tiles_id_;
  const void* tile_;
  uint64_t new_content_id_;
  uint64_t previous_content_id_;
  uint64_t resource_content_id_;
  int source_frame_number_;
  const base::Callback<void(bool)> reply_;
  scoped_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(RasterTaskImpl);
};

// Task priorities that make sure that the task set done tasks run before any
// other remaining tasks.
const size_t kRequiredForActivationDoneTaskPriority = 1u;
const size_t kRequiredForDrawDoneTaskPriority = 2u;
const size_t kAllDoneTaskPriority = 3u;

// For correctness, |kTileTaskPriorityBase| must be greater than
// all task set done task priorities.
size_t kTileTaskPriorityBase = 10u;

void InsertNodeForTask(TaskGraph* graph,
                       TileTask* task,
                       uint16_t category,
                       uint16_t priority,
                       size_t dependencies) {
  DCHECK(std::find_if(graph->nodes.begin(), graph->nodes.end(),
                      [task](const TaskGraph::Node& node) {
                        return node.task == task;
                      }) == graph->nodes.end());
  graph->nodes.push_back(
      TaskGraph::Node(task, category, priority, dependencies));
}

void InsertNodesForRasterTask(TaskGraph* graph,
                              RasterTask* raster_task,
                              const ImageDecodeTask::Vector& decode_tasks,
                              size_t priority,
                              bool use_gpu_rasterization,
                              bool high_priority) {
  size_t dependencies = 0u;

  // Determine the TaskCategory for raster tasks - if a task uses GPU, it
  // cannot run concurrently and is assigned
  // TASK_CATEGORY_NONCONCURRENT_FOREGROUND, regardless of its priority.
  // Otherwise its category is based on its priority.
  TaskCategory raster_task_category;
  if (use_gpu_rasterization) {
    raster_task_category = TASK_CATEGORY_NONCONCURRENT_FOREGROUND;
  } else {
    raster_task_category =
        high_priority ? TASK_CATEGORY_FOREGROUND : TASK_CATEGORY_BACKGROUND;
  }

  // Determine the TaskCategory for decode tasks. This category is based on
  // the priority of the raster task which depends on it.
  TaskCategory decode_task_category =
      high_priority ? TASK_CATEGORY_FOREGROUND : TASK_CATEGORY_BACKGROUND;

  // Insert image decode tasks.
  for (ImageDecodeTask::Vector::const_iterator it = decode_tasks.begin();
       it != decode_tasks.end(); ++it) {
    ImageDecodeTask* decode_task = it->get();

    // Skip if already decoded.
    if (decode_task->HasCompleted())
      continue;

    dependencies++;

    // Add decode task if it doesn't already exists in graph.
    TaskGraph::Node::Vector::iterator decode_it =
        std::find_if(graph->nodes.begin(), graph->nodes.end(),
                     [decode_task](const TaskGraph::Node& node) {
                       return node.task == decode_task;
                     });

    // Tasks are inserted in priority order, so existing decode tasks should
    // already be FOREGROUND if this is a high priority task.
    DCHECK(decode_it == graph->nodes.end() || !high_priority ||
           static_cast<uint16_t>(TASK_CATEGORY_FOREGROUND) ==
               decode_it->category);

    if (decode_it == graph->nodes.end()) {
      InsertNodeForTask(graph, decode_task, decode_task_category, priority, 0u);
    }

    graph->edges.push_back(TaskGraph::Edge(decode_task, raster_task));
  }

  InsertNodeForTask(graph, raster_task, raster_task_category, priority,
                    dependencies);
}

class TaskSetFinishedTaskImpl : public TileTask {
 public:
  explicit TaskSetFinishedTaskImpl(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& on_task_set_finished_callback)
      : task_runner_(task_runner),
        on_task_set_finished_callback_(on_task_set_finished_callback) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "TaskSetFinishedTaskImpl::RunOnWorkerThread");
    TaskSetFinished();
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {}
  void CompleteOnOriginThread(TileTaskClient* client) override {}

 protected:
  ~TaskSetFinishedTaskImpl() override {}

  void TaskSetFinished() {
    task_runner_->PostTask(FROM_HERE, on_task_set_finished_callback_);
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Closure on_task_set_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskSetFinishedTaskImpl);
};

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats) {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  state->SetInteger("completed_count",
                    base::saturated_cast<int>(stats.completed_count));
  state->SetInteger("canceled_count",
                    base::saturated_cast<int>(stats.canceled_count));
  return state;
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    base::SequencedTaskRunner* task_runner,
    size_t scheduled_raster_task_limit,
    bool use_partial_raster) {
  return make_scoped_ptr(new TileManager(
      client, task_runner, scheduled_raster_task_limit, use_partial_raster));
}

TileManager::TileManager(
    TileManagerClient* client,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    size_t scheduled_raster_task_limit,
    bool use_partial_raster)
    : client_(client),
      task_runner_(task_runner),
      resource_pool_(nullptr),
      tile_task_runner_(nullptr),
      scheduled_raster_task_limit_(scheduled_raster_task_limit),
      use_partial_raster_(use_partial_raster),
      use_gpu_rasterization_(false),
      all_tiles_that_need_to_be_rasterized_are_scheduled_(true),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      did_oom_on_last_assign_(false),
      more_tiles_need_prepare_check_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::CheckIfMoreTilesNeedToBePrepared,
                     base::Unretained(this))),
      signals_check_notifier_(task_runner_.get(),
                              base::Bind(&TileManager::CheckAndIssueSignals,
                                         base::Unretained(this))),
      has_scheduled_tile_tasks_(false),
      prepare_tiles_count_(0u),
      next_tile_id_(0u),
      task_set_finished_weak_ptr_factory_(this) {}

TileManager::~TileManager() {
  FinishTasksAndCleanUp();
}

void TileManager::FinishTasksAndCleanUp() {
  if (!tile_task_runner_)
    return;

  global_state_ = GlobalStateThatImpactsTilePriority();

  // This cancels tasks if possible, finishes pending tasks, and release any
  // uninitialized resources.
  tile_task_runner_->Shutdown();

  // Now that all tasks have been finished, we can clear any
  // |orphan_tasks_|.
  orphan_tasks_.clear();

  tile_task_runner_->CheckForCompletedTasks();

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  tile_task_runner_ = nullptr;
  resource_pool_ = nullptr;
  more_tiles_need_prepare_check_notifier_.Cancel();
  signals_check_notifier_.Cancel();
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();
}

void TileManager::SetResources(ResourcePool* resource_pool,
                               TileTaskRunner* tile_task_runner,
                               size_t scheduled_raster_task_limit,
                               bool use_gpu_rasterization) {
  DCHECK(!tile_task_runner_);
  DCHECK(tile_task_runner);

  use_gpu_rasterization_ = use_gpu_rasterization;
  scheduled_raster_task_limit_ = scheduled_raster_task_limit;
  resource_pool_ = resource_pool;
  tile_task_runner_ = tile_task_runner;
  image_decode_controller_.SetIsUsingGpuRasterization(use_gpu_rasterization_);
}

void TileManager::Release(Tile* tile) {
  released_tiles_.push_back(tile);
}

void TileManager::FreeResourcesForReleasedTiles() {
  for (auto* tile : released_tiles_)
    FreeResourcesForTile(tile);
}

void TileManager::CleanUpReleasedTiles() {
  std::vector<Tile*> tiles_to_retain;
  for (auto* tile : released_tiles_) {
    if (tile->HasRasterTask()) {
      tiles_to_retain.push_back(tile);
      continue;
    }

    DCHECK(!tile->draw_info().has_resource());
    DCHECK(tiles_.find(tile->id()) != tiles_.end());
    tiles_.erase(tile->id());

    delete tile;
  }
  released_tiles_.swap(tiles_to_retain);
}

void TileManager::DidFinishRunningTileTasksRequiredForActivation() {
  TRACE_EVENT0("cc",
               "TileManager::DidFinishRunningTileTasksRequiredForActivation");
  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
  signals_.ready_to_activate = true;
  signals_check_notifier_.Schedule();
}

void TileManager::DidFinishRunningTileTasksRequiredForDraw() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningTileTasksRequiredForDraw");
  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
  signals_.ready_to_draw = true;
  signals_check_notifier_.Schedule();
}

void TileManager::DidFinishRunningAllTileTasks() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningAllTileTasks");
  TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  DCHECK(resource_pool_);
  DCHECK(tile_task_runner_);

  has_scheduled_tile_tasks_ = false;

  bool memory_usage_above_limit = resource_pool_->memory_usage_bytes() >
                                  global_state_.soft_memory_limit_in_bytes;

  if (all_tiles_that_need_to_be_rasterized_are_scheduled_ &&
      !memory_usage_above_limit) {
    // TODO(ericrk): We should find a better way to safely handle re-entrant
    // notifications than always having to schedule a new task.
    // http://crbug.com/498439
    signals_.all_tile_tasks_completed = true;
    signals_check_notifier_.Schedule();
    return;
  }

  more_tiles_need_prepare_check_notifier_.Schedule();
}

bool TileManager::PrepareTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  ++prepare_tiles_count_;

  TRACE_EVENT1("cc", "TileManager::PrepareTiles", "prepare_tiles_id",
               prepare_tiles_count_);

  if (!tile_task_runner_) {
    TRACE_EVENT_INSTANT0("cc", "PrepareTiles aborted",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  signals_.reset();
  global_state_ = state;

  // We need to call CheckForCompletedTasks() once in-between each call
  // to ScheduleTasks() to prevent canceled tasks from being scheduled.
  if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
    tile_task_runner_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
  }

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  PrioritizedTileVector tiles_that_need_to_be_rasterized;
  scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority,
                                RasterTilePriorityQueue::Type::ALL));
  AssignGpuMemoryToTiles(raster_priority_queue.get(),
                         scheduled_raster_task_limit_,
                         &tiles_that_need_to_be_rasterized);

  // Inform the client that will likely require a draw if the highest priority
  // tile that will be rasterized is required for draw.
  client_->SetIsLikelyToRequireADraw(
      !tiles_that_need_to_be_rasterized.empty() &&
      tiles_that_need_to_be_rasterized.front().tile()->required_for_draw());

  // Schedule tile tasks.
  ScheduleTasks(tiles_that_need_to_be_rasterized);

  TRACE_EVENT_INSTANT1("cc", "DidPrepareTiles", TRACE_EVENT_SCOPE_THREAD,
                       "state", BasicStateAsValue());
  return true;
}

void TileManager::Flush() {
  TRACE_EVENT0("cc", "TileManager::Flush");

  if (!tile_task_runner_) {
    TRACE_EVENT_INSTANT0("cc", "Flush aborted", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  tile_task_runner_->CheckForCompletedTasks();

  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1("cc", "DidFlush", TRACE_EVENT_SCOPE_THREAD, "stats",
                       RasterTaskCompletionStatsAsValue(flush_stats_));
  flush_stats_ = RasterTaskCompletionStats();
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
TileManager::BasicStateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue();
  BasicStateAsValueInto(value.get());
  return value;
}

void TileManager::BasicStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("tile_count", base::saturated_cast<int>(tiles_.size()));
  state->SetBoolean("did_oom_on_last_assign", did_oom_on_last_assign_);
  state->BeginDictionary("global_state");
  global_state_.AsValueInto(state);
  state->EndDictionary();
}

scoped_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesUntilUsageIsWithinLimit(
    scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    Tile* tile = eviction_priority_queue->Top().tile();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
}

scoped_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
    scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    const TilePriority& other_priority,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    const PrioritizedTile& prioritized_tile = eviction_priority_queue->Top();
    if (!other_priority.IsHigherPriorityThan(prioritized_tile.priority()))
      break;

    Tile* tile = prioritized_tile.tile();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
}

bool TileManager::TilePriorityViolatesMemoryPolicy(
    const TilePriority& priority) {
  switch (global_state_.memory_limit_policy) {
    case ALLOW_NOTHING:
      return true;
    case ALLOW_ABSOLUTE_MINIMUM:
      return priority.priority_bin > TilePriority::NOW;
    case ALLOW_PREPAINT_ONLY:
      return priority.priority_bin > TilePriority::SOON;
    case ALLOW_ANYTHING:
      return priority.distance_to_visible ==
             std::numeric_limits<float>::infinity();
  }
  NOTREACHED();
  return true;
}

void TileManager::AssignGpuMemoryToTiles(
    RasterTilePriorityQueue* raster_priority_queue,
    size_t scheduled_raster_task_limit,
    PrioritizedTileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT_BEGIN0("cc", "TileManager::AssignGpuMemoryToTiles");

  DCHECK(resource_pool_);
  DCHECK(tile_task_runner_);

  // Maintain the list of released resources that can potentially be re-used
  // or deleted. If this operation becomes expensive too, only do this after
  // some resource(s) was returned. Note that in that case, one also need to
  // invalidate when releasing some resource from the pool.
  resource_pool_->CheckBusyResources();

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  unsigned schedule_priority = 1u;
  all_tiles_that_need_to_be_rasterized_are_scheduled_ = true;
  bool had_enough_memory_to_schedule_tiles_needed_now = true;

  MemoryUsage hard_memory_limit(global_state_.hard_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage soft_memory_limit(global_state_.soft_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage memory_usage(resource_pool_->memory_usage_bytes(),
                           resource_pool_->resource_count());

  scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue;
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    const PrioritizedTile& prioritized_tile = raster_priority_queue->Top();
    Tile* tile = prioritized_tile.tile();
    TilePriority priority = prioritized_tile.priority();

    if (TilePriorityViolatesMemoryPolicy(priority)) {
      TRACE_EVENT_INSTANT0(
          "cc", "TileManager::AssignGpuMemory tile violates memory policy",
          TRACE_EVENT_SCOPE_THREAD);
      break;
    }

    if (tile->use_picture_analysis() && kUseColorEstimator) {
      // We analyze for solid color here, to decide to continue
      // or drop the tile for scheduling and raster.
      // TODO(sohanjg): Check if we could use a shared analysis
      // canvas which is reset between tiles.
      SkColor color = SK_ColorTRANSPARENT;
      bool is_solid_color =
          prioritized_tile.raster_source()->PerformSolidColorAnalysis(
              tile->content_rect(), tile->contents_scale(), &color);
      if (is_solid_color) {
        tile->draw_info().set_solid_color(color);
        tile->draw_info().set_was_ever_ready_to_draw();
        client_->NotifyTileStateChanged(tile);
        continue;
      }
    }

    // We won't be able to schedule this tile, so break out early.
    if (tiles_that_need_to_be_rasterized->size() >=
        scheduled_raster_task_limit) {
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    tile->scheduled_priority_ = schedule_priority++;

    DCHECK(tile->draw_info().mode() == TileDrawInfo::OOM_MODE ||
           !tile->draw_info().IsReadyToDraw());

    // If the tile already has a raster_task, then the memory used by it is
    // already accounted for in memory_usage. Otherwise, we'll have to acquire
    // more memory to create a raster task.
    MemoryUsage memory_required_by_tile_to_be_scheduled;
    if (!tile->raster_task_.get()) {
      memory_required_by_tile_to_be_scheduled = MemoryUsage::FromConfig(
          tile->desired_texture_size(), DetermineResourceFormat(tile));
    }

    bool tile_is_needed_now = priority.priority_bin == TilePriority::NOW;

    // This is the memory limit that will be used by this tile. Depending on
    // the tile priority, it will be one of hard_memory_limit or
    // soft_memory_limit.
    MemoryUsage& tile_memory_limit =
        tile_is_needed_now ? hard_memory_limit : soft_memory_limit;

    const MemoryUsage& scheduled_tile_memory_limit =
        tile_memory_limit - memory_required_by_tile_to_be_scheduled;
    eviction_priority_queue =
        FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
            std::move(eviction_priority_queue), scheduled_tile_memory_limit,
            priority, &memory_usage);
    bool memory_usage_is_within_limit =
        !memory_usage.Exceeds(scheduled_tile_memory_limit);

    // If we couldn't fit the tile into our current memory limit, then we're
    // done.
    if (!memory_usage_is_within_limit) {
      if (tile_is_needed_now)
        had_enough_memory_to_schedule_tiles_needed_now = false;
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    memory_usage += memory_required_by_tile_to_be_scheduled;
    tiles_that_need_to_be_rasterized->push_back(prioritized_tile);
  }

  // Note that we should try and further reduce memory in case the above loop
  // didn't reduce memory. This ensures that we always release as many resources
  // as possible to stay within the memory limit.
  eviction_priority_queue = FreeTileResourcesUntilUsageIsWithinLimit(
      std::move(eviction_priority_queue), hard_memory_limit, &memory_usage);

  UMA_HISTOGRAM_BOOLEAN("TileManager.ExceededMemoryBudget",
                        !had_enough_memory_to_schedule_tiles_needed_now);
  did_oom_on_last_assign_ = !had_enough_memory_to_schedule_tiles_needed_now;

  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.total_bytes_used = memory_usage.memory_bytes();
  DCHECK_GE(memory_stats_from_last_assign_.total_bytes_used, 0);
  memory_stats_from_last_assign_.had_enough_memory =
      had_enough_memory_to_schedule_tiles_needed_now;

  TRACE_EVENT_END2("cc", "TileManager::AssignGpuMemoryToTiles",
                   "all_tiles_that_need_to_be_rasterized_are_scheduled",
                   all_tiles_that_need_to_be_rasterized_are_scheduled_,
                   "had_enough_memory_to_schedule_tiles_needed_now",
                   had_enough_memory_to_schedule_tiles_needed_now);
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_) {
    resource_pool_->ReleaseResource(draw_info.resource_, tile->id());
    draw_info.resource_ = nullptr;
  }
}

void TileManager::FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(
    Tile* tile) {
  bool was_ready_to_draw = tile->draw_info().IsReadyToDraw();
  FreeResourcesForTile(tile);
  if (was_ready_to_draw)
    client_->NotifyTileStateChanged(tile);
}

void TileManager::ScheduleTasks(
    const PrioritizedTileVector& tiles_that_need_to_be_rasterized) {
  TRACE_EVENT1("cc", "TileManager::ScheduleTasks", "count",
               tiles_that_need_to_be_rasterized.size());

  DCHECK(did_check_for_completed_tasks_since_last_schedule_tasks_);

  if (!has_scheduled_tile_tasks_) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);
  }

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  // Even when scheduling an empty set of tiles, the TTWP does some work, and
  // will always trigger a DidFinishRunningTileTasks notification. Because of
  // this we unconditionally set |has_scheduled_tile_tasks_| to true.
  has_scheduled_tile_tasks_ = true;

  // Track the number of dependents for each *_done task.
  size_t required_for_activate_count = 0;
  size_t required_for_draw_count = 0;
  size_t all_count = 0;

  size_t priority = kTileTaskPriorityBase;

  graph_.Reset();

  scoped_refptr<TileTask> required_for_activation_done_task =
      CreateTaskSetFinishedTask(
          &TileManager::DidFinishRunningTileTasksRequiredForActivation);
  scoped_refptr<TileTask> required_for_draw_done_task =
      CreateTaskSetFinishedTask(
          &TileManager::DidFinishRunningTileTasksRequiredForDraw);
  scoped_refptr<TileTask> all_done_task =
      CreateTaskSetFinishedTask(&TileManager::DidFinishRunningAllTileTasks);

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (auto& prioritized_tile : tiles_that_need_to_be_rasterized) {
    Tile* tile = prioritized_tile.tile();

    DCHECK(tile->draw_info().requires_resource());
    DCHECK(!tile->draw_info().resource_);

    if (!tile->raster_task_)
      tile->raster_task_ = CreateRasterTask(prioritized_tile);

    RasterTask* task = tile->raster_task_.get();
    DCHECK(!task->HasCompleted());

    if (tile->required_for_activation()) {
      required_for_activate_count++;
      graph_.edges.push_back(
          TaskGraph::Edge(task, required_for_activation_done_task.get()));
    }
    if (tile->required_for_draw()) {
      required_for_draw_count++;
      graph_.edges.push_back(
          TaskGraph::Edge(task, required_for_draw_done_task.get()));
    }
    all_count++;
    graph_.edges.push_back(TaskGraph::Edge(task, all_done_task.get()));

    bool high_priority =
        tile->required_for_draw() || tile->required_for_activation();
    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++,
                             use_gpu_rasterization_, high_priority);
  }

  // Insert nodes for our task completion tasks. We enqueue these using
  // FOREGROUND priority as they are relatively quick tasks and we'd like
  // to trigger our callbacks quickly to aid in scheduling.
  InsertNodeForTask(&graph_, required_for_activation_done_task.get(),
                    TASK_CATEGORY_FOREGROUND,
                    kRequiredForActivationDoneTaskPriority,
                    required_for_activate_count);
  InsertNodeForTask(&graph_, required_for_draw_done_task.get(),
                    TASK_CATEGORY_FOREGROUND, kRequiredForDrawDoneTaskPriority,
                    required_for_draw_count);
  InsertNodeForTask(&graph_, all_done_task.get(), TASK_CATEGORY_FOREGROUND,
                    kAllDoneTaskPriority, all_count);

  // We must reduce the amount of unused resoruces before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();
  image_decode_controller_.ReduceCacheUsage();

  // Schedule running of |raster_queue_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_queue_|.
  tile_task_runner_->ScheduleTasks(&graph_);

  // It's now safe to clean up orphan tasks as raster worker pool is not
  // allowed to keep around unreferenced raster tasks after ScheduleTasks() has
  // been called.
  orphan_tasks_.clear();

  // It's also now safe to replace our *_done_task_ tasks.
  required_for_activation_done_task_ =
      std::move(required_for_activation_done_task);
  required_for_draw_done_task_ = std::move(required_for_draw_done_task);
  all_done_task_ = std::move(all_done_task);

  did_check_for_completed_tasks_since_last_schedule_tasks_ = false;

  TRACE_EVENT_ASYNC_STEP_INTO1("cc", "ScheduledTasks", this, "running", "state",
                               ScheduledTasksStateAsValue());
}

scoped_refptr<RasterTask> TileManager::CreateRasterTask(
    const PrioritizedTile& prioritized_tile) {
  Tile* tile = prioritized_tile.tile();

  // Get the resource.
  uint64_t resource_content_id = 0;
  Resource* resource = nullptr;
  if (use_partial_raster_ && tile->invalidated_id()) {
    // TODO(danakj): For resources that are in use, we should still grab them
    // and copy from them instead of rastering everything. crbug.com/492754
    resource =
        resource_pool_->TryAcquireResourceWithContentId(tile->invalidated_id());
  }
  if (resource) {
    resource_content_id = tile->invalidated_id();
    DCHECK_EQ(DetermineResourceFormat(tile), resource->format());
  } else {
    resource = resource_pool_->AcquireResource(tile->desired_texture_size(),
                                               DetermineResourceFormat(tile));
  }

  // Create and queue all image decode tasks that this tile depends on.
  ImageDecodeTask::Vector decode_tasks;
  std::vector<DrawImage>& images = scheduled_draw_images_[tile->id()];
  images.clear();
  prioritized_tile.raster_source()->GetDiscardableImagesInRect(
      tile->enclosing_layer_rect(), tile->contents_scale(), &images);
  for (auto it = images.begin(); it != images.end();) {
    scoped_refptr<ImageDecodeTask> task;
    bool need_to_unref_when_finished =
        image_decode_controller_.GetTaskForImageAndRef(
            *it, prepare_tiles_count_, &task);
    if (task)
      decode_tasks.push_back(task);

    if (need_to_unref_when_finished)
      ++it;
    else
      it = images.erase(it);
  }

  return make_scoped_refptr(new RasterTaskImpl(
      resource, prioritized_tile.raster_source(), tile->content_rect(),
      tile->invalidated_content_rect(), tile->contents_scale(),
      prioritized_tile.priority().resolution, tile->layer_id(),
      prepare_tiles_count_, static_cast<const void*>(tile), tile->id(),
      tile->invalidated_id(), resource_content_id, tile->source_frame_number(),
      base::Bind(&TileManager::OnRasterTaskCompleted, base::Unretained(this),
                 tile->id(), resource),
      &decode_tasks));
}

void TileManager::OnRasterTaskCompleted(
    Tile::Id tile_id,
    Resource* resource,
    bool was_canceled) {
  DCHECK(tiles_.find(tile_id) != tiles_.end());

  Tile* tile = tiles_[tile_id];
  TileDrawInfo& draw_info = tile->draw_info();
  DCHECK(tile->raster_task_.get());
  orphan_tasks_.push_back(tile->raster_task_);
  tile->raster_task_ = nullptr;

  // Unref all the images.
  auto images_it = scheduled_draw_images_.find(tile->id());
  const std::vector<DrawImage>& images = images_it->second;
  for (const auto& image : images)
    image_decode_controller_.UnrefImage(image);
  scheduled_draw_images_.erase(images_it);

  if (was_canceled) {
    ++flush_stats_.canceled_count;
    // TODO(ericrk): If more partial raster work is done in the future, it may
    // be worth returning the resource to the pool with its previous ID (not
    // currently tracked). crrev.com/1370333002/#ps40001 has a possible method
    // of achieving this.
    resource_pool_->ReleaseResource(resource, 0 /* content_id */);
    return;
  }

  ++flush_stats_.completed_count;

  draw_info.set_use_resource();
  draw_info.resource_ = resource;
  draw_info.contents_swizzled_ = DetermineResourceRequiresSwizzle(tile);

  DCHECK(draw_info.IsReadyToDraw());
  draw_info.set_was_ever_ready_to_draw();

  client_->NotifyTileStateChanged(tile);
}

ScopedTilePtr TileManager::CreateTile(const Tile::CreateInfo& info,
                                      int layer_id,
                                      int source_frame_number,
                                      int flags) {
  // We need to have a tile task worker pool to do anything meaningful with
  // tiles.
  DCHECK(tile_task_runner_);
  ScopedTilePtr tile(
      new Tile(this, info, layer_id, source_frame_number, flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile.get();
  return tile;
}

void TileManager::SetTileTaskRunnerForTesting(
    TileTaskRunner* tile_task_runner) {
  tile_task_runner_ = tile_task_runner;
}

bool TileManager::AreRequiredTilesReadyToDraw(
    RasterTilePriorityQueue::Type type) const {
  scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority, type));
  // It is insufficient to check whether the raster queue we constructed is
  // empty. The reason for this is that there are situations (rasterize on
  // demand) when the tile both needs raster and it's ready to draw. Hence, we
  // have to iterate the queue to check whether the required tiles are ready to
  // draw.
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    if (!raster_priority_queue->Top().tile()->draw_info().IsReadyToDraw())
      return false;
  }

#if DCHECK_IS_ON()
  scoped_ptr<RasterTilePriorityQueue> all_queue(
      client_->BuildRasterQueue(global_state_.tree_priority, type));
  for (; !all_queue->IsEmpty(); all_queue->Pop()) {
    Tile* tile = all_queue->Top().tile();
    DCHECK(!tile->required_for_activation() ||
           tile->draw_info().IsReadyToDraw());
  }
#endif
  return true;
}

bool TileManager::IsReadyToActivate() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToActivate");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
}

bool TileManager::IsReadyToDraw() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToDraw");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
}

void TileManager::CheckAndIssueSignals() {
  TRACE_EVENT0("cc", "TileManager::CheckAndIssueSignals");
  tile_task_runner_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  // Ready to activate.
  if (signals_.ready_to_activate && !signals_.did_notify_ready_to_activate) {
    signals_.ready_to_activate = false;
    if (IsReadyToActivate()) {
      TRACE_EVENT0("disabled-by-default-cc.debug",
                   "TileManager::CheckAndIssueSignals - ready to activate");
      signals_.did_notify_ready_to_activate = true;
      client_->NotifyReadyToActivate();
    }
  }

  // Ready to draw.
  if (signals_.ready_to_draw && !signals_.did_notify_ready_to_draw) {
    signals_.ready_to_draw = false;
    if (IsReadyToDraw()) {
      TRACE_EVENT0("disabled-by-default-cc.debug",
                   "TileManager::CheckAndIssueSignals - ready to draw");
      signals_.did_notify_ready_to_draw = true;
      client_->NotifyReadyToDraw();
    }
  }

  // All tile tasks completed.
  if (signals_.all_tile_tasks_completed &&
      !signals_.did_notify_all_tile_tasks_completed) {
    signals_.all_tile_tasks_completed = false;
    if (!has_scheduled_tile_tasks_) {
      TRACE_EVENT0(
          "disabled-by-default-cc.debug",
          "TileManager::CheckAndIssueSignals - all tile tasks completed");
      signals_.did_notify_all_tile_tasks_completed = true;
      client_->NotifyAllTileTasksCompleted();
    }
  }
}

void TileManager::CheckIfMoreTilesNeedToBePrepared() {
  tile_task_runner_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  PrioritizedTileVector tiles_that_need_to_be_rasterized;
  scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority,
                                RasterTilePriorityQueue::Type::ALL));
  AssignGpuMemoryToTiles(raster_priority_queue.get(),
                         scheduled_raster_task_limit_,
                         &tiles_that_need_to_be_rasterized);

  // Inform the client that will likely require a draw if the highest priority
  // tile that will be rasterized is required for draw.
  client_->SetIsLikelyToRequireADraw(
      !tiles_that_need_to_be_rasterized.empty() &&
      tiles_that_need_to_be_rasterized.front().tile()->required_for_draw());

  // |tiles_that_need_to_be_rasterized| will be empty when we reach a
  // steady memory state. Keep scheduling tasks until we reach this state.
  if (!tiles_that_need_to_be_rasterized.empty()) {
    ScheduleTasks(tiles_that_need_to_be_rasterized);
    return;
  }

  FreeResourcesForReleasedTiles();

  resource_pool_->ReduceResourceUsage();
  image_decode_controller_.ReduceCacheUsage();

  signals_.all_tile_tasks_completed = true;
  signals_check_notifier_.Schedule();

  // We don't reserve memory for required-for-activation tiles during
  // accelerated gestures, so we just postpone activation when we don't
  // have these tiles, and activate after the accelerated gesture.
  // Likewise if we don't allow any tiles (as is the case when we're
  // invisible), if we have tiles that aren't ready, then we shouldn't
  // activate as activation can cause checkerboards.
  bool wait_for_all_required_tiles =
      global_state_.tree_priority == SMOOTHNESS_TAKES_PRIORITY ||
      global_state_.memory_limit_policy == ALLOW_NOTHING;

  // If we have tiles left to raster for activation, and we don't allow
  // activating without them, then skip activation and return early.
  if (wait_for_all_required_tiles)
    return;

  // Mark any required tiles that have not been been assigned memory after
  // reaching a steady memory state as OOM. This ensures that we activate/draw
  // even when OOM. Note that we can't reuse the queue we used for
  // AssignGpuMemoryToTiles, since the AssignGpuMemoryToTiles call could have
  // evicted some tiles that would not be picked up by the old raster queue.
  bool need_to_signal_activate = MarkTilesOutOfMemory(client_->BuildRasterQueue(
      global_state_.tree_priority,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION));
  bool need_to_signal_draw = MarkTilesOutOfMemory(client_->BuildRasterQueue(
      global_state_.tree_priority,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW));

  DCHECK(IsReadyToActivate());
  DCHECK(IsReadyToDraw());
  signals_.ready_to_activate = need_to_signal_activate;
  signals_.ready_to_draw = need_to_signal_draw;
  // TODO(ericrk): Investigate why we need to schedule this (not just call it
  // inline). http://crbug.com/498439
  signals_check_notifier_.Schedule();
}

bool TileManager::MarkTilesOutOfMemory(
    scoped_ptr<RasterTilePriorityQueue> queue) const {
  // Mark required tiles as OOM so that we can activate/draw without them.
  if (queue->IsEmpty())
    return false;

  for (; !queue->IsEmpty(); queue->Pop()) {
    Tile* tile = queue->Top().tile();
    if (tile->draw_info().IsReadyToDraw())
      continue;
    tile->draw_info().set_oom();
    client_->NotifyTileStateChanged(tile);
  }
  return true;
}

ResourceFormat TileManager::DetermineResourceFormat(const Tile* tile) const {
  return tile_task_runner_->GetResourceFormat(!tile->is_opaque());
}

bool TileManager::DetermineResourceRequiresSwizzle(const Tile* tile) const {
  return tile_task_runner_->GetResourceRequiresSwizzle(!tile->is_opaque());
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
TileManager::ScheduledTasksStateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->BeginDictionary("tasks_pending");
  state->SetBoolean("ready_to_activate", signals_.ready_to_activate);
  state->SetBoolean("ready_to_draw", signals_.ready_to_draw);
  state->SetBoolean("all_tile_tasks_completed",
                    signals_.all_tile_tasks_completed);
  state->EndDictionary();
  return state;
}

// Utility function that can be used to create a "Task set finished" task that
// posts |callback| to |task_runner| when run.
scoped_refptr<TileTask> TileManager::CreateTaskSetFinishedTask(
    void (TileManager::*callback)()) {
  return make_scoped_refptr(new TaskSetFinishedTaskImpl(
      task_runner_.get(),
      base::Bind(callback, task_set_finished_weak_ptr_factory_.GetWeakPtr())));
}

TileManager::MemoryUsage::MemoryUsage()
    : memory_bytes_(0), resource_count_(0) {}

TileManager::MemoryUsage::MemoryUsage(size_t memory_bytes,
                                      size_t resource_count)
    : memory_bytes_(static_cast<int64_t>(memory_bytes)),
      resource_count_(static_cast<int>(resource_count)) {
  // MemoryUsage is constructed using size_ts, since it deals with memory and
  // the inputs are typically size_t. However, during the course of usage (in
  // particular operator-=) can cause internal values to become negative. Thus,
  // member variables are signed.
  DCHECK_LE(memory_bytes,
            static_cast<size_t>(std::numeric_limits<int64_t>::max()));
  DCHECK_LE(resource_count,
            static_cast<size_t>(std::numeric_limits<int>::max()));
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromConfig(
    const gfx::Size& size,
    ResourceFormat format) {
  // We can use UncheckedSizeInBytes here since this is used with a tile
  // size which is determined by the compositor (it's at most max texture size).
  return MemoryUsage(ResourceUtil::UncheckedSizeInBytes<size_t>(size, format),
                     1);
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromTile(const Tile* tile) {
  const TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_) {
    return MemoryUsage::FromConfig(draw_info.resource_->size(),
                                   draw_info.resource_->format());
  }
  return MemoryUsage();
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator+=(
    const MemoryUsage& other) {
  memory_bytes_ += other.memory_bytes_;
  resource_count_ += other.resource_count_;
  return *this;
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator-=(
    const MemoryUsage& other) {
  memory_bytes_ -= other.memory_bytes_;
  resource_count_ -= other.resource_count_;
  return *this;
}

TileManager::MemoryUsage TileManager::MemoryUsage::operator-(
    const MemoryUsage& other) {
  MemoryUsage result = *this;
  result -= other;
  return result;
}

bool TileManager::MemoryUsage::Exceeds(const MemoryUsage& limit) const {
  return memory_bytes_ > limit.memory_bytes_ ||
         resource_count_ > limit.resource_count_;
}

TileManager::Signals::Signals() {
  reset();
}

void TileManager::Signals::reset() {
  ready_to_activate = false;
  did_notify_ready_to_activate = false;
  ready_to_draw = false;
  did_notify_ready_to_draw = false;
  all_tile_tasks_completed = false;
  did_notify_all_tile_tasks_completed = false;
}

}  // namespace cc
