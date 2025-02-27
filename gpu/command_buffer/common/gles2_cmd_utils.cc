// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include <sstream>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "base/numerics/safe_math.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

namespace gl_error_bit {
enum GLErrorBit {
  kNoError = 0,
  kInvalidEnum = (1 << 0),
  kInvalidValue = (1 << 1),
  kInvalidOperation = (1 << 2),
  kOutOfMemory = (1 << 3),
  kInvalidFrameBufferOperation = (1 << 4),
  kContextLost = (1 << 5)
};
}

int GLES2Util::GLGetNumValuesReturned(int id) const {
  switch (id) {
    // -- glGetBooleanv, glGetFloatv, glGetIntergerv
    case GL_ACTIVE_TEXTURE:
      return 1;
    case GL_ALIASED_LINE_WIDTH_RANGE:
      return 2;
    case GL_ALIASED_POINT_SIZE_RANGE:
      return 2;
    case GL_ALPHA_BITS:
      return 1;
    case GL_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_BLEND:
      return 1;
    case GL_BLEND_COLOR:
      return 4;
    case GL_BLEND_DST_ALPHA:
      return 1;
    case GL_BLEND_DST_RGB:
      return 1;
    case GL_BLEND_EQUATION_ALPHA:
      return 1;
    case GL_BLEND_EQUATION_RGB:
      return 1;
    case GL_BLEND_SRC_ALPHA:
      return 1;
    case GL_BLEND_SRC_RGB:
      return 1;
    case GL_BLUE_BITS:
      return 1;
    case GL_COLOR_CLEAR_VALUE:
      return 4;
    case GL_COLOR_WRITEMASK:
      return 4;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      return num_compressed_texture_formats_;
    case GL_CULL_FACE:
      return 1;
    case GL_CULL_FACE_MODE:
      return 1;
    case GL_CURRENT_PROGRAM:
      return 1;
    case GL_DEPTH_BITS:
      return 1;
    case GL_DEPTH_CLEAR_VALUE:
      return 1;
    case GL_DEPTH_FUNC:
      return 1;
    case GL_DEPTH_RANGE:
      return 2;
    case GL_DEPTH_TEST:
      return 1;
    case GL_DEPTH_WRITEMASK:
      return 1;
    case GL_DITHER:
      return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_FRAMEBUFFER_BINDING:
      return 1;
    case GL_FRONT_FACE:
      return 1;
    case GL_GENERATE_MIPMAP_HINT:
      return 1;
    case GL_GREEN_BITS:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      return 1;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      return 1;
    case GL_LINE_WIDTH:
      return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      return 1;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_RENDERBUFFER_SIZE:
      return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_TEXTURE_SIZE:
      return 1;
    case GL_MAX_VARYING_VECTORS:
      return 1;
    case GL_MAX_VERTEX_ATTRIBS:
      return 1;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      return 1;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      return 1;
    case GL_MAX_VIEWPORT_DIMS:
      return 2;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      return 1;
    case GL_NUM_SHADER_BINARY_FORMATS:
      return 1;
    case GL_PACK_ALIGNMENT:
      return 1;
    case GL_POLYGON_OFFSET_FACTOR:
      return 1;
    case GL_POLYGON_OFFSET_FILL:
      return 1;
    case GL_POLYGON_OFFSET_UNITS:
      return 1;
    case GL_RED_BITS:
      return 1;
    case GL_RENDERBUFFER_BINDING:
      return 1;
    case GL_SAMPLE_BUFFERS:
      return 1;
    case GL_SAMPLE_COVERAGE_INVERT:
      return 1;
    case GL_SAMPLE_COVERAGE_VALUE:
      return 1;
    case GL_SAMPLES:
      return 1;
    case GL_SCISSOR_BOX:
      return 4;
    case GL_SCISSOR_TEST:
      return 1;
    case GL_SHADER_BINARY_FORMATS:
      return num_shader_binary_formats_;
    case GL_SHADER_COMPILER:
      return 1;
    case GL_STENCIL_BACK_FAIL:
      return 1;
    case GL_STENCIL_BACK_FUNC:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_BACK_REF:
      return 1;
    case GL_STENCIL_BACK_VALUE_MASK:
      return 1;
    case GL_STENCIL_BACK_WRITEMASK:
      return 1;
    case GL_STENCIL_BITS:
      return 1;
    case GL_STENCIL_CLEAR_VALUE:
      return 1;
    case GL_STENCIL_FAIL:
      return 1;
    case GL_STENCIL_FUNC:
      return 1;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return 1;
    case GL_STENCIL_PASS_DEPTH_PASS:
      return 1;
    case GL_STENCIL_REF:
      return 1;
    case GL_STENCIL_TEST:
      return 1;
    case GL_STENCIL_VALUE_MASK:
      return 1;
    case GL_STENCIL_WRITEMASK:
      return 1;
    case GL_SUBPIXEL_BITS:
      return 1;
    case GL_TEXTURE_BINDING_2D:
      return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      return 1;
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      return 1;
    case GL_TEXTURE_BINDING_RECTANGLE_ARB:
      return 1;
    case GL_UNPACK_ALIGNMENT:
      return 1;
    case GL_VIEWPORT:
      return 4;

    // ES3
    case GL_COPY_READ_BUFFER_BINDING:
      return 1;
    case GL_COPY_WRITE_BUFFER_BINDING:
      return 1;
    case GL_PIXEL_PACK_BUFFER_BINDING:
      return 1;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
      return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      return 1;
    case GL_UNIFORM_BUFFER_BINDING:
      return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      return 1;
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      return 1;
    case GL_UNIFORM_BUFFER_SIZE:
      return 1;
    case GL_UNIFORM_BUFFER_START:
      return 1;

    // -- glGetBooleanv, glGetFloatv, glGetIntergerv with
    //    GL_CHROMIUM_framebuffer_multisample
    case GL_MAX_SAMPLES_EXT:
      return 1;
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
      return 1;

    // -- glGetBufferParameteriv
    case GL_BUFFER_SIZE:
      return 1;
    case GL_BUFFER_USAGE:
      return 1;

    // ES3
    case GL_BUFFER_MAPPED:
      return 1;
    case GL_BUFFER_ACCESS_FLAGS:
      return 1;
    case GL_BUFFER_MAP_LENGTH:
      return 1;
    case GL_BUFFER_MAP_OFFSET:
      return 1;

    // -- glGetFramebufferAttachmentParameteriv
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      return 1;
    // -- glGetFramebufferAttachmentParameteriv with
    //    GL_EXT_multisampled_render_to_texture
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT:
      return 1;
    // -- glGetFramebufferAttachmentParameteriv with
    //    GL_EXT_sRGB
    case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
      return 1;
    // ES3
    case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
      return 1;
    case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
      return 1;

    // -- glGetProgramiv
    case GL_DELETE_STATUS:
      return 1;
    case GL_LINK_STATUS:
      return 1;
    case GL_VALIDATE_STATUS:
      return 1;
    case GL_INFO_LOG_LENGTH:
      return 1;
    case GL_ATTACHED_SHADERS:
      return 1;
    case GL_ACTIVE_ATTRIBUTES:
      return 1;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      return 1;
    case GL_ACTIVE_UNIFORMS:
      return 1;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      return 1;

    // -- glGetRenderbufferAttachmentParameteriv
    case GL_RENDERBUFFER_WIDTH:
      return 1;
    case GL_RENDERBUFFER_HEIGHT:
      return 1;
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      return 1;
    case GL_RENDERBUFFER_RED_SIZE:
      return 1;
    case GL_RENDERBUFFER_GREEN_SIZE:
      return 1;
    case GL_RENDERBUFFER_BLUE_SIZE:
      return 1;
    case GL_RENDERBUFFER_ALPHA_SIZE:
      return 1;
    case GL_RENDERBUFFER_DEPTH_SIZE:
      return 1;
    case GL_RENDERBUFFER_STENCIL_SIZE:
      return 1;
    // -- glGetRenderbufferAttachmentParameteriv with
    //    GL_EXT_multisampled_render_to_texture
    case GL_RENDERBUFFER_SAMPLES_EXT:
      return 1;

    // -- glGetShaderiv
    case GL_SHADER_TYPE:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_DELETE_STATUS:
    //   return 1;
    case GL_COMPILE_STATUS:
      return 1;
    // Already defined under glGetFramebufferAttachemntParameteriv.
    // case GL_INFO_LOG_LENGTH:
    //   return 1;
    case GL_SHADER_SOURCE_LENGTH:
      return 1;
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      return 1;

    // -- glGetTexParameterfv, glGetTexParameteriv
    case GL_TEXTURE_MAG_FILTER:
      return 1;
    case GL_TEXTURE_MIN_FILTER:
      return 1;
    case GL_TEXTURE_WRAP_R:
      return 1;
    case GL_TEXTURE_WRAP_S:
      return 1;
    case GL_TEXTURE_WRAP_T:
      return 1;
    case GL_TEXTURE_COMPARE_FUNC:
      return 1;
    case GL_TEXTURE_COMPARE_MODE:
      return 1;
    case GL_TEXTURE_MAX_LOD:
      return 1;
    case GL_TEXTURE_MIN_LOD:
      return 1;
    case GL_TEXTURE_BASE_LEVEL:
      return 1;
    case GL_TEXTURE_MAX_LEVEL:
      return 1;
    case GL_TEXTURE_IMMUTABLE_FORMAT:
      return 1;
    case GL_TEXTURE_IMMUTABLE_LEVELS:
      return 1;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      return 1;

    // -- glGetVertexAttrib
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      return 1;
    case GL_CURRENT_VERTEX_ATTRIB:
      return 4;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      return 1;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      return 1;

    // -- glGetSynciv
    case GL_OBJECT_TYPE:
      return 1;
    case GL_SYNC_STATUS:
      return 1;
    case GL_SYNC_CONDITION:
      return 1;
    case GL_SYNC_FLAGS:
      return 1;

    // -- glHint with GL_OES_standard_derivatives
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      return 1;

    // Chromium internal bind_generates_resource query
    case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
      return 1;

    // bad enum
    default:
      return 0;
  }
}

