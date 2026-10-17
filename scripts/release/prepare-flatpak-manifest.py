"""Make a local packaging manifest whose stamp token invalidates the app cache."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path


def prepare(root: Path, output: Path, token: str):
    root = root.resolve()
    if not (root / "CMakeLists.txt").is_file():
        raise ValueError("The source root must contain CMakeLists.txt")
    text = (root / "packaging/linux/flatpak/com.rtsoft.patchy.yml").read_text(encoding="utf-8")
    path_line = "        path: ../../.."
    config_line = "      - -DCMAKE_BUILD_TYPE=Release"
    if text.count(path_line) != 1 or text.count(config_line) != 1:
        raise ValueError("The Flatpak source path or release configuration is ambiguous")
    text = text.replace(path_line, "        path: " + json.dumps(root.as_posix()))
    text = text.replace(config_line, config_line + "\n      - " + json.dumps(
        "-DPATCHY_BUILD_STAMP_REFRESH_TOKEN=" + token))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.root, args.output, datetime.now(timezone.utc).isoformat(timespec="microseconds"))
