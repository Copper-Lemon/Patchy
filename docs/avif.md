# AVIF (.avif, .avifs)

Read before changing `avif_document_io`, the vendored libavif or dav1d, or their filter and
registry rows. Registry and filter wiring rules live in [file-formats.md](file-formats.md);
the licensing rule in [legal-constraints.md](legal-constraints.md) and the research record
in [patent-research-formats.md](patent-research-formats.md). Added October 16, 2026 for
GitHub issue 120: until then AVIF opened only through an OS decoder (the Windows AV1
extension, macOS ImageIO) and never in the AppImage.

## Decode only, everywhere native

- Windows, macOS, Linux Flatpak and AppImage decode in-process through libavif 1.4.2 over
  dav1d 1.5.4. Seth accepted the AV1 licensing position on October 16, 2026 (AOMedia's
  royalty-free patent grant versus the Sisvel pool; GIMP, Firefox, Chrome and ImageMagick
  ship AV1 decoders). dav1d has no encoder, so Patchy writes no AVIF: the registry row has
  no writer, the filter row is read-only, and `write_flat_image_file` rejects the extension
  (the Flatpak runtime's kimg_avif plug-in could otherwise AV1-encode a hand-typed
  `.avif`). Adding an encoder (libaom, SVT-AV1, rav1e) needs a fresh licence read.
- The browser build (`EMSCRIPTEN`) compiles neither library: `avif::is_available()` is false,
  the filter row is hidden, the registry reader throws its browser message. Qt's wasm build
  has no AVIF plug-in either, so AVIF stays unopenable there.

## Where it lives

- `cmake/patchy_avif.cmake` builds `patchy_dav1d` (the common sources plus the thirteen
  `*_tmpl.c` bit-depth templates compiled twice as `patchy_dav1d_8bpc` / `_16bpc` object
  libraries, `BITDEPTH` 8 and 16, exactly meson's split) and `patchy_avif` (libavif's
  `src/*.c` with `codec_dav1d.c` and `AVIF_CODEC_DAV1D=1`, plus the five-file libyuv
  scaling subset upstream bundles for builds without a system libyuv; it is pure C). No
  assembly anywhere (`HAVE_ASM 0`): no nasm on any host, the same bytes on every toolchain,
  fast enough for stills. `config.h` and `vcs_version.h`, which meson generates, are written
  by the CMake function from fixed facts per OS (Windows: the win32 pthread shim and
  dav1d's MSVC `stdatomic.h` compat header, prepended to the include path because MSVC's own
  header needs an experimental switch in C; macOS: no `memalign`, no affinity calls; Linux:
  `_GNU_SOURCE`, `memalign`, affinity). Warnings are off for both archives like every other
  vendored codec. Upstream sources are unmodified (`.gitattributes` keeps them byte-exact);
  `src/formats/dav1d` holds `COPYING`, `include/` and `src/` without the `arm`, `x86`,
  `ppc`, `riscv`, `loongarch` and `ext` assembly directories; `src/formats/libavif` holds
  `LICENSE`, `include/avif`, the needed `src/*.c` and `third_party/libyuv` with its LICENSE.
- `src/formats/avif_document_io.{hpp,cpp}`: the Qt-free reader. `sniff` accepts an ftyp
  whose major or compatible brand is `avif` or `avis`; `heif::sniff` still rejects those
  brands and this sniff rejects the HEVC brands, so the two readers never claim each other's
  files (`avif_decodes_synthetic_still_and_rejects_damage`).
- Registry row `patchy.formats.avif` (reader, no writer, sniff); filter row "AVIF Image"
  (open only, gated on `is_available()`); `kDensitylessFormats` entry; the `.avif`/`.avifs`
  branch in `load_document_from_path` runs before the registry row so sequence frames get
  translated "Frame N" names and `pixels_in_source_space` / `orientation_applied` reach
  File Info; the extensions sit in the Windows "Open with" list.
- `image_open_diagnosis` no longer has an `AvifNeedsSystemDecoder` reason: a damaged `.avif`
  keeps the decoder's own message, and AVIF bytes under another extension get the rename
  hint.

## What the reader does

- `avifDecoder` with `ignoreExif`/`ignoreXMP` (File Info reads the items from the file
  through `extract_bmff`), `AVIF_STRICT_DISABLED` (older encoders left out `pixi` or wrote
  odd `clap` boxes; viewers open those), `imageSizeLimit` 268M pixels, threads from the
  shared worker budget (1 to 16).
- The primary image converts through `avifImageYUVToRGB` into `AVIF_RGB_FORMAT_RGBA`,
  straight alpha, at depth 8, or 16 when the file stores 10 or 12-bit samples and deep
  editing is on (libavif rescales to 16). Gray (4:0:0) and every chroma layout come out as
  RGBA; the document is always RGBA.
- Transforms are applied in MIAF order on the RGBA pixels: `clap` (through
  `avifCropRectFromCleanApertureBox`; an invalid box is ignored with a notice), `irot`
  (anti-clockwise quarter turns), `imir` (axis 0 top/bottom, 1 left/right). `orientation_applied`
  is always true: the AVIF rules make these properties replace any EXIF orientation, so
  File Info writes the tag back as normal.
- Color: an ICC profile lands in `color_state().embedded_icc_profile` with
  `pixels_in_source_space` true (libavif converts only the YUV matrix, not primaries or
  transfer). Without a profile the nclx values are not converted; PQ or HLG files get a
  notice that no tone mapping happened. Gain maps are ignored.
- Image sequences (`imageCount > 1`): one full-canvas layer per frame, frame 1 on top, named
  through `ReadOptions::frame_name` (the UI's translated "Frame N <delay>s"), delays from
  `imageTiming.duration`, `repetitionCount` in `avif.loop_count` (0 = forever, otherwise
  plays). A frame that changes size or fails to decode ends the import with a notice; the
  budget is 1G decoded pixels. Progressive layers are not exposed (`allowProgressive` off),
  so a progressive still opens as one image.
- Tests (tests/core/flat_formats_misc_tests.cpp): the 511-byte lossless quadrant file
  embedded as bytes (`kQuadrantsAvif`, written by Pillow 12.1), the committed
  `test-fixtures/avif/frames.avif` sequence (three frames, 100/200/300 ms), and the
  twelve cgohlke/imagecodecs-samples files in `local-test-fixtures` (gray/RGB, alpha, 8 and
  16-bit, sequences; [SKIP] when absent). Decoded colors are checked to a tolerance of 4,
  never byte-pinned: lossless AVIF still goes through a YUV matrix.
