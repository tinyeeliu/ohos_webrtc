#!/usr/bin/env python3
"""Build the pinned Harmony fork on macOS; all downloads/products stay in out/."""
import argparse
import base64
import hashlib
import gzip
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
GN_REVISION = '85944ebc24a90ec1e489e85a46fdc68542c3146f'
GN_SHA256 = 'ed3e2c6164b5e10022956b74c31c9a56ac91fb87326ccd1f3b5d5a4e7c11d0b3'
THIRD_PARTY_REVISION = '8bc95681d66c67b16516023758213df89774a825'
BASE_URL = 'https://ohpm.openharmony.cn/ohpm/@ohos/webrtc/-/webrtc-1.0.5.har'
BASE_SHA256 = '15c8a65a1e0fa2b46f4a0e41b4822527cc511a986d5ea7cc297aedb760d3ceb5'


def run(*args, cwd=ROOT, capture=False):
    return subprocess.run([str(arg) for arg in args], cwd=cwd, check=True,
                          text=True, stdout=subprocess.PIPE if capture else None).stdout


def download(url, destination, expected):
    if destination.exists() and hashlib.sha256(destination.read_bytes()).hexdigest() == expected:
        return
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != expected:
        raise RuntimeError(f'Archive integrity mismatch: {url}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=Path(os.environ.get('HARMONY_NATIVE_SDK', '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')))
    parser.add_argument('--jobs', type=int, default=10)
    parser.add_argument('--package-only', action='store_true')
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    if not (sdk / 'llvm/bin/clang++').exists():
        raise RuntimeError('Select the native Harmony SDK with --sdk')
    tools = ROOT / 'out/academy-tools'
    gn_archive = tools / 'gn-mac-arm64.zip'
    gn = tools / 'gn-pinned/gn'
    if not gn.exists():
        download(f'https://chrome-infra-packages.appspot.com/dl/gn/gn/mac-arm64/+/git_revision:{GN_REVISION}', gn_archive, GN_SHA256)
        with zipfile.ZipFile(gn_archive) as archive:
            archive.extractall(gn.parent)
        gn.chmod(0o755)
    third_party = ROOT / 'third_party'
    if not third_party.exists():
        checkout = ROOT / 'out/academy-third-party'
        run('git', '-c', 'http.version=HTTP/1.1', 'clone', '--depth', '1',
            'https://gitcode.com/openharmony-tpc-incubate/webrtc_third_party.git', checkout)
        third_party.symlink_to(checkout / 'third_party', target_is_directory=True)
    third_party_root = third_party.resolve().parent
    if run('git', 'rev-parse', 'HEAD', cwd=third_party_root, capture=True).strip() != THIRD_PARTY_REVISION:
        raise RuntimeError('The pinned third-party source revision is required')
    # The old zlib distinguishes classic Mac OS from UNIX. Modern Apple SDKs
    # define TARGET_OS_MAC in ordinary POSIX compilation too.
    zutil = third_party / 'zlib/zutil.h'
    old = '#if defined(MACOS) || defined(TARGET_OS_MAC)'
    new = '#if (defined(MACOS) || defined(TARGET_OS_MAC)) && !defined(__APPLE__)'
    text = zutil.read_text()
    if old in text:
        zutil.write_text(text.replace(old, new))
    elif new not in text:
        raise RuntimeError('Unexpected zlib source; review the macOS host compatibility patch')
    compiler = run(sdk / 'llvm/bin/clang++', '--version', capture=True)
    sdk_identity = (sdk / 'oh-uni-package.json').read_text()
    sdk_key = hashlib.sha256((str(sdk) + sdk_identity + compiler).encode()).hexdigest()
    output = ROOT / 'out/academy-arm64'
    metadata = output / 'sdk-identity.json'
    if metadata.exists() and json.loads(metadata.read_text())['key'] != sdk_key:
        output = ROOT / f'out/academy-arm64-{sdk_key[:12]}'
        metadata = output / 'sdk-identity.json'
    if not args.package_only:
        xcode = run('xcrun', '--find', 'clang', capture=True).strip()
        clang_root = str(Path(xcode).parents[1])
        gn_args = f'target_os="ohos" target_cpu="arm64" ohos_sdk_native_root="{sdk}" ohos_mac_host_clang_path="{clang_root}" is_debug=false is_component_build=false rtc_include_tests=false rtc_build_examples=false use_custom_libcxx=false clang_use_chrome_plugins=false use_sysroot=false treat_warnings_as_errors=false use_lld=false'
        run(gn, 'gen', output, '--args=' + gn_args)
        metadata.write_text(json.dumps({'key': sdk_key, 'sdk': sdk_identity, 'compiler': compiler}, indent=2) + '\n')
        run(sdk / 'build-tools/cmake/bin/ninja', '-C', output, '-j', args.jobs, 'sdk/ohos:libohos_webrtc')
    library = output / 'libohos_webrtc.so'
    if not library.exists():
        raise RuntimeError('Build did not produce libohos_webrtc.so')
    base = tools / 'upstream-1.0.5.har'
    download(BASE_URL, base, BASE_SHA256)
    stage = ROOT / 'out/academy-package'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    with tarfile.open(base) as archive:
        archive.extractall(stage, filter='data')
    package = stage / 'package'
    shutil.copy2(library, package / 'libs/arm64-v8a/libohos_webrtc.so')
    shutil.copy2(ROOT / 'sdk/ohos/api/libohos_webrtc/webrtc.d.ts', package / 'src/main/libohos_webrtc/webrtc.d.ts')
    info = json.loads((package / 'oh-package.json5').read_text())
    info.update(version='1.0.5-sm.1', repository='https://github.com/tinyeeliu/ohos_webrtc', homepage='https://github.com/tinyeeliu/ohos_webrtc')
    (package / 'oh-package.json5').write_text(json.dumps(info, indent=2) + '\n')
    artifact = ROOT / 'out/academy-webrtc-1.0.5-sm.1.har'
    def normalized(member):
        member.mtime = 0
        member.uid = member.gid = 0
        member.uname = member.gname = ''
        return member
    with artifact.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w|') as archive:
            archive.add(package, arcname='package', filter=normalized)
    data = artifact.read_bytes()
    manifest = {'name': '@ohos/webrtc', 'version': info['version'], 'repository': info['repository'],
                'revision': run('git', 'rev-parse', 'HEAD', capture=True).strip(),
                'dirty': bool(run('git', 'status', '--porcelain', capture=True).strip()),
                'sdkIdentity': sdk_key, 'compiler': compiler.strip(), 'gnRevision': GN_REVISION,
                'thirdPartyRevision': THIRD_PARTY_REVISION, 'sha256': hashlib.sha256(data).hexdigest(),
                'integrity': 'sha512-' + base64.b64encode(hashlib.sha512(data).digest()).decode(),
                'abi': 'arm64-v8a', 'minimumApi': 21, 'extensions': ['native-playout-v1', 'external-rgba-v1']}
    artifact.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'artifact': str(artifact), **manifest}, indent=2))


if __name__ == '__main__':
    main()
