/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NV_SHADERMODULEMANAGER_INCLUDED
#define NV_SHADERMODULEMANAGER_INCLUDED

#include <mutex>
#include <stdio.h>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#ifdef NVP_SUPPORTS_SHADERC
#  define NV_EXTENSIONS
#  include <shaderc/shaderc.h>
#  undef NV_EXTENSIONS
#endif

#define NVP_SUPPORTS_SHADERC 1
#include <shaderc/shaderc.hpp>
#include <stdio.h>
#include <string>
#include <vector>

namespace nvh {

class ShaderFileManager {

  //////////////////////////////////////////////////////////////////////////
  /**
    \class nvh::ShaderFileManager

    The nvh::ShaderFileManager class is meant to be derived from to create the actual api-specific
    shader/program managers.

    The ShaderFileManager provides a system to find/load shader files.
    It also allows resolving #include instructions in HLSL/GLSL source files.
    Such includes can be registered before pointing to strings in memory.

    If m_handleIncludePasting is true, then `#include`s are replaced by
    the include file contents (recursively) before presenting the
    loaded shader source code to the caller. Otherwise, the include file
    loader is still available but `#include`s are left unchanged.

    Furthermore it handles injecting prepended strings (typically used
    for #defines) after the #version statement of GLSL files,
    regardless of m_handleIncludePasting's value.

  */

 public:
  enum FileType {
    FILETYPE_DEFAULT,
    FILETYPE_GLSL,
    FILETYPE_HLSL,
    FILETYPE_SPIRV,
  };

  struct IncludeEntry {
    std::string name;
    std::string filename;
    std::string content;
  };

  typedef std::vector<IncludeEntry> IncludeRegistry;

  static std::string format(const char *msg, ...);

 public:
  class IncludeID {
   public:
    size_t m_value;

    IncludeID() : m_value(size_t(~0))
    {
    }

    IncludeID(size_t b) : m_value((uint32_t)b)
    {
    }

    IncludeID &operator=(size_t b)
    {
      m_value = b;
      return *this;
    }

    bool isValid() const
    {
      return m_value != size_t(~0);
    }

    operator bool() const
    {
      return isValid();
    }
    operator size_t() const
    {
      return m_value;
    }

    friend bool operator==(const IncludeID &lhs, const IncludeID &rhs)
    {
      return rhs.m_value == lhs.m_value;
    }
  };

  struct Definition {
    Definition()
    {
    }
    Definition(uint32_t type, std::string const &prepend, std::string const &filename)
        : type(type), filename(filename), prepend(prepend)
    {
    }
    Definition(uint32_t type, std::string const &filename) : type(type), filename(filename)
    {
    }

    uint32_t type = 0;
    std::string filename;
    std::string prepend;
    std::string entry = "main";
    FileType filetype = FILETYPE_DEFAULT;
    std::string filenameFound;
    std::string content;
  };

  // optionally register files to be included, optionally provide content directly rather than from
  // disk
  //
  // name: name used within shader files
  // diskname = filename on disk (defaults to name if not set)
  // content = provide content as string rather than loading from disk

  IncludeID registerInclude(std::string const &name,
                            std::string const &diskname = std::string(),
                            std::string const &content = std::string());

  // Use m_prepend to pass global #defines
  // Derived api classes will use this as global prepend to the per-definition prepends in
  // combination with the source files actualSoure = m_prepend + definition.prepend +
  // definition.content
  std::string m_prepend;

  // per file state, used when FILETYPE_DEFAULT is provided in the Definition
  FileType m_filetype;

  // add search directories
  void addDirectory(const std::string &dir)
  {
    m_directories.push_back(dir);
  }

  ShaderFileManager(bool handleIncludePasting = true)
      : m_filetype(FILETYPE_GLSL),
        m_lineMarkers(true),
        m_forceLineFilenames(false),
        m_forceIncludeContent(false),
        m_supportsExtendedInclude(false),
        m_handleIncludePasting(handleIncludePasting)
  {
    m_directories.push_back(".");
  }

  //////////////////////////////////////////////////////////////////////////

  // in rare cases you may want to access the included content in detail yourself