namespace {

// Return the number of elements per group of a specified format.
int ElementsPerGroup(int format, int type) {
  switch (type) {
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
       return 1;
    default:
       break;
    }

    switch (format) {
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_SRGB_EXT:
       return 3;
    case GL_LUMINANCE_ALPHA:
    case GL_RG_EXT:
    case GL_RG_INTEGER:
       return 2;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_BGRA_EXT:
    case GL_SRGB_ALPHA_EXT:
       return 4;
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT24_OES:
    case GL_DEPTH_COMPONENT32_OES:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH24_STENCIL8_OES:
    case GL_DEPTH_STENCIL_OES:
    case GL_RED_EXT:
    case GL_RED_INTEGER:
       return 1;
    default:
       return 0;
  }
}

// Return the number of bytes per element, based on the element type.
int BytesPerElement(int type) {
  switch (type) {
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
      return 8;
    case GL_FLOAT:
    case GL_UNSIGNED_INT_24_8_OES:
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
      return 4;
    case GL_HALF_FLOAT:
    case GL_HALF_FLOAT_OES:
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
       return 2;
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
       return 1;
    default:
       return 0;
  }
}

}  // anonymous namespace

uint32_t GLES2Util::ComputeImageGroupSize(int format, int type) {
  int bytes_per_element = BytesPerElement(type);
  DCHECK_GE(8, bytes_per_element);
  int elements_per_group = ElementsPerGroup(format, type);
  DCHECK_GE(4, elements_per_group);
  return  bytes_per_element * elements_per_group;
}

