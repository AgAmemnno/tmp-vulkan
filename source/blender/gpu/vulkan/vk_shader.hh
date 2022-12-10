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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include <vulkan/vulkan.h>
#define WITH_VULKAN_SHADER_COMPILATION


#include "gpu_shader_private.hh"


#include "vk_context.hh"
#include "vk_shader_interface.hh"
#include "vk_layout.hh"
#include <shaderc/shaderc.hpp>

#if VK_SHADER_TRANSLATION_DEBUG_OUTPUT
#  define shader_debug_printf printf
#else
#  define shader_debug_printf(...) /* Null print. */
#endif

namespace blender {
namespace gpu {

enum class VKShaderStageType {
  VertexShader,
  GeometryShader,
  FragmentShader,
  ComputeShader,
};


std::ostream &operator<<(std::ostream &os, const VKShaderStageType &stage);

/**
 * Implementation of shader compilation and uniforms handling using Vulkan.
 **/
class VKShader : public Shader {



 private:
  VKContext *context_ = nullptr;
  enum SHADER {
    VKSHADER_VERTEX,
    VKSHADER_FRAGMENT,
    VK_SHADER_GEOMETRY,
    VKSHADER_COMPUTE,
    VK_SHADER_ALL
  };
  ShaderModule shaders_[VK_SHADER_ALL];
  Vector<VkPipelineShaderStageCreateInfo> shaderstages;

  /* Transform feedback mode. */
  eGPUShaderTFBType transform_feedback_type_ = GPU_SHADER_TFB_NONE;
  bool transform_feedback_active_ = false;
  GPUVertBuf *transform_feedback_vertbuf_ = nullptr;

  VkPipeline                                                           pipe = VK_NULL_HANDLE;

  /** True if any shader failed to compile. */
  bool compilation_failed_ = false;

  /// <summary>
  /// This compiler flag affects pipeline layout.
  /// Whether to include unused variables in the layout ? .. etc
  /// </summary>
  shaderc_optimization_level m_shadercOptimizationLevel = shaderc_optimization_level_zero;//shaderc_optimization_level_performance;


 public:


  VKShader(const char *name);
  VKShader(
           VKShaderInterface *interface,
           const char *name,
           std::string input_vertex_source,
           std::string input_fragment_source,
           std::string&& vert_function_name,
           std::string&& frag_function_name);
  ~VKShader();
  bool getShaderModule(ShaderModule & sm,int i)
  {
    if (shaders_[i].module == VK_NULL_HANDLE)
      return false;
    sm.module = shaders_[i].module;
    sm.shaderModuleInfo = shaders_[i].shaderModuleInfo;
    return true;
  }

  void set_optimization_level(shaderc_optimization_level level){
    m_shadercOptimizationLevel = level;
  };

  void vertex_shader_from_glsl(MutableSpan<const char *> sources) override;
  void geometry_shader_from_glsl(MutableSpan<const char *> sources) override;
  void fragment_shader_from_glsl(MutableSpan<const char *> sources) override;
  void compute_shader_from_glsl(MutableSpan<const char *> sources) override;
  /* Return true on success. */

  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;

  bool is_valid()
  {
    return true;
  };
  VkPipeline get_pipeline()
  {
    return pipe;
  };

  VKShaderInterface *get_interface()
  {
    return static_cast<VKShaderInterface *>(this->interface);
  }

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;
  void transform_feedback_names_set(Span<const char *> name_list,
                                    const eGPUShaderTFBType geom_type) override;
  bool transform_feedback_enable(GPUVertBuf *buf) override;
  void transform_feedback_disable(void) override;
  void bind(void) override;
  void unbind(void) override;


  void append_write_descriptor(VKTexture *tex,eGPUSamplerState samp_state,uint binding);
  bool update_descriptor_set();


  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;


  int program_handle_get() const override
  {
    return -1;
  }


  void set_interface(VKShaderInterface *interface);
  VkPipeline CreatePipeline(VkRenderPass renderpass);


  VkCommandBuffer current_cmd_ = VK_NULL_HANDLE;
  VkPipelineLayout current_layout_ = VK_NULL_HANDLE;
  Vector<VkWriteDescriptorSet> write_descs_;

 private:
  bool is_valid_ = false;
  GHOST_TSuccess compile_source(Span<const char *> sources, VKShaderStageType stage);
  VkShaderModule create_shader_module(MutableSpan<const char *> sources, VKShaderStageType stage);

  MEM_CXX_CLASS_ALLOC_FUNCS("VKShader");
};

class VKLogParser : public GPULogParser {
 public:
  char *parse_line(char *log_line, GPULogItem &log_item) override;

 protected:
  char *skip_name_and_stage(char *log_line);
  char *skip_severity_keyword(char *log_line, GPULogItem &log_item);

  MEM_CXX_CLASS_ALLOC_FUNCS("VKLogParser");
};

}  // namespace gpu
}  // namespace blender