  IncludeID findInclude(std::string const &name) const;
  bool loadIncludeContent(IncludeID);
  const IncludeEntry &getIncludeEntry(IncludeID idx) const;

  std::string getProcessedContent(std::string const &filename, std::string &filenameFound);

 protected:
  std::string markerString(int line, std::string const &filename, int fileid);
  std::string getIncludeContent(IncludeID idx, std::string &filenameFound);
  std::string getContent(std::string const &filename, std::string &filenameFound);
  std::string getContentWithRequestingSourceDirectory(std::string const &filename,
                                                      std::string &filenameFound,
                                                      std::string const &requestingSource);

  static std::string getDirectoryComponent(std::string filename);

  std::string manualInclude(std::string const &filename,
                            std::string &filenameFound,
                            std::string const &prepend,
                            bool foundVersion);
  std::string manualIncludeText(std::string const &sourceText,
                                std::string const &textFilename,
                                std::string const &prepend,
                                bool foundVersion);

  bool m_lineMarkers;
  bool m_forceLineFilenames;
  bool m_forceIncludeContent;
  bool m_supportsExtendedInclude;
  bool m_handleIncludePasting;

  std::vector<std::string> m_directories;
  IncludeRegistry m_includes;

  // Used as temporary storage in getContentWithRequestingSourceDirectory; saves on dynamic
  // allocation.
  std::vector<std::string> m_extendedDirectories;
};

}  // namespace nvh

namespace nvvk {

//////////////////////////////////////////////////////////////////////////
/**
  \class nvvk::ShaderModuleManager

  The nvvk::ShaderModuleManager manages VkShaderModules stored in files (SPIR-V or GLSL)

  Using ShaderFileManager it will find the files and resolve #include for GLSL.
  You must add include directories to the base-class for this.

  It also comes with some convenience functions to reload shaders etc.
  That is why we pass out the ShaderModuleID rather than a VkShaderModule directly.

  To change the compilation behavior manipulate the public member variables
  prior createShaderModule.

  m_filetype is crucial for this. You can pass raw spir-v files or GLSL.
  If GLSL is used, shaderc must be used as well (which must be added via
  _add_package_ShaderC() in CMake of the project)

  Example:

  \code{.cpp}
  ShaderModuleManager mgr(myDevice);

  // derived from ShaderFileManager
  mgr.addDirectory("spv/");

  // all shaders get this injected after #version statement
  mgr.m_prepend = "#define USE_NOISE 1\n";

  vid = mgr.createShaderModule(VK_SHADER_STAGE_VERTEX_BIT,   "object.vert.glsl");
  fid = mgr.createShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT, "object.frag.glsl");

  // ... later use module
  info.module = mgr.get(vid);
  \endcode
*/

class ShaderModuleID {
 public:
  size_t m_value;

  ShaderModuleID() : m_value(size_t(~0))
  {
  }

  ShaderModuleID(size_t b) : m_value(b)
  {
  }
  ShaderModuleID &operator=(size_t b)
  {
    m_value = b;
    return *this;
  }

  bool isValid() const
  {
    return m_value != size_t(~0);
  }

  operator bool() const
  {
    return isValid();
  }
  operator size_t() const
  {
    return m_value;
  }

  friend bool operator==(const ShaderModuleID &lhs, const ShaderModuleID &rhs)
  {
    return rhs.m_value == lhs.m_value;
  }
};

class ShaderModuleManager : public nvh::ShaderFileManager {
 public:
  struct ShaderModule {
    ShaderModule() : module(0)
    {
    }

    VkShaderModule module;
    std::string moduleSPIRV;
    Definition definition;
  };

  void init(VkDevice device, int apiMajor = 1, int apiMinor = 1);

  // also calls deleteShaderModules
  void deinit();

  ShaderModuleID createShaderModule(uint32_t type,
                                    std::string const &filename,
                                    std::string const &prepend = "",
                                    FileType fileType = FILETYPE_DEFAULT,
                                    std::string const &entryname = "main");

  void destroyShaderModule(ShaderModuleID idx);
  void reloadModule(ShaderModuleID idx);

