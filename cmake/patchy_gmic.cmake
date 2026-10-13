# G'MIC filter engine (src/filters/gmic, docs/gmic.md, GitHub issue 87).
#
# patchy_gmic always exists: the catalog parser (no engine dependency) plus either
# the engine wrapper or a stub that reports the engine as unavailable. That keeps
# the G'MIC Filter Library's interface code and its translations in every build,
# with or without the interpreter.
#
# PATCHY_ENABLE_GMIC (ON by default; OFF for the web build, which has only been
# compile-checked) adds patchy_gmic_vendor, the vendored
# interpreter (gmic.cpp + CImg.h), isolated like patchy_libraw: its own language
# standard, warnings off. It is built with no display, network library, FFTW,
# image codecs, external programs (cimg_no_system_calls, gmic_noexec) or
# patch-based algorithms (patchy_gmic_no_patch_algorithms; see
# docs/legal-constraints.md). Its FFT is the vendored pocketfft
# (src/filters/gmic/pocketfft, BSD-3-Clause; patchy_gmic_use_pocketfft).
# A build directory configured before the default changed keeps its cached value:
# pass -UPATCHY_ENABLE_GMIC once to pick up the default.
if(EMSCRIPTEN)
  set(PATCHY_GMIC_DEFAULT OFF)
else()
  set(PATCHY_GMIC_DEFAULT ON)
endif()
option(PATCHY_ENABLE_GMIC "Build the vendored G'MIC filter engine" ${PATCHY_GMIC_DEFAULT})

add_library(patchy_gmic STATIC src/filters/gmic/gmic_catalog.cpp)

if(PATCHY_ENABLE_GMIC)
  add_library(patchy_gmic_vendor STATIC src/filters/gmic/gmic.cpp)
  # patchy_stub first: its gmic_stdlib_community.h replaces the generated one.
  # It also holds zlib.h, a shim onto the vendored miniz that patchy_psd compiles.
  target_include_directories(patchy_gmic_vendor PRIVATE
    ${PROJECT_SOURCE_DIR}/src/filters/gmic/patchy_stub ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(patchy_gmic_vendor PUBLIC patchy_psd)
  target_include_directories(patchy_gmic_vendor SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/src/filters/gmic)
  target_compile_definitions(patchy_gmic_vendor PRIVATE
    gmic_core cimg_use_abort cimg_use_zlib gmic_noexec cimg_no_system_calls patchy_gmic_no_patch_algorithms
    patchy_gmic_use_pocketfft)
  target_compile_definitions(patchy_gmic_vendor PUBLIC cimg_display=0)
  # G'MIC is developed against C++11 through C++17.
  set_target_properties(patchy_gmic_vendor PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_definitions(patchy_gmic_vendor PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(patchy_gmic_vendor PRIVATE /w /bigobj)
  else()
    target_compile_options(patchy_gmic_vendor PRIVATE -w)
  endif()

  # CImg's loops run in parallel with OpenMP (docs/gmic.md, "Threads"). Only this target is
  # compiled with it; the runtime it needs ships with each package.
  # Off on macOS: Apple's compiler ships no OpenMP runtime, and Homebrew's libomp is built
  # for the build host's macOS, newer than Patchy's deployment target.
  if(APPLE)
    set(OpenMP_CXX_FOUND FALSE)
  else()
    find_package(OpenMP COMPONENTS CXX)
  endif()
  option(PATCHY_GMIC_OPENMP "Run the G'MIC engine's loops in parallel with OpenMP" ${OpenMP_CXX_FOUND})
  if(PATCHY_GMIC_OPENMP)
    if(NOT OpenMP_CXX_FOUND)
      message(FATAL_ERROR "PATCHY_GMIC_OPENMP is ON but no OpenMP was found for the C++ compiler.")
    endif()
    target_compile_definitions(patchy_gmic_vendor PRIVATE cimg_use_openmp)
    target_link_libraries(patchy_gmic_vendor PUBLIC OpenMP::OpenMP_CXX)
  endif()

  target_sources(patchy_gmic PRIVATE src/filters/gmic/gmic_engine.cpp)
  target_link_libraries(patchy_gmic PRIVATE patchy_gmic_vendor)
  target_compile_definitions(patchy_gmic PUBLIC PATCHY_HAVE_GMIC=1)
  if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(patchy_gmic PUBLIC Threads::Threads)
  endif()
else()
  target_sources(patchy_gmic PRIVATE src/filters/gmic/gmic_engine_stub.cpp)
endif()
patchy_configure_target(patchy_gmic)