bool GLES2Util::ComputeImageRowSizeHelper(int width,
                                          uint32_t bytes_per_group,
                                          int alignment,
                                          uint32_t* rt_unpadded_row_size,
                                          uint32_t* rt_padded_row_size,
                                          uint32_t* rt_padding) {
  DCHECK(alignment == 1 || alignment == 2 ||
         alignment == 4 || alignment == 8);
  uint32_t unpadded_row_size;
  if (!SafeMultiplyUint32(width, bytes_per_group, &unpadded_row_size)) {
    return false;
  }
  uint32_t residual = unpadded_row_size % alignment;
  uint32_t padding = 0;
  uint32_t padded_row_size = unpadded_row_size;
  if (residual > 0) {
    padding = alignment - residual;
    if (!SafeAddUint32(unpadded_row_size, padding, &padded_row_size)) {
      return false;
    }
  }

  if (rt_unpadded_row_size)
    *rt_unpadded_row_size = unpadded_row_size;
  if (rt_padded_row_size)
    *rt_padded_row_size = padded_row_size;
  if (rt_padding)
    *rt_padding = padding;
  return true;
}

bool GLES2Util::ComputeImagePaddedRowSize(int width,
                                          int format,
                                          int type,
                                          int alignment,
                                          uint32_t* padded_row_size) {
  uint32_t bytes_per_group = ComputeImageGroupSize(format, type);
  return ComputeImageRowSizeHelper(
      width, bytes_per_group, alignment, nullptr, padded_row_size, nullptr);
}

// Returns the amount of data glTexImage*D or glTexSubImage*D will access.
bool GLES2Util::ComputeImageDataSizes(int width,
                                      int height,
                                      int depth,
                                      int format,
                                      int type,
                                      int alignment,
                                      uint32_t* size,
                                      uint32_t* opt_unpadded_row_size,
                                      uint32_t* opt_padded_row_size) {
  PixelStoreParams params;
  params.alignment = alignment;
  return ComputeImageDataSizesES3(
      width, height, depth, format, type, params,
      size, opt_unpadded_row_size, opt_padded_row_size, nullptr, nullptr);
}

bool GLES2Util::ComputeImageDataSizesES3(
    int width, int height, int depth, int format, int type,
    const PixelStoreParams& params,
    uint32_t* size, uint32_t* opt_unpadded_row_size,
    uint32_t* opt_padded_row_size, uint32_t* opt_skip_size,
    uint32_t* opt_padding) {
  DCHECK(width >= 0 && height >= 0 && depth >= 0);

  uint32_t bytes_per_group = ComputeImageGroupSize(format, type);

  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  if (!ComputeImageRowSizeHelper(width, bytes_per_group, params.alignment,
                                 &unpadded_row_size, &padded_row_size,
                                 opt_padding)) {
    return false;
  }
  if (params.row_length > 0 &&
      !ComputeImageRowSizeHelper(params.row_length, bytes_per_group,
                                 params.alignment, nullptr, &padded_row_size,
                                 opt_padding)) {
    // Here we re-compute the padded_row_size, but the unpadded_row_size
    // isn't affected. That is, the last row isn't affected by ROW_LENGTH.
    return false;
  }

  int image_height = params.image_height > 0 ? params.image_height : height;
  uint32_t num_of_rows;
  if (depth > 0) {
    if (!SafeMultiplyUint32(image_height, depth - 1, &num_of_rows) ||
        !SafeAddUint32(num_of_rows, height, &num_of_rows)) {
      return false;
    }
  } else {
    num_of_rows = 0;
  }

  if (num_of_rows > 0) {
    uint32_t size_of_all_but_last_row;
    if (!SafeMultiplyUint32((num_of_rows - 1), padded_row_size,
                            &size_of_all_but_last_row)) {
      return false;
    }
    if (!SafeAddUint32(size_of_all_but_last_row, unpadded_row_size, size)) {
      return false;
    }
  } else {
    *size = 0;
  }

  uint32_t skip_size = 0;
  if (params.skip_images > 0) {
    uint32_t image_size;
    if (!SafeMultiplyUint32(image_height, padded_row_size, &image_size))
      return false;
    if (!SafeMultiplyUint32(image_size, params.skip_images, &skip_size))
      return false;
  }
  if (params.skip_rows > 0) {
    uint32_t temp;
    if (!SafeMultiplyUint32(padded_row_size, params.skip_rows, &temp))
      return false;
    if (!SafeAddUint32(skip_size, temp, &skip_size))
      return false;
  }
  if (params.skip_pixels > 0) {
    uint32_t temp;
    if (!SafeMultiplyUint32(bytes_per_group, params.skip_pixels, &temp))
      return false;
    if (!SafeAddUint32(skip_size, temp, &skip_size))
      return false;
  }
  uint32_t total_size;
  if (!SafeAddUint32(*size, skip_size, &total_size))
    return false;

  if (opt_padded_row_size) {
    *opt_padded_row_size = padded_row_size;
  }
  if (opt_unpadded_row_size) {
    *opt_unpadded_row_size = unpadded_row_size;
  }
  if (opt_skip_size)
    *opt_skip_size = skip_size;
  return true;
}

