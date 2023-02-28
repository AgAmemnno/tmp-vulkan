/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vk_shaders.hh"
#include "vk_context.hh"
#include <algorithm>
#include <algorithm>  // std::max
#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <vector>

#ifdef NVP_SUPPORTS_SHADERC
#  include <shaderc/shaderc.hpp>
#endif

//#include "nvprint.hpp"

/**
 # functions in nvh

 - nvh::fileExists : check if file exists
 - nvh::findFile : finds filename in provided search directories
 - nvh::loadFile : (multiple overloads) loads file as std::string, binary or text, can also search
 in provided directories
 - nvh::getFileName : splits filename from filename with path
 - nvh::getFilePath : splits filepath from filename with path
 */
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "BLI_string.h"
#include "BLI_vector.hh"

#include "GPU_capabilities.h"
#include "GPU_platform.h"

#include "gl_debug.hh"
#include "gl_vertex_buffer.hh"

#include "gl_shader.hh"
#include "gl_shader_interface.hh"

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

namespace nvh {

inline bool fileExists(const char *filename)
{
  std::ifstream stream;
  stream.open(filename);
  return stream.is_open();
}

// returns first found filename (searches within directories provided)
inline std::string findFile(const std::string &infilename,
                            const std::vector<std::string> &directories,
                            bool warn = false)
{
  std::ifstream stream;

  {
    stream.open(infilename.c_str());
    if (stream.is_open()) {
      // nvprintfLevel(LOGLEVEL_INFO, "Found: %s\n", infilename.c_str());
      return infilename;
    }
  }

  for (const auto &directory : directories) {
    std::string filename = directory + "/" + infilename;
    stream.open(filename.c_str());
    if (stream.is_open()) {
      // nvprintfLevel(LOGLEVEL_INFO, "Found: %s\n", filename.c_str());
      return filename;
    }
  }

  if (warn) {
    GPU_VK_DEBUG_PRINTF("File not found: %s\n", infilename.c_str());
    GPU_VK_DEBUG_PRINTF("In directories: \n");
    for (const auto &directory : directories) {
      GPU_VK_DEBUG_PRINTF(" - %s\n", directory.c_str());
    }
    GPU_VK_DEBUG_PRINTF("\n");
  }

  return {};
}

inline std::string loadFile(const std::string &filename, bool binary)
{
  std::string result;
  std::ifstream stream(filename,
                       std::ios::ate | (binary ? std::ios::binary : std::ios_base::openmode(0)));

  if (!stream.is_open()) {
    return result;
  }

  result.reserve(stream.tellg());
  stream.seekg(0, std::ios::beg);

  result.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  return result;
}

inline std::string loadFile(const char *filename, bool binary)
{
  std::string name(filename);
  return loadFile(name, binary);
}

inline std::string loadFile(const std::string &filename,
                            bool binary,
                            const std::vector<std::string> &directories,
                            std::string &filenameFound,
                            bool warn = false)
{
  filenameFound = findFile(filename, directories, warn);
  if (filenameFound.empty()) {
    return {};
  }
  else {
    return loadFile(filenameFound, binary);
  }
}

inline std::string loadFile(const std::string filename,
                            bool binary,
                            const std::vector<std::string> &directories,
                            bool warn = false)
{
  std::string filenameFound;
  return loadFile(filename, binary, directories, filenameFound, warn);
}

// splits filename excluding path
inline std::string getFileName(std::string const &fullPath)
{
  size_t istart;
  for (istart = fullPath.size() - 1;
       istart != -1 && fullPath[istart] != '\\' && fullPath[istart] != '/';
       istart--)
    ;
  return std::string(&fullPath[istart + 1]);
}

// splits path from filename
inline std::string getFilePath(const char *filename)
{
  std::string path;
  // find path in filename
  {
    std::string filepath(filename);

    size_t pos0 = filepath.rfind('\\');
    size_t pos1 = filepath.rfind('/');

    pos0 = pos0 == std::string::npos ? 0 : pos0;
    pos1 = pos1 == std::string::npos ? 0 : pos1;

    path = filepath.substr(0, std::max(pos0, pos1));
  }

  if (path.empty()) {
    path = ".";
  }

  return path;
}

// Return true if the filename ends with ending. i.e. ".png"
inline bool endsWith(std::string const &value, std::string const &ending)
{
  if (ending.size() > value.size())
    return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

}  // namespace nvh

namespace nvh {

std::string ShaderFileManager::format(const char *msg, ...)
{
  char text[8192];
  va_list list;

  if (msg == 0)
    return std::string();

  va_start(list, msg);
  vsnprintf(text, sizeof(text), msg, list);
  va_end(list);

  return std::string(text);
}

inline std::string ShaderFileManager::markerString(int line,
                                                   std::string const &filename,
                                                   int fileid)
{
  if (m_supportsExtendedInclude || m_forceLineFilenames) {
#if defined(_WIN32) && 1
    std::string fixedname;
    for (size_t i = 0; i < filename.size(); i++) {
      char c = filename[i];
      if (c == '/' || c == '\\') {
        fixedname.append("\\\\");
      }
      else {
        fixedname.append(1, c);
      }
    }
#else
    std::string fixedname = filename;
#endif
    return ShaderFileManager::format("#line %d \"", line) + fixedname + std::string("\"\n");
  }
  else {
    return ShaderFileManager::format("#line %d %d\n", line, fileid);
  }
}

std::string ShaderFileManager::getIncludeContent(IncludeID idx, std::string &filename)
{
  IncludeEntry &entry = m_includes[idx];

  filename = entry.filename;

  if (m_forceIncludeContent) {
    return entry.content;
  }

  if (!entry.content.empty() && !findFile(entry.filename, m_directories).empty()) {
    return entry.content;
  }

  std::string content = loadFile(entry.filename, false, m_directories, filename, true);
  return content.empty() ? entry.content : content;
}

std::string ShaderFileManager::getContent(std::string const &filename, std::string &filenameFound)
{
  if (filename.empty()) {
    return std::string();
  }

  IncludeID idx = findInclude(filename);

  if (idx.isValid()) {
    return getIncludeContent(idx, filenameFound);
  }

  // fall back
  filenameFound = filename;
  return loadFile(filename, false, m_directories, filenameFound, true);
}

std::string ShaderFileManager::getContentWithRequestingSourceDirectory(
    std::string const &filename, std::string &filenameFound, std::string const &requestingSource)
{
  if (filename.empty()) {
    return std::string();
  }

  IncludeID idx = findInclude(filename);

  if (idx.isValid()) {
    return getIncludeContent(idx, filenameFound);
  }

  // fall back; check requestingSource's directory first.
  filenameFound = filename;
  m_extendedDirectories.resize(m_directories.size() + 1);
  m_extendedDirectories[0] = getDirectoryComponent(requestingSource);
  for (size_t i = 0; i < m_directories.size(); ++i) {
    m_extendedDirectories[i + 1] = m_directories[i];
  }
  return loadFile(filename, false, m_extendedDirectories, filenameFound, true);
}

std::string ShaderFileManager::getDirectoryComponent(std::string filename)
{
  while (!filename.empty()) {
    auto popped = filename.back();
    filename.pop_back();
    switch (popped) {
      case '/':
        goto exitLoop;
#if defined(_WIN32)
      case '\\':
        goto exitLoop;
#endif
    }
  }
exitLoop:
  if (filename.empty())
    filename.push_back('.');
  return filename;
}

std::string ShaderFileManager::manualInclude(std::string const &filename,
                                             std::string &filenameFound,
                                             std::string const &prepend,
                                             bool foundVersion)
{
  std::string source = getContent(filename, filenameFound);
  return manualIncludeText(source, filenameFound, prepend, foundVersion);
}

std::string ShaderFileManager::manualIncludeText(std::string const &sourceText,
                                                 std::string const &textFilename,
                                                 std::string const &prepend,
                                                 bool foundVersion)
{
  if (sourceText.empty()) {
    return std::string();
  }

  std::stringstream stream;
  stream << sourceText;
  std::string line, text;

  // Handle command line defines
  text += prepend;
  if (m_lineMarkers) {
    text += markerString(1, textFilename, 0);
  }

  int lineCount = 0;
  while (std::getline(stream, line)) {
    std::size_t offset = 0;
    lineCount++;

    // Version
    offset = line.find("#version");
    if (offset != std::string::npos) {
      std::size_t commentOffset = line.find("//");
      if (commentOffset != std::string::npos && commentOffset < offset)
        continue;

      if (foundVersion) {
        // someone else already set the version, so just comment out
        text += std::string("//") + line + std::string("\n");
      }
      else {
        // Reorder so that the #version line is always the first of a shader text
        text = line + std::string("\n") + text + std::string("//") + line + std::string("\n");
        foundVersion = true;
      }
      continue;
    }

    // Handle replacing #include with text if configured to do so.
    // Otherwise just insert the #include command verbatim, for shaderc to handle.
    if (m_handleIncludePasting) {
      offset = line.find("#include");
      if (offset != std::string::npos) {
        std::size_t commentOffset = line.find("//");
        if (commentOffset != std::string::npos && commentOffset < offset)
          continue;

        size_t firstQuote = line.find("\"", offset);
        size_t secondQuote = line.find("\"", firstQuote + 1);

        std::string include = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);

        std::string includeFound;
        std::string includeContent = manualInclude(
            include, includeFound, std::string(), foundVersion);

        if (!includeContent.empty()) {
          text += includeContent;
          if (m_lineMarkers) {
            text += std::string("\n") + markerString(lineCount + 1, textFilename, 0);
          }
        }
        continue;  // Skip adding the original #include line.
      }
    }

    text += line + "\n";
  }

