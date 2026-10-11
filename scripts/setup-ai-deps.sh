#!/usr/bin/env bash
# One-time (idempotent) provisioning of the automatic-selection dependencies on macOS and
# Linux: the pinned ONNX Runtime CPU package and the pinned segmentation models, into
# .deps (gitignored). Every download is checked against the SHA-256 pinned below; a file
# that fails the check is deleted, never used. See docs/ai-selection.md.
#
#   bash scripts/setup-ai-deps.sh            # provisions <main checkout>/.deps
#   bash scripts/setup-ai-deps.sh /some/deps  # explicit deps folder
# Rerunning is a fast no-op once everything is present and verified.
set -euo pipefail

# Keep in sync with scripts/setup-ai-deps.ps1 and cmake/patchy_ai.cmake.
ORT_VERSION="1.30.0"
# The int8 models Patchy ships: converted from the pinned upstream fp32 files by
# scripts/ai/quantize_models.py (byte-reproducible) and hosted on the ai-models-1 release.
MODELS=(
  "mobile_sam_image_encoder_int8.onnx|https://github.com/SethRobinson/Patchy/releases/download/ai-models-1/mobile_sam_image_encoder_int8.onnx|6af3f249f7a5a6a8a3ef988b6814015e8c3a300977c135d1f9572ed2f2930cd7"
  "sam_mask_decoder_multi_int8.onnx|https://github.com/SethRobinson/Patchy/releases/download/ai-models-1/sam_mask_decoder_multi_int8.onnx|aa1410bec8f050b6017fba29ca07f16317029a8f74a98e5998aeed46680c10a6"
  "birefnet_lite_general_1024_int8.onnx|https://github.com/SethRobinson/Patchy/releases/download/ai-models-1/birefnet_lite_general_1024_int8.onnx|0374fa9286b28506399d1ef44c5bd975e61deb2c0d5c6655b18f0815e27fed5e"
)

case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) ORT_PLATFORM="osx-arm64"; ORT_SHA256="6ebb5062a934537c352937821f9fe9718e7de1a2db1122a93dd363ffd53a7012" ;;
  Linux-x86_64) ORT_PLATFORM="linux-x64"; ORT_SHA256="a5ed5a3cac51fbb2e90da632ae43d19212faaa20e76484e62bcb7c23ddb3b3fd" ;;
  Linux-aarch64) ORT_PLATFORM="linux-aarch64"; ORT_SHA256="e16a27a8ed330bbc698df7330b0cf56e722f354e3bcc92118682c74ef3c3e3da" ;;
  *) echo "setup-ai-deps: unsupported platform $(uname -s)-$(uname -m)" >&2; exit 1 ;;
esac

if [[ $# -ge 1 ]]; then
  DEPS_DIR="$1"
else
  COMMON_DIR="$(git -C "$(dirname "$0")" rev-parse --path-format=absolute --git-common-dir)"
  MAIN_CHECKOUT="$(dirname "$COMMON_DIR")"
  [[ -f "$MAIN_CHECKOUT/CMakeLists.txt" ]] || { echo "main checkout not found at $MAIN_CHECKOUT; pass a deps folder" >&2; exit 1; }
  DEPS_DIR="$MAIN_CHECKOUT/.deps"
fi
mkdir -p "$DEPS_DIR"

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

# Downloads to a .part sibling, verifies, then renames, so an interrupted run never leaves a
# file a later run would mistake for a verified one.
fetch_verified() {
  local url="$1" destination="$2" expected="$3"
  if [[ -f "$destination" ]]; then
    [[ "$(sha256_of "$destination")" == "$expected" ]] && return 0
    echo "checksum mismatch, downloading again: $destination"
    rm -f "$destination"
  fi
  echo "downloading $url"
  curl -fL --retry 3 -o "$destination.part" "$url"
  local actual
  actual="$(sha256_of "$destination.part")"
  if [[ "$actual" != "$expected" ]]; then
    rm -f "$destination.part"
    echo "SHA-256 mismatch for $url (expected $expected, got $actual)" >&2
    exit 1
  fi
  mv "$destination.part" "$destination"
}

echo "== ONNX Runtime $ORT_VERSION ($ORT_PLATFORM) =="
ORT_ROOT="$DEPS_DIR/onnxruntime/$ORT_VERSION"
ORT_DIR="$ORT_ROOT/$ORT_PLATFORM"
if [[ ! -f "$ORT_DIR/include/onnxruntime_cxx_api.h" ]]; then
  mkdir -p "$ORT_ROOT"
  ARCHIVE="$ORT_ROOT/onnxruntime-$ORT_PLATFORM-$ORT_VERSION.tgz"
  fetch_verified "https://github.com/microsoft/onnxruntime/releases/download/v$ORT_VERSION/onnxruntime-$ORT_PLATFORM-$ORT_VERSION.tgz" \
    "$ARCHIVE" "$ORT_SHA256"
  rm -rf "${DEPS_DIR:?}/onnxruntime/$ORT_VERSION/$ORT_PLATFORM"
  tar -xzf "$ARCHIVE" -C "$ORT_ROOT"
  mv "$ORT_ROOT/onnxruntime-$ORT_PLATFORM-$ORT_VERSION" "$ORT_DIR"
  rm -f "$ARCHIVE"
fi
echo "ONNX Runtime ready at $ORT_DIR"

echo "== Segmentation models =="
MODEL_DIR="$DEPS_DIR/ai-models"
mkdir -p "$MODEL_DIR"
for entry in "${MODELS[@]}"; do
  IFS='|' read -r name url sha <<<"$entry"
  fetch_verified "$url" "$MODEL_DIR/$name" "$sha"
done
echo "models ready at $MODEL_DIR"
echo "== automatic-selection dependencies ready =="
