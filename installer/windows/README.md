# Windows installer — local unsigned review

The application remains version 1.1.44. `ToneGrid.iss` and the Python wrapper default to a separate **ToneGrid Review** installation identity. No provider acceptance, new production release or signature is implied.

## Storage policy

`Source/Platform/AppPaths.h` resolves shipped resources beside ToneGrid.exe on Windows. Mutable presets/settings, imported libraries, editable catalogues/translations and browser state use the Roaming `ToneGrid` profile. `Source/App/RecordIntegration.inl` stores ordinary RECORD takes under Documents/ToneGrid/Recordings/Active. Exports are user-selected; Documents is only the initial suggestion.

Program files therefore default to `%LOCALAPPDATA%\Programs\ToneGrid Review` for this review, with no administrator requirement. The future production identity uses `%LOCALAPPDATA%\Programs\ToneGrid`. No data-path migration or profile rename is introduced. The installer refuses application directories inside the ToneGrid Roaming/recording folders.

Upgrade reuses the previous installation directory. Only a previously installed file that is absent from the new payload, still has its recorded SHA256, stays inside the program directory and has no reparse-point ancestor may be retired. Modified old files and untracked user files are preserved during upgrade. Uninstall uses Inno's owned-file log; it has no profile/recording deletion rule. Program files, including edited files that were originally installed, remain uninstallable; keep personal content in the app's user folders.

Start-menu and optional desktop shortcuts are provided. Launch after installation is optional and unchecked; silent installation never launches ToneGrid. Setup does not force-close an audio session. Close ToneGrid before replacing files.

## Build from the verified package

Requires Python 3.11+, PowerShell and official Inno Setup 7.1.0 x64. The application must already have been compiled and packaged using tools/package-windows.py. The wrapper verifies every package member's path, size and SHA256 and generates an explicit file list, never a wildcard over the build directory.

```powershell
./tools/setup-inno.ps1 -ToolDirectory ./build-tools
python tools/build-windows-installer.py --package ./dist-windows/ToneGrid-1.1.44-Windows-x64.zip --compiler ./build-tools/InnoSetup/ISCC.exe --work ./build-installer --output ./dist-installer
```

Use fresh work/output directories. The compiler download is pinned to its official SHA256 and checked for a valid Pyrsys publisher signature. No signing credentials are installed. The generated installer remains unsigned even though the upstream compiler is signed.

For the local upgrade test only, `--upgrade-fixture` produces a prior Review installer with two synthetic old owned files. This does not represent an actual old ToneGrid application version. It is not a distributable release. The final installer has no fixture files.

`/TESTROOT=<marked-directory>` is available only in Review builds and redirects shortcut creation into that marked test root. It does not override application storage. Native application tests must use the existing `--startup143-ui-test` and `--startup143-existing-ui-test` modes, which explicitly isolate application data and use a synthetic audio device. A normal manual launch uses the real ToneGrid profile; the Review installation identity does not change that behavior.

## Future signing integration, not active

1. Obtain provider approval and its artifact policy first. Record the approved public source commit, build environment, unsigned hash, artifact provenance and manual approval.
2. Submit only the ToneGrid executable built by the trusted workflow for application signing. Verify the returned signature/product/version; regenerate the package manifest from those exact signed bytes. Preserve the unsigned-to-signed mapping. Do not sign LAME or Microsoft DLLs under the ToneGrid identity.
3. Build the reviewed installer with `ReviewBuild=0`, a stable production AppId, and `SignedRelease` only after the approved signing bridge exists. The ISS has `SignTool=ToneGridVerifiedSigner`, `SignedUninstaller=yes` and an explicit `SignedUninstallerDir` integration point. No command or credentials for this bridge are invented here.
4. Inno can pre-generate the uninstaller for external signing and reuse the returned signed file on the next identical compile. For asynchronous trusted-CI signing, agree the generate/upload/sign/retrieve/recompile sequence with SignPath. Never leave an interactive signing prompt in CI or accept an unsigned cached uninstaller.
5. Verify application, setup and extracted uninstaller signatures, timestamp and hashes after a clean installation. Test upgrade/uninstall again. Only then publish a signed release after owner approval.

The unsigned production identity, admin/system-wide installs, Windows ARM64 and signing paths have not been exercised by this local Review installer lifecycle. Current Mac correspondence is a separate documented workstream and does not gate Windows build/tests.

Production signing must align the executable, setup and uninstaller product/version policy. Local fixture/review InstallerRevision values are test identifiers; set/review the production value explicitly against the application metadata and provider policy. None of the local review revision values is a provider-approved release configuration.
