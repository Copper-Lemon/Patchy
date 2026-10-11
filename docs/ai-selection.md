# Automatic selection: Select Subject and the Object Selection tool

Photoshop-style automatic selection that runs on the CPU only, so it works on every desktop Patchy supports without a GPU. The binding patent rules are the "Automatic selection" bullet in [legal-constraints.md](legal-constraints.md); the claim record is in [patent-research.md](patent-research.md). Read both before changing how prompts, masks or modes reach a network.

## What ships

| Piece | Where | Notes |
|---|---|---|
| Select > Subject (`select.subject`) | `src/ui/main_window_ai_selection.cpp` | BiRefNet_lite, one pass over the merged document at 1024x1024 (stretched), soft matte. Also the "Select Subject" options-bar button on Quick Select and Object Selection, which uses the tool's combine mode. |
| Object Selection tool (`tools.object_select`, `CanvasTool::ObjectSelect`) | `src/ui/canvas_widget_object_select.cpp` | Third member of the Magic Wand flyout (Shift+W cycles). Drag a box, or click; solves on release. Shift adds, Alt subtracts, no Intersect (Shift+Alt is Add, like Quick Select). Options: New/Add/Subtract, Feather, Anti-alias, Sample All Layers, Select Subject. Settings keys `tools/objectSelectFeather`, `tools/objectSelectAntiAlias`, `tools/objectSelectSampleAllLayers`. |
| Script API | `src/ui/script_ai_selection.cpp` | `doc.selection.selectSubject(opts)`, `doc.selection.objectSelect({rect} or {point}, ...)`, `doc.selection.automaticSelectionAvailable`. See patchy.d.ts. |
| Engine | `src/ai/` (`patchy_ai` library, Qt-free) | `ai_selection_math.*` (pre/post-processing, always built), `segmentation_engine.hpp` API, `segmentation_engine_onnx.cpp` (the only ONNX Runtime TU), `segmentation_engine_stub.cpp` (builds without it), `segmentation_engine_common.cpp`. |

Builds without the models or ONNX Runtime (every wasm build for now, or a dev tree that never ran the setup script) compile the stub: the menu item, the tool and the button are left out, and scripts get `automaticSelectionAvailable == false` and a clear error from the calls.

## Models and runtime

Patchy ships int8 conversions of the upstream models, pinned in `scripts/setup-ai-deps.ps1` / `.sh`, `cmake/patchy_ai.cmake` and the Flatpak manifest (keep all four in sync). The files are assets of the GitHub release **`ai-models-1`** (`https://github.com/SethRobinson/Patchy/releases/download/ai-models-1/<file>`), a release that must never be marked Latest (the README and `latest_version.json` download through `/releases/latest/`) and whose assets are never replaced: a new model set gets a new tag (`ai-models-2`) and new pins. Nothing is committed to git.

| File (in `ai-models/`) | Size | Converted from | License | SHA-256 |
|---|---|---|---|---|
| `mobile_sam_image_encoder_int8.onnx` | 14.4 MB | huggingface.co/Acly/MobileSAM `mobile_sam_image_encoder.onnx` (28.2 MB; preprocessing built in: HWC float 0..255, longest side 1024, the graph normalizes and pads) | MIT export of Apache-2.0 MobileSAM | `6af3f249...930cd7` |
| `sam_mask_decoder_multi_int8.onnx` | 4.8 MB | same repo, `sam_mask_decoder_multi.onnx` (16.5 MB; standard SAM decoder, 4 masks + IoU predictions) | Apache-2.0 (SAM decoder) | `aa1410be...c10a6` |
| `birefnet_lite_general_1024_int8.onnx` | 143.2 MB | github.com/ZhengPeng7/BiRefNet release v1 `BiRefNet-general-bb_swin_v1_tiny-epoch_232.onnx` (224 MB) | MIT | `0374fa92...27fed5e` |
| ONNX Runtime 1.30.0 CPU | | github.com/microsoft/onnxruntime releases (win-x64, win-arm64, osx-arm64, linux-x64, linux-aarch64, SHA-256 pinned per package) | MIT (ThirdPartyNotices.txt ships beside it) | per package in the scripts |

`scripts/ai/quantize_models.py` reproduces the three int8 files byte for byte from the pinned upstream fp32 files (it checks their SHA-256): dynamic int8, weights int8 per channel, MatMul/Gemm only. BiRefNet's export routes its MatMul weights through Identity nodes; the script removes those first, otherwise the quantizer keeps every float weight beside its int8 copy and the file grows to 253 MB. The script is dev tooling (Python, `onnxruntime==1.30.0`, `onnx==1.23.2`); builds only download.

