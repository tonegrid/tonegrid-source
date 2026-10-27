# Code signing policy — proposed

Status: no SignPath Foundation acceptance, certificate purchase or signing request has been made by this preparation. Existing downloads are not relabelled as newly signed. SignPath Foundation is the preferred option to evaluate; Certum remains a separately evaluated alternative.

Maintainer: [L’équipe ToneGrid](https://github.com/tonegrid).

Proposed roles: the ToneGrid project account coordinates authors and reviewers. The owner confirms GitHub two-factor authentication and intends to approve Windows signing requests personally. Actual provider accounts, role configuration and approval rules remain to be agreed with SignPath; no full personal name is added to these public files.

Only a ToneGrid executable built by reviewed public-source CI may be submitted for project signing. Upstream LAME and Microsoft/runtime binaries must not be signed under a ToneGrid-authored policy. The public Windows workflow is manually dispatched; publication alone does not run it or sign artifacts. A new unsigned local-review Windows build/installer was tested; no existing public binary was changed or signed.

Each signing request requires deliberate release approval. The source commit, dependency pins, exact CI artifact and checksums must be recorded. Releases must identify the signing state accurately. Privacy and portable uninstall details are in PRIVACY.md.

Owner content-scope and licence confirmations were recorded on 27 October 2026. The owner approved source publication on 27 October 2026. Provider eligibility and trusted public CI remain separate requirements before signing. If the project is accepted, add the provider's required attribution/policy wording only after approval; do not imply sponsorship now. See the local SIGNING-READINESS report and https://signpath.org/terms.html.
