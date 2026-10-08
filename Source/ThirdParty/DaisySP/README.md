# DaisySP (vendored subset)

From <https://github.com/electro-smith/DaisySP> (master, fetched 2026-10-08),
MIT - see [LICENSE](LICENSE). The drum models are Electrosmith's ports of
Emilie Gillet's Mutable Instruments Plaits (MIT).

Only what FMBox's Kick, Snare and hats need is here:

    Utility/dsp.h
    Filters/svf.{h,cpp}
    Synthesis/oscillator.{h,cpp}       (HiHat's RingModNoise needs it to compile)
    Drums/synthbassdrum.{h,cpp}
    Drums/synthsnaredrum.{h,cpp}
    Drums/hihat.{h,cpp}

## FMBox's changes

Each is marked `FMBox` in the source.

- `dsp.h`: `RandFloat()`, a per-thread xorshift, replaces `rand() * kRandFrac`
  in the drums - `rand()` is one generator shared by every plugin instance in
  the process. The `custom_dsp.h` hook is removed.
- `synthsnaredrum`: the file-scope `bool even` is a member, so two snares do
  not share it.
- `hihat.h`: the two envelope constants were per sample at 48 kHz; they are
  scaled by `48000 / sample rate`, so a hat is as long at 96 kHz as at 48 kHz.
  `GetMetallicNoise()` gives FMBox access to its metallic source (FMBox's FM
  source replaces the six squares).
