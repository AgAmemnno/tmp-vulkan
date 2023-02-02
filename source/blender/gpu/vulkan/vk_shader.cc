/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 * Settings:

Source type: Kind of shader to compile (vertex/compute/fragment/etc)
Source profile: None, core
Target type: SpirV assembly
OptimizationLevel (None,Size,Performance)
Draft API (intern_shader_compiler). API is CPP as its only usage is inside GPU module that is
already CPP.

/* Initialize the shader compiler (needs to be ones per process)
 * Will configure compiler cache (on/off, cache location)
 */
/// shader_compiler::init(...);

/* Compiler can be constructed or reused. */
// shader_compiler::Compiler *compiler = new shader_compiler::Compiler();

/* Create a job for the compiler. Here the sources are added, source type, target type etc. */
// shader_compiler::Job job;
// job. ...

/* Perform the compilation */
// shader_compiler::Result result = compiler.compile(&job);

/* Result contains the state (num warnings, num errors), log and binary result. */

// delete compiler;
// shader_compiler::free();
// the API would also have functions to remove cached files. The implementation of the caching
// would not be part of the initial implementation.

/** \file
 * \ingroup gpu
 */
#include "BKE_global.h"

#include "BLI_string.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>

#include <cstring>

#include "GPU_platform.h"
#include "GPU_vertex_format.h"

#include "vk_layout.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state.hh"

/// <summary>
/// early development. Use the sources below.
/// https://github.com/nvpro-samples/nvpro_core/blob/master/nvvk/shadermodulemanager_vk.hpp
/// </summary>
#include "vk_shaders.hh"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include <string>

#include "BLI_vector.hh"