  return text;
}

ShaderFileManager::IncludeID ShaderFileManager::registerInclude(std::string const &name,
                                                                std::string const &filename,
                                                                std::string const &content)
{
  // find if already registered
  for (size_t i = 0; i < m_includes.size(); i++) {
    if (m_includes[i].name == name) {
      m_includes[i].content = content;
      return i;
    }
  }

  IncludeEntry entry;
  entry.name = name;
  entry.filename = filename.empty() ? name : filename;
  entry.content = content;

  m_includes.push_back(entry);

  return m_includes.size() - 1;
}

ShaderFileManager::IncludeID ShaderFileManager::findInclude(std::string const &name) const
{
  // check registered includes first
  for (std::size_t i = 0; i < m_includes.size(); ++i) {
    if (m_includes[i].name == name) {
      return IncludeID(i);
    }
  }

  return IncludeID();
}

bool ShaderFileManager::loadIncludeContent(IncludeID idx)
{
  std::string filenameFound;
  m_includes[idx].content = getIncludeContent(idx, filenameFound);
  return !m_includes[idx].content.empty();
}

const ShaderFileManager::IncludeEntry &ShaderFileManager::getIncludeEntry(IncludeID idx) const
{
  return m_includes[idx];
}

std::string ShaderFileManager::getProcessedContent(std::string const &filename,
                                                   std::string &filenameFound)
{
  return manualInclude(filename, filenameFound, "", false);
}

}  // namespace nvh

