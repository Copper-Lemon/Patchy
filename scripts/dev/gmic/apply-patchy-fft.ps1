# Routes CImg's FFT through the vendored pocketfft (src\filters\gmic\pocketfft, BSD-3-Clause)
# when patchy_gmic_use_pocketfft is defined, so transforms work at any size without FFTW (GPL).
# Run after apply-patchy-changes.ps1 on a freshly vendored CImg.h. Byte-transparent (Latin-1 in
# and out, LF line ends); every inserted block carries a PATCHY marker. See docs/gmic.md.
#
#   powershell -NoProfile -File scripts\dev\gmic\apply-patchy-fft.ps1 -Dir src\filters\gmic
param([Parameter(Mandatory)][string]$Dir)
$ErrorActionPreference = 'Stop'
$enc = [Text.Encoding]::GetEncoding(28591)
$path = Join-Path $Dir 'CImg.h'
$lines = [Collections.Generic.List[string]]([IO.File]::ReadAllText($path, $enc) -split "`n")
if ($lines -match 'patchy_gmic_use_pocketfft') { throw 'CImg.h already routes FFT through pocketfft' }

function IndexOf([string]$pattern, [int]$from = 0) {
  for ($i = $from; $i -lt $lines.Count; $i++) { if ($lines[$i] -match $pattern) { return $i } }
  throw "pattern not found: $pattern"
}

# The two FFT entry points: per axis, then N-dimensional.
$axis = IndexOf 'static void FFT\(CImg<T>& real, CImg<T>& imag, const char axis, const bool is_inverse=false,'
$whole = IndexOf 'static void FFT\(CImg<T>& real, CImg<T>& imag, const bool is_inverse=false,' ($axis + 1)
$axisIfdef = IndexOf '^#ifdef cimg_use_fftw3\s*$' $axis
$wholeIfdef = IndexOf '^#ifdef cimg_use_fftw3\s*$' $whole
if ($axisIfdef -ge $whole) { throw 'unexpected FFT layout' }

# Bottom-up so earlier indices stay valid.
$lines.RemoveAt($wholeIfdef)
$lines.InsertRange($wholeIfdef, [string[]]@(
  '#ifdef patchy_gmic_use_pocketfft',
  '      // PATCHY: any-size transform through pocketfft (docs/gmic.md).',
  '      _patchy_pocketfft(real,imag,7U,is_inverse);',
  '#elif defined(cimg_use_fftw3)'))
$lines.RemoveAt($axisIfdef)
$lines.InsertRange($axisIfdef, [string[]]@(
  '#ifdef patchy_gmic_use_pocketfft',
  '      // PATCHY: any-size transform through pocketfft (docs/gmic.md).',
  "      _patchy_pocketfft(real,imag,_axis=='x'?1U:_axis=='y'?2U:4U,is_inverse);",
  '#elif defined(cimg_use_fftw3)'))

# The helper goes ahead of the per-axis function's documentation comment.
$doc = $axis
while ($doc -gt 0 -and $lines[$doc] -notmatch '^\s*//! ') { $doc-- }
$lines.InsertRange($doc, [string[]]@(
  '#ifdef patchy_gmic_use_pocketfft',
  '    // PATCHY: complex FFT of every channel along the axes in axes_mask (1 = X, 2 = Y, 4 = Z)',
  '    // through pocketfft, with FFTW''s conventions as CImg uses them: the forward transform is',
  '    // not scaled, the inverse is divided by the length of each transformed axis.',
  '    static void _patchy_pocketfft(CImg<T>& real, CImg<T>& imag, const unsigned int axes_mask,',
  '                                  const bool is_inverse) {',
  '      const size_t w = real._width, h = real._height, d = real._depth, n = w*h*d;',
  '      const ptrdiff_t unit = (ptrdiff_t)sizeof(std::complex<double>);',
  '      const pocketfft::shape_t shape{ d,h,w };',
  '      const pocketfft::stride_t stride{ unit*(ptrdiff_t)(h*w),unit*(ptrdiff_t)w,unit };',
  '      pocketfft::shape_t axes;',
  '      double scale = 1;',
  '      if ((axes_mask&4U) && d>1) { axes.push_back(0); if (is_inverse) scale/=(double)d; }',
  '      if ((axes_mask&2U) && h>1) { axes.push_back(1); if (is_inverse) scale/=(double)h; }',
  '      if ((axes_mask&1U) && w>1) { axes.push_back(2); if (is_inverse) scale/=(double)w; }',
  '      if (axes.empty() || !n) return;',
  '      std::vector<std::complex<double> > buffer(n);',
  '      cimg_forC(real,c) {',
  '        T *const ptr_r = real.data(0,0,0,c), *const ptr_i = imag.data(0,0,0,c);',
  '        for (size_t k = 0; k<n; ++k) buffer[k] = std::complex<double>((double)ptr_r[k],(double)ptr_i[k]);',
  '        pocketfft::c2c(shape,stride,stride,axes,!is_inverse,buffer.data(),buffer.data(),scale,1);',
  '        for (size_t k = 0; k<n; ++k) { ptr_r[k] = (T)buffer[k].real(); ptr_i[k] = (T)buffer[k].imag(); }',
  '      }',
  '    }',
  '#endif // PATCHY: patchy_gmic_use_pocketfft',
  ''))

# The include, after the FFTW include block near the top of the file.
$fftwInclude = IndexOf '^#include "fftw3.h"\s*$'
$after = IndexOf '^#endif' $fftwInclude
$lines.InsertRange($after + 1, [string[]]@(
  '',
  '// PATCHY: pocketfft (BSD-3-Clause, vendored beside this file) computes the FFT at any size',
  '// when FFTW is not used. Its own thread pool stays off: G''MIC parallelises above it.',
  '#ifdef patchy_gmic_use_pocketfft',
  '#ifndef POCKETFFT_NO_MULTITHREADING',
  '#define POCKETFFT_NO_MULTITHREADING',
  '#endif',
  '#include <complex>',
  '#include <vector>',
  '#include "pocketfft/pocketfft_hdronly.h"',
  '#endif'))

[IO.File]::WriteAllText($path, ($lines -join "`n"), $enc)
'patched'