Rejected for licensing: EdgeSAM (S-Lab, non-commercial), RMBG-1.4/2.0 (non-commercial), SAM 3 (restricted). The decoder graph still has SAM's `mask_input`/`has_mask_input` inputs; the engine always feeds zeros and 0 (a legal rule, not an optimization).

### Why int8, and which int8 (measured October 2026)

Compared with fp32 on real photos (a 3000x1820 photo, the Vermeer IFF fixture, the San Francisco readme photo), same prompts, IoU of the 50% masks against fp32, i9-12900KS:

| Variant | Size (deflate) | IoU vs fp32 | Speed |
|---|---|---|---|
| MobileSAM fp32 | 44.7 MB (36.7) | reference | encode 410 to 450 ms, decode 75 ms |
| **MobileSAM int8, MatMul/Gemm (shipped)** | **19.2 MB (12.4)** | **0.9935 to 0.9998**, mean alpha diff under 1/255 | encode about 10 to 15% faster, decode same |
| MobileSAM int8 including Conv | 16.2 MB | as low as 0.02 (clicks select the wrong region) | encode 3x slower |
| BiRefNet fp32 | 224 MB (165) | reference | 5.5 to 6.1 s |
| **BiRefNet int8, MatMul/Gemm (shipped)** | **143 MB (88)** | **0.997 to 0.9999** | about 5 to 7% faster |
| BiRefNet int8 including Conv | 93 MB (39) | 0.997 to 0.9997 | 2.3x slower (ConvInteger is slow on CPU) |
| BiRefNet static QDQ int8 (MinMax calibration) | 94 MB (41) | 0.94 on the Vermeer (bleeds into the background) | no faster, 8 s load |

Differences in the shipped variants are sub-pixel edge shifts. Not measured: Apple Silicon and pre-VNNI x86 speed (results stay correct; the speedup may differ). About 43 MB of BiRefNet is float Constant nodes (Swin position tables) that dynamic quantization does not touch.

`setup-ai-deps` provisions `<main checkout>/.deps/onnxruntime/<ver>/<platform>/` and `.deps/ai-models/`; worktrees find the main checkout through `git rev-parse --git-common-dir`, and CMake searches both `.deps` folders. Packagers pass `PATCHY_ONNXRUNTIME_DIR` and `PATCHY_AI_MODELS_DIR`. `PATCHY_ENABLE_AI_SELECTION` (default ON) and `PATCHY_REQUIRE_AI_SELECTION` (default OFF; release builds turn it on so a missing model is a configure error) control the build. `patchy_ai_runtime` is the copy-once target staging `ai-models/` and the runtime libraries into the build directory; `patchy_copy_ai_runtime(target)` attaches it.

ONNX Runtime is **loaded at run time, never linked** (`ORT_API_MANUAL_INIT`, `LoadLibraryExW` by full path beside the executable on Windows, since System32 can hold an unrelated older `onnxruntime.dll`; `dlopen` from beside the executable, `../Frameworks`, then by plain name elsewhere). A machine that cannot load it still runs Patchy and reports the feature unavailable. This matters on macOS: the 1.30 dylib requires macOS 14 while Patchy targets 12, so macOS 12 and 13 run without automatic selection.

At run time `configure_automatic_selection_models()` (main_window_ai_selection.cpp) points the engine at `ai-models` beside the executable, `../Resources/ai-models` (macOS bundle) or `../share/patchy/ai-models` (Linux install). Tests set the folder themselves.

## Engine behavior

- Preprocessing composites RGBA over white. SAM: area-averaging resize so the longest side is 1024 (aspect kept, top-left anchored). BiRefNet: stretch to 1024x1024, ImageNet mean/std, NCHW.
- Postprocessing upsamples logits bilinearly to document resolution once and crops to covered bounds (`document_mask_from_logits`). SAM masks map through a fixed narrow ramp around logit 0 (`127.5 + 64 * logit`) to anti-alias the edge; BiRefNet keeps the sigmoid matte.
- Point prompt: one positive point plus SAM's padding point (label -1), multimask output, the alternative with the highest predicted IoU. Box prompt: two corners (labels 2, 3), the single-mask output.
- Sessions: `ORT_ENABLE_ALL`, CPU arena disabled (activation memory returns after each run), ONNX Runtime's default thread pool (physical cores; measured fastest) unless `PATCHY_RENDER_THREADS` is set. Encoder and decoder stay loaded; the subject session stays loaded after a run and `MainWindow` frees it after 3 minutes idle (`release_subject_model`; the timer is stopped while a run is in flight).
- Cancellation: `InferenceCancel::request()` terminates the running ONNX Runtime call (`RunOptions::SetTerminate`); the call throws `InferenceCancelled`.
- Model outputs are CPU-dependent float math, so this module is exempt from the byte-determinism rule: tests compare masks by IoU, never bytes.