#define NV_LINE_MARKERS 1

namespace nvvk {

const VkShaderModule ShaderModuleManager::PREPROCESS_ONLY_MODULE = (VkShaderModule)~0;

#ifdef NVP_SUPPORTS_SHADERC
// Shared shaderc compiler, and reference count + mutex protecting it.
shaderc_compiler_t ShaderModuleManager::s_shadercCompiler = nullptr;
uint32_t ShaderModuleManager::s_shadercCompilerUsers{0};
std::mutex ShaderModuleManager::s_shadercCompilerMutex;

// Adapts the include file loader of nvh::ShaderFileManager to what shaderc expects.
class ShadercIncludeBridge : public shaderc::CompileOptions::IncluderInterface {
  // Borrowed pointer to our include file loader.
  nvvk::ShaderModuleManager *m_pShaderFileManager;

  // Inputs/outputs reused for manualInclude.
  std::string m_filenameFound;
  const std::string m_emptyString;

  // Subtype of shaderc_include_result that holds the include data
  // we found; MUST be static_cast to this type before delete-ing as
  // shaderc_include_result lacks virtual destructor.
  class Result : public shaderc_include_result {
    // Containers for actual data; shaderc_include_result pointers
    // point to data held within.
    const std::string m_content;
    const std::string m_filenameFound;

   public:
    Result(std::string content, std::string filenameFound)
        : m_content(std::move(content)), m_filenameFound(std::move(filenameFound))
    {
      this->source_name = m_filenameFound.data();
      this->source_name_length = m_filenameFound.size();
      this->content = m_content.data();
      this->content_length = m_content.size();
      this->user_data = nullptr;
    }
  };

 public:
  ShadercIncludeBridge(nvvk::ShaderModuleManager *pShaderFileManager)
  {
    m_pShaderFileManager = pShaderFileManager;
  }