//#include "shader_compiler.hh"
#include <shaderc/shaderc.hpp>

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Shader stages
 * \{ */

constexpr StringRef SHADER_STAGE_VERTEX_SHADER = "vertex";
constexpr StringRef SHADER_STAGE_GEOMETRY_SHADER = "geometry";
constexpr StringRef SHADER_STAGE_FRAGMENT_SHADER = "fragment";
constexpr StringRef SHADER_STAGE_COMPUTE_SHADER = "compute";

std::ostream &operator<<(std::ostream &os, const VKShaderStageType &stage)
{
  switch (stage) {
    case VKShaderStageType::VertexShader:
      os << SHADER_STAGE_VERTEX_SHADER;
      break;
    case VKShaderStageType::GeometryShader:
      os << SHADER_STAGE_GEOMETRY_SHADER;
      break;
    case VKShaderStageType::FragmentShader:
      os << SHADER_STAGE_FRAGMENT_SHADER;
      break;
    case VKShaderStageType::ComputeShader:
      os << SHADER_STAGE_COMPUTE_SHADER;
      break;
  }
  return os;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Construction/Destruction
 * \{ */

VKShader::VKShader(const char *name) : Shader(name)
{
  interface = new VKShaderInterface();
  context_ = VKContext::get();
  pipe = VK_NULL_HANDLE;
  write_iub_.clear();
  write_descs_.clear();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compilation
 * \{ */

static std::string to_stage_name(VKShaderStageType stage)
{
  std::stringstream ss;
  ss << stage;
  return ss.str();
}

static std::string to_stage_name(StringRef name, VKShaderStageType stage)
{
  std::stringstream ss;
  ss << name << "." << stage;
  return ss.str();
}

static std::string combine_sources(Span<const char *> sources)
{
  std::stringstream combined;
  for (int i = 0; i < sources.size(); i++) {
    combined << sources[i];
  }
  return combined.str();
}
static char *glsl_patch_get()
{
  static char patch[512] = "\0";
  if (patch[0] != '\0') {
    return patch;
  }

  size_t slen = 0;
  /* Version need to go first. */
  STR_CONCAT(patch, slen, "#version 460\n");

  BLI_assert(slen < sizeof(patch));
  return patch;
}

GHOST_TSuccess VKShader::compile_source(Span<const char *> sources, VKShaderStageType stage)
{

  std::string stage_name = to_stage_name(name, stage);
  VkShaderStageFlagBits shaderflag = VK_SHADER_STAGE_VERTEX_BIT;
  int m_apiMajor = 1;
  int m_apiMinor = 2;
  switch (stage) {
    case VKShaderStageType::VertexShader:
      shaderflag = VK_SHADER_STAGE_VERTEX_BIT;
      break;
    case VKShaderStageType::GeometryShader:
      shaderflag = VK_SHADER_STAGE_GEOMETRY_BIT;
      break;
    case VKShaderStageType::FragmentShader:
      shaderflag = VK_SHADER_STAGE_FRAGMENT_BIT;
      break;
    case VKShaderStageType::ComputeShader:
      shaderflag = VK_SHADER_STAGE_COMPUTE_BIT;
      break;
  }

  std::string source = combine_sources(sources);
  shaderc_compilation_result_t result = nullptr;
  static uint32_t __shadercCompilerUsers = 0;
  static shaderc_compiler_t __shadercCompiler;  // Lock mutex below while using.
  static std::mutex __shadercCompilerMutex;
  shaderc_compile_options_t m_shadercOptions = nullptr;

  __shadercCompilerUsers++;
  if (!__shadercCompiler) {
    __shadercCompiler = shaderc_compiler_initialize();
  }
  m_shadercOptions = shaderc_compile_options_initialize();

  struct SetupInterface {
    // This class is to aid using a shaderc library version that is not
    // provided by the Vulkan SDK, but custom. Therefore it allows custom settings etc.
    // Useful for driver development of new shader stages, otherwise can be pretty much ignored.

    virtual std::string getTypeDefine(uint32_t type) const = 0;
    virtual uint32_t getTypeShadercKind(uint32_t type) const = 0;
    virtual void *getShadercCompileOption(void *shadercCompiler)
    {
      return nullptr;
    }
  };
  struct DefaultInterface : public SetupInterface {
    std::string getTypeDefine(uint32_t type) const override
    {
      switch (type) {
        case VK_SHADER_STAGE_VERTEX_BIT:
          return "#define _VERTEX_SHADER_ 1\n";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
          return "#define _FRAGMENT_SHADER_ 1\n";
        case VK_SHADER_STAGE_COMPUTE_BIT:
          return "#define _COMPUTE_SHADER_ 1\n";
        case VK_SHADER_STAGE_GEOMETRY_BIT:
          return "#define _GEOMETRY_SHADER_ 1\n";
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
          return "#define _TESS_CONTROL_SHADER_ 1\n";
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
          return "#define _TESS_EVALUATION_SHADER_ 1\n";
#if VK_NV_mesh_shader
        case VK_SHADER_STAGE_MESH_BIT_NV:
          return "#define _MESH_SHADER_ 1\n";
        case VK_SHADER_STAGE_TASK_BIT_NV:
          return "#define _TASK_SHADER_ 1\n";
#endif
#if VK_NV_ray_tracing
        case VK_SHADER_STAGE_RAYGEN_BIT_NV:
          return "#define _RAY_GENERATION_SHADER_ 1\n";
        case VK_SHADER_STAGE_ANY_HIT_BIT_NV:
          return "#define _RAY_ANY_HIT_SHADER_ 1\n";
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV:
          return "#define _RAY_CLOSEST_HIT_SHADER_ 1\n";
        case VK_SHADER_STAGE_MISS_BIT_NV:
          return "#define _RAY_MISS_SHADER_ 1\n";
        case VK_SHADER_STAGE_INTERSECTION_BIT_NV:
          return "#define _RAY_INTERSECTION_SHADER_ 1\n";
        case VK_SHADER_STAGE_CALLABLE_BIT_NV:
          return "#define _RAY_CALLABLE_BIT_SHADER_ 1\n";
#endif
      }

      return std::string();
    };
    uint32_t getTypeShadercKind(uint32_t type) const override
    {
#if NVP_SUPPORTS_SHADERC
      switch (type) {
        case VK_SHADER_STAGE_VERTEX_BIT:
          return shaderc_glsl_vertex_shader;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
          return shaderc_glsl_fragment_shader;
        case VK_SHADER_STAGE_COMPUTE_BIT:
          return shaderc_glsl_compute_shader;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
          return shaderc_glsl_geometry_shader;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
          return shaderc_glsl_tess_control_shader;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
          return shaderc_glsl_tess_evaluation_shader;

#  if VK_NV_mesh_shader
        case VK_SHADER_STAGE_MESH_BIT_NV:
          return shaderc_glsl_mesh_shader;
        case VK_SHADER_STAGE_TASK_BIT_NV:
          return shaderc_glsl_task_shader;
#  endif
#  if VK_NV_ray_tracing
        case VK_SHADER_STAGE_RAYGEN_BIT_NV:
          return shaderc_glsl_raygen_shader;
        case VK_SHADER_STAGE_ANY_HIT_BIT_NV:
          return shaderc_glsl_anyhit_shader;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV:
          return shaderc_glsl_closesthit_shader;
        case VK_SHADER_STAGE_MISS_BIT_NV:
          return shaderc_glsl_miss_shader;
        case VK_SHADER_STAGE_INTERSECTION_BIT_NV:
          return shaderc_glsl_intersection_shader;
        case VK_SHADER_STAGE_CALLABLE_BIT_NV:
          return shaderc_glsl_callable_shader;
#  endif
      }

      return shaderc_glsl_infer_from_source;
#else
      return 0;
#endif
    };
  };

  static const VkShaderModule PREPROCESS_ONLY_MODULE;

  /// VkDevice m_device = nullptr;
  DefaultInterface m_defaultSetupIF;
  DefaultInterface m_usedSetupIF;

  std::lock_guard<std::mutex> guard(__shadercCompilerMutex);
  shaderc_shader_kind shaderkind = (shaderc_shader_kind)m_usedSetupIF.getTypeShadercKind(
      shaderflag);
  shaderc_compile_options_t options = (shaderc_compile_options_t)
                                          m_usedSetupIF.getShadercCompileOption(__shadercCompiler);
  if (!options) {
    if (m_apiMajor == 1 && m_apiMinor == 0) {
      shaderc_compile_options_set_target_env(
          m_shadercOptions, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    }
    else if (m_apiMajor == 1 && m_apiMinor == 1) {
      shaderc_compile_options_set_target_env(
          m_shadercOptions, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
    }
    else if (m_apiMajor == 1 && m_apiMinor == 2) {
      shaderc_compile_options_set_target_env(
          m_shadercOptions, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    }
    else if (m_apiMajor == 1 && m_apiMinor == 3) {
      fprintf(stderr, "This version <= 1.2 \n");
      exit(-1);
      // shaderc_compile_options_set_target_env(
      //    m_shadercOptions, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    }
    else {
      GPU_VK_DEBUG_PRINTF("nvvk::ShaderModuleManager: Unsupported Vulkan version: %i.%i\n",
                          int(m_apiMajor),
                          int(m_apiMinor));
      assert(0);
    }

    shaderc_compile_options_set_optimization_level(m_shadercOptions, m_shadercOptimizationLevel);

    // Keep debug info, doesn't cost shader execution perf, only compile-time and memory size.
    // Improves usage for debugging tools, not recommended for shipping application,
    // but good for developmenent builds.
    shaderc_compile_options_set_generate_debug_info(m_shadercOptions);

    options = m_shadercOptions;
  }

  // Tell shaderc to use this class (really our base class, nvh::ShaderFileManager) to load
  // include files.
  nvvk::ShaderModuleManager smm;
  nvvk::__ShadercIncludeBridge shadercIncludeBridge(&smm);
  shadercIncludeBridge.setAsIncluder(options);

  // Note: need filenameFound, not filename, so that relative includes work.
  result = shaderc_compile_into_spv(
      __shadercCompiler, source.c_str(), source.size(), shaderkind, "filename", "main", options);

  if (G.debug_value & -7777) {
    printf("Shader Context \n\n\n %s ", source.c_str());
  }

  if (!result) {
    return GHOST_kFailure;
  }

  if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
    bool failedToOptimize = strstr(shaderc_result_get_error_message(result), "failed to optimize");
    // int level = failedToOptimize ? LOGLEVEL_WARNING : LOGLEVEL_ERROR;

    GPU_VK_DEBUG_PRINTF("  %s\n", shaderc_result_get_error_message(result));
    shaderc_result_release(result);

    if (!failedToOptimize || options != m_shadercOptions) {
      return GHOST_kFailure;
    }

    // try again without optimization
    shaderc_compile_options_set_optimization_level(m_shadercOptions,
                                                   shaderc_optimization_level_zero);

    result = shaderc_compile_into_spv(
        __shadercCompiler, source.c_str(), source.size(), shaderkind, "filename", "main", options);
  }

  if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {

    GPU_VK_DEBUG_PRINTF("  %s\n", shaderc_result_get_error_message(result));
    shaderc_result_release(result);
    return GHOST_kFailure;
  }

  shaders_[(uint32_t)stage].shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaders_[(uint32_t)stage].shaderModuleInfo.pNext = NULL;
  shaders_[(uint32_t)stage].shaderModuleInfo.flags = 0;
  shaders_[(uint32_t)stage].shaderModuleInfo.codeSize = shaderc_result_get_length(result);
  shaders_[(uint32_t)stage].shaderModuleInfo.pCode = (const uint32_t *)shaderc_result_get_bytes(
      result);

  auto vkresult = ::vkCreateShaderModule(VKContext::get()->device_get(),
                                         &shaders_[(uint32_t)stage].shaderModuleInfo,
                                         nullptr,
                                         &shaders_[(uint32_t)stage].module);
  compilation_failed_ = false;
  if (vkresult != VK_SUCCESS) {
    compilation_failed_ = true;
    shaders_[(uint32_t)stage].module = VK_NULL_HANDLE;
    shaders_[(uint32_t)stage].shaderModuleInfo.pCode = NULL;
    shaders_[(uint32_t)stage].shaderModuleInfo.codeSize = 0;
    return GHOST_kFailure;
  }

  return GHOST_kSuccess;
}

VkShaderModule VKShader::create_shader_module(MutableSpan<const char *> sources,
                                              VKShaderStageType stage)
{
  /* Patch the shader code using the first source slot. */
  sources[0] = glsl_patch_get();
  compile_source(sources, stage);

  return shaders_[uint32_t(stage)].module;
}

void VKShader::vertex_shader_from_glsl(MutableSpan<const char *> sources)
{
#ifdef WITH_VULKAN_SHADER_COMPILATION
  this->create_shader_module(sources, VKShaderStageType::VertexShader);
#endif
}

void VKShader::geometry_shader_from_glsl(MutableSpan<const char *> sources)
{
#ifdef WITH_VULKAN_SHADER_COMPILATION
  this->create_shader_module(sources, VKShaderStageType::GeometryShader);
#endif
}

void VKShader::fragment_shader_from_glsl(MutableSpan<const char *> sources)
{
#ifdef WITH_VULKAN_SHADER_COMPILATION
  this->create_shader_module(sources, VKShaderStageType::FragmentShader);
#endif
}

void VKShader::compute_shader_from_glsl(MutableSpan<const char *> sources)
{
#ifdef WITH_VULKAN_SHADER_COMPILATION
  this->create_shader_module(sources, VKShaderStageType::ComputeShader);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Linking
 * \{ */

static bool do_geometry_shader_injection(const shader::ShaderCreateInfo *info)
{
  /// <summary>
  /// TODO geometry shader
  /// </summary>
  /// <param name="info"></param>
  /// <returns></returns>
  return true;
}

bool VKShader::finalize(const shader::ShaderCreateInfo *info)
{

  if (compilation_failed_) {
    return false;
  }
  VKShaderInterface &iface = *((VKShaderInterface *)interface);
  if (!iface.valid) {
    blender::gpu::ShaderModule vsm, fsm;
    getShaderModule(vsm, 0);
    getShaderModule(fsm, 2);
    iface.parse(vsm, fsm, info, this);
    auto fb = static_cast<VKFrameBuffer *>(VKContext::get()->active_fb);
    CreatePipeline(fb->get_render_pass());

    iface.valid = true;
  }

  return true;
};

using namespace blender::gpu::shader;

static const char *to_string(const Interpolation &interp)
{
  switch (interp) {
    case Interpolation::SMOOTH:
      return "smooth";
    case Interpolation::FLAT:
      return "flat";
    case Interpolation::NO_PERSPECTIVE:
      return "noperspective";
    default:
      return "unknown";
  }
}

static const char *to_string(const Type &type)
{
  switch (type) {
    case Type::FLOAT:
      return "float";
    case Type::VEC2:
      return "vec2";
    case Type::VEC3:
      return "vec3";
    case Type::VEC4:
      return "vec4";
    case Type::MAT3:
      return "mat3";
    case Type::MAT4:
      return "mat4";
    case Type::UINT:
      return "uint";
    case Type::UVEC2:
      return "uvec2";
    case Type::UVEC3:
      return "uvec3";
    case Type::UVEC4:
      return "uvec4";
    case Type::INT:
      return "int";
    case Type::IVEC2:
      return "ivec2";
    case Type::IVEC3:
      return "ivec3";
    case Type::IVEC4:
      return "ivec4";
    case Type::BOOL:
      return "bool";
    default:
      return "unknown";
  }
}
static const uint32_t to_size(const Type &type, uint32_t arraysize = 1)
{
  switch (type) {
    case Type::FLOAT:
      return 4 * arraysize;
    case Type::VEC2:
      return 8 * arraysize;
    case Type::VEC3:
      return 12 * arraysize;
    case Type::VEC4:
      return 16 * arraysize;
    case Type::MAT3:
      return 36 * arraysize;
    case Type::MAT4:
      return 64 * arraysize;
    case Type::UINT:
      return 4 * arraysize;
    case Type::UVEC2:
      return 8 * arraysize;
    case Type::UVEC3:
      return 12 * arraysize;
    case Type::UVEC4:
      return 16 * arraysize;
    case Type::INT:
      return 4 * arraysize;
    case Type::IVEC2:
      return 8 * arraysize;
    case Type::IVEC3:
      return 12 * arraysize;
    case Type::IVEC4:
      return 16 * arraysize;
    case Type::BOOL:
      return 4 * arraysize;
    default:
      return -1;
  }
}

static const char *to_string(const eGPUTextureFormat &type)
{
  switch (type) {
    case GPU_RGBA8UI:
      return "rgba8ui";
    case GPU_RGBA8I:
      return "rgba8i";
    case GPU_RGBA8:
      return "rgba8";
    case GPU_RGBA32UI:
      return "rgba32ui";
    case GPU_RGBA32I:
      return "rgba32i";
    case GPU_RGBA32F:
      return "rgba32f";
    case GPU_RGBA16UI:
      return "rgba16ui";
    case GPU_RGBA16I:
      return "rgba16i";
    case GPU_RGBA16F:
      return "rgba16f";
    case GPU_RGBA16:
      return "rgba16";
    case GPU_RG8UI:
      return "rg8ui";
    case GPU_RG8I:
      return "rg8i";
    case GPU_RG8:
      return "rg8";
    case GPU_RG32UI:
      return "rg32ui";
    case GPU_RG32I:
      return "rg32i";
    case GPU_RG32F:
      return "rg32f";
    case GPU_RG16UI:
      return "rg16ui";
    case GPU_RG16I:
      return "rg16i";
    case GPU_RG16F:
      return "rg16f";
    case GPU_RG16:
      return "rg16";
    case GPU_R8UI:
      return "r8ui";
    case GPU_R8I:
      return "r8i";
    case GPU_R8:
      return "r8";
    case GPU_R32UI:
      return "r32ui";
    case GPU_R32I:
      return "r32i";
    case GPU_R32F:
      return "r32f";
    case GPU_R16UI:
      return "r16ui";
    case GPU_R16I:
      return "r16i";
    case GPU_R16F:
      return "r16f";
    case GPU_R16:
      return "r16";
    case GPU_R11F_G11F_B10F:
      return "r11f_g11f_b10f";
    case GPU_RGB10_A2:
      return "rgb10_a2";
    default:
      return "unknown";
  }
}

static const char *to_string(const PrimitiveIn &layout)
{
  switch (layout) {
    case PrimitiveIn::POINTS:
      return "points";
    case PrimitiveIn::LINES:
      return "lines";
    case PrimitiveIn::LINES_ADJACENCY:
      return "lines_adjacency";
    case PrimitiveIn::TRIANGLES:
      return "triangles";
    case PrimitiveIn::TRIANGLES_ADJACENCY:
      return "triangles_adjacency";
    default:
      return "unknown";
  }
}

static const char *to_string(const PrimitiveOut &layout)
{
  switch (layout) {
    case PrimitiveOut::POINTS:
      return "points";
    case PrimitiveOut::LINE_STRIP:
      return "line_strip";
    case PrimitiveOut::TRIANGLE_STRIP:
      return "triangle_strip";
    default:
      return "unknown";
  }
}

static const char *to_string(const DepthWrite &value)
{
  switch (value) {
    case DepthWrite::ANY:
      return "depth_any";
    case DepthWrite::GREATER:
      return "depth_greater";
    case DepthWrite::LESS:
      return "depth_less";
    default:
      return "depth_unchanged";
  }
}

static void print_image_type(std::ostream &os,
                             const ImageType &type,
                             const ShaderCreateInfo::Resource::BindType bind_type)
{
  switch (type) {
    case ImageType::INT_BUFFER:
    case ImageType::INT_1D:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_3D:
    case ImageType::INT_CUBE:
    case ImageType::INT_CUBE_ARRAY:
      os << "i";
      break;
    case ImageType::UINT_BUFFER:
    case ImageType::UINT_1D:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_3D:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_CUBE_ARRAY:
      os << "u";
      break;
    default:
      break;
  }

  if (bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
    os << "image";
  }
  else {
    os << "sampler";
  }

  switch (type) {
    case ImageType::FLOAT_BUFFER:
    case ImageType::INT_BUFFER:
    case ImageType::UINT_BUFFER:
      os << "Buffer";
      break;
    case ImageType::FLOAT_1D:
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::INT_1D:
    case ImageType::INT_1D_ARRAY:
    case ImageType::UINT_1D:
    case ImageType::UINT_1D_ARRAY:
      os << "1D";
      break;
    case ImageType::FLOAT_2D:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::INT_2D:
    case ImageType::INT_2D_ARRAY:
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::SHADOW_2D:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::DEPTH_2D:
    case ImageType::DEPTH_2D_ARRAY:
      os << "2D";
      break;
    case ImageType::FLOAT_3D:
    case ImageType::INT_3D:
    case ImageType::UINT_3D:
      os << "3D";
      break;
    case ImageType::FLOAT_CUBE:
    case ImageType::FLOAT_CUBE_ARRAY:
    case ImageType::INT_CUBE:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::SHADOW_CUBE:
    case ImageType::SHADOW_CUBE_ARRAY:
    case ImageType::DEPTH_CUBE:
    case ImageType::DEPTH_CUBE_ARRAY:
      os << "Cube";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::FLOAT_CUBE_ARRAY:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::SHADOW_CUBE_ARRAY:
    case ImageType::DEPTH_2D_ARRAY:
    case ImageType::DEPTH_CUBE_ARRAY:
      os << "Array";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::SHADOW_2D:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::SHADOW_CUBE:
    case ImageType::SHADOW_CUBE_ARRAY:
      os << "Shadow";
      break;
    default:
      break;
  }
  os << " ";
}

static std::ostream &print_qualifier(std::ostream &os, const Qualifier &qualifiers)
{
  if (bool(qualifiers & Qualifier::NO_RESTRICT) == false) {
    os << "restrict ";
  }
  if (bool(qualifiers & Qualifier::READ) == false) {
    os << "writeonly ";
  }
  if (bool(qualifiers & Qualifier::WRITE) == false) {
    os << "readonly ";
  }
  return os;
}

static void print_resource(std::ostream &os, const ShaderCreateInfo::Resource &res, int slot = -1)
{

  if (true) {
    os << "layout(set = 0 , binding = " << slot;
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      os << ", " << to_string(res.image.format);
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      os << ", std140";
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      os << ", std430";
    }
    os << ") ";
  }
  else if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
    os << "layout(std140) ";
  }

  int64_t array_offset;
  StringRef name_no_array;

  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::SAMPLER:
      os << "uniform ";
      print_image_type(os, res.sampler.type, res.bind_type);
      os << res.sampler.name << ";\n";
      break;
    case ShaderCreateInfo::Resource::BindType::IMAGE:
      os << "uniform ";
      print_qualifier(os, res.image.qualifiers);
      print_image_type(os, res.image.type, res.bind_type);
      os << res.image.name << ";\n";
      break;
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      array_offset = res.uniformbuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.uniformbuf.name :
                                             StringRef(res.uniformbuf.name.c_str(), array_offset);
      os << "uniform " << name_no_array << " { " << res.uniformbuf.type_name << " _"
         << res.uniformbuf.name << "; };\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.c_str(), array_offset);
      print_qualifier(os, res.storagebuf.qualifiers);
      os << "buffer ";
      os << name_no_array << " { " << res.storagebuf.type_name << " _" << res.storagebuf.name
         << "; };\n";
      break;
  }
}

static void print_resource_alias(std::ostream &os, const ShaderCreateInfo::Resource &res)
{
  int64_t array_offset;
  StringRef name_no_array;

  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      array_offset = res.uniformbuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.uniformbuf.name :
                                             StringRef(res.uniformbuf.name.c_str(), array_offset);
      os << "#define " << name_no_array << " (_" << name_no_array << ")\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.c_str(), array_offset);
      os << "#define " << name_no_array << " (_" << name_no_array << ")\n";
      break;
    default:
      break;
  }
}

static void print_interface(std::ostream &os,
                            const StringRefNull &prefix,
                            const StageInterfaceInfo &iface,
                            const StringRefNull &suffix = "")
{
  /* TODO(@fclem): Move that to interface check. */
  // if (iface.instance_name.is_empty()) {
  //   BLI_assert_msg(0, "Interfaces require an instance name for geometry shader.");
  //   std::cout << iface.name << ": Interfaces require an instance name for geometry shader.\n";
  //   continue;
  // }
  os << prefix << " " << iface.name << "{" << std::endl;
  for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
    os << "  " << to_string(inout.interp) << " " << to_string(inout.type) << " " << inout.name
       << ";\n";
  }
  os << "}";
  os << (iface.instance_name.is_empty() ? "" : "\n") << iface.instance_name << suffix << ";\n";
}

static int constant_type_size(Type type)
{
  switch (type) {
    case Type::BOOL:
    case Type::FLOAT:
    case Type::INT:
    case Type::UINT:
    case Type::UCHAR4:
    case Type::CHAR4:
    case blender::gpu::shader::Type::VEC3_101010I2:
      return 4;
      break;
    case Type::VEC2:
    case Type::UVEC2:
    case Type::IVEC2:
      return 8;
      break;
    case Type::VEC3:
    case Type::UVEC3:
    case Type::IVEC3:
      return 12;
      break;
    case Type::VEC4:
    case Type::UVEC4:
    case Type::IVEC4:
      return 16;
      break;
    case Type::MAT3:
      return 36 + 3 * 4;
    case Type::MAT4:
      return 64;
      break;
    case blender::gpu::shader::Type::UCHAR:
    case blender::gpu::shader::Type::CHAR:
      return 1;
      break;
    case blender::gpu::shader::Type::UCHAR2:
    case blender::gpu::shader::Type::CHAR2:
      return 2;
      break;
    case blender::gpu::shader::Type::UCHAR3:
    case blender::gpu::shader::Type::CHAR3:
      return 3;
      break;
  }
  BLI_assert(false);
  return -1;
}

std::string VKShader::resources_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;

  /* NOTE: We define macros in GLSL to trigger compilation error if the resource names
   * are reused for local variables. This is to match other backend behavior which needs accessors
   * macros. */

  ss << "\n/* Pass Resources. */\n";
  int slot = 0;
  for (const shader::ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, res, slot);
    slot++;
  }
  for (const shader::ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource_alias(ss, res);
  }

  ss << "\n/* Batch Resources. */\n";
  for (const shader::ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, res);
  }
  for (const shader::ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource_alias(ss, res);
  }
  /*Check the size of pushconstants for development.
Since the limit is mostly 256 bytes or less, we use a uniformbuffer instead.
we can also use bufferreference.*/
  /*for calc std140, ported #constants_calc_size  file :: gpu_py_shader_create_info.cc line :: 717
   */
  auto get_size =
      [](const shader::ShaderCreateInfo::PushConst &uniform, int &size_prev, int &size_last) {
        int pad = 0;
        int size = constant_type_size(uniform.type);
        if (size_last && size_last != size) {
          /* Calc pad. */
          int pack = (size == 8) ? 8 : 16;
          if (size_last < size) {
            pad = pack - (size_last % pack);
          }
          else {
            pad = size_prev % pack;
          }
        }
        else if (size == 12) {
          /* It is still unclear how Vulkan handles padding for `vec3` constants. For now let's
           * follow the rules of the `std140` layout. */
          pad = 4;
        }
        size_prev += pad + size * std::max(1, uniform.array_size);
        size_last = size;
        return;
      };

  Vector<shader::ShaderCreateInfo::PushConst> ubo;
  int size_prev = 0;
  int size_last = 0;

  ss << "\n/* Push Constants. */\n";
  int pushN = 0;
  /// uint32_t push_ofs = 0;
  for (const shader::ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
    get_size(uniform, size_prev, size_last);
    if (uniform.name == "parameters") {
      ubo.append(uniform);
      continue;
    }
    if (pushN == 0) {
      ss << "layout(push_constant) uniform PushConsts \n{\n";
    }
    pushN++;
    /// ss << " layout(offset = " + std::to_string(push_ofs) + ") ";
    ss << to_string(uniform.type) << " " << uniform.name;
    if (uniform.array_size > 0) {
      ss << "[" << uniform.array_size << "]";
    }
    // else
    // push_ofs += to_size(uniform.type, 1);
    ss << ";\n";
  };

  if (pushN > 0) {
    ss << "};\n";
    if (size_prev > VKContext::max_push_constants_size) {
      printf("Warning: require pushconstants reconstruct.  \n");
    };
  }

  ss << "\n/* UBO. Alternate ubo assumes set number 1.*/\n";
  if (ubo.size() > 0) {
    int i = 0;
    for (auto &uniform : ubo) {
      ss << "layout(set = 1, binding = " << std::to_string(i) << " ) uniform ubo_" << uniform.name
         << "\n";
      ss << "{\n";
      ss << to_string(uniform.type) << " " << uniform.name;
      if (uniform.array_size > 0) {
        ss << "[" << uniform.array_size << "]";
      }
      ss << ";\n";
      ss << "};\n";
    };
  };
  ss << "\n";
  return ss.str();
}

static std::string main_function_wrapper(std::string &pre_main, std::string &post_main)
{
  std::stringstream ss;
  /* Prototype for the original main. */
  ss << "\n";
  ss << "void main_function_();\n";
  /* Wrapper to the main function in order to inject code processing on globals. */
  ss << "void main() {\n";
  ss << pre_main;
  ss << "  main_function_();\n";
  ss << post_main;
  ss << "}\n";
  /* Rename the original main. */
  ss << "#define main main_function_\n";
  ss << "\n";
  return ss.str();
}

std::string VKShader::vertex_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string post_main;
  ss << "#define gl_VertexID gl_VertexIndex \n";
  ss << "#define gl_InstanceID gl_InstanceIndex \n";
  ss << "#extension GL_EXT_debug_printf : enable \n ";

  /*Scratch for uploading color in uchar4.*/
  if (info.name_ == "gpu_shader_text") {
    /*
    ss << "#extension GL_EXT_shader_8bit_storage : enable\n";
    ss << "#extension GL_EXT_shader_explicit_arithmetic_types  : enable\n";
    ss << "#extension  GL_EXT_shader_explicit_arithmetic_types_int8  : enable\n";
    */
    ss << "\n/* Inputs. */\n";
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      /// if (VKContext::explicit_location_support &&
      if (true &&
          /* Fix issue with AMDGPU-PRO + workbench_prepass_mesh_vert.glsl being quantized. */
          GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) == false) {
        ss << "layout(location = " << attr.index << ") ";
      }
      if (attr.index == 1) {
        ss << "in  uint col_;\n";
      }
      else if (attr.index == 2) {
        ss << "in  int offset;\n";
      }
      else if (attr.index == 3) {
        ss << "in  ivec2 glyph_size;\n";
      }
      else {
        ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
      }
    }
    ss << "#define col unpackUnorm4x8(col_) \n";
  }
  else if (info.name_ == "gpu_shader_3D_flat_color") {

    ss << "\n/* Inputs. */\n";
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      /// if (VKContext::explicit_location_support &&
      if (true &&
          /* Fix issue with AMDGPU-PRO + workbench_prepass_mesh_vert.glsl being quantized. */
          GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) == false) {
        ss << "layout(location = " << attr.index << ") ";
      }
      if (attr.index == 0) {
        ss << "in  vec2 pos_;\n";
      }
      else {
        ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
      }
    }
    ss << "#define pos vec3(pos_,0) \n";
  }
  else {
    ss << "\n/* Inputs. */\n";
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      /// if (VKContext::explicit_location_support &&
      if (true &&
          /* Fix issue with AMDGPU-PRO + workbench_prepass_mesh_vert.glsl being quantized. */
          GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) == false) {
        ss << "layout(location = " << attr.index << ") ";
      }
      ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
    }
  }
  /* NOTE(D4490): Fix a bug where shader without any vertex attributes do not behave correctly. */
  if (GPU_type_matches_ex(GPU_DEVICE_APPLE, GPU_OS_MAC, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL) &&
      info.vertex_inputs_.is_empty()) {
    ss << "in float gpu_dummy_workaround;\n";
  }

  int loc_out = 0;
  ss << "\n/* Interfaces. */\n";
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    auto prefix = "layout(location =" + std::to_string(loc_out) + ") out ";
    print_interface(ss, prefix, *iface);
    loc_out++;
  }

  // if (!VKContext::layered_rendering_support
  if (!true && bool(info.builtins_ & BuiltinBits::LAYER)) {
    ss << "layout(location =" + std::to_string(loc_out) + ") out gpu_Layer;\n";
    /// ss << "out int gpu_Layer;\n";
    loc_out++;
  }

  if (bool(info.builtins_ & BuiltinBits::BARYCENTRIC_COORD)) {
    // if (!VKContext::native_barycentric_support) {
    if (false) {
      /* Disabled or unsupported. */
    }
    else if (epoxy_has_gl_extension("GL_AMD_shader_explicit_vertex_parameter")) {
      /* Need this for stable barycentric. */
      ss << "layout(location =" + std::to_string(loc_out) + ") flat out vec4 gpu_pos_flat;\n";
      loc_out++;
      ss << "layout(location =" + std::to_string(loc_out) + ") flat  vec4 gpu_pos\n";
      loc_out++;
      // ss << "flat out vec4 gpu_pos_flat;\n";
      // ss << "out vec4 gpu_pos;\n";

      post_main += "  gpu_pos = gpu_pos_flat = gl_Position;\n";
    }
  }

  ss << "\n";
  if (true) {
    std::string pre_main = "";
    if ("workbench_opaque_mesh_tex_none_no_clip" == info.name_){
      post_main = "";
       
    }
    else if ("gpu_shader_2D_image_multi_rect_color" == info.name_) {
      post_main +=
          "debugPrintfEXT(\"Here pos %v2f  texCoord_interp  %v2f  \",pos.xy,texCoord_interp);\n\n";
    }
    else if ("gpu_shader_2D_widget_base" == info.name_) {
      post_main +=
          "debugPrintfEXT(\"Here pos    %v2f , VID  %i   butCo %f  uv  %v2f "
          "\",gl_Position.xy,gl_VertexIndex, butCo, uvInterp);\n\n";
    }
    else if ("gpu_shader_icon" == info.name_) {
      post_main += "debugPrintfEXT(\"Here texCoord_interp   %v2f  \",texCoord_interp.xy);\n\n";
    }
    else if ("gpu_shader_text" == info.name_) {
      pre_main +=
          " /* Quad expansion using instanced rendering. */ \n \
      float x = float(gl_VertexID % 2);\n \
      float y = float(gl_VertexID / 2);\n \
      texCoord_interp = vec2(x,y);\n \
      glyph_offset  = 4*gl_InstanceIndex + gl_VertexID ;\n \
      gl_Position  = vec4(x, y,0,1);\n return;   ";

      pre_main =
          " \
         int size_x = textureSize(glyph, 0).r; \
        float c = texelFetch(glyph, ivec2(0, 0), 0).r; \
        float c1 = texelFetch(glyph, ivec2(1, 0), 0).r; \
        float c2 = texelFetch(glyph, ivec2(2, 0), 0).r; \
        debugPrintfEXT(\"Here  sampling  %i   glyph.R  %f  %f  %f   size_x %i \", glyph_offset,c,c1,c2,size_x);\n\n";
      pre_main = "";

    }



    pre_main += "gl_PointSize = 10.0f; \n";
    // post_main += "gl_Position.y *= -1.;\n\n";
    ss << main_function_wrapper(pre_main, post_main);
  }

  return ss.str();
}

