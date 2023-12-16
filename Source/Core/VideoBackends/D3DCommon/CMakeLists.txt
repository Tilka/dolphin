add_library(videod3dcommon
  D3DCommon.cpp
  D3DCommon.h
  Shader.cpp
  Shader.h
  SwapChain.cpp
  SwapChain.h
)

target_link_libraries(videod3dcommon
PUBLIC
  common
  videocommon
  spirv_cross
)

target_include_directories(videod3dcommon
SYSTEM PUBLIC
  ${CMAKE_SOURCE_DIR}/Externals/glslang/glslang/Public
SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/Externals/glslang/StandAlone
  ${CMAKE_SOURCE_DIR}/Externals/glslang/SPIRV
  ${CMAKE_SOURCE_DIR}/Externals/glslang
)

target_precompile_headers(videod3dcommon REUSE_FROM use_pch)