size_t GLES2Util::RenderbufferBytesPerPixel(int format) {
  switch (format) {
    case GL_STENCIL_INDEX8:
      return 1;
    case GL_RGBA4:
    case GL_RGB565:
    case GL_RGB5_A1:
    case GL_DEPTH_COMPONENT16:
      return 2;
    case GL_RGB:
    case GL_RGBA:
    case GL_DEPTH24_STENCIL8_OES:
    case GL_RGB8_OES:
    case GL_RGBA8_OES:
    case GL_DEPTH_COMPONENT24_OES:
      return 4;
    default:
      return 0;
  }
}

uint32_t GLES2Util::GetElementSizeForUniformType(int type) {
  switch (type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
      return sizeof(GLfloat);
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
    case GL_BOOL:
    case GL_BOOL_VEC2:
    case GL_BOOL_VEC3:
    case GL_BOOL_VEC4:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_2D_RECT_ARB:  // extension.
    case GL_SAMPLER_EXTERNAL_OES:  // extension.
      return sizeof(GLint);

    // ES3 types.
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_VEC2:
    case GL_UNSIGNED_INT_VEC3:
    case GL_UNSIGNED_INT_VEC4:
      return sizeof(GLuint);
    case GL_SAMPLER_3D:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      return sizeof(GLint);
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
      return sizeof(GLfloat);

    default:
      return 0;
  }
}

uint32_t GLES2Util::GetElementCountForUniformType(int type) {
  switch (type) {
    case GL_FLOAT:
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_2D_RECT_ARB:  // extension.
    case GL_SAMPLER_EXTERNAL_OES:  // extension.
      return 1;
    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
      return 2;
    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
      return 3;
    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
    case GL_FLOAT_MAT2:
      return 4;
    case GL_FLOAT_MAT3:
      return 9;
    case GL_FLOAT_MAT4:
      return 16;

    // ES3 types.
    case GL_UNSIGNED_INT:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      return 1;
    case GL_UNSIGNED_INT_VEC2:
      return 2;
    case GL_UNSIGNED_INT_VEC3:
      return 3;
    case GL_UNSIGNED_INT_VEC4:
      return 4;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
      return 6;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
      return 8;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
      return 12;

    default:
      return 0;
  }
}

size_t GLES2Util::GetGLTypeSizeForBuffers(uint32_t type) {
  switch (type) {
    case GL_BYTE:
      return sizeof(GLbyte);  // NOLINT
    case GL_UNSIGNED_BYTE:
      return sizeof(GLubyte);  // NOLINT
    case GL_SHORT:
      return sizeof(GLshort);  // NOLINT
    case GL_UNSIGNED_SHORT:
      return sizeof(GLushort);  // NOLINT
    case GL_INT:
      return sizeof(GLint);  // NOLINT
    case GL_UNSIGNED_INT:
      return sizeof(GLuint);  // NOLINT
    case GL_FLOAT:
      return sizeof(GLfloat);  // NOLINT
    case GL_FIXED:
      return sizeof(GLfixed);  // NOLINT
    case GL_HALF_FLOAT:
      return sizeof(GLushort);  // NOLINT
    case GL_INT_2_10_10_10_REV:
      return sizeof(GLint);  // NOLINT
    case GL_UNSIGNED_INT_2_10_10_10_REV:
      return sizeof(GLuint);  // NOLINT
    default:
      return 0;
  }
}