std::string VKShader::fragment_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string pre_main;
  ss << "#extension GL_EXT_debug_printf : enable \n ";

  ss << "\n/* Interfaces. */\n";
  const Vector<StageInterfaceInfo *> &in_interfaces = info.geometry_source_.is_empty() ?
                                                          info.vertex_out_interfaces_ :
                                                          info.geometry_out_interfaces_;

  int loc_in = 0;
  for (const StageInterfaceInfo *iface : in_interfaces) {
    auto prefix = "layout(location =" + std::to_string(loc_in) + ") in\n";
    print_interface(ss, prefix, *iface);
    loc_in++;
  }
  if (bool(info.builtins_ & BuiltinBits::BARYCENTRIC_COORD)) {
    /// if (!VKContext::native_barycentric_support) {
    if (false) {
      ss << "layout(location =" + std::to_string(loc_in) << ") flat in vec4 gpu_pos[3];\n ";
      loc_in++;
      ss << "layout(location =" + std::to_string(loc_in) << ") smooth in vec3 gpu_BaryCoord;\n ";
      loc_in++;
      ss << "layout(location =" + std::to_string(loc_in)
         << ") noperspective in vec3 gpu_BaryCoordNoPersp;\n ";
      loc_in++;
      ss << "#define gpu_position_at_vertex(v) gpu_pos[v]\n";
    }
    else if
        ///(epoxy_has_gl_extension("GL_AMD_shader_explicit_vertex_parameter"))
        (true) {
      std::cout << "native" << std::endl;
      /* NOTE(fclem): This won't work with geometry shader. Hopefully, we don't need geometry
       * shader workaround if this extension/feature is detected. */
      ss << "\n/* Stable Barycentric Coordinates. */\n";

      ss << "layout(location =" + std::to_string(loc_in) << ") flat invec4 gpu_pos_flat;\n";
      loc_in++;
      ss << "layout(location =" + std::to_string(loc_in)
         << ") __explicitInterpAMD in vec4 gpu_pos;\n ";
      loc_in++;

      /* Globals. */
      ss << "vec3 gpu_BaryCoord;\n";
      ss << "vec3 gpu_BaryCoordNoPersp;\n";
      ss << "\n";
      ss << "vec2 stable_bary_(vec2 in_bary) {\n";
      ss << "  vec3 bary = vec3(in_bary, 1.0 - in_bary.x - in_bary.y);\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 0) == gpu_pos_flat) { return bary.zxy; }\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 2) == gpu_pos_flat) { return bary.yzx; }\n";
      ss << "  return bary.xyz;\n";
      ss << "}\n";
      ss << "\n";
      ss << "vec4 gpu_position_at_vertex(int v) {\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 0) == gpu_pos_flat) { v = (v + 2) % 3; }\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 2) == gpu_pos_flat) { v = (v + 1) % 3; }\n";
      ss << "  return interpolateAtVertexAMD(gpu_pos, v);\n";
      ss << "}\n";

      pre_main += "  gpu_BaryCoord = stable_bary_(gl_BaryCoordSmoothAMD);\n";
      pre_main += "  gpu_BaryCoordNoPersp = stable_bary_(gl_BaryCoordNoPerspAMD);\n";
    }
  }
  if (info.early_fragment_test_) {
    ss << "layout(early_fragment_tests) in;\n";
  }
  // if (epoxy_has_gl_extension("GL_ARB_conservative_depth")) {
  if (true) {
    ss << "layout(" << to_string(info.depth_write_) << ") out float gl_FragDepth;\n";
  }

  ss << "\n/* Outputs. */\n";
  int out_index = 0;
  for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
    ss << "layout(location = " << output.index;
    switch (output.blend) {
      case DualBlend::SRC_0:
        ss << ", index = 0";
        break;
      case DualBlend::SRC_1:
        ss << ", index = 1";
        break;
      default:
        break;
    }
    ss << ") ";
    ss << "out " << to_string(output.type) << " " << output.name << ";\n";
    out_index++;
  }
  ss << "\n";

  /// ss << "layout(location = " << std::to_string(out_index) << " ) out vec4 fragColor;\n";

  if (true) {  //(pre_main.empty() == false) {
    std::string post_main =
        "";  ///"debugPrintfEXT(\"Here Frag texco %v2f    color %v4f \",texCoord_interp,color);";
    if ("gpu_shader_2D_image_multi_rect_color" == info.name_) {
      // pre_main += "fragColor = vec4(0.,0.,1.,1); return;//debugPrintfEXT(\"Here  texCoord %v2f
      // finalColor  %v4f    \",texCoord_interp,finalColor);\n\n";
    }
    else if ("gpu_shader_icon" == info.name_) {
      pre_main +=
          "vec4 fragColor_ = texture(image, texCoord_interp); \n debugPrintfEXT(\"Here  sampling  "
          "%v4f  color uniform %v4f  \",fragColor_,color);\n\n";
    }
    else if ("gpu_shader_text" == info.name_) {

      // pre_main = "fragColor =  color_flat.rgba;return;";
    }
    else if ("gpu_shader_2D_widget_base" == info.name_) {
      /*TODO uv coord inverse y.*/
#if 0
      pre_main += " \
        vec2 uv = uvInterp;\n \
        bool upper_half = uv.y > outRectSize.y * 0.5; \n \
        bool right_half = uv.x > outRectSize.x * 0.5; \n \
        float corner_rad; \n \
 \n \
        /* Correct aspect ratio for 2D views not using uniform scaling. \n \
         * uv is already in pixel space so a uniform scale should give us a ratio of 1. */ \n \
        float ratio = (butCo != -2.0) ? (dFdy(uv.y) / dFdx(uv.x)) : 1.0; \n \
        vec2 uv_sdf = uv; \n \
        uv_sdf.x *= ratio; \n \
 \n \
        if (right_half) { \n \
          uv_sdf.x = outRectSize.x * ratio - uv_sdf.x; \n \
        } \n \
        if (upper_half) { \n \
          uv_sdf.y = outRectSize.y - uv_sdf.y; \n \
          corner_rad = right_half ? outRoundCorners.z : outRoundCorners.w; \n \
        } \n \
        else { \n \
          corner_rad = right_half ? outRoundCorners.y : outRoundCorners.x; \n \
        } \n \
 \n \
        /* Fade emboss at the border. */ \n \
        float emboss_size = upper_half ? 0.0 : min(1.0, uv_sdf.x / (corner_rad * ratio)); \n \
 \n \
        /* Signed distance field from the corner (in pixel). \n \
         * inner_sdf is sharp and outer_sdf is rounded. */ \n \
        uv_sdf -= corner_rad; \n \
        float inner_sdf = max(0.0, min(uv_sdf.x, uv_sdf.y)); \n \
        float outer_sdf = -length(min(uv_sdf, 0.0)); \n \
        float sdf = inner_sdf + outer_sdf + corner_rad; \n \
 \n \
        /* Clamp line width to be at least 1px wide. This can happen if the projection matrix \n \
         * has been scaled (i.e: Node editor)... */ \n \
        float line_width = (lineWidth > 0.0) ? max(fwidth(uv.y), lineWidth) : 0.0; \n \
 \n \
        const float aa_radius = 0.5; \n \
        vec3 masks; \n \
        masks.x = smoothstep(-aa_radius, aa_radius, sdf); \n \
        masks.y = smoothstep(-aa_radius, aa_radius, sdf - line_width); \n \
        masks.z = smoothstep(-aa_radius, aa_radius, sdf + line_width * emboss_size); \n \
 \n \
        /* Compose masks together to avoid having too much alpha. */ \n \
        masks.zx = max(vec2(0.0), masks.zx - masks.xy); \n \
 \n \
    debugPrintfEXT(\"Here  roundcorners  %v4f sdf %f  line width %f fragColor %v3f   \",outRoundCorners,sdf, line_width, masks); \n\n";
#endif
    }

    ss << main_function_wrapper(pre_main, post_main);
  }

  return ss.str();
}

