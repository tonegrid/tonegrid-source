ToneGrid JAM - editing the backing-track selection

Edit backing-tracks.json in this folder with a text editor. Save as UTF-8.
ToneGrid automatically reloads saved changes while JAM is open. No rebuild is needed.

The catalogue contains links only. Audio and videos are not bundled or downloaded.
Play opens one inline widescreen YouTube player, limited to 200 pixels high.
Close player, leaving JAM, changing filters, or scrolling the player out of view
stops playback. Press Play again to restart. Open on YouTube remains a fallback.
Switching tracks starts a fresh player, preventing cancelled requests from the
previous video from interrupting the next one.
The scales and track catalogue work offline; playing YouTube requires internet.
YouTube uses the Windows system audio output, which may differ from ToneGrid's
ASIO output. Select your interface as Windows sound output if you want both through
the same speakers. YouTube audio is not included in ToneGrid's exported recordings.

Keep the outer structure: { "version": 1, "tracks": [ ... ] }
Each track has these fields:

  {
    "title": "A useful track title",
    "creator": "Original YouTube creator",
    "style": "Blues",
    "key": "A",
    "mode": "Minor",
    "bpm": 80,
    "url": "https://www.youtube.com/watch?v=VIDEO_ID_11",
    "suggestedScale": "minor-pentatonic"
  }

Replace VIDEO_ID_11 with the actual 11-character YouTube video ID.
URLs must be unique. Only HTTPS YouTube video links are accepted.
An id is derived automatically from the URL; you do not need to add an id field.
Use C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb or B for key.
Use "" for mode if the creator does not specify major/minor or a mode.
BPM may be a decimal number. Tempo filters include both endpoints.
Style is free text; any new style is added automatically to the filter.
Creator may be empty if unknown. Do not guess missing musical information.

suggestedScale is optional. Use "" to keep the selected scale when choosing a track.
Common choices: minor-pentatonic, major-pentatonic, minor-blues, major-blues,
major, minor, dorian, mixolydian, phrygian, lydian, harmonic-minor, melodic-minor.
The suggestion is a starting point, not a guarantee that every note suits every chord.

If the JSON contains an error, ToneGrid shows a message and keeps the last working
catalogue. A built-in copy is available if the file is missing or invalid at startup.
The app never overwrites your edits. Keep your own backup when installing updates.

Initial selection: 100 unique links each for Blues, Pop, Rock, Hard Rock, Metal,
Jazz, Funk, Soul and Country. Public titles/creator metadata checked 14 October 2026.
Keys and tempos are supplied by creators; they are not measurements by ToneGrid.

The inline player requires the Microsoft Edge WebView2 Runtime on Windows.
Videos that cannot be embedded can still be opened using Open on YouTube.
