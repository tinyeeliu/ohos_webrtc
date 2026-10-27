#!/usr/bin/env python3
"""Compile/run the production H.264 SEI codec without OHOS/WebRTC dependencies."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cxx', default=os.environ.get('CXX'), help='Host C++17 compiler path (also supports zig.exe)')
parser.add_argument('--sanitize', action='store_true', help='Enable ASan/UBSan on a supporting host toolchain')
parser.add_argument('--transformers', action='store_true', help='Also build frame-transformer tests (requires Abseil and GoogleTest sources)')
parser.add_argument('--abseil', type=Path, help='Abseil source root containing absl/')
parser.add_argument('--gtest', type=Path, help='GoogleTest source root containing include/ and src/')
args = parser.parse_args()
cxx = args.cxx or shutil.which('clang++') or shutil.which('g++')
if not cxx:
    parser.error('Pass --cxx /path/to/host/clang++ or g++; the OHOS cross compiler cannot run host tests')
root = Path(__file__).resolve().parents[3]
source = root / 'sdk/ohos/src/ohos_webrtc'
with tempfile.TemporaryDirectory(prefix='h264-sei-test-') as tmp:
    binary = Path(tmp) / ('sei_test.exe' if os.name == 'nt' else 'sei_test')
    command = [cxx]
    if Path(cxx).stem == 'zig':
        command.append('c++')
    command += ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread', '-I' + str(source),
                str(source / 'sei/h264_sei.cpp'), str(Path(__file__).with_name('h264_sei_test.cpp')),
                '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
    if args.transformers:
        abseil = args.abseil or root / 'third_party/abseil-cpp'
        gtest = args.gtest or root / 'testing/gtest'
        if not (abseil / 'absl/base/config.h').is_file() or not (gtest / 'src/gtest-all.cc').is_file():
            parser.error('--transformers requires --abseil and --gtest pointing to complete source directories')
        # WebRTC's test/gtest.h uses this repository-style include path.
        staging = Path(tmp) / 'includes/testing/gtest/include'
        shutil.copytree(gtest / 'include', staging)
        binary = Path(tmp) / ('transformer_test.exe' if os.name == 'nt' else 'transformer_test')
        command = [cxx]
        if Path(cxx).stem == 'zig':
            command.append('c++')
        command += ['-std=c++17', '-DNDEBUG', '-pthread', '-I' + str(Path(tmp) / 'includes'),
                    '-I' + str(root), '-I' + str(source), '-I' + str(abseil),
                    '-I' + str(gtest / 'include'), '-I' + str(gtest),
                    '-DWEBRTC_WIN' if os.name == 'nt' else '-DWEBRTC_POSIX',
                    str(source / 'sei/h264_sei.cpp'), str(source / 'sei/sei_transformer.cpp'),
                    str(root / 'api/video/video_frame_metadata.cc'),
                    str(Path(__file__).with_name('sei_transformer_unittest.cpp')),
                    str(gtest / 'src/gtest-all.cc'), str(gtest / 'src/gtest_main.cc'), '-o', str(binary)]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