size_t GLES2Util::GetGroupSizeForBufferType(uint32_t count, uint32_t type) {
  size_t type_size = GetGLTypeSizeForBuffers(type);
  // For packed types, group size equals to the type size.
  if (type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) {
    DCHECK_EQ(4u, count);
    return type_size;
  }
  return type_size * count;
}
size_t GLES2Util::GetComponentCountForGLTransformType(uint32_t type) {
  switch (type) {
    case GL_TRANSLATE_X_CHROMIUM:
    case GL_TRANSLATE_Y_CHROMIUM:
      return 1;
    case GL_TRANSLATE_2D_CHROMIUM:
      return 2;
    case GL_TRANSLATE_3D_CHROMIUM:
      return 3;
    case GL_AFFINE_2D_CHROMIUM:
    case GL_TRANSPOSE_AFFINE_2D_CHROMIUM:
      return 6;
    case GL_AFFINE_3D_CHROMIUM:
    case GL_TRANSPOSE_AFFINE_3D_CHROMIUM:
      return 12;
    default:
      return 0;
  }
}
size_t GLES2Util::GetCoefficientCountForGLPathFragmentInputGenMode(
    uint32_t gen_mode) {
  switch (gen_mode) {
    case GL_EYE_LINEAR_CHROMIUM:
      return 4;
    case GL_OBJECT_LINEAR_CHROMIUM:
      return 3;
    case GL_CONSTANT_CHROMIUM:
      return 1;
    case GL_NONE:
    default:
      return 0;
  }
}

size_t GLES2Util::GetGLTypeSizeForPathCoordType(uint32_t type) {
  switch (type) {
    case GL_BYTE:
      return sizeof(GLbyte);  // NOLINT
    case GL_UNSIGNED_BYTE:
      return sizeof(GLubyte);  // NOLINT
    case GL_SHORT:
      return sizeof(GLshort);  // NOLINT
    case GL_UNSIGNED_SHORT:
      return sizeof(GLushort);  // NOLINT
    case GL_FLOAT:
      return sizeof(GLfloat);  // NOLINT
    default:
      return 0;
  }
}

size_t GLES2Util::GetGLTypeSizeForGLPathNameType(uint32_t type) {
  switch (type) {
    case GL_BYTE:
      return sizeof(GLbyte);  // NOLINT
    case GL_UNSIGNED_BYTE:
      return sizeof(GLubyte);  // NOLINT
    case GL_SHORT:
      return sizeof(GLshort);  // NOLINT
    case GL_UNSIGNED_SHORT:
      return sizeof(GLushort);  // NOLINT
    case GL_INT:
      return sizeof(GLint);  // NOLINT
    case GL_UNSIGNED_INT:
      return sizeof(GLuint);  // NOLINT
    default:
      return 0;
  }
}

uint32_t GLES2Util::GLErrorToErrorBit(uint32_t error) {
  switch (error) {
    case GL_INVALID_ENUM:
      return gl_error_bit::kInvalidEnum;
    case GL_INVALID_VALUE:
      return gl_error_bit::kInvalidValue;
    case GL_INVALID_OPERATION:
      return gl_error_bit::kInvalidOperation;
    case GL_OUT_OF_MEMORY:
      return gl_error_bit::kOutOfMemory;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return gl_error_bit::kInvalidFrameBufferOperation;
    case GL_CONTEXT_LOST_KHR:
      return gl_error_bit::kContextLost;
    default:
      NOTREACHED();
      return gl_error_bit::kNoError;
  }
}

uint32_t GLES2Util::GLErrorBitToGLError(uint32_t error_bit) {
  switch (error_bit) {
    case gl_error_bit::kInvalidEnum:
      return GL_INVALID_ENUM;
    case gl_error_bit::kInvalidValue:
      return GL_INVALID_VALUE;
    case gl_error_bit::kInvalidOperation:
      return GL_INVALID_OPERATION;
    case gl_error_bit::kOutOfMemory:
      return GL_OUT_OF_MEMORY;
    case gl_error_bit::kInvalidFrameBufferOperation:
      return GL_INVALID_FRAMEBUFFER_OPERATION;
    case gl_error_bit::kContextLost:
      return GL_CONTEXT_LOST_KHR;
    default:
      NOTREACHED();
      return GL_NO_ERROR;
  }
}

uint32_t GLES2Util::IndexToGLFaceTarget(int index) {
  static uint32_t faces[] = {
      GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };
  return faces[index];
}

size_t GLES2Util::GLTargetToFaceIndex(uint32_t target) {
  switch (target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_EXTERNAL_OES:
    case GL_TEXTURE_RECTANGLE_ARB:
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
      return 0;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      return 0;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      return 1;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      return 2;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      return 3;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      return 4;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return 5;
    default:
      NOTREACHED();
      return 0;
  }
}

uint32_t GLES2Util::GetGLReadPixelsImplementationFormat(
    uint32_t internal_format) {
  switch (internal_format) {
    case GL_R8:
    case GL_R16F:
    case GL_R32F:
      return GL_RED;
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
      return GL_RED_INTEGER;
    case GL_RG8:
    case GL_RG16F:
    case GL_RG32F:
      return GL_RG;
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
      return GL_RG_INTEGER;
    case GL_RGB:
    case GL_RGB8:
    case GL_RGB565:
    case GL_R11F_G11F_B10F:
    case GL_RGB16F:
    case GL_RGB32F:
      return GL_RGB;
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
      return GL_RGBA_INTEGER;
    default:
      return GL_RGBA;
  }
}

