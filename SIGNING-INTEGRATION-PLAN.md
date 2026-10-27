# Signing integration plan (not executed)

The staged Windows workflow is unsigned, manual and blocked by PUBLICATION-STATUS.json until review is complete. It has read-only repository permission and no signing secret. It has not been uploaded or run on GitHub. Action revisions are pinned to official commit objects.

After source publication and provider acceptance, establish the provider's trusted CI/artifact integration, signing policy, artifact metadata restrictions, authorized roles/MFA and a manual release approval. Submit only the resulting reviewed ToneGrid executable/artifact through the verified CI chain. Do not upload an arbitrary existing local binary or sign upstream LAME/Microsoft DLLs as ToneGrid code. Preserve unsigned input and signed output hashes/source mapping.

The workflow does not apply for a service, purchase a certificate, submit a signing request, release to production or change the public download repository. No SignPath secret/policy identifier is invented. Native Mac CI remains a separate task after exact source correspondence and Mac-only fixes are reconciled; no approximate source is substituted.

The staged workflow now includes the complete unsigned Windows build, selected suites, verified runtime package, Inno Review installer and isolated upgrade/uninstall lifecycle. Local components passed; the public workflow has not run. See installer/windows/README.md for the prepared application/setup/uninstaller signing boundaries, asynchronous external signing sequence and production metadata requirements. None of those signing paths is enabled or claimed as tested.
