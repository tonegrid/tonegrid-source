"""Package only verified runtime inputs; never archive the build folder wholesale."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile
from content_paths import validate_inventory

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_DLLS = (
    'concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
    'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll',
    'vcruntime140.dll', 'vcruntime140_1.dll',
)
AUDIO = {'.nam', '.wav', '.mp3', '.flac', '.aif', '.aiff', '.ogg'}


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def verify_x64(path):
    with path.open('rb') as stream:
        if stream.read(2) != b'MZ':
            raise RuntimeError('Expected a Windows executable: ' + str(path))
        stream.seek(0x3c)
        stream.seek(struct.unpack('<I', stream.read(4))[0])
        if stream.read(4) != b'PE\0\0' or struct.unpack('<H', stream.read(2))[0] != 0x8664:
            raise RuntimeError('Expected a native x64 PE file: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--check-only', action='store_true', help='Verify runtime inputs without creating an archive')
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    header = (build / 'generated/ToneGridBuildInfo.h').read_text(encoding='utf-8')
    version = re.search(r'version = "([0-9.]+)"', header).group(1)
    revision = re.search(r'sourceRevision = "([0-9a-f]+)"', header).group(1)
    commit = (subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', revision], text=True).strip()
              if (ROOT / '.git').exists() else (ROOT / 'SOURCE-REVISION.txt').read_text(encoding='utf-8').strip())
    if not commit.startswith(revision):
        raise RuntimeError('Build revision does not match the reviewed source snapshot')
    runtime = build / f'ToneGrid_artefacts/Release-{version}'
    archive = output / f'ToneGrid-{version}-Windows-x64.zip'
    if archive.exists():
        raise SystemExit('Refusing to replace an existing package: ' + str(archive))
    lock_path = ROOT / 'Resources/factory-content-lock.json'
    lock = json.loads(lock_path.read_text(encoding='utf-8'))
    files = {}

    def include(relative, expected_hash=None, expected_size=None):
        path = runtime / relative
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(runtime):
            raise RuntimeError('Missing, linked or external runtime file: ' + relative)
        sha, size = digest(path), path.stat().st_size
        if expected_hash is not None and sha != expected_hash:
            raise RuntimeError('Runtime hash mismatch: ' + relative)
        if expected_size is not None and size != expected_size:
            raise RuntimeError('Runtime size mismatch: ' + relative)
        files[relative] = {'sha256': sha, 'bytes': size}

    locked = set()
    for entry in lock['files']:
        relative = entry['path'].removeprefix('Resources/')
        if relative == entry['path']:
            raise RuntimeError('Unexpected factory lock path')
        locked.add(relative)
        include(relative, entry['sha256'], entry['bytes'])
    for folder in ('FactoryAmps', 'FactoryCabinetIR', 'Models', 'ToneBank', 'StarterPresets'):
        actual = {p.relative_to(runtime).as_posix() for p in (runtime / folder).rglob('*') if p.is_file()}
        if actual != {name for name in locked if name.startswith(folder + '/')}:
            raise RuntimeError('Unexpected or missing factory file in ' + folder)

    for original, target in (
        ('PedalAssets/V137', 'PedalSkins'), ('Jam', 'Jam'), ('Learn', 'Learn'),
        ('Calibration', 'Calibration'), ('Translations', 'Translations'), ('Licenses', 'Licenses'),
    ):
        expected = set()
        for source in (ROOT / 'Resources' / original).rglob('*'):
            if source.is_file():
                relative = target + '/' + source.relative_to(ROOT / 'Resources' / original).as_posix()
                expected.add(relative)
                include(relative, digest(source), source.stat().st_size)
        if target == 'Licenses':
            expected.add('Licenses/LAME-COPYING.txt')
            include('Licenses/LAME-COPYING.txt', digest(ROOT / 'vendor/lame/lame-3.100/COPYING'))
        actual = {p.relative_to(runtime).as_posix() for p in (runtime / target).rglob('*') if p.is_file()}
        if actual != expected:
            raise RuntimeError('Unexpected files in runtime content: ' + target)
    for name in ('LICENSE.txt', 'LICENSE-NOTICE.txt'):
        include(name, digest(ROOT / name))
    include('Tools/lame-3.100-source.tar.gz', digest(ROOT / 'vendor/lame/lame-3.100.tar.gz'))
    encoder = next((p for p in (build / 'Release/lame.exe', build / 'lame.exe') if p.is_file()), None)
    if encoder is None:
        raise RuntimeError('Cannot find the native encoder build output')
    include('Tools/lame.exe', digest(encoder))
    include('ToneGrid.exe')
    for name in RUNTIME_DLLS:
        include(name)
    for name in ('ToneGrid.exe', 'Tools/lame.exe', *RUNTIME_DLLS):
        verify_x64(runtime / name)
    if revision.encode('ascii') not in (runtime / 'ToneGrid.exe').read_bytes():
        raise RuntimeError('Executable does not contain its configured source revision')
    audio = {name for name in files if Path(name).suffix.lower() in AUDIO}
    if audio != {name for name in locked if Path(name).suffix.lower() in AUDIO}:
        raise RuntimeError('Unexpected audio outside the locked factory inventory')
    longest_path = validate_inventory(files, version)
    content_commit = (subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', revision], text=True).strip()
              if (ROOT / '.git').exists() else (ROOT / 'SOURCE-REVISION.txt').read_text(encoding='utf-8').strip())
    manifest = dict(version=version, platform='Windows x64', sourceCommit=commit,
                    contentSourceCommit=content_commit, longestArchivePath=longest_path,
                    factoryArchiveSHA256=lock['sha256'], factoryLockSHA256=digest(lock_path),
                    factoryFiles=len(locked), files=files)
    if args.check_only:
        print(json.dumps(dict(version=version, sourceCommit=commit, factoryFiles=len(locked),
                              runtimeFiles=len(files), runtimeVerified=True), indent=2))
        return
    output.mkdir(parents=True, exist_ok=True)
    prefix = f'ToneGrid-{version}/'
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for name in sorted(files):
            zipped.write(runtime / name, prefix + name)
        zipped.writestr(prefix + 'PACKAGE-MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
    with zipfile.ZipFile(archive) as zipped:
        expected = {prefix + name for name in files} | {prefix + 'PACKAGE-MANIFEST.json'}
        if set(zipped.namelist()) != expected or zipped.testzip() is not None:
            raise RuntimeError('Archive inventory or CRC failure')
        for name, details in files.items():
            result = hashlib.sha256()
            with zipped.open(prefix + name) as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b''):
                    result.update(block)
            if result.hexdigest() != details['sha256']:
                raise RuntimeError('Archived byte mismatch: ' + name)
    sha = digest(archive)
    archive.with_suffix('.zip.sha256').write_text(sha + '  ' + archive.name + '\n', encoding='utf-8')
    report = dict(version=version, sourceCommit=commit, contentSourceCommit=content_commit,
                  longestArchivePath=longest_path, archive=archive.name,
                  bytes=archive.stat().st_size, sha256=sha, runtimeFiles=len(files),
                  factoryFiles=len(locked), factoryArchiveSHA256=lock['sha256'],
                  factoryLockSHA256=digest(lock_path), exactArchiveHashesVerified=True,
                  personalAudioIncluded=False)
    archive.with_suffix('.validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