uint32_t GLES2Util::GetGLReadPixelsImplementationType(uint32_t internal_format,
                                                      uint32_t texture_type) {
  switch (internal_format) {
    case GL_R16UI:
    case GL_RG16UI:
    case GL_RGBA16UI:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
      return GL_UNSIGNED_SHORT;
    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGBA32UI:
      return GL_UNSIGNED_INT;
    case GL_R8I:
    case GL_RG8I:
    case GL_RGBA8I:
      return GL_BYTE;
    case GL_R16I:
    case GL_RG16I:
    case GL_RGBA16I:
      return GL_SHORT;
    case GL_R32I:
    case GL_RG32I:
    case GL_RGBA32I:
      return GL_INT;
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
      return GL_FLOAT;
    case GL_R16F:
    case GL_RG16F:
    case GL_R11F_G11F_B10F:
    case GL_RGB16F:
    case GL_RGBA16F:
      // TODO(zmo): Consider return GL_UNSIGNED_INT_10F_11F_11F_REV and
      // GL_HALF_FLOAT.
      return GL_FLOAT;
    case GL_RGBA:
    case GL_RGB:
      // Unsized internal format, check the type
      switch (texture_type) {
        case GL_FLOAT:
        case GL_HALF_FLOAT_OES:
          return GL_FLOAT;
        // TODO(zmo): Consider return GL_UNSIGNED_SHORT_5_6_5,
        // GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_SHORT_5_5_5_1, and
        // GL_UNSIGNED_INT_2_10_10_10_REV.
        default:
          return GL_UNSIGNED_BYTE;
      }
    default:
      return GL_UNSIGNED_BYTE;
  }
}

uint32_t GLES2Util::GetChannelsForFormat(int format) {
  switch (format) {
    case GL_ALPHA:
    case GL_ALPHA16F_EXT:
    case GL_ALPHA32F_EXT:
      return kAlpha;
    case GL_LUMINANCE:
      return kRGB;
    case GL_LUMINANCE_ALPHA:
      return kRGBA;
    case GL_RGB:
    case GL_RGB8_OES:
    case GL_RGB565:
    case GL_RGB16F_EXT:
    case GL_RGB32F_EXT:
    case GL_SRGB_EXT:
    case GL_SRGB8:
    case GL_RGB8_SNORM:
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
    case GL_RGB8UI:
    case GL_RGB8I:
    case GL_RGB16UI:
    case GL_RGB16I:
    case GL_RGB32UI:
    case GL_RGB32I:
      return kRGB;
    case GL_BGRA_EXT:
    case GL_BGRA8_EXT:
    case GL_RGBA16F_EXT:
    case GL_RGBA32F_EXT:
    case GL_RGBA:
    case GL_RGBA8_OES:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8_EXT:
    case GL_RGBA8_SNORM:
    case GL_RGB10_A2:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
      return kRGBA;
    case GL_DEPTH_COMPONENT32_OES:
    case GL_DEPTH_COMPONENT24_OES:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT32F:
      return kDepth;
    case GL_STENCIL_INDEX8:
      return kStencil;
    case GL_DEPTH_STENCIL_OES:
    case GL_DEPTH24_STENCIL8_OES:
    case GL_DEPTH32F_STENCIL8:
      return kDepth | kStencil;
    case GL_RED_EXT:
    case GL_R8:
    case GL_R8_SNORM:
    case GL_R16F:
    case GL_R32F:
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
      return kRed;
    case GL_RG_EXT:
    case GL_RG8:
    case GL_RG8_SNORM:
    case GL_RG16F:
    case GL_RG32F:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
      return kRed | kGreen;
    default:
      return 0x0000;
  }
}

uint32_t GLES2Util::GetChannelsNeededForAttachmentType(
    int type,
    uint32_t max_color_attachments) {
  switch (type) {
    case GL_DEPTH_ATTACHMENT:
      return kDepth;
    case GL_STENCIL_ATTACHMENT:
      return kStencil;
    case GL_DEPTH_STENCIL_ATTACHMENT:
      return kDepth | kStencil;
    default:
      if (type >= GL_COLOR_ATTACHMENT0 &&
          type < static_cast<int>(
              GL_COLOR_ATTACHMENT0 + max_color_attachments)) {
        return kRGBA;
      }
      return 0x0000;
  }
}

std::string GLES2Util::GetStringEnum(uint32_t value) {
  const EnumToString* entry = enum_to_string_table_;
  const EnumToString* end = entry + enum_to_string_table_len_;
  for (;entry < end; ++entry) {
    if (value == entry->value) {
      return entry->name;
    }
  }
  std::stringstream ss;
  ss.fill('0');
  ss.width(value < 0x10000 ? 4 : 8);
  ss << std::hex << value;
  return "0x" + ss.str();
}

std::string GLES2Util::GetStringError(uint32_t value) {
  static EnumToString string_table[] = {
    { GL_NONE, "GL_NONE" },
  };
  return GLES2Util::GetQualifiedEnumString(
      string_table, arraysize(string_table), value);
}

