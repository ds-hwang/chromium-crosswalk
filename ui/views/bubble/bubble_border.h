// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

class SkPath;

namespace gfx {
class Path;
class Rect;
}

namespace views {
class Painter;

namespace internal {

// A helper that combines each border image-set painter with arrows and metrics.
struct BorderImages {
  BorderImages(const int border_image_ids[],
               const int arrow_image_ids[],
               int border_interior_thickness,
               int arrow_interior_thickness,
               int corner_radius);
  virtual ~BorderImages();

  scoped_ptr<Painter> border_painter;
  gfx::ImageSkia left_arrow;
  gfx::ImageSkia top_arrow;
  gfx::ImageSkia right_arrow;
  gfx::ImageSkia bottom_arrow;

  // The thickness of border and arrow images and their interior areas.
  // Thickness is the width of left/right and the height of top/bottom images.
  // The interior is measured without including stroke or shadow pixels. The tip
  // of the arrow is |arrow_interior_thickness| from the border and the base is
  // always twice that; drawn in the background color.
  int border_thickness;
  int border_interior_thickness;
  int arrow_thickness;
  int arrow_interior_thickness;

  // Width of an arrow (on the horizontal), including any shadows. Defaults to
  // the width of the |top_arrow| asset.
  int arrow_width;

  // The corner radius of the bubble's rounded-rect interior area.
  int corner_radius;
};

}  // namespace internal

// Renders a border, with optional arrow, and a custom dropshadow.
// This can be used to produce floating "bubble" objects with rounded corners.
class VIEWS_EXPORT BubbleBorder : public Border {
 public:
  // Possible locations for the (optional) arrow.
  // 0 bit specifies left or right.
  // 1 bit specifies top or bottom.
  // 2 bit specifies horizontal or vertical.
  // 3 bit specifies whether the arrow at the center of its residing edge.
  enum ArrowMask {
    RIGHT    = 0x01,
    BOTTOM   = 0x02,
    VERTICAL = 0x04,
    CENTER   = 0x08,
  };

  enum Arrow {
    TOP_LEFT      = 0,
    TOP_RIGHT     = RIGHT,
    BOTTOM_LEFT   = BOTTOM,
    BOTTOM_RIGHT  = BOTTOM | RIGHT,
    LEFT_TOP      = VERTICAL,
    RIGHT_TOP     = VERTICAL | RIGHT,
    LEFT_BOTTOM   = VERTICAL | BOTTOM,
    RIGHT_BOTTOM  = VERTICAL | BOTTOM | RIGHT,
    TOP_CENTER    = CENTER,
    BOTTOM_CENTER = CENTER | BOTTOM,
    LEFT_CENTER   = CENTER | VERTICAL,
    RIGHT_CENTER  = CENTER | VERTICAL | RIGHT,
    NONE  = 16,  // No arrow. Positioned under the supplied rect.
    FLOAT = 17,  // No arrow. Centered over the supplied rect.
  };

  enum Shadow {
    NO_SHADOW = 0,
    NO_SHADOW_OPAQUE_BORDER,
    BIG_SHADOW,
    SMALL_SHADOW,
    NO_ASSETS,
    SHADOW_COUNT,
  };

  // The position of the bubble in relation to the anchor.
  enum BubbleAlignment {
    // The tip of the arrow points to the middle of the anchor.
    ALIGN_ARROW_TO_MID_ANCHOR,
    // The edge nearest to the arrow is lined up with the edge of the anchor.
    ALIGN_EDGE_TO_ANCHOR_EDGE,
  };

  // The way the arrow should be painted.
  enum ArrowPaintType {
    // Fully render the arrow.
    PAINT_NORMAL,
    // Leave space for the arrow, but do not paint it.
    PAINT_TRANSPARENT,
    // Neither paint nor leave space for the arrow.
    PAINT_NONE,
  };

  BubbleBorder(Arrow arrow, Shadow shadow, SkColor color);
  ~BubbleBorder() override;

  // Returns the radius of the corner of the border.
  // TODO(xiyuan): Get rid of this since it's part of BorderImages now?
  static int GetCornerRadius() {
    // We can't safely calculate a border radius by comparing the sizes of the
    // side and corner images, because either may have been extended in various
    // directions in order to do more subtle dropshadow fading or other effects.
    // So we hardcode the most accurate value.
    return 4;
  }

  static bool has_arrow(Arrow a) { return a < NONE; }

