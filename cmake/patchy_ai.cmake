# Automatic selection (docs/ai-selection.md): the patchy_ai library, ONNX Runtime, and the
# bundled models. scripts/setup-ai-deps.ps1 / .sh provision both into .deps; a git worktree
# uses its main checkout's .deps. Flatpak and other packagers pass PATCHY_ONNXRUNTIME_DIR
# and PATCHY_AI_MODELS_DIR explicitly. Without them the stub engine builds and the feature
# reports itself unavailable; PATCHY_REQUIRE_AI_SELECTION turns that into a configure error
# (release builds set it).

option(PATCHY_ENABLE_AI_SELECTION "Build automatic selection (ONNX Runtime and bundled models)" ON)
option(PATCHY_REQUIRE_AI_SELECTION "Fail the configure when automatic selection cannot be built" OFF)

# Keep in sync with scripts/setup-ai-deps.ps1 and scripts/setup-ai-deps.sh.
set(PATCHY_ONNXRUNTIME_VERSION "1.30.0")
set(PATCHY_AI_MODEL_FILES
  mobile_sam_image_encoder.onnx
  sam_mask_decoder_multi.onnx
  birefnet_lite_general_1024.onnx
)

set(PATCHY_AI_DEPS_CANDIDATES "${PROJECT_SOURCE_DIR}/.deps")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --path-format=absolute --git-common-dir
    OUTPUT_VARIABLE PATCHY_GIT_COMMON_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE PATCHY_GIT_COMMON_DIR_RESULT)
  if(PATCHY_GIT_COMMON_DIR_RESULT EQUAL 0 AND PATCHY_GIT_COMMON_DIR)
    get_filename_component(PATCHY_MAIN_CHECKOUT "${PATCHY_GIT_COMMON_DIR}" DIRECTORY)
    list(APPEND PATCHY_AI_DEPS_CANDIDATES "${PATCHY_MAIN_CHECKOUT}/.deps")
  endif()
endif()

if(WIN32)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
    set(PATCHY_ONNXRUNTIME_PLATFORM "win-arm64")
  else()
    set(PATCHY_ONNXRUNTIME_PLATFORM "win-x64")
  endif()