std::string VKShader::geometry_layout_declare(const shader::ShaderCreateInfo &info) const
{

  std::stringstream ss;
#if 0
    int max_verts = info.geometry_layout_.max_vertices;
  int invocations = info.geometry_layout_.invocations;
  if (GLContext::geometry_shader_invocations == false && invocations != -1) {
    max_verts *= invocations;
    invocations = -1;
  }

  
  ss << "\n/* Geometry Layout. */\n";
  ss << "layout(" << to_string(info.geometry_layout_.primitive_in);
  if (invocations != -1) {
    ss << ", invocations = " << invocations;
  }
  ss << ") in;\n";

  ss << "layout(" << to_string(info.geometry_layout_.primitive_out)
     << ", max_vertices = " << max_verts << ") out;\n";
  ss << "\n";
#endif
  return ss.str();
}

static shader::StageInterfaceInfo *find_interface_by_name(
    const Vector<shader::StageInterfaceInfo *> &ifaces, const StringRefNull &name)
{
  for (auto *iface : ifaces) {
    if (iface->instance_name == name) {
      return iface;
    }
  }
  return nullptr;
}

std::string VKShader::geometry_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;

  ss << "\n/* Interfaces. */\n";
#if 0
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    bool has_matching_output_iface = find_interface_by_name(info.geometry_out_interfaces_,
                                                            iface->instance_name) != nullptr;
    const char *suffix = (has_matching_output_iface) ? "_in[]" : "[]";
    print_interface(ss, "in", *iface, suffix);
  }
  ss << "\n";
  for (const StageInterfaceInfo *iface : info.geometry_out_interfaces_) {
    bool has_matching_input_iface = find_interface_by_name(info.vertex_out_interfaces_,
                                                           iface->instance_name) != nullptr;
    const char *suffix = (has_matching_input_iface) ? "_out" : "";
    print_interface(ss, "out", *iface, suffix);
  }
  ss << "\n";
