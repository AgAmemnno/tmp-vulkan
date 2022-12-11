# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2014 Blender Foundation. All rights reserved.

# avoid noisy warnings
if(CMAKE_COMPILER_IS_GNUCC)
  remove_cc_flag(
    "-Wmissing-declarations"
  )
endif()

set(INC
    src
)

set(INC_SYS
    src
)


set(spirv-cross-core-sources
		src/GLSL.std.450.h
		src/spirv_common.hpp
		src/spirv_cross_containers.hpp
		src/spirv_cross_error_handling.hpp
		src/spirv.hpp
		src/spirv_cross.hpp
		src/spirv_cross.cpp
		src/spirv_parser.hpp
		src/spirv_parser.cpp
		src/spirv_cross_parsed_ir.hpp
		src/spirv_cross_parsed_ir.cpp
		src/spirv_cfg.hpp
		src/spirv_cfg.cpp
)


set(spirv-cross-glsl-sources
		src/spirv_glsl.cpp
		src/spirv_glsl.hpp)


set(SRC ${spirv-cross-core-sources})
blender_add_lib(extern_spirv_cross "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


set(SRC ${spirv-cross-glsl-sources})
blender_add_lib(extern_spirv_cross_glsl "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