  // Handles shaderc_include_resolver_fn callbacks.
  virtual shaderc_include_result *GetInclude(const char *requested_source,
                                             shaderc_include_type type,
                                             const char *requesting_source,
                                             size_t /*include_depth*/) override
  {
    std::string filename = requested_source;
    std::string includeFileText;
    bool versionFound = false;  // Trying to match glslc behavior: it doesn't allow #version
                                // directives in include files.
    if (type == shaderc_include_type_relative)  // "header.h"
    {
      includeFileText = m_pShaderFileManager->getContentWithRequestingSourceDirectory(
          filename, m_filenameFound, requesting_source);
    }
    else  // shaderc_include_type_standard <header.h>
    {
      includeFileText = m_pShaderFileManager->getContent(filename, m_filenameFound);
    }
    std::string content = m_pShaderFileManager->manualIncludeText(
        includeFileText, m_filenameFound, m_emptyString, versionFound);
    return new Result(std::move(content), std::move(m_filenameFound));
  }

  // Handles shaderc_include_result_release_fn callbacks.
  virtual void ReleaseInclude(shaderc_include_result *data) override
  {
    delete static_cast<Result *>(data);
  }

  // Set as the includer for the given shaderc_compile_options_t.
  // This ShadercIncludeBridge MUST not be destroyed while in-use by a
  // shaderc compiler using these options.
  void setAsIncluder(shaderc_compile_options_t options)
  {
    shaderc_compile_options_set_include_callbacks(
        options,
        [](void *pvShadercIncludeBridge,
           const char *requestedSource,
           int type,
           const char *requestingSource,
           size_t includeDepth) {
          return static_cast<ShadercIncludeBridge *>(pvShadercIncludeBridge)
              ->GetInclude(
                  requestedSource, (shaderc_include_type)type, requestingSource, includeDepth);
        },
        [](void *pvShadercIncludeBridge, shaderc_include_result *includeResult) {
          return static_cast<ShadercIncludeBridge *>(pvShadercIncludeBridge)
              ->ReleaseInclude(includeResult);
        },
        this);
  }
};
#endif /* NVP_SUPPORTS_SHADERC */

std::string ShaderModuleManager::DefaultInterface::getTypeDefine(uint32_t type) const
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
}

uint32_t ShaderModuleManager::DefaultInterface::getTypeShadercKind(uint32_t type) const
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
}

bool ShaderModuleManager::setupShaderModule(ShaderModule &module)
{
  Definition &definition = module.definition;

  module.module = VK_NULL_HANDLE;
  if (definition.filetype == FILETYPE_DEFAULT) {
    definition.filetype = m_filetype;
  }

  std::string combinedPrepend = m_prepend;
  std::string combinedFilenames;
  combinedPrepend += definition.prepend;
  combinedFilenames += definition.filename;

  if (definition.filetype == FILETYPE_SPIRV) {
    std::string filenameFound;
    definition.content = nvh::loadFile(definition.filename, true, m_directories, filenameFound);
  }
  else {
    std::string prepend = m_usedSetupIF->getTypeDefine(definition.type);

    definition.content = manualInclude(definition.filename,
                                       definition.filenameFound,
                                       prepend + m_prepend + definition.prepend,
                                       false);
  }

  if (definition.content.empty()) {
    return false;
  }

  if (m_preprocessOnly) {
    module.module = PREPROCESS_ONLY_MODULE;
    return true;
  }
  else {
    VkResult vkresult = VK_ERROR_INVALID_SHADER_NV;
    VkShaderModuleCreateInfo shaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

#if NVP_SUPPORTS_SHADERC
    shaderc_compilation_result_t result = nullptr;
    if (definition.filetype == FILETYPE_GLSL) {
      std::lock_guard<std::mutex> guard(s_shadercCompilerMutex);
      shaderc_shader_kind shaderkind = (shaderc_shader_kind)m_usedSetupIF->getTypeShadercKind(
          definition.type);
      shaderc_compile_options_t options =
          (shaderc_compile_options_t)m_usedSetupIF->getShadercCompileOption(s_shadercCompiler);
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

        shaderc_compile_options_set_optimization_level(m_shadercOptions,
                                                       m_shadercOptimizationLevel);

        // Keep debug info, doesn't cost shader execution perf, only compile-time and memory size.
        // Improves usage for debugging tools, not recommended for shipping application,
        // but good for developmenent builds.
        shaderc_compile_options_set_generate_debug_info(m_shadercOptions);

        options = m_shadercOptions;
      }

      // Tell shaderc to use this class (really our base class, nvh::ShaderFileManager) to load
      // include files.
      ShadercIncludeBridge shadercIncludeBridge(this);
      shadercIncludeBridge.setAsIncluder(options);

      // Note: need filenameFound, not filename, so that relative includes work.
      result = shaderc_compile_into_spv(s_shadercCompiler,
                                        definition.content.c_str(),
                                        definition.content.size(),
                                        shaderkind,
                                        definition.filenameFound.c_str(),
                                        "main",
                                        options);
      printf("Shader Context \n\n\n %s ", definition.content.c_str());
      if (!result) {
        return false;
      }

      if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
        bool failedToOptimize = strstr(shaderc_result_get_error_message(result),
                                       "failed to optimize");
        // int level = failedToOptimize ? LOGLEVEL_WARNING : LOGLEVEL_ERROR;
        GPU_VK_DEBUG_PRINTF("%s: optimization_level_performance\n", definition.filename.c_str());
        GPU_VK_DEBUG_PRINTF("  %s\n", definition.prepend.c_str());
        GPU_VK_DEBUG_PRINTF("  %s\n", shaderc_result_get_error_message(result));
        shaderc_result_release(result);

        if (!failedToOptimize || options != m_shadercOptions) {
          return false;
        }

        // try again without optimization
        shaderc_compile_options_set_optimization_level(m_shadercOptions,
                                                       shaderc_optimization_level_zero);

        result = shaderc_compile_into_spv(s_shadercCompiler,
                                          definition.content.c_str(),
                                          definition.content.size(),
                                          shaderkind,
                                          definition.filename.c_str(),
                                          "main",
                                          options);
      }

      if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
        GPU_VK_DEBUG_PRINTF("%s: optimization_level_zero\n", definition.filename.c_str());
        GPU_VK_DEBUG_PRINTF("  %s\n", definition.prepend.c_str());
        GPU_VK_DEBUG_PRINTF("  %s\n", shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        return false;
      }

      shaderModuleInfo.codeSize = shaderc_result_get_length(result);
      shaderModuleInfo.pCode = (const uint32_t *)shaderc_result_get_bytes(result);
    }
    else