std::string GLES2Util::GetStringBool(uint32_t value) {
  return value ? "GL_TRUE" : "GL_FALSE";
}

std::string GLES2Util::GetQualifiedEnumString(const EnumToString* table,
                                              size_t count,
                                              uint32_t value) {
  for (const EnumToString* end = table + count; table < end; ++table) {
    if (table->value == value) {
      return table->name;
    }
  }
  return GetStringEnum(value);
}

GLSLArrayName::GLSLArrayName(const std::string& name) : element_index_(-1) {
  if (name.size() < 4)
    return;
  if (name[name.size() - 1] != ']')
    return;

  size_t open_pos = name.find_last_of('[');
  if (open_pos >= name.size() - 2)
    return;

  base::CheckedNumeric<int> index = 0;
  size_t last = name.size() - 1;
  for (size_t pos = open_pos + 1; pos < last; ++pos) {
    int8_t digit = name[pos] - '0';
    if (digit < 0 || digit > 9)
      return;
    index = index * 10 + digit;
  }
  if (!index.IsValid())
    return;
  element_index_ = index.ValueOrDie();
  base_name_ = name.substr(0, open_pos);
}

size_t GLES2Util::CalcClearBufferivDataCount(int buffer) {
  switch (buffer) {
    case GL_COLOR:
      return 4;
    case GL_STENCIL:
      return 1;
    default:
      return 0;
  }
}

size_t GLES2Util::CalcClearBufferfvDataCount(int buffer) {
  switch (buffer) {
    case GL_COLOR:
      return 4;
    case GL_DEPTH:
      return 1;
    default:
      return 0;
  }
}

// static
void GLES2Util::MapUint64ToTwoUint32(
    uint64_t v64, uint32_t* v32_0, uint32_t* v32_1) {
  DCHECK(v32_0 && v32_1);
  *v32_0 = static_cast<uint32_t>(v64 & 0xFFFFFFFF);
  *v32_1 = static_cast<uint32_t>((v64 & 0xFFFFFFFF00000000) >> 32);
}

// static
uint64_t GLES2Util::MapTwoUint32ToUint64(uint32_t v32_0, uint32_t v32_1) {
  uint64_t v64 = v32_1;
  return (v64 << 32) | v32_0;
}

// static
uint32_t GLES2Util::MapBufferTargetToBindingEnum(uint32_t target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      return GL_ARRAY_BUFFER_BINDING;
    case GL_COPY_READ_BUFFER:
      return GL_COPY_READ_BUFFER_BINDING;
    case GL_COPY_WRITE_BUFFER:
      return GL_COPY_WRITE_BUFFER_BINDING;
    case GL_ELEMENT_ARRAY_BUFFER:
      return GL_ELEMENT_ARRAY_BUFFER_BINDING;
    case GL_PIXEL_PACK_BUFFER:
      return GL_PIXEL_PACK_BUFFER_BINDING;
    case GL_PIXEL_UNPACK_BUFFER:
      return GL_PIXEL_UNPACK_BUFFER_BINDING;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case GL_UNIFORM_BUFFER:
      return GL_UNIFORM_BUFFER_BINDING;
    default:
      return 0;
  }
}

// static
bool GLES2Util::IsUnsignedIntegerFormat(uint32_t internal_format) {
  switch (internal_format) {
    case GL_R8UI:
    case GL_R16UI:
    case GL_R32UI:
    case GL_RG8UI:
    case GL_RG16UI:
    case GL_RG32UI:
    case GL_RGBA8UI:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA32UI:
      return true;
    default:
      return false;
  }
}

// static
bool GLES2Util::IsSignedIntegerFormat(uint32_t internal_format) {
  switch (internal_format) {
    case GL_R8I:
    case GL_R16I:
    case GL_R32I:
    case GL_RG8I:
    case GL_RG16I:
    case GL_RG32I:
    case GL_RGBA8I:
    case GL_RGBA16I:
    case GL_RGBA32I:
      return true;
    default:
      return false;
  }
}

// static
bool GLES2Util::IsIntegerFormat(uint32_t internal_format) {
  return (IsUnsignedIntegerFormat(internal_format) ||
          IsSignedIntegerFormat(internal_format));
}

// static
bool GLES2Util::IsFloatFormat(uint32_t internal_format) {
  switch (internal_format) {
    case GL_R16F:
    case GL_R32F:
    case GL_RG16F:
    case GL_RG32F:
    case GL_R11F_G11F_B10F:
    case GL_RGB16F:
    case GL_RGB32F:
    case GL_RGBA16F:
    case GL_RGBA32F:
      return true;
    default:
      return false;
  }
}