#endif
  return ss.str();
}

std::string VKShader::compute_layout_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  ss << "\n/* Compute Layout. */\n";
  ss << "layout(local_size_x = " << info.compute_layout_.local_size_x;
  if (info.compute_layout_.local_size_y != -1) {
    ss << ", local_size_y = " << info.compute_layout_.local_size_y;
  }
  if (info.compute_layout_.local_size_z != -1) {
    ss << ", local_size_z = " << info.compute_layout_.local_size_z;
  }
  ss << ") in;\n";
  ss << "\n";
  return ss.str();
}

void VKShader::set_interface(VKShaderInterface *interface)
{
  /* Assign gpu::Shader super-class interface. */
  Shader::interface = interface;
}
void VKShader::transform_feedback_names_set(Span<const char *> name_list,
                                            const eGPUShaderTFBType geom_type)
{
}

bool VKShader::transform_feedback_enable(GPUVertBuf *buf)
{
  BLI_assert(transform_feedback_type_ != GPU_SHADER_TFB_NONE);
  BLI_assert(buf);
  transform_feedback_active_ = true;
  transform_feedback_vertbuf_ = buf;
  /* TODO(Metal): Enable this assertion once #MTLVertBuf lands. */
  // BLI_assert(static_cast<MTLVertBuf *>(unwrap(transform_feedback_vertbuf_))->get_usage_type() ==
  //            GPU_USAGE_DEVICE_ONLY);
  return true;
}