#else
    if (definition.filetype == FILETYPE_GLSL) {
      LOGW("No direct GLSL support\n");
      return false;
    }
    else
#endif
    {
      shaderModuleInfo.codeSize = definition.content.size();
      shaderModuleInfo.pCode = (const uint32_t *)definition.content.c_str();
    }

    vkresult = ::vkCreateShaderModule(m_device, &shaderModuleInfo, nullptr, &module.module);

    if (vkresult == VK_SUCCESS && m_keepModuleSPIRV) {
      module.moduleSPIRV = std::string((const char *)shaderModuleInfo.pCode,
                                       shaderModuleInfo.codeSize);
    }

#if NVP_SUPPORTS_SHADERC
    if (result) {
      shaderc_result_release(result);
    }
#endif

    return vkresult == VK_SUCCESS;
  }
}

void ShaderModuleManager::init(VkDevice device, int apiMajor, int apiMinor)
{
  assert(!m_device);
  m_device = device;
  m_apiMajor = apiMajor;
  m_apiMinor = apiMinor;

#if NVP_SUPPORTS_SHADERC
  // First user initializes compiler.
  std::lock_guard<std::mutex> lock(s_shadercCompilerMutex);
  s_shadercCompilerUsers++;
  if (!s_shadercCompiler) {
    s_shadercCompiler = shaderc_compiler_initialize();
  }
  m_shadercOptions = shaderc_compile_options_initialize();
#endif
}

void ShaderModuleManager::deinit()
{
  if (m_device) {
#if NVP_SUPPORTS_SHADERC
    // Last user de-inits compiler.
    std::lock_guard<std::mutex> lock(s_shadercCompilerMutex);
    s_shadercCompilerUsers--;
    if (s_shadercCompiler && s_shadercCompilerUsers == 0) {
      shaderc_compiler_release(s_shadercCompiler);
      s_shadercCompiler = nullptr;
    }
    if (m_shadercOptions) {
      shaderc_compile_options_release(m_shadercOptions);
    }
#endif
  }
  deleteShaderModules();
  m_device = nullptr;
}

