// Force-included into the vendored G'MIC on macOS when it runs with OpenMP
// (cmake/patchy_gmic.cmake; docs/gmic.md, "Threads").
//
// gmic.h leaves cimg_abort_test undefined on Apple platforms (its thread id there is the
// process id, so a run cannot find its own abort flag), and CImg.h then also drops the
// try/catch it wraps around the bodies of its OpenMP loops. An error raised inside a
// parallel fill (a formula's run() of a refused command, any math parser error) then
// leaves the parallel region and std::terminate ends the process. These are CImg.h's own
// definitions of those wrappers. Without cimg_abort_test nothing throws
// CImgAbortException, so only the fill catch acts: the error becomes the run's error, as
// on Windows and Linux.
#pragma once

#define _cimg_abort_init_openmp \
  unsigned int _cimg_abort_go_openmp = 1; \
  cimg_library::cimg::unused(_cimg_abort_go_openmp)
#define _cimg_abort_try_openmp if (_cimg_abort_go_openmp) try
#define _cimg_abort_catch_openmp catch (CImgAbortException&) { \
  cimg_pragma_openmp(atomic) _cimg_abort_go_openmp&=0; \
}
#define _cimg_abort_catch_fill_openmp \
  catch (CImgException& e) { cimg_pragma(omp critical(abort)) CImg<charT>::string(e._message).move_to(is_error_expr); \
                             cimg_pragma_openmp(atomic) _cimg_abort_go_openmp&=0; }
