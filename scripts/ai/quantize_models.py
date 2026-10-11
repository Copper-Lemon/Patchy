"""Reproduces the int8 automatic-selection models Patchy ships (docs/ai-selection.md).

Dev tooling only: builds never run this. It converts the pinned upstream fp32 models to
dynamic int8 (weights int8 per channel, activations quantized at run time) on MatMul/Gemm
only. Quantizing convolutions too was measured and rejected: MobileSAM's masks break and
BiRefNet runs 2.3x slower on CPU. The outputs are published as assets of the GitHub release
`ai-models-1` and pinned by SHA-256 in scripts/setup-ai-deps.*.

Requirements (match the pinned runtime):
    python -m venv venv
    venv/Scripts/python -m pip install onnxruntime==1.30.0 onnx==1.23.2 numpy
Usage:
    venv/Scripts/python -I scripts/ai/quantize_models.py <fp32 models folder> <output folder>
The fp32 folder is what scripts/setup-ai-deps.* fetched before October 2026 (or download the
upstream files listed below and check their SHA-256).
"""
import hashlib
import os
import sys

import onnx
from onnxruntime.quantization import QuantType, quantize_dynamic

# Upstream fp32 sources: (input name, upstream URL, SHA-256, output name).
MODELS = [
    ("mobile_sam_image_encoder.onnx",
     "https://huggingface.co/Acly/MobileSAM/resolve/main/mobile_sam_image_encoder.onnx",
     "580f5fb648ea1062c0aabc26217aed56921985f03f0cbbd852bba81d760cc749",
     "mobile_sam_image_encoder_int8.onnx"),
    ("sam_mask_decoder_multi.onnx",
     "https://huggingface.co/Acly/MobileSAM/resolve/main/sam_mask_decoder_multi.onnx",
     "8976b90a87ba50a6a72217a5ff994f7d25ce16f2229fcc1ed259e1294c622ffe",
     "sam_mask_decoder_multi_int8.onnx"),
    ("birefnet_lite_general_1024.onnx",
     "https://github.com/ZhengPeng7/BiRefNet/releases/download/v1/BiRefNet-general-bb_swin_v1_tiny-epoch_232.onnx",
     "5600024376f572a557870a5eb0afb1e5961636bef4e1e22132025467d0f03333",
     "birefnet_lite_general_1024_int8.onnx"),
]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def strip_initializer_identities(source, destination):
    """The BiRefNet export routes MatMul weights through Identity nodes; with them in place
    the quantizer keeps each float weight alive beside its int8 copy (the file grows)."""
    model = onnx.load(source)
    graph = model.graph
    initializers = {item.name for item in graph.initializer}
    outputs = {item.name for item in graph.output}
    rename = {}
    kept = []
    for node in graph.node:
        if node.op_type == "Identity" and node.input[0] in initializers and node.output[0] not in outputs:
            rename[node.output[0]] = node.input[0]
        else:
            kept.append(node)
    for node in kept:
        for index, name in enumerate(node.input):
            while name in rename:
                name = rename[name]
            node.input[index] = name
    del graph.node[:]
    graph.node.extend(kept)
    onnx.checker.check_model(model)
    onnx.save(model, destination)
    return len(rename)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source_dir, output_dir = sys.argv[1], sys.argv[2]
    os.makedirs(output_dir, exist_ok=True)
    for name, url, expected, output_name in MODELS:
        source = os.path.join(source_dir, name)
        actual = sha256(source)
        if actual != expected:
            sys.exit(f"{name}: SHA-256 {actual} does not match the pinned upstream {expected} ({url})")
        staged = os.path.join(output_dir, name + ".stripped.onnx")
        removed = strip_initializer_identities(source, staged)
        output = os.path.join(output_dir, output_name)
        quantize_dynamic(staged, output, weight_type=QuantType.QInt8, per_channel=True,
                         op_types_to_quantize=["MatMul", "Gemm"])
        os.remove(staged)
        print(f"{output_name}: {os.path.getsize(output) / 1e6:.1f} MB, removed {removed} Identity nodes, "
              f"sha256 {sha256(output)}")


main()