void VKShader::transform_feedback_disable()
{
  transform_feedback_active_ = false;
  transform_feedback_vertbuf_ = nullptr;
}
/* -------------------------------------------------------------------- */

void VKShader::bind()
{

  VKContext *ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  VKShaderInterface &iface = *((VKShaderInterface *)interface);
  if (!iface.valid)
    return;
  ctx->pipeline_state.active_shader = this;
  attr_mask_unbound_ = iface.enabled_attr_mask_;
}

void VKShader::unbind()
{
  VKContext *ctx = static_cast<VKContext *>(unwrap(GPU_context_active_get()));
  ctx->pipeline_state.active_shader = nullptr;
}

void VKShader::append_write_descriptor(VKTexture *tex, eGPUSamplerState samp_state, uint binding)
{
  auto info = tex->get_image_info(samp_state);
  if (!tex->get_needs_update())
    return;

  /// set number is always 0. swapchain nums => set nums.
  for (int swapchainID = 0; swapchainID < 2; swapchainID++) {

    auto vkinterface = (VKShaderInterface *)(interface);
    auto Set = vkinterface->sets_vec_[0][swapchainID];
    VkWriteDescriptorSet wd = {};
    wd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wd.dstSet = Set;
    wd.dstBinding = binding;
    wd.pImageInfo = info;
    wd.dstArrayElement = 0;
    wd.descriptorCount = 1;
    wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_descs_.append(wd);
  };
};
void VKShader::append_write_descriptor(
    VkDescriptorSet set, void *data, VkDeviceSize size, uint binding, bool iubo)
{

  if (iubo) {

    VkWriteDescriptorSet writeDescriptorSet{};
    write_descs_.append(writeDescriptorSet);
    auto &desc = write_descs_.last();
    desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
    desc.dstSet = set;
    desc.dstBinding = binding;
    desc.descriptorCount = size;

    VkWriteDescriptorSetInlineUniformBlockEXT writeDescriptorSetInlineUniformBlock{};
    write_iub_.append(writeDescriptorSetInlineUniformBlock);
    auto &iub = write_iub_.last();
    iub.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
    iub.dataSize = size;
    iub.pData = data;
    iub.pNext = NULL;
    desc.pNext = &iub;
  }
  else {

    push_ubo->update(data);
    push_ubo->bind(binding);
  }

  // vkUpdateDescriptorSets(VK_DEVICE, write_descs_.size(), write_descs_.data(), 0, NULL);
};

