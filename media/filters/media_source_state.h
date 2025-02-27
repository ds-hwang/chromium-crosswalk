// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MEDIA_SOURCE_STATE_H_
#define MEDIA_FILTERS_MEDIA_SOURCE_STATE_H_

#include <list>

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

using base::TimeDelta;

class ChunkDemuxerStream;
class FrameProcessor;

// Contains state belonging to a source id.
class MEDIA_EXPORT MediaSourceState {
 public:
  // Callback signature used to create ChunkDemuxerStreams.
  typedef base::Callback<ChunkDemuxerStream*(DemuxerStream::Type)>
      CreateDemuxerStreamCB;

  typedef base::Closure InitSegmentReceivedCB;

  typedef base::Callback<void(ChunkDemuxerStream*, const TextTrackConfig&)>
      NewTextTrackCB;

  MediaSourceState(scoped_ptr<StreamParser> stream_parser,
                   scoped_ptr<FrameProcessor> frame_processor,
                   const CreateDemuxerStreamCB& create_demuxer_stream_cb,
                   const scoped_refptr<MediaLog>& media_log);

  ~MediaSourceState();

  void Init(const StreamParser::InitCB& init_cb,
            bool allow_audio,
            bool allow_video,
            const StreamParser::EncryptedMediaInitDataCB&
                encrypted_media_init_data_cb,
            const NewTextTrackCB& new_text_track_cb);

  // Appends new data to the StreamParser.
  // Returns true if the data was successfully appended. Returns false if an
  // error occurred. |*timestamp_offset| is used and possibly updated by the
  // append. |append_window_start| and |append_window_end| correspond to the MSE
  // spec's similarly named source buffer attributes that are used in coded
  // frame processing. |init_segment_received_cb| is run for each new fully
  // parsed initialization segment.
  bool Append(const uint8_t* data,
              size_t length,
              TimeDelta append_window_start,
              TimeDelta append_window_end,
              TimeDelta* timestamp_offset,
              const InitSegmentReceivedCB& init_segment_received_cb);

  // Aborts the current append sequence and resets the parser.
  void ResetParserState(TimeDelta append_window_start,
                        TimeDelta append_window_end,
                        TimeDelta* timestamp_offset);

  // Calls Remove(|start|, |end|, |duration|) on all
  // ChunkDemuxerStreams managed by this object.
  void Remove(TimeDelta start, TimeDelta end, TimeDelta duration);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  bool EvictCodedFrames(DecodeTimestamp media_time, size_t newDataSize);

  // Returns true if currently parsing a media segment, or false otherwise.
  bool parsing_media_segment() const { return parsing_media_segment_; }

  // Sets |frame_processor_|'s sequence mode to |sequence_mode|.
  void SetSequenceMode(bool sequence_mode);

  // Signals the coded frame processor to update its group start timestamp to be
  // |timestamp_offset| if it is in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(base::TimeDelta timestamp_offset);

  // Returns the range of buffered data in this source, capped at |duration|.
  // |ended| - Set to true if end of stream has been signaled and the special
  // end of stream range logic needs to be executed.
  Ranges<TimeDelta> GetBufferedRanges(TimeDelta duration, bool ended) const;

  // Returns the highest buffered duration across all streams managed
  // by this object.
  // Returns TimeDelta() if none of the streams contain buffered data.
  TimeDelta GetMaxBufferedDuration() const;

  // Helper methods that call methods with similar names on all the
  // ChunkDemuxerStreams managed by this object.
  void StartReturningData();
  void AbortReads();
  void Seek(TimeDelta seek_time);
  void CompletePendingReadIfPossible();
  void OnSetDuration(TimeDelta duration);
  void MarkEndOfStream();
  void UnmarkEndOfStream();
  void Shutdown();
  // Sets the memory limit on each stream of a specific type.
  // |memory_limit| is the maximum number of bytes each stream of type |type|
  // is allowed to hold in its buffer.
  void SetMemoryLimits(DemuxerStream::Type type, size_t memory_limit);
  bool IsSeekWaitingForData() const;