  void reloadShaderModules();
  void deleteShaderModules();
  bool areShaderModulesValid();

#ifdef NVP_SUPPORTS_SHADERC
  void setOptimizationLevel(shaderc_optimization_level level)
  {
    m_shadercOptimizationLevel = level;
  }
#endif

  bool isValid(ShaderModuleID idx) const;
  VkShaderModule get(ShaderModuleID idx) const;
  ShaderModule &getShaderModule(ShaderModuleID idx);
  const ShaderModule &getShaderModule(ShaderModuleID idx) const;
  const char *getCode(ShaderModuleID idx, size_t *len = NULL) const;
  const size_t getCodeLen(ShaderModuleID idx) const;
  bool dumpSPIRV(ShaderModuleID idx, const char *filename) const;
  bool getSPIRV(ShaderModuleID idx, size_t *pLen, const uint32_t **pCode) const;

  // state will affect the next created shader module
  // also keep m_filetype in mind!
  bool m_preprocessOnly = false;
  bool m_keepModuleSPIRV = false;

  //////////////////////////////////////////////////////////////////////////
  //
  // for internal development, useful when we have new shader types that
  // are not covered by public VulkanSDK

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

  void setSetupIF(SetupInterface *setupIF);

  ShaderModuleManager(ShaderModuleManager const &) = delete;
  ShaderModuleManager &operator=(ShaderModuleManager const &) = delete;

  // Constructors reference-count the shared shaderc compiler, and
  // disable ShaderFileManager's homemade #include mechanism iff we're
  // using shaderc.
#if NVP_SUPPORTS_SHADERC
  static constexpr bool s_handleIncludePasting = false;
#else
  static constexpr bool s_handleIncludePasting = true;
#endif

  ShaderModuleManager(VkDevice device = nullptr) : ShaderFileManager(s_handleIncludePasting)
  {
    m_usedSetupIF = &m_defaultSetupIF;
    m_supportsExtendedInclude = true;
    m_keepModuleSPIRV = true;
    if (device)
      init(device);
  }

  ~ShaderModuleManager()
  {
    deinit();
  }

  // Shaderc has its own interface for handling include files that I
  // have to subclass; this needs access to protected
  // ShaderFileManager functions.
  friend class ShadercIncludeBridge;
  friend class __ShadercIncludeBridge;

 private:
  ShaderModuleID createShaderModule(const Definition &def);
  bool setupShaderModule(ShaderModule &prog);

  struct DefaultInterface : public SetupInterface {
    std::string getTypeDefine(uint32_t type) const override;
    uint32_t getTypeShadercKind(uint32_t type) const override;
  };

  static const VkShaderModule PREPROCESS_ONLY_MODULE;

  VkDevice m_device = nullptr;
  DefaultInterface m_defaultSetupIF;
  SetupInterface *m_usedSetupIF = nullptr;

  int m_apiMajor = 1;
  int m_apiMinor = 1;

#if NVP_SUPPORTS_SHADERC
  static uint32_t s_shadercCompilerUsers;
  static shaderc_compiler_t s_shadercCompiler;  // Lock mutex below while using.
  static std::mutex s_shadercCompilerMutex;
  shaderc_compile_options_t m_shadercOptions = nullptr;
  shaderc_optimization_level m_shadercOptimizationLevel = shaderc_optimization_level_performance;
#endif

  std::vector<ShaderModule> m_shadermodules;
};

class __ShadercIncludeBridge : public shaderc::CompileOptions::IncluderInterface {
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
  __ShadercIncludeBridge(nvvk::ShaderModuleManager *pShaderFileManager)
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
          return static_cast<__ShadercIncludeBridge *>(pvShadercIncludeBridge)
              ->GetInclude(
                  requestedSource, (shaderc_include_type)type, requestingSource, includeDepth);
        },
        [](void *pvShadercIncludeBridge, shaderc_include_result *includeResult) {
          return static_cast<__ShadercIncludeBridge *>(pvShadercIncludeBridge)
              ->ReleaseInclude(includeResult);
        },
        this);
  }
};

}  // namespace nvvk

#endif  // NV_PROGRAM_INCLUDED
