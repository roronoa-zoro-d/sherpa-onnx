#!/usr/bin/env python3
# Copyright (c) 2026  Xiaomi Corporation
#
# Non-interactive download of Sherpa-ONNX dependency archives into <repo>/depends/
# for offline builds. FetchContent extract dirs default to <build>/depend (see root CMakeLists.txt).
#
# Typical configure-time downloads (put these exact basenames under depends/):
#
#   Sherpa cmake/*.cmake (root CMakeLists include): kaldi-native-fbank, kaldi-decoder, onnxruntime,
#   simple-sentencepiece, portaudio (optional), json — each has possible_file_locations + prepend_depdir
#   + explicit ${CMAKE_SOURCE_DIR}/depends/<file> in those modules.
#
#   kissfft      — kaldi_native_fbank-src/cmake/kissfft.cmake
#                  → kissfft-febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip
#   kaldifst     — kaldi_decoder-src/cmake/kaldifst.cmake
#                  → kaldifst-1.8.0.tar.gz
#   openfst      — kaldifst-src/cmake/openfst.cmake → …-04-10.tar.gz ; cmake/openfst.cmake → …-04-11.tar.gz
#   eigen        — include(eigen) may load cmake/eigen.cmake (5.0.1) before kaldi_decoder’s eigen (3.4):
#                  put eigen-5.0.1.tar.gz and eigen-3.4.0.tar.gz in depends/
#   onnxruntime  — cmake/onnxruntime-<platform>.cmake → e.g. onnxruntime-osx-arm64-static_lib-1.24.4.zip

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import sys
import urllib.parse
import urllib.request
from pathlib import Path

# kaldi_native_fbank-src/cmake/kissfft.cmake
NESTED_KFBANK_KISSFFT = [
    (
        "kissfft",
        "https://github.com/mborgerding/kissfft/archive/febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip",
        "",
        "497103e664168ebe39580b757adbe616f6cf85a16572af581ca7bc42d0ab13fd",
    ),
]

# Eigen / OpenFst inside kaldi-decoder → kaldifst (not sherpa-onnx/cmake/eigen.cmake).
NESTED_KALDI_DECODER = [
    (
        "eigen_kaldi_34",
        "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz",
        "https://huggingface.co/csukuangfj/kaldi-hmm-gmm-cmake-deps/resolve/main/eigen-3.4.0.tar.gz",
        "8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72",
    ),
    (
        "openfst_kaldifst",
        "https://github.com/csukuangfj/openfst/archive/refs/tags/v1.8.5-2026-04-10.tar.gz",
        "",
        "c3549940384cbe4fa9f18c2bcfb1bfbd0a80492fd1b0bfa27433cee395a6a199",
    ),
]

# Shorthand: maps to cmake/onnxruntime-<name>.cmake for --ort
# Configure logs: “Downloading kaldifst from …” / “Downloading openfst from …” (04-11 is cmake/openfst.cmake).
EXPLICIT_KALDIFST_AND_OPENFST_SHERPA = [
    (
        "kaldifst",
        "https://github.com/k2-fsa/kaldifst/archive/refs/tags/v1.8.0.tar.gz",
        "",
        "3f247b7e5a2409071202f5e2bc6200060f66728c0a3443c03923ad2723e040b3",
    ),
    (
        "openfst_sherpa",
        "https://github.com/csukuangfj/openfst/archive/refs/tags/v1.8.5-2026-04-11.tar.gz",
        "",
        "57fbc4b950ae81b1a0e1e298af15652da968a6723a592b7874e9b4027a80a5b4",
    ),
]

ORT_PRESET_FILES = {
    "osx-arm64-static": "onnxruntime-osx-arm64-static.cmake",
    "osx-arm64": "onnxruntime-osx-arm64.cmake",
    "osx-universal2-static": "onnxruntime-osx-universal-static.cmake",
    "osx-universal2": "onnxruntime-osx-universal.cmake",
    "linux-x64": "onnxruntime-linux-x86_64.cmake",
    "linux-x64-static": "onnxruntime-linux-x86_64-static.cmake",
    "linux-aarch64": "onnxruntime-linux-aarch64.cmake",
    "linux-aarch64-static": "onnxruntime-linux-aarch64-static.cmake",
}


