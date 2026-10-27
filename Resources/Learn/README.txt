ToneGrid LEARN - Guitar Club lesson catalogue

134 lesson cards, 163 original YouTube videos, and 134 original thumbnails.
Public pages checked on 14 October 2026, across all 12 blog archive pages.
The eight posts without a YouTube video are listed in other-posts.json; they do
not appear as playable video cards. Videos and audio are never bundled.

USING LEARN
Choose LEARN below the ToneGrid logo. Search songs, artists or instructors.
Combine Date, Style and Artist filters, or tick Favourites to show saved lessons.
Click a card to open its large 16:9 player. Multi-part lessons have a Video selector.
The YouTube fullscreen control fills the current monitor. Press Escape or use
the player's fullscreen control again to return without restarting the video.
Use the star or Add to favourites to save a lesson; Back to lessons restores the grid.
Leaving LEARN stops its video. Switching between JAM and LEARN stops the old player.
The description scrolls independently, so the video remains visible while you read.
Original HTTPS links in the YouTube description are clickable.
Open on YouTube is available if a video cannot play inside the app.

YouTube uses the Windows system audio output, which may differ from ToneGrid's
ASIO output. Select your interface in Windows sound settings to hear both through it.
YouTube audio is not included in ToneGrid recordings. Internet and WebView2 are
required for video playback; the catalogue and favourites work offline.

EDITING lessons.json
This UTF-8 JSON is deliberately simple: version, partner, and lessons.
Save changes while LEARN is visible; ToneGrid reloads the file automatically.
A valid edit closes the current video and refreshes the grid. Invalid JSON keeps
the last working catalogue and is never overwritten. Keep a backup of your edits.

partner contains name, url, headline, message and button. Banner copy can be
adapted for the collaboration. Its link must remain an HTTPS guitarclub.io link.
The button text has two lines, separated by \n in JSON. Its default link opens
the Guitar Club homepage, as requested for the beta.

Each lesson contains:
  id           Stable identifier. Keep it unchanged to retain existing favourites.
  title        Song or lesson title.
  artist       Original artist or group; empty when not applicable.
  instructor   Lesson presenter, independently of the article's write-up author.
  date         Publication date, YYYY-MM-DD. Newest lessons are listed first.
  style        Editorial musical category; freely editable, not a publisher tag.
  summary      Short original blog introduction.
  lessonUrl    Original Guitar Club article and resources.
  thumbnail    Relative file under thumbnails/, for example thumbnails/my-lesson.jpg.
  videos       One or more objects with title, url, description.

videos[].description is the original complete YouTube description, including
its original links. Three interactive-tab guide videos have no description;
their fields remain empty. No substitute description was invented.
Additional parts remain within the same lesson card. Add their video objects
to videos[]; the selector appears automatically.

Musical styles are editorial groupings for browsing and can be refined in JSON.
Favourites are stored only in the user's ToneGrid/LEARN/favourites.json profile,
separately from the distributed catalogue, so updates do not overwrite them.

ATTRIBUTION
Guitar Club / Your Guitar Academy: https://www.guitarclub.io/
Original public catalogue: https://www.guitarclub.io/blog/posts
Lesson videos, thumbnails and descriptions remain the content of their original
creators. They are separate from ToneGrid's AGPL software license.
The banner links to Guitar Club; it does not claim an official endorsement.
