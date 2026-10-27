# Factory and third-party content

The code licence and third-party content terms are separate. The reviewed library
contains 1,355 amp NAMs, 151 pedal NAMs and 2,248 cabinet IRs. Its current ToneBank
has 182 entries and editorial changes newer than the published Windows archive.

## Source publication scope

Code, catalogues, build-required images/layouts, translations and notices are
included. The source tree excludes all 3,754 Factory NAM/WAV audio files, downloaded
Factory runtime thumbnails, precompiled runtime/SDK binaries, signing material,
private correspondence and personal profiles or recordings.

The public retrieval tool restores allowed runtime inputs from the already public
Windows distribution, using Resources/public-content-lock.json with checksums and
an exact member inventory. It does not require private GitHub credentials, scrape
TONE3000 or publish a new standalone Factory pack. Changed downloads fail integrity
verification. The tool writes only into the checkout, never an installed app.

## Rights and licences

Existing creator credits, source URLs, notices, metadata and approved bundling
scopes remain intact. The publisher's 27 October confirmations resolve the
previous owner questions about IR/normalization scope, the 33 RX Guitars custom
IRs, Guitar Club public-source media/logo permission and preset contributions.
See [CONTENT-RIGHTS-STATUS.md](CONTENT-RIGHTS-STATUS.md).

ToneGrid's original ChatGPT-generated illustrations and the partner-contributed
presets covered by the publisher's confirmed AGPL permission use AGPL-3.0-only,
to the extent applicable rights are held. Preserve attribution. This grant does
not extend to separately licensed factory audio, third-party logos, lesson media,
reference photographs or other independently licensed content.

Guitar Club's thumbnails, descriptions and logo are included under the permission
confirmed by the publisher. TONE3000 branding is retained for the integration under
its [API design requirements](https://www.tone3000.com/api), not relicensed as AGPL.
No YouTube audio or video is included in the source publication.

## Limits and pending work

The owner confirmations do not themselves transfer third-party ownership, provide
a general standalone redistribution licence for audio, or establish provider
approval. SignPath eligibility and the exact signing scope for mixed code/data
remain pending. Integration behaviour is subject to TONE3000's published terms;
no project-specific platform approval is claimed here.

The owner approved public source publication on 27 October 2026. Public GitHub CI remains a separate next step. Native Mac
source correspondence remains separately documented and does not block the
completed Windows local validation. No equivalent empty-Factory build is claimed.
