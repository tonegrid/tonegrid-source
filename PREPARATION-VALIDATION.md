# Windows publication preparation validation — 26 October 2026

## Completed locally

- Independently extracted the reviewed source archive into a new tree and checked every source member hash. Downloaded the pinned existing public Windows data ZIP and official Microsoft WebView2 SDK afresh into that work tree. No private repository credentials/access, private release or personal dependency cache was used. Normal installed compiler/Python/Node toolchains were used.
- Configured with Visual Studio 2022 x64, private package registries disabled and FetchContent disconnected. Fully compiled the Release ToneGrid application, native LAME and offline tests. Application build: 205 seconds; offline-test build: 73 seconds on this host. These are measured local durations, not promises.
- Eighteen distinct suites were exercised: InputCompensation136, RigStorage, Normalization125, Portability, DeviceRate, Isolation, Cabinet, PopupOnboarding, NamControls140, Audit142, LibraryFeedback142, FactoryRefinements142, Startup143ExistingHost, Startup143Host, Release143, Language144, Record141 and RecordVideoBridge.
- Initial result was 16/18. Fresh startup found an omitted required Hiwatt provenance JSON referenced by the Factory manifest; restoring it and recording it in the public lock fixed resource seeding. Isolation had a stale fixture that assigned the same fake production/test root; only the test fixture was corrected. Application Source/ C++ and real profile policy were not changed. The corrected six affected/related suites then passed 6/6. All 18 latest relevant results pass. The repaired resource inventory has 4,060 records.
- The verified portable payload contains 4,242 runtime files; hashes/CRC/allowlist verified. The installer adds its privacy notice, package manifest and owned-file receipt. No personal recordings/profiles are included.
- Inno Setup 7.1.0 x64 came from its official release, matched its published SHA256 and valid Pyrsys Authenticode signature. The resulting ToneGrid executable, Review installer and extracted uninstaller have no Authenticode certificate.
- Twenty-nine installer lifecycle checks passed: marked isolated program/test profile, per-user Review registration, start/desktop shortcuts, upgrade reusing the previous path, retirement of an unchanged obsolete owned fixture, preservation of modified/untracked files during upgrade, installed payload SHA256, native installed fresh/existing startup and synthetic finite audio, uninstall removal of owned app/shortcuts/registration, and byte-identical retention of test presets/settings/language/recording.
- The upgrade uses two local installer revisions of the same 1.1.44 application with explicitly synthetic retired-file fixtures. It is not a claim of upgrading an actual released 1.1.43 installer. The final Review installer contains no fixture files. Normal manual launches share the normal ToneGrid profile; test modes explicitly isolate it.
- Eight public-input synthetic safety tests also passed in the initial preparation. Application C++ files remain unchanged from the development snapshot. Vendor/source hashes, bounded sensitive-pattern review and corresponding Windows source-archive checks are recorded separately.

## Limits and remaining work

No physical ASIO/interface listening, disconnect/reconnect, microphone permission, real YouTube sync/fullscreen, clean external Windows machine, admin/system-wide installation, ARM64, production installer identity or signed setup/uninstaller path has been tested here. GitHub CI has not been published or executed; the workflow remains gated and unsigned.

Latest installed fresh startup reported 69.44 seconds; an earlier pass reported 80.93 seconds, mostly Factory preparation. Latest existing-preset sound readiness reported 8.54 seconds; the whole GUI test process takes longer because it also waits/checks/snapshots and closes. The first-start cost is a performance follow-up. UI screenshots were reviewed; synthetic mode's interface/status text is not a real hardware-readiness claim.

Current source includes later ToneBank editorial content than the distributed Windows 1.1.44 ZIP. Its local-review build is separately hashed; no identical-public-package or bit-reproducible claim is made. The exact old Mac sources remain a separate unresolved workstream and do not block the Windows work completed here.

Content scope and asset-licence decisions, provider eligibility, roles/MFA, privacy/cleanup notices and trusted signing CI remain review gates. No candidate, provider request, website change or download replacement has been published or submitted.

## Documentation refresh — 27 October 2026

Owner content-scope confirmations, the original-art and contributed-preset AGPL
decisions, GitHub 2FA attestation and intended manual release approval were recorded.
This refresh changes documentation/status only. It does not rerun or extend the
earlier application, installer or hardware validation, alter runtime resources,
authorise publication, submit a provider application or establish signing approval.

## Publication approval

The owner subsequently approved creating the separate public tonegrid-source
repository and publishing this source snapshot on 27 October 2026. Publication
status metadata reflects that approval. The manually triggered public CI has not
been run as part of source publication; no SignPath acceptance or Windows signing
is implied. Earlier validation results retain their stated scope and limitations.