ShaderModuleID ShaderModuleManager::createShaderModule(const Definition &definition)
{
  ShaderModule module;
  module.definition = definition;

  setupShaderModule(module);

  // find unused
  for (size_t i = 0; i < m_shadermodules.size(); i++) {
    if (m_shadermodules[i].definition.type == 0) {
      m_shadermodules[i] = module;
      return i;
    }
  }

  m_shadermodules.push_back(module);
  return m_shadermodules.size() - 1;
}

ShaderModuleID ShaderModuleManager::createShaderModule(uint32_t type,
                                                       std::string const &filename,
                                                       std::string const &prepend,
                                                       FileType fileType /*= FILETYPE_DEFAULT*/,
                                                       std::string const &entryname /*= "main"*/)
{
  Definition def;
  def.type = type;
  def.filename = filename;
  def.prepend = prepend;
  def.filetype = fileType;
  def.entry = entryname;
  return createShaderModule(def);
}

bool ShaderModuleManager::areShaderModulesValid()
{
  bool valid = true;
  for (size_t i = 0; i < m_shadermodules.size(); i++) {
    valid = valid && isValid(i);
  }
  return valid;
}

void ShaderModuleManager::deleteShaderModules()
{
  for (size_t i = 0; i < m_shadermodules.size(); i++) {
    destroyShaderModule((ShaderModuleID)i);
  }
  m_shadermodules.clear();
}

void ShaderModuleManager::reloadModule(ShaderModuleID idx)
{
  if (!isValid(idx))
    return;

  ShaderModule &module = getShaderModule(idx);

  bool old = m_preprocessOnly;
  m_preprocessOnly = module.module == PREPROCESS_ONLY_MODULE;
  if (module.module && module.module != PREPROCESS_ONLY_MODULE) {
    vkDestroyShaderModule(m_device, module.module, nullptr);
    module.module = nullptr;
  }
  if (module.definition.type != 0) {
    setupShaderModule(module);
  }

  m_preprocessOnly = old;
}

void ShaderModuleManager::reloadShaderModules()
{
  GPU_VK_DEBUG_PRINTF("Reloading programs...\n");

  for (size_t i = 0; i < m_shadermodules.size(); i++) {
    reloadModule((ShaderModuleID)i);
  }

  GPU_VK_DEBUG_PRINTF("done\n");
}

bool ShaderModuleManager::isValid(ShaderModuleID idx) const
{
  return idx.isValid() &&
         ((m_shadermodules[idx].definition.type && m_shadermodules[idx].module != 0) ||
          !m_shadermodules[idx].definition.type);
}

VkShaderModule ShaderModuleManager::get(ShaderModuleID idx) const
{
  return m_shadermodules[idx].module;
}

ShaderModuleManager::ShaderModule &ShaderModuleManager::getShaderModule(ShaderModuleID idx)
{
  return m_shadermodules[idx];
}

const ShaderModuleManager::ShaderModule &ShaderModuleManager::getShaderModule(
    ShaderModuleID idx) const
{
  return m_shadermodules[idx];
}

void ShaderModuleManager::destroyShaderModule(ShaderModuleID idx)
{
  if (!isValid(idx))
    return;

  ShaderModule &module = getShaderModule(idx);

  if (module.module && module.module != PREPROCESS_ONLY_MODULE) {
    vkDestroyShaderModule(m_device, module.module, nullptr);
    module.module = 0;
  }
  module.definition = Definition();
}

const char *ShaderModuleManager::getCode(ShaderModuleID idx, size_t *len) const
{
  return m_shadermodules[idx].definition.content.c_str();
}
const size_t ShaderModuleManager::getCodeLen(ShaderModuleID idx) const
{
  return m_shadermodules[idx].definition.content.size();
}

bool ShaderModuleManager::dumpSPIRV(ShaderModuleID idx, const char *filename) const
{
  if (m_shadermodules[idx].moduleSPIRV.empty())
    return false;

  FILE *f = fopen(filename, "wb");
  if (f) {
    fwrite(m_shadermodules[idx].moduleSPIRV.data(), m_shadermodules[idx].moduleSPIRV.size(), 1, f);
    fclose(f);
    return true;
  }

  return false;
}

bool ShaderModuleManager::getSPIRV(ShaderModuleID idx, size_t *pLen, const uint32_t **pCode) const
{
  if (m_shadermodules[idx].moduleSPIRV.empty())
    return false;

  *pLen = m_shadermodules[idx].moduleSPIRV.size();
  *pCode = reinterpret_cast<const uint32_t *>(m_shadermodules[idx].moduleSPIRV.data());
  return true;
}

}  // namespace nvvk