bool VKShader::update_descriptor_set()
{
  auto size = write_descs_.size();
  if (size > 0) {
    for (auto &desc : write_descs_) {
      printf("  Set %llx    binding %d    type  %d    \n\n",
             (uintptr_t)desc.dstSet,
             desc.dstBinding,
             (int)desc.descriptorType);
    }
    vkUpdateDescriptorSets(VK_DEVICE, write_descs_.size(), write_descs_.data(), 0, NULL);
    write_descs_.clear();
    write_iub_.clear();
    return true;
  }
  return false;
}

void VKShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  auto vkinterface = (VKShaderInterface *)interface;
  if (!vkinterface->is_valid_push_location(location))
    return;
  VkDeviceSize size = sizeof(float);
  switch (comp_len) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 16:
      size *= comp_len;
      break;
    default:
      BLI_assert(0);
      break;
  }
  ShaderInput &input = vkinterface->inputs_[location];

  if (input.binding >= 1000) {
    int binding = input.binding - 1000;
    int currentImage = VKContext::get()->get_current_image_index();

    /*inline uniform block binding. IUBB

    append_write_descriptor(vkinterface->sets_vec_[1][currentImage], (void*)data, size *
    array_size, binding);
    */
    append_write_descriptor(
        vkinterface->sets_vec_[1][currentImage], (void *)data, size * array_size, binding, false);
  }
  else {
    BLI_assert(input.binding + size <= vkinterface->push_range_.size);
    memcpy(vkinterface->push_cache_ + input.binding, data, size);
    /// vkCmdPushConstants( current_cmd_, current_layout_, stage, input.binding, size, &data);
  }
}

void VKShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  auto vkinterface = (VKShaderInterface *)interface;
  if (!vkinterface->is_valid_push_location(location))
    return;

  VkDeviceSize size = sizeof(int);
  switch (comp_len) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 16:
      size *= comp_len;
      break;
    default:
      BLI_assert(0);
      break;
  }

  ShaderInput &input = vkinterface->inputs_[location];
  BLI_assert(input.binding + size <= vkinterface->push_range_.size);

  memcpy(vkinterface->push_cache_ + input.binding, data, size);
  // vkCmdPushConstants(current_cmd_, current_layout_, stage, input.binding, size, &data);
}

VKShader::~VKShader()
{
  bool valid = true;
  VkDevice device = context_->device_get();

  if (push_ubo != nullptr) {
    delete push_ubo;
    push_ubo = nullptr;
  }

  for (auto &sh : shaders_) {
    if (sh.module != VK_NULL_HANDLE) {
      valid = this->is_valid();
      vkDestroyShaderModule(device, sh.module, nullptr);
      sh.module = VK_NULL_HANDLE;
    }
  }

  /* Free Pipeline retained. */

  if (pipe != VK_NULL_HANDLE) {
    valid = this->is_valid();
    vkDestroyPipeline(device, pipe, nullptr);
  }

  /// <summary>
  /// On invalid,may be memory leak.
  /// </summary>
  /// BLI_assert(valid );
  context_ = nullptr;
  is_valid_ = false;
}

VkPipeline VKShader::CreatePipeline(

    VkRenderPass renderpass)
{

  VKContext *ctx = VKContext::get();

  auto vkinterface = (VKShaderInterface *)interface;

  if (pipe != VK_NULL_HANDLE) {
    vkDestroyPipeline(VK_DEVICE, pipe, nullptr);
  }

  BLI_assert(this);
  VKShaderInterface *vk_interface = this->get_interface();
  BLI_assert(vk_interface);
  VkPipelineLayout layout;
  vk_interface->finalize(&layout);

  BLI_assert(layout != VK_NULL_HANDLE);
  current_cmd_ = ctx->current_cmd_;
  current_layout_ = layout;

  auto stman = (VKStateManager *)(VKContext::get()->state_manager);

  shaderstages.resize(0);
  VkPipelineShaderStageCreateInfo PSSci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  PSSci.pName = "main";
  PSSci.stage = VK_SHADER_STAGE_VERTEX_BIT;
  PSSci.module = shaders_[0].module;
  shaderstages.append(PSSci);

  if (transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
    PSSci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    PSSci.module = shaders_[2].module;
    shaderstages.append(PSSci);
  }
  else {
    stman->set_raster_discard();
  };

  BLI_assert(vkinterface->desc_inputs_.size() <= 1);  /// input binding number == 0.

  auto &vkPVISci = vkinterface->desc_inputs_[0].vkPVISci;
  if (vkinterface->desc_inputs_.size() == 0) {

    vkPVISci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vkPVISci.vertexBindingDescriptionCount = 0;
    vkPVISci.vertexAttributeDescriptionCount = 0;
    vkPVISci.pVertexAttributeDescriptions = nullptr;
    vkPVISci.pVertexBindingDescriptions = nullptr;
  }

  auto device = ctx->device_get();
  auto ci = stman->get_pipelinecreateinfo(renderpass, layout);

  ci.pStages = shaderstages.data();
  ci.stageCount = shaderstages.size();
  ci.pVertexInputState = &vkPVISci;
  ci.subpass = 0;
  ci.pTessellationState = nullptr;

  VK_CHECK2(
      vkCreateGraphicsPipelines(device, stman->get_pipeline_cache(), 1, &ci, nullptr, &pipe));

  return pipe;
};

}  // namespace blender::gpu
