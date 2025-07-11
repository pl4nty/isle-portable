add_library(miniwin-headers INTERFACE)
target_include_directories(miniwin-headers INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

add_library(miniwin STATIC EXCLUDE_FROM_ALL
  # Core
  src/windows/windows.cpp

  # DDraw
  src/ddraw/ddpalette.cpp
  src/ddraw/ddraw.cpp
  src/ddraw/ddsurface.cpp
  src/ddraw/framebuffer.cpp

  # D3DRM
  src/d3drm/d3drm.cpp
  src/d3drm/d3drmdevice.cpp
  src/d3drm/d3drmframe.cpp
  src/d3drm/d3drmlight.cpp
  src/d3drm/d3drmmesh.cpp
  src/d3drm/d3drmtexture.cpp
  src/d3drm/d3drmviewport.cpp
  src/internal/meshutils.cpp

  # D3DRM backends
  src/d3drm/backends/sdl3gpu/renderer.cpp
  src/d3drm/backends/sdl3gpu/shaders/generated/ShaderIndex.cpp
  src/d3drm/backends/software/renderer.cpp
)

target_compile_definitions(miniwin PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

find_package(OpenGL)
find_package(GLEW)
if(OpenGL_FOUND AND GLEW_FOUND)
  message(STATUS "Found OpenGL and GLEW: enabling OpenGL 1.x renderer")
  target_sources(miniwin PRIVATE src/d3drm/backends/opengl1/renderer.cpp)
  target_compile_definitions(miniwin PRIVATE USE_OPENGL1)
  target_link_libraries(miniwin PRIVATE OpenGL::GL GLEW::GLEW)
else()
  message(STATUS "🧩 OpenGL 1.x support not enabled — needs OpenGL and GLEW")
endif()

find_library(OPENGL_ES2_LIBRARY NAMES GLESv2)
if(OPENGL_ES2_LIBRARY)
  message(STATUS "Found OpenGL: enabling OpenGL ES 2.x renderer")
  target_sources(miniwin PRIVATE src/d3drm/backends/opengles2/renderer.cpp)
  target_compile_definitions(miniwin PRIVATE USE_OPENGLES2)
  target_link_libraries(miniwin PRIVATE OpenGL::GL)
else()
  message(STATUS "🧩 OpenGL ES 2.x support not enabled")
endif()

if(WIN32)
  target_sources(miniwin PRIVATE
    src/d3drm/backends/directx9/actual.cpp
    src/d3drm/backends/directx9/renderer.cpp
  )
  target_link_libraries(miniwin PRIVATE d3d9)
endif()

target_compile_definitions(miniwin PUBLIC MINIWIN)

target_include_directories(miniwin
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/generated
)
target_link_libraries(miniwin PUBLIC miniwin-headers)

target_link_libraries(miniwin PRIVATE SDL3::SDL3)

# Shader stuff

set(shader_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/src")
set(shader_gen_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/generated")
set(py_gencshadersource "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/gencshadersource.py")

set(miniwin_shaders
  "${shader_src_dir}/PositionColor.vert.hlsl"
  "${shader_src_dir}/SolidColor.frag.hlsl"
)
set(miniwin_shaders_DEPENDS
  "${shader_src_dir}/Common.hlsl"
)

if(ISLE_COMPILE_SHADERS)
  set(compiled_bindir "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled")
  set(generated_dirs "${compiled_bindir}/dxil" "${compiled_bindir}/msl" "${compiled_bindir}/spirv")

  add_custom_command(OUTPUT ${generated_dirs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory ${generated_dirs}
  )
  add_custom_target(create-generated-shader-directories DEPENDS ${generated_dirs})
endif()

target_sources(miniwin PRIVATE ${py_gencshadersource})

set(shader_variables )
set(shader_headers )
set(shader_stages )
foreach(shader_src IN LISTS miniwin_shaders)
  get_filename_component(filename_wle "${shader_src}" NAME_WLE)
  get_filename_component(shader_name "${filename_wle}" NAME_WLE)
  get_filename_component(src_format_ext "${shader_src}" LAST_EXT)
  get_filename_component(src_stage_ext "${filename_wle}" LAST_EXT)
  string(MAKE_C_IDENTIFIER "${filename_wle}" shader_variable)
  set(shader_json "${shader_src}.json")
  if(src_format_ext STREQUAL ".hlsl")
    set(src_format "HLSL")
  else()
    message(FATAL_ERROR "Unknown source format (${src_format_ext})")
  endif()
  if(src_stage_ext STREQUAL ".vert")
    set(shader_stage "vertex")
  elseif(src_stage_ext STREQUAL ".frag")
    set(shader_stage "fragment")
  elseif(src_stage_ext STREQUAL ".comp")
    set(shader_stage "compute")
    message(FATAL_ERROR "Compute shaders are not (yet) supported")
  else()
    message(FATAL_ERROR "Unknown stage (${src_stage_ext})")
  endif()
  set(dxil "${compiled_bindir}/dxil/${filename_wle}.dxil")
  set(msl "${compiled_bindir}/msl/${filename_wle}.msl")
  set(spirv "${compiled_bindir}/spirv/${filename_wle}.spv")
  set(shader_filename "${filename_wle}.h")
  set(shader_header "${shader_gen_dir}/${shader_filename}")
  if(ISLE_COMPILE_SHADERS)
    add_custom_command(OUTPUT "${dxil}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${dxil}"
        -s "${src_format}" -d "DXIL" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${msl}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${msl}"
        -s "${src_format}" -d "MSL" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${spirv}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${spirv}"
        -s "${src_format}" -d "SPIRV" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${shader_header}"
      COMMAND Python3::Interpreter "${py_gencshadersource}" "header" "--output" "${shader_header}"
        "--stage" "${shader_stage}" "--variable" "${shader_variable}"
        "--dxil" "${dxil}" "--msl" "${msl}" "--spirv" "${spirv}"
      DEPENDS "${py_gencshadersource}" "${dxil}" "${msl}" "${spirv}")
  endif()
  target_sources(miniwin PRIVATE "${shader_header}")
  list(APPEND shader_names "${shader_name}")
  list(APPEND shader_variables "${shader_variable}")
  list(APPEND shader_headers "${shader_header}")
  list(APPEND shader_stages "${shader_stage}")
  list(APPEND shader_jsons "${shader_json}")
endforeach()

set(index_cpp "${shader_gen_dir}/ShaderIndex.cpp")
set(index_h "${shader_gen_dir}/ShaderIndex.h")

if(ISLE_COMPILE_SHADERS)
  add_custom_command(OUTPUT "${index_h}" "${index_cpp}"
    COMMAND Python3::Interpreter "${py_gencshadersource}" "index"
      "--output" "${index_cpp}"
      "--header" "${index_h}"
      "--shader-names" ${shader_names}
      "--shader-variables" ${shader_variables}
      "--shader-headers" ${shader_headers}
      "--shader-stages" ${shader_stages}
      "--shader-jsons" ${shader_jsons}
    DEPENDS "${py_gencshadersource}" ${shader_headers} ${shader_jsons})
endif()
target_sources(miniwin PRIVATE "${index}" "${index_cpp}")