def _load_download_all_deps(repo_root: Path):
    path = repo_root / "cmake" / "download-all-deps.py"
    spec = importlib.util.spec_from_file_location("download_all_deps", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _filename_from_url(url: str, dep_name: str, mod, cmake_dir: Path) -> str:
    return mod.filename_from_url(url, dep_name, cmake_dir)


def _download_file(url: str, filepath: Path, expected_sha256: str, retries: int = 3) -> bool:
    if filepath.exists():
        got = hashlib.sha256(filepath.read_bytes()).hexdigest()
        if got == expected_sha256:
            print(f"  OK (cached): {filepath.name}")
            return True
        print(f"  Wrong hash, re-downloading: {filepath.name}")
        filepath.unlink()

    for attempt in range(1, retries + 1):
        try:
            if attempt > 1:
                print(f"  Retry {attempt}/{retries}: {url}")
            else:
                print(f"  Downloading: {url}")
            urllib.request.urlretrieve(url, filepath)
        except Exception as e:
            print(f"  ERROR: {e}")
            if filepath.exists():
                filepath.unlink()
            if attempt >= retries:
                return False
            continue

        got = hashlib.sha256(filepath.read_bytes()).hexdigest()
        if got != expected_sha256:
            print(f"  SHA256 mismatch for {filepath.name}")
            filepath.unlink()
            if attempt >= retries:
                return False
            continue
        print(f"  OK: {filepath.name}")
        return True
    return False


def _collect_deps(args: argparse.Namespace, repo: Path, mod, cmake_dir: Path):
    common = mod.get_hardcoded_deps() + mod.discover_common_deps(cmake_dir)

    if args.preset == "kws":
        names = {
            "kaldi_native_fbank",
            "kaldi_decoder",
            "simple-sentencepiece",
            "json",
            "kaldifst",
            "portaudio",
            "eigen",
            "openfst",
        }
        common = [r for r in common if r[0] in names]
        common.extend(NESTED_KALDI_DECODER)
        common.extend(NESTED_KFBANK_KISSFFT)
    elif args.preset == "full":
        common.extend(NESTED_KALDI_DECODER)
        common.extend(NESTED_KFBANK_KISSFFT)

    common.extend(EXPLICIT_KALDIFST_AND_OPENFST_SHERPA)

    seen = set()
    out = []
    for row in common:
        key = (row[1], row[3])
        if key in seen:
            continue
        seen.add(key)
        out.append(row)

    ort_file = args.ort
    if args.ort_preset:
        ort_file = ORT_PRESET_FILES.get(args.ort_preset)
        if not ort_file:
            raise SystemExit(f"Unknown --ort-preset {args.ort_preset}")
    if ort_file:
        ort_path = cmake_dir / ort_file
        if not ort_path.exists():
            raise SystemExit(f"Missing {ort_path}")
        ort_deps = mod.parse_url_and_hash(ort_path)
        out.extend(ort_deps)

    return out


def main() -> None:
    repo = Path(__file__).resolve().parent.parent
    cmake_dir = repo / "cmake"
    mod = _load_download_all_deps(repo)

    ap = argparse.ArgumentParser(
        description="Download dependency archives into depends/ for offline Sherpa-ONNX builds."
    )
    ap.add_argument(
        "--dest",
        type=Path,
        default=repo / "depends",
        help="Output directory (default: <repo>/depends)",
    )
    ap.add_argument(
        "--preset",
        choices=("kws", "full"),
        default="kws",
        help="kws: minimal set for keyword spotting; full: all discover_common_deps + nested kaldi tarballs",
    )
    ap.add_argument(
        "--ort",
        metavar="FILE",
        help="Fetch ONNX Runtime using cmake/<FILE>.cmake (e.g. onnxruntime-osx-arm64-static.cmake)",
    )
    ap.add_argument(
        "--ort-preset",
        choices=sorted(ORT_PRESET_FILES.keys()),
        metavar="NAME",
        help="Shorthand for --ort (e.g. osx-arm64-static → onnxruntime-osx-arm64-static.cmake)",
    )
    args = ap.parse_args()
    if args.ort and args.ort_preset:
        raise SystemExit("Use only one of --ort or --ort-preset")

    dest: Path = args.dest
    dest.mkdir(parents=True, exist_ok=True)

    rows = _collect_deps(args, repo, mod, cmake_dir)
    if not rows:
        print("No dependencies to download.")
        sys.exit(1)

    print(f"Destination: {dest}\nTotal: {len(rows)} artifact(s)\n")

    failed = []
    for name, url, url2, sha256 in rows:
        fname = _filename_from_url(url, name, mod, cmake_dir)
        # Match nested CMake “Downloads/…” basenames (avoid openfst_kaldifst-*.tar.gz).
        if name == "openfst_kaldifst":
            fname = "openfst-1.8.5-2026-04-10.tar.gz"
        elif name in ("openfst_sherpa", "openfst"):
            fname = "openfst-1.8.5-2026-04-11.tar.gz"
        elif name == "eigen_kaldi_34":
            fname = "eigen-3.4.0.tar.gz"
        elif name == "kissfft":
            fname = "kissfft-febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip"
        fp = dest / fname
        if not _download_file(url, fp, sha256):
            if url2 and _download_file(url2, fp, sha256):
                continue
            failed.append(fname)

    print()
    if failed:
        print(f"Failed ({len(failed)}): {failed}")
        sys.exit(1)
    print("All downloads OK. Copy this folder (depends/) for offline machines; run cmake from a build dir.")
    print(
        "Note: nested OpenFst may require running `cmake` twice fully offline — "
        "see cmake/kaldi-decoder.cmake (staging into <build>/depend/kaldifst-src)."
    )


if __name__ == "__main__":
    main()
