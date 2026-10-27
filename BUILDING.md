# Public-source build instructions

This review candidate removes compulsory private GitHub authentication from build-input retrieval. Do not reuse the private development factory-content script or an old private lock. No prebuilt ToneGrid executable, private history, signing key or user profile is in the source tree.

## Prerequisites

- Python 3.11 or newer (standard library only for input tools).
- Keep the extracted source in a short path, such as `C:\tg-src`; the initial deep-path Windows configure hit FileTracker FTK1011. Moving this review copy to a temporary short drive path resolved it without a registry change.
- Node.js on PATH is required to register/run the RecordVideoBridge JavaScript test; it is not an application runtime dependency.
- CMake 3.22 or newer; Git is useful but archives also have SOURCE-REVISION.txt.
- Windows: Visual Studio 2022 C++ Build Tools, Windows SDK, x64 compiler; WebView2 Runtime for application browser playback.
- Mac: Apple Silicon, native Xcode Command Line Tools (clang/make), CMake, Python 3; target macOS 13.0. Intel/Rosetta is not supported by this configuration.
- Linux is experimental: a native C++20 toolchain and JUCE's ALSA, X11, FreeType, curl and WebKitGTK development packages are required. This audit does not claim a Linux build.

## Fetch data publicly

From a clean source tree:

```sh
python3 tools/factory-content.py sync
python3 tools/factory-content.py verify
```

On Windows use `python` instead of `python3` if appropriate. The downloader uses HTTPS, size limits and SHA256, extracts only pinned members and refuses to overwrite local edits. The existing public 1.1.44 Windows ZIP is the pinned data transport on all platforms; Windows executables are never installed into a Mac build. Checked-in source metadata/ToneBank overlays take precedence over the older data in that ZIP. It is a data transport, not a grant of new distribution rights.

For an already downloaded copy, add `--archive /path/to/WINDOWS.ToneGrid-1.1.44.zip`. Its bytes must match the lock. No GitHub account or private token is used. The source vendor trees contain the required open-source code. `DEPENDENCIES.json` records exact included-tree hashes and known upstream pins.

Windows additionally needs the pinned Microsoft WebView2 SDK:

```sh
python tools/factory-content.py sdk
```

It comes from the public NuGet URL/checksum in dependency-downloads.json. Optional `--archive` accepts that exact .nupkg. Do not add retrieved SDK binaries or caches to source control.

## Configure and compile (native platform)

Windows, from a normal Visual Studio-capable developer terminal:

```powershell
cmake -S . -B build-public -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build-public --config Release --target ToneGrid --parallel 2
cmake --build build-public --config Release --target ToneGridOfflineTests --parallel 2
ctest --test-dir build-public -C Release --output-on-failure -R "^ToneGrid\.(Cabinet|RigStorage|Portability|RecordVideoBridge)$"
python tools/package-windows.py --build build-public --output dist-windows
```

Mac, from a native arm64 terminal:

```sh
cmake -S . -B build-public-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DBUILD_TESTING=ON
cmake --build build-public-mac --target ToneGrid ToneGridOfflineTests --parallel 2
ctest --test-dir build-public-mac --output-on-failure -R '^ToneGrid\.(Cabinet|RigStorage|Portability|RecordVideoBridge)$'
python3 tools/package-mac-local.py build-public-mac/ToneGrid_artefacts/Release-1.1.44/ToneGrid.app --output dist-mac
```

Mac local packaging applies an ad-hoc signature only; it is not Developer ID signing or notarization. SignPath Windows signing does not solve Apple distribution requirements. Keep real profile data out of tests and packages.

## Actual validation and limits

The local Windows audit compiled the full Release application, native LAME encoder and offline-test executable from an independently extracted public-source candidate with fresh public/pinned input downloads. It ran 18 selected suites and tested native startup with fresh/existing isolated profiles and synthetic audio. After restoring required Factory provenance metadata and correcting a stale test fixture, all latest relevant results pass. The installed Review application also passed fresh/existing startup tests. The original application C++ is unchanged.

Read PREPARATION-VALIDATION.md for the complete scope, initial failures/corrections and limits. The unsigned Inno installer passed 29 lifecycle checks. No real audio interface, YouTube playback, other Windows machine, native Mac/Linux compilation or public GitHub CI execution is claimed. The first isolated Factory setup was slow (about 69.44 seconds in the latest installed test (an earlier pass measured about 81 seconds)); existing-preset sound readiness was about 8.54 seconds in the latest installed test. These are observed test timings, not hardware performance guarantees.

The old Mac source correspondence is a separate workstream, not a Windows test gate. Instructions for Mac are preparation instructions and remain unvalidated on this Windows host. Build/install instructions for the Windows Review installer are in installer/windows/README.md.

## Own content and permission boundaries

Users may import their own licensed NAMs/IRs using the application's library/import controls. A complete curated Factory experience requires the corresponding data; supplying a different model is not an equivalent preset. A fully empty-content startup/public build profile has not been validated here. The changes needed for a separately distributable minimal-content edition are listed in CONTENT.md; it is not represented as an already working fallback.