## UI behavior that is load-bearing

- Object Selection computes the image embedding on a tracked worker at the first press (no prompt, nothing displayed), keyed on (Sample All Layers, sampled layer id) plus a 64-bit signature of the sampled pixels. `notify_document_changed` and document swaps (undo/redo) only mark it unverified; the next press hashes the source and re-encodes only when the pixels differ, so a selection-only undo keeps it. The worker owns its promise, so dropping never blocks. A release waits for a pending encode under the processing overlay ("Analyzing image..."). A second click on an unchanged document reuses the embedding (`object_select_encodes_started()` counts encodes for tests).
- During a drag the canvas draws only the user's rectangle (black line plus white dashes). A release within 4 widget px of the press is a click. One undo entry per release ("Object Selection").
- `CanvasWidget::apply_automatic_selection_mask` is the shared combine (Feather and Anti-alias of the active tool apply).
- Select Subject runs on a worker under a window-modal, immediately shown progress dialog with Cancel; the dialog keeps the document from changing under the run.

## Measured performance (dev box i9-12900KS, 24 threads, Windows)

fp32 numbers come from an idle box; the int8 rows from an fp32/int8 run alternated under the same background load, so compare within a row pair, not across the table.

| Step | fp32 | int8 (shipped) |
|---|---|---|
| MobileSAM encode, 1024x768, session warm | about 200 ms idle (330 ms at 6 threads); 394 ms loaded | 320 ms loaded |
| MobileSAM session load | about 290 ms | about 290 ms |
| SAM decode per click or box (warm) | 30 to 100 ms; the first call loads the decoder (about 400 ms) | same |
| BiRefNet_lite run, warm | about 4.8 s idle (barely scales with threads); 5.9 s loaded | 5.6 s loaded |
| BiRefNet_lite session load | about 2 to 3 s idle | same as fp32 |

Published numbers for a 6-core Ryzen 5600X: MobileSAM encode 0.8 s, BiRefNet_lite 7 s (ONNX Runtime). About 60% of BiRefNet's time is Transpose/Sum/Mul/GatherND, the export's spelling of deformable convolution and Swin window shuffles.

## Tests

- Core (`patchy_core_tests ai_`): transform and preprocessing math, logit mappings, upsample/crop, IoU, then real runs on a self-authored synthetic scene (shaded disc on mottled noise): box and point IoU >= 0.9, Select Subject IoU >= 0.85, cancel before and during a run, model load from a Unicode folder. In a build without the engine the model tests print `[SKIP]`; in a build with it, missing models fail.
- UI (`PATCHY_UI_TEST_FILTER=object_select` and `select_subject`): no selection change before release, one undo entry, booleans, embedding reuse, the options bar and menu, and the script API.

## Platform packaging

- Windows: `onnxruntime.dll` and `onnxruntime_providers_shared.dll` beside `patchy.exe`, `ai-models\` beside them; the installer and zip pick up the build folder contents.
- macOS: the dylib goes in `Contents/Frameworks` and `ai-models` in `Contents/Resources`, both staged by the bundle step and signed with the app.
- Linux/Flatpak: an `onnxruntime` archive module installs into `/app`; models install to `share/patchy/ai-models`.

## Not done yet

- WebAssembly: link ONNX Runtime's `libonnxruntime_webassembly.a` built with Qt's emsdk (threads and SIMD), and fetch the models lazily instead of putting 160 MB into `patchy.data`.
- Faster Select Subject: a BiRefNet re-export using ONNX `DeformConv` (about 60% of its time is the export's decomposed deformable convolution and Swin window shuffles). It would ship the same way as the int8 files: converted once, hosted on a new `ai-models-N` release.
- EfficientViT-SAM-L0 (Apache-2.0, about 3x faster than MobileSAM on CPU in published benchmarks) once an ONNX export is verified.