elseif(APPLE)
  set(PATCHY_ONNXRUNTIME_PLATFORM "osx-arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(PATCHY_ONNXRUNTIME_PLATFORM "linux-aarch64")
else()
  set(PATCHY_ONNXRUNTIME_PLATFORM "linux-x64")
endif()

if(NOT PATCHY_ONNXRUNTIME_DIR)
  foreach(candidate IN LISTS PATCHY_AI_DEPS_CANDIDATES)
    set(dir "${candidate}/onnxruntime/${PATCHY_ONNXRUNTIME_VERSION}/${PATCHY_ONNXRUNTIME_PLATFORM}")
    if(EXISTS "${dir}/include/onnxruntime_cxx_api.h")
      set(PATCHY_ONNXRUNTIME_DIR "${dir}")
      break()
    endif()
  endforeach()
endif()
if(NOT PATCHY_AI_MODELS_DIR)
  foreach(candidate IN LISTS PATCHY_AI_DEPS_CANDIDATES)
    if(EXISTS "${candidate}/ai-models/birefnet_lite_general_1024.onnx")
      set(PATCHY_AI_MODELS_DIR "${candidate}/ai-models")
      break()
    endif()
  endforeach()
endif()

set(PATCHY_AI_SELECTION_BUILT OFF)
set(PATCHY_AI_MISSING "")
if(EMSCRIPTEN)
  set(PATCHY_AI_MISSING "not supported on WebAssembly yet")
elseif(NOT PATCHY_ENABLE_AI_SELECTION)
  set(PATCHY_AI_MISSING "PATCHY_ENABLE_AI_SELECTION is OFF")
elseif(NOT PATCHY_ONNXRUNTIME_DIR)
  set(PATCHY_AI_MISSING "ONNX Runtime ${PATCHY_ONNXRUNTIME_VERSION} not found (run scripts/setup-ai-deps)")
elseif(NOT PATCHY_AI_MODELS_DIR)
  set(PATCHY_AI_MISSING "models not found (run scripts/setup-ai-deps)")
else()
  foreach(model IN LISTS PATCHY_AI_MODEL_FILES)
    if(NOT EXISTS "${PATCHY_AI_MODELS_DIR}/${model}")
      set(PATCHY_AI_MISSING "model ${model} missing from ${PATCHY_AI_MODELS_DIR}")
    endif()
  endforeach()
endif()

if(PATCHY_AI_MISSING STREQUAL "")
  if(WIN32)
    set(PATCHY_ONNXRUNTIME_RUNTIME_FILES
      "${PATCHY_ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
      "${PATCHY_ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll")
    set(PATCHY_ONNXRUNTIME_LINK_FILE "${PATCHY_ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
  elseif(APPLE)
    set(PATCHY_ONNXRUNTIME_RUNTIME_FILES
      "${PATCHY_ONNXRUNTIME_DIR}/lib/libonnxruntime.${PATCHY_ONNXRUNTIME_VERSION}.dylib")
    set(PATCHY_ONNXRUNTIME_LINK_FILE "${PATCHY_ONNXRUNTIME_DIR}/lib/libonnxruntime.${PATCHY_ONNXRUNTIME_VERSION}.dylib")
  else()
    set(PATCHY_ONNXRUNTIME_RUNTIME_FILES
      "${PATCHY_ONNXRUNTIME_DIR}/lib/libonnxruntime.so.1"
      "${PATCHY_ONNXRUNTIME_DIR}/lib/libonnxruntime.so.${PATCHY_ONNXRUNTIME_VERSION}")
    set(PATCHY_ONNXRUNTIME_LINK_FILE "${PATCHY_ONNXRUNTIME_DIR}/lib/libonnxruntime.so.${PATCHY_ONNXRUNTIME_VERSION}")
  endif()
  foreach(runtime_file IN LISTS PATCHY_ONNXRUNTIME_RUNTIME_FILES PATCHY_ONNXRUNTIME_LINK_FILE)
    if(NOT EXISTS "${runtime_file}")
      set(PATCHY_AI_MISSING "ONNX Runtime file ${runtime_file} missing")
    endif()
  endforeach()
endif()

add_library(patchy_ai
  src/ai/ai_selection_math.cpp
  src/ai/segmentation_engine_common.cpp
)
target_link_libraries(patchy_ai PUBLIC patchy_core)
patchy_configure_target(patchy_ai)

if(PATCHY_AI_MISSING STREQUAL "")
  set(PATCHY_AI_SELECTION_BUILT ON)
  # ONNX Runtime is loaded at run time, never linked (segmentation_engine_onnx.cpp): a
  # machine that cannot load it still runs Patchy. Only its headers are needed here;
  # SYSTEM keeps strict warnings on Patchy's code alone.
  if(WIN32)
    set(PATCHY_ONNXRUNTIME_LIBRARY_NAME "onnxruntime.dll")
  elseif(APPLE)
    set(PATCHY_ONNXRUNTIME_LIBRARY_NAME "libonnxruntime.${PATCHY_ONNXRUNTIME_VERSION}.dylib")
  else()
    set(PATCHY_ONNXRUNTIME_LIBRARY_NAME "libonnxruntime.so.1")
  endif()
  target_sources(patchy_ai PRIVATE src/ai/segmentation_engine_onnx.cpp)
  target_include_directories(patchy_ai SYSTEM PRIVATE "${PATCHY_ONNXRUNTIME_DIR}/include")
  set_source_files_properties(src/ai/segmentation_engine_onnx.cpp PROPERTIES
    COMPILE_DEFINITIONS "PATCHY_ONNXRUNTIME_LIBRARY=\"${PATCHY_ONNXRUNTIME_LIBRARY_NAME}\"")
  target_link_libraries(patchy_ai PRIVATE ${CMAKE_DL_LIBS})
  target_compile_definitions(patchy_ai PUBLIC PATCHY_HAVE_AI_SELECTION=1)
  message(STATUS "Automatic selection: ONNX Runtime at ${PATCHY_ONNXRUNTIME_DIR}, models at ${PATCHY_AI_MODELS_DIR}")
else()
  target_sources(patchy_ai PRIVATE src/ai/segmentation_engine_stub.cpp)
  if(PATCHY_REQUIRE_AI_SELECTION)
    message(FATAL_ERROR "Automatic selection is required but cannot be built: ${PATCHY_AI_MISSING}")
  endif()
  message(STATUS "Automatic selection disabled: ${PATCHY_AI_MISSING}")
endif()

# Runtime staging: the ONNX Runtime shared libraries beside the executables and the models
# in <build>/ai-models. One copy-once target shared by every executable (never per-target
# POST_BUILD copies into the shared output directory). Clean first so a renamed model never
# lingers in a long-lived build tree.
if(PATCHY_AI_SELECTION_BUILT)
  set(PATCHY_AI_MODEL_SOURCES "")
  foreach(model IN LISTS PATCHY_AI_MODEL_FILES)
    list(APPEND PATCHY_AI_MODEL_SOURCES "${PATCHY_AI_MODELS_DIR}/${model}")
  endforeach()
  # License texts travel with the models: the tracked model licenses (Apache-2.0 requires
  # its text to ship) and ONNX Runtime's own license and third-party notices.
  set(PATCHY_AI_LICENSE_FILE "${PROJECT_SOURCE_DIR}/third_party/ai-models/MODEL-LICENSES.txt")
  set(PATCHY_AI_RUNTIME_STAMP "${CMAKE_BINARY_DIR}/CMakeFiles/patchy_ai_runtime.stamp")
  add_custom_command(
    OUTPUT "${PATCHY_AI_RUNTIME_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/ai-models"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/ai-models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PATCHY_AI_MODEL_SOURCES} "${CMAKE_BINARY_DIR}/ai-models/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PATCHY_AI_LICENSE_FILE}" "${CMAKE_BINARY_DIR}/ai-models/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PATCHY_ONNXRUNTIME_DIR}/LICENSE"
            "${CMAKE_BINARY_DIR}/ai-models/onnxruntime-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PATCHY_ONNXRUNTIME_DIR}/ThirdPartyNotices.txt"
            "${CMAKE_BINARY_DIR}/ai-models/onnxruntime-ThirdPartyNotices.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PATCHY_ONNXRUNTIME_RUNTIME_FILES} "${CMAKE_BINARY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E touch "${PATCHY_AI_RUNTIME_STAMP}"
    DEPENDS ${PATCHY_AI_MODEL_SOURCES} ${PATCHY_ONNXRUNTIME_RUNTIME_FILES} "${PATCHY_AI_LICENSE_FILE}"
    COMMENT "Copying ONNX Runtime and the automatic selection models into the build directory"
  )
  add_custom_target(patchy_ai_runtime DEPENDS "${PATCHY_AI_RUNTIME_STAMP}")
endif()
function(patchy_copy_ai_runtime target)
  if(TARGET patchy_ai_runtime)
    # The engine loads the library from beside the executable (build trees, the Windows
    # package), from ../Frameworks (the macOS bundle), or by plain name (the Flatpak's
    # /app/lib), so no rpath is involved.
    add_dependencies(${target} patchy_ai_runtime)
  endif()
endfunction()
