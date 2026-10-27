# ToneGrid interface languages

These UTF-8 JSON files provide the mild interface translation introduced in
ToneGrid 1.1.44. Do not translate PLAY, JAM, LEARN, RECORD, pedal artwork, standard
music control names, NAM names or metadata, songs, artists, user text, Guitar Club
content, YouTube content, logs or Send Diagnostic.

## Edit without compiling

Edit the copies in your own profile, then restart ToneGrid:

- Windows: `%APPDATA%/ToneGrid/Content/Translations`
- macOS: `~/Library/Application Support/ToneGrid/Content/Translations`
- Linux: the ToneGrid application-data folder, under `Content/Translations`.

Each locale uses its exact language-tag filename, for example `fr.json` or
`pt-BR.json`. ToneGrid creates missing editable copies on launch. Existing copies
are never overwritten. To restore a shipped language, close ToneGrid and remove
that editable JSON file. The next launch restores its default copy.

Change only the text on the right of a key in `strings`. Preserve `{number}`,
`{version}`, `{error}` and all other named placeholders exactly. `\n` is a newline.
Use the localized button names consistently in guidance and tooltips.

## Add a language

Copy `en.json`, choose a unique BCP 47 language tag, set `locale` to that tag and
`nativeName` to the language's own spelling, and give the file the same tag as its
name. Translate `strings`, leaving the keys unchanged. Save in the profile's
Translations folder and restart. The language will appear in the dropdown.

The shipped languages are English, French, Spanish, German, Italian, Brazilian
Portuguese, Japanese, Simplified Chinese, Korean and Russian. There are no flags.
The system language is used on first launch when supported; the user's selection
then takes precedence. Language changes do not reopen the audio device.

## Compatibility contract

- `schemaVersion: 1` is unchanged when adding strings or optional metadata.
- Stable semantic keys must not be renamed or change meaning.
- Extra keys and metadata are safe; older applications ignore them.
- Missing, empty, non-string or incompatible-placeholder entries keep the
  shipped translation, then built-in English as the final fallback.
- Malformed JSON or an unsupported schema cannot block startup. Shipped text
  remains available. An in-session reload retains the last valid language pack.
- A missing external English file still leaves embedded English available.
- A new interface feature needs an app build; editing or adding language packs
  for existing keys does not.
- No file watcher or network translation service is used.

Run `tools/check-translations.py` before distributing edited packs. Translation
quality and native Windows/macOS layout should also be reviewed by speakers of
the language, including CJK and Cyrillic fonts.