  typedef std::list<Ranges<TimeDelta>> RangesList;
  static Ranges<TimeDelta> ComputeRangesIntersection(
      const RangesList& activeRanges,
      bool ended);

 private:
  // Called by the |stream_parser_| when a new initialization segment is
  // encountered.
  // Returns true on a successful call. Returns false if an error occurred while
  // processing decoder configurations.
  bool OnNewConfigs(bool allow_audio,
                    bool allow_video,
                    const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config,
                    const StreamParser::TextTrackConfigMap& text_configs);

  // Called by the |stream_parser_| at the beginning of a new media segment.
  void OnNewMediaSegment();

  // Called by the |stream_parser_| at the end of a media segment.
  void OnEndOfMediaSegment();

  // Called by the |stream_parser_| when new buffers have been parsed.
  // It processes the new buffers using |frame_processor_|, which includes
  // appending the processed frames to associated demuxer streams for each
  // frame's track.
  // Returns true on a successful call. Returns false if an error occurred while
  // processing the buffers.
  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers,
                    const StreamParser::TextBufferQueueMap& text_map);

  void OnSourceInitDone(const StreamParser::InitParameters& params);

  // EstimateVideoDataSize uses some heuristics to estimate the size of the
  // video size in the chunk of muxed audio/video data without parsing it.
  // This is used by EvictCodedFrames algorithm, which happens before Append
  // (and therefore before parsing is performed) to prepare space for new data.
  size_t EstimateVideoDataSize(size_t muxed_data_chunk_size) const;

  // Tracks the number of MEDIA_LOGs emitted for segments missing expected audio
  // or video blocks. Useful to prevent log spam.
  int num_missing_track_logs_ = 0;

  CreateDemuxerStreamCB create_demuxer_stream_cb_;
  NewTextTrackCB new_text_track_cb_;

  // During Append(), if OnNewBuffers() coded frame processing updates the
  // timestamp offset then |*timestamp_offset_during_append_| is also updated
  // so Append()'s caller can know the new offset. This pointer is only non-NULL
  // during the lifetime of an Append() call.
  TimeDelta* timestamp_offset_during_append_;

  // During Append(), coded frame processing triggered by OnNewBuffers()
  // requires these two attributes. These are only valid during the lifetime of
  // an Append() call.
  TimeDelta append_window_start_during_append_;
  TimeDelta append_window_end_during_append_;

  // Keeps track of whether a media segment is being parsed.
  bool parsing_media_segment_;

  // Valid only while |parsing_media_segment_| is true. These flags enable
  // warning when at least one frame for each A/V track is not in a parsed media
  // segment.
  bool media_segment_contained_audio_frame_;
  bool media_segment_contained_video_frame_;

  // The object used to parse appended data.
  scoped_ptr<StreamParser> stream_parser_;

  ChunkDemuxerStream* audio_;  // Not owned by |this|.
  ChunkDemuxerStream* video_;  // Not owned by |this|.

  typedef std::map<StreamParser::TrackId, ChunkDemuxerStream*> TextStreamMap;
  TextStreamMap text_stream_map_;  // |this| owns the map's stream pointers.

  scoped_ptr<FrameProcessor> frame_processor_;
  scoped_refptr<MediaLog> media_log_;
  StreamParser::InitCB init_cb_;

  // During Append(), OnNewConfigs() will trigger the initialization segment
  // received algorithm. This callback is only non-NULL during the lifetime of
  // an Append() call. Note, the MSE spec explicitly disallows this algorithm
  // during an Abort(), since Abort() is allowed only to emit coded frames, and
  // only if the parser is PARSING_MEDIA_SEGMENT (not an INIT segment).
  InitSegmentReceivedCB init_segment_received_cb_;

  // Indicates that timestampOffset should be updated automatically during
  // OnNewBuffers() based on the earliest end timestamp of the buffers provided.
  // TODO(wolenetz): Refactor this function while integrating April 29, 2014
  // changes to MSE spec. See http://crbug.com/371499.
  bool auto_update_timestamp_offset_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceState);
};

}  // namespace media

#endif  // MEDIA_FILTERS_MEDIA_SOURCE_STATE_H_
