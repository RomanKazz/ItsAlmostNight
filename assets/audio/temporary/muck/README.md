# Temporary Muck music

Source: [Muck Wiki music list](https://muck.fandom.com/wiki/Music). The user
reports permission to use these files in this game until original music is ready.
The music is not covered by the Kenney licenses used for other game audio.

| Game context | Files |
| --- | --- |
| Main menu | `Day0.mp3` |
| Day | `Day1.mp3`, `Day2.mp3`, `Day0.mp3` |
| Night | `Night0.mp3`, `Night1.mp3` |
| Boss | `Big_Chunks_Theme.mp3`, `Bob_theme.mp3`, `Gronk_Stonks.mp3` |
| Background ambience | `Ambience.mp3` |

Tracks within a context rotate after playback. Successive boss encounters start
with a different boss theme. Music fades between contexts and follows the music
volume setting. Ambience follows effects volume.

To replace the temporary soundtrack, put your own MP3 files at these paths, or
edit `MusicPaths` in `src/audio/AudioSystem.cpp` to point at different filenames.