  static bool is_arrow_on_left(Arrow a) {
    return has_arrow(a) && (a == LEFT_CENTER || !(a & (RIGHT | CENTER)));
  }

  static bool is_arrow_on_top(Arrow a) {
    return has_arrow(a) && (a == TOP_CENTER || !(a & (BOTTOM | CENTER)));
  }

  static bool is_arrow_on_horizontal(Arrow a) {
    return a >= NONE ? false : !(a & VERTICAL);
  }

  static bool is_arrow_at_center(Arrow a) {
    return has_arrow(a) && !!(a & CENTER);
  }

  static Arrow horizontal_mirror(Arrow a) {
    return (a == TOP_CENTER || a == BOTTOM_CENTER || a >= NONE) ?
        a : static_cast<Arrow>(a ^ RIGHT);
  }

  static Arrow vertical_mirror(Arrow a) {
    return (a == LEFT_CENTER || a == RIGHT_CENTER || a >= NONE) ?
        a : static_cast<Arrow>(a ^ BOTTOM);
  }

  // Get or set the arrow type.
  void set_arrow(Arrow arrow) { arrow_ = arrow; }
  Arrow arrow() const { return arrow_; }

  // Get or set the bubble alignment.
  void set_alignment(BubbleAlignment alignment) { alignment_ = alignment; }
  BubbleAlignment alignment() const { return alignment_; }

  // Get the shadow type.
  Shadow shadow() const { return shadow_; }

  // Get or set the background color for the bubble and arrow body.
  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  // If true, the background color should be determined by the host's
  // NativeTheme.
  void set_use_theme_background_color(bool use_theme_background_color) {
    use_theme_background_color_ = use_theme_background_color;
  }
  bool use_theme_background_color() { return use_theme_background_color_; }

  // Sets a desired pixel distance between the arrow tip and the outside edge of
  // the neighboring border image. For example:    |----offset----|
  // '(' represents shadow around the '{' edge:    ((({           ^   })))
  // The arrow will still anchor to the same location but the bubble will shift
  // location to place the arrow |offset| pixels from the perpendicular edge.
  void set_arrow_offset(int offset) { arrow_offset_ = offset; }

  // Sets the way the arrow is actually painted.  Default is PAINT_NORMAL.
  void set_paint_arrow(ArrowPaintType value) { arrow_paint_type_ = value; }

  // Get the desired widget bounds (in screen coordinates) given the anchor rect
  // and bubble content size; calculated from shadow and arrow image dimensions.
  virtual gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                              const gfx::Size& contents_size) const;

  // Get the border exterior thickness, including stroke and shadow, in pixels.
  int GetBorderThickness() const;

  // Returns the corner radius of the current image set.
  int GetBorderCornerRadius() const;

  // Gets the arrow offset to use.
  int GetArrowOffset(const gfx::Size& border_size) const;

  // Retreives the arrow path given |view_bounds|. |view_bounds| should be local
  // bounds of the view.
  // Returns false if |path| is unchanged, which is the case when there is no
  // painted arrow.
  // The returned path does not account for arrow stroke and shadow.
  bool GetArrowPath(const gfx::Rect& view_bounds, gfx::Path* path) const;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetSizeForContentsSizeTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetBoundsOriginTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, ShadowTypes);

  // The border and arrow stroke size used in image assets, in pixels.
  static const int kStroke;

  gfx::Size GetSizeForContentsSize(const gfx::Size& contents_size) const;
  gfx::ImageSkia* GetArrowImage() const;
  gfx::Rect GetArrowRect(const gfx::Rect& bounds) const;
  void GetArrowPathFromArrowBounds(const gfx::Rect& arrow_bounds,
                                   SkPath* path) const;
  void DrawArrow(gfx::Canvas* canvas, const gfx::Rect& arrow_bounds) const;

  internal::BorderImages* GetImagesForTest() const;

  Arrow arrow_;
  int arrow_offset_;
  ArrowPaintType arrow_paint_type_;
  BubbleAlignment alignment_;
  Shadow shadow_;
  internal::BorderImages* images_;
  SkColor background_color_;
  bool use_theme_background_color_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder);
};

// A Background that clips itself to the specified BubbleBorder and uses
// the background color of the BubbleBorder.
class VIEWS_EXPORT BubbleBackground : public Background {
 public:
  explicit BubbleBackground(BubbleBorder* border) : border_(border) {}

  // Overridden from Background:
  void Paint(gfx::Canvas* canvas, View* view) const override;

 private:
  BubbleBorder* border_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBackground);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
