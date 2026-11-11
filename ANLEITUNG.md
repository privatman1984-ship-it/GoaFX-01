# GoaFX – Goa-Trance-FX-Synth (VST3)

Spiele eine MIDI-Note, und GoaFX löst einen FX-Sound aus. Die Note bestimmt die Tonhöhe.

**Modi:** Riser, Downlifter, Impact, Laser Up, Laser Down, Alien (zufällige 16tel-Steps, Tempo vom Host), Whoosh, Drone.
**Regler:** Length, Pitch Range, Wellenform, Unison und Detune, Noise- und Osc-Level, Filter (LP/HP/BP, Cutoff Max, Resonanz), Drive, Ping-Pong-Delay (synchron zum Host-BPM), Reverb, Volume.

## Fertige Datei über GitHub bauen lassen (ca. 10 Minuten)

1. Erstelle einen kostenlosen Account auf https://github.com und lege ein neues Repository an (z. B. `GoaFX`, öffentlich oder privat).
2. Lade den **gesamten Inhalt** dieses Ordners hoch (Add file -> Upload files). Wichtig sind `CMakeLists.txt`, der Ordner `Source` und der versteckte Ordner `.github`.
   - Falls `.github` beim Hochladen fehlt: Add file -> Create new file, als Namen `.github/workflows/build.yml` eingeben und den Inhalt aus `WORKFLOW_build.yml.txt` einfügen.
3. Öffne den Reiter **Actions**. Der Build startet automatisch (dauert ca. 10 bis 15 Minuten, weil JUCE geladen wird).
4. Wenn der Lauf grün ist, klicke ihn an und lade unten unter **Artifacts** `GoaFX-windows-latest` (Windows) oder `GoaFX-macos-latest` (Mac) herunter.
5. Entpacken und den Ordner `GoaFX.vst3` hierhin kopieren:
   - Windows: `C:\Program Files\Common Files\VST3`
   - Mac: `/Library/Audio/Plug-Ins/VST3`
6. DAW neu scannen. GoaFX erscheint als Instrument.

**Mac:** Das Plugin ist nicht signiert. Im Terminal einmal `xattr -cr /Library/Audio/Plug-Ins/VST3/GoaFX.vst3` ausführen.

Wenn der Build rot ist: Auf den Lauf klicken, den Fehlertext kopieren und an Claude schicken.
