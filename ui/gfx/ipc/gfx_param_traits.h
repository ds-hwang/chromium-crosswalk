// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_GFX_PARAM_TRAITS_H_
#define UI_GFX_IPC_GFX_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/ipc/gfx_ipc_export.h"
#include "ui/gfx/ipc/gfx_param_traits_macros.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "ui/gfx/mac/io_surface.h"
#endif

class SkBitmap;

namespace gfx {
class Point;
class PointF;
class Point3F;
class Range;
class Rect;
class RectF;
class ScrollOffset;
class Size;
class SizeF;
class Vector2d;
class Vector2dF;
}  // namespace gfx

namespace IPC {

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Point> {
  typedef gfx::Point param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::PointF> {
  typedef gfx::PointF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Point3F> {
  typedef gfx::Point3F param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Size> {
  typedef gfx::Size param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::SizeF> {
  typedef gfx::SizeF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Vector2d> {
  typedef gfx::Vector2d param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Vector2dF> {
  typedef gfx::Vector2dF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Rect> {
  typedef gfx::Rect param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::RectF> {
  typedef gfx::RectF param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<SkBitmap> {
  typedef SkBitmap param_type;
  static void Write(base::Pickle* m, const param_type& p);

  // Note: This function expects parameter |r| to be of type &SkBitmap since
  // r->SetConfig() and r->SetPixels() are called.
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);

  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::Range> {
  typedef gfx::Range param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::ScrollOffset> {
  typedef gfx::ScrollOffset param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_MACOSX) && !defined(OS_IOS)
template <>
struct GFX_IPC_EXPORT ParamTraits<gfx::ScopedRefCountedIOSurfaceMachPort> {
  typedef gfx::ScopedRefCountedIOSurfaceMachPort param_type;
  static void Write(base::Pickle* m, const param_type p);
  // Note: Read() passes ownership of the Mach send right from the IPC message
  // to the ScopedRefCountedIOSurfaceMachPort. Therefore, Read() may only be
  // called once for a given message, otherwise the singular right will be
  // managed and released by two objects.
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace IPC

#endif  // UI_GFX_IPC_GFX_PARAM_TRAITS_H_
