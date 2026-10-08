# FMBox

An eight-sound FM drum synth for macOS - AU (aumu/Fmbx/Tzor), VST3 and a
standalone app, built with JUCE 9. Twelve knobs per sound, Kitbox's three send
effects and a master level - every knob on screen at once.

Sibling of [Kitbox](https://github.com/TZorr/Kitbox) (16-pad sampler) and
[Transmute](https://github.com/TZorr/Transmute) (drum resynthesis).

## Sounds

| Sound      | Note    | Model                                         | Tone knob |
|------------|---------|-----------------------------------------------|-----------|
| Kick       | C1 36   | Plaits synthetic bass drum (DaisySP)          | Tone      |
| Snare      | D1 38   | Plaits synthetic snare drum (DaisySP)         | Snappy    |
| Clap       | D#1 39  | 4 noise bursts + tail, band-passed; FM noise  | Spread    |
| Closed Hat | F#1 42  | Plaits hi-hat, FM metallic source             | Tone      |
| Open Hat   | A#1 46  | the same, longer; choked by the closed hat    | Tone      |
| Tom        | A1 45   | PM pair, ratio 1.59, falling pitch            | Bend      |
| Rim        | C#1 37  | PM pair, ratio 2.27, low body, noise click    | Click     |
| Bell       | G#2 56  | two PM pairs 1.48 apart - cowbell to bell     | Ratio     |

Notes start on the GM drum map (C1 = 36, as Logic counts). To change one,
click the pad (it is selected and plays), press **LEARN** top right in the
display and play a key: that key becomes the pad's note. If another sound had
it, the two swap, so every sound stays playable. The notes are saved with the
session. Every sound has
**Tune** (Hz), **Decay**, **FM** and its **Tone** knob; **Drive** (Kitbox's
soft clipper, anti-aliased), **Click** (a few milliseconds of noise and a
3.5 kHz blip on top of the hit); **Level**, **Pan**, sends to **Reverb**,
**Delay** and **Mod** (post-level, pre-pan) and **Velo**, how much velocity
matters (0 % plays every hit at full).
On the Plaits kick FM is the pitch sweep; on the hats it fades the 808's six
square waves over to three inharmonic PM pairs; on the others it is the
modulation index, which falls faster than the level, so a hit starts bright.

Each sound is monophonic: a new hit replaces the one still ringing, faded over
2 ms. Velocity sets level and, on the FM voices, a little brightness. Click a
pad to audition it - higher on the pad is louder.

## Send effects

Kitbox's three slots, built from Rackbox's effects:

    Reverb   Plate / Room / Hall                     Size, Damp
    Delay    Digital / Tape / Ping-Pong, tempo-synced Time (note value), Feedback
    Mod      Chorus / Phaser / Flanger / Tremolo / Shifter

Each has its Level (the return). Switching the type crossfades over 30 ms.

    voice + click -> drive -> level -> pan -> mix
                                    \-> sends -> Reverb / Delay / Mod -> mix
    mix -> master

## Build

    Scripts/build.sh      # configure, build, FMBoxCheck, EditorShot
    Scripts/install.sh    # AU + VST3 to ~/Library, app to /Applications, auval
    Scripts/package.sh    # installer .pkg (AU + VST3, /Library)

`build.sh` measures every sound at 44.1, 48 and 96 kHz (level, length, sleep,
finite values at every knob corner, choke, retrigger, click, drive, velocity,
the three sends), writes them to
`build/sounds/*.wav`, round-trips the session state, checks the MIDI notes land
sample-accurately and renders the panel to `build/shots/fmbox.png`.

Requires JUCE 9 at `~/JUCE`, CMake and Ninja; Apple Silicon, macOS 26.5 or later.

## License

AGPLv3 - see [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
The DaisySP drum models in `Source/ThirdParty/DaisySP` are MIT.