namespace {

// WebGraphicsContext3DCommandBufferImpl configuration attributes. Those in
// the 16-bit range are the same as used by EGL. Those outside the 16-bit range
// are unique to Chromium. Attributes are matched using a closest fit algorithm.

// From <EGL/egl.h>.
#include <stddef.h>
#include <stdint.h>
const int32_t kAlphaSize = 0x3021;        // EGL_ALPHA_SIZE
const int32_t kBlueSize = 0x3022;         // EGL_BLUE_SIZE
const int32_t kGreenSize = 0x3023;        // EGL_GREEN_SIZE
const int32_t kRedSize = 0x3024;          // EGL_RED_SIZE
const int32_t kDepthSize = 0x3025;        // EGL_DEPTH_SIZE
const int32_t kStencilSize = 0x3026;      // EGL_STENCIL_SIZE
const int32_t kSamples = 0x3031;          // EGL_SAMPLES
const int32_t kSampleBuffers = 0x3032;    // EGL_SAMPLE_BUFFERS
const int32_t kNone = 0x3038;             // EGL_NONE
const int32_t kSwapBehavior = 0x3093;     // EGL_SWAP_BEHAVIOR
const int32_t kBufferPreserved = 0x3094;  // EGL_BUFFER_PRESERVED
const int32_t kBufferDestroyed = 0x3095;  // EGL_BUFFER_DESTROYED

// Chromium only.
const int32_t kBindGeneratesResource = 0x10000;
const int32_t kFailIfMajorPerfCaveat = 0x10001;
const int32_t kLoseContextWhenOutOfMemory = 0x10002;
const int32_t kContextType = 0x10003;

}  // namespace

ContextCreationAttribHelper::ContextCreationAttribHelper()
    : alpha_size(-1),
      blue_size(-1),
      green_size(-1),
      red_size(-1),
      depth_size(-1),
      stencil_size(-1),
      samples(-1),
      sample_buffers(-1),
      buffer_preserved(true),
      bind_generates_resource(true),
      fail_if_major_perf_caveat(false),
      lose_context_when_out_of_memory(false),
      context_type(CONTEXT_TYPE_OPENGLES2) {}

void ContextCreationAttribHelper::Serialize(
    std::vector<int32_t>* attribs) const {
  if (alpha_size != -1) {
    attribs->push_back(kAlphaSize);
    attribs->push_back(alpha_size);
  }
  if (blue_size != -1) {
    attribs->push_back(kBlueSize);
    attribs->push_back(blue_size);
  }
  if (green_size != -1) {
    attribs->push_back(kGreenSize);
    attribs->push_back(green_size);
  }
  if (red_size != -1) {
    attribs->push_back(kRedSize);
    attribs->push_back(red_size);
  }
  if (depth_size != -1) {
    attribs->push_back(kDepthSize);
    attribs->push_back(depth_size);
  }
  if (stencil_size != -1) {
    attribs->push_back(kStencilSize);
    attribs->push_back(stencil_size);
  }
  if (samples != -1) {
    attribs->push_back(kSamples);
    attribs->push_back(samples);
  }
  if (sample_buffers != -1) {
    attribs->push_back(kSampleBuffers);
    attribs->push_back(sample_buffers);
  }
  attribs->push_back(kSwapBehavior);
  attribs->push_back(buffer_preserved ? kBufferPreserved : kBufferDestroyed);
  attribs->push_back(kBindGeneratesResource);
  attribs->push_back(bind_generates_resource ? 1 : 0);
  attribs->push_back(kFailIfMajorPerfCaveat);
  attribs->push_back(fail_if_major_perf_caveat ? 1 : 0);
  attribs->push_back(kLoseContextWhenOutOfMemory);
  attribs->push_back(lose_context_when_out_of_memory ? 1 : 0);
  attribs->push_back(kContextType);
  attribs->push_back(context_type);
  attribs->push_back(kNone);
}

bool ContextCreationAttribHelper::Parse(const std::vector<int32_t>& attribs) {
  for (size_t i = 0; i < attribs.size(); i += 2) {
    const int32_t attrib = attribs[i];
    if (i + 1 >= attribs.size()) {
      if (attrib == kNone) {
        return true;
      }

      DLOG(ERROR) << "Missing value after context creation attribute: "
                  << attrib;
      return false;
    }

    const int32_t value = attribs[i + 1];
    switch (attrib) {
      case kAlphaSize:
        alpha_size = value;
        break;
      case kBlueSize:
        blue_size = value;
        break;
      case kGreenSize:
        green_size = value;
        break;
      case kRedSize:
        red_size = value;
        break;
      case kDepthSize:
        depth_size = value;
        break;
      case kStencilSize:
        stencil_size = value;
        break;
      case kSamples:
        samples = value;
        break;
      case kSampleBuffers:
        sample_buffers = value;
        break;
      case kSwapBehavior:
        buffer_preserved = value == kBufferPreserved;
        break;
      case kBindGeneratesResource:
        bind_generates_resource = value != 0;
        break;
      case kFailIfMajorPerfCaveat:
        fail_if_major_perf_caveat = value != 0;
        break;
      case kLoseContextWhenOutOfMemory:
        lose_context_when_out_of_memory = value != 0;
        break;
      case kContextType:
        context_type = static_cast<ContextType>(value);
        break;
      case kNone:
        // Terminate list, even if more attributes.
        return true;
      default:
        DLOG(ERROR) << "Invalid context creation attribute: " << attrib;
        return false;
    }
  }

  return true;
}

#include "gpu/command_buffer/common/gles2_cmd_utils_implementation_autogen.h"

}  // namespace gles2
}  // namespace gpu

