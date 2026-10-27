# Privacy and local data

ToneGrid processes guitar audio locally. Presets, calibration, imported captures and recordings are stored locally. This source preparation does not upload or bundle a real user profile, recording or diagnostic log.

The application automatically checks tonegrid.net for app/ToneBank updates on its configured schedule (72 elapsed hours). Those HTTPS connections expose normal network information to the server; this is not an offline-only application. ToneBank refresh can be requested manually. The currently released update code can accept an older compatible online bank; correcting that behavior is a separate application task, not silently changed here.

JAM/LEARN/RECORD YouTube features and the TONE3000 browser make third-party connections when used. Those services have their own policies and may use browser storage/cookies. Sharing a preset or diagnostic prepares content for the user to review/send; it does not automatically email a report. The ordinary app log may contain capture filenames and device information and must not be committed publicly.

Service policies: [Google/YouTube](https://policies.google.com/privacy), [TONE3000](https://www.tone3000.com/privacy). The project must confirm its own server retention/contact policy and the provider's notice/opt-out requirements before declaring this policy final.

Existing releases silently attempt background deletion of the retired ToneGridIR profile and its contents, while preserving the canonical ToneGrid profile. This behavior was explicitly approved for the development release and has not been changed by publication staging. It must be documented/reviewed for the public distribution and signing-provider policy; do not describe this as a no-filesystem-change application.

Portable uninstall: close ToneGrid, then remove the extracted application directory. Keep personal profiles/recordings unless the user explicitly chooses to remove them. Do not provide a blanket recursive deletion of a user's data as a routine uninstall step.
