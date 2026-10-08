# Third-party notices

FMBox is © 2026 T'Zorr and is distributed under the GNU Affero General
Public License, version 3 (AGPLv3) - see [LICENSE](LICENSE). This follows
from JUCE, below.

## JUCE

- **What:** the JUCE 9 framework (audio plugin client, GUI).
  Not included in this repository - `CMakeLists.txt` expects it at `~/JUCE`
  (or a path passed with `-DJUCE_PATH`) and links it as an external framework.
- **Copyright:** © Raw Material Software Limited - <https://juce.com>.
- **License:** JUCE 9 is dual-licensed. This project uses JUCE's free tier,
  the GNU Affero General Public License, version 3 (AGPLv3). Because JUCE is
  AGPLv3 here and FMBox links it, FMBox is itself AGPLv3 in turn - the
  AGPL's own requirement for any work that incorporates AGPL-licensed code.
  The full JUCE licence terms are at
  <https://github.com/juce-framework/JUCE/blob/master/LICENSE.md>.
- **What AGPLv3 asks of anyone who distributes this plugin (including a
  built AU/VST3, not only source):** make the complete corresponding source
  available under the same licence, and preserve the copyright and licence
  notices. This repository already is that source.

## DaisySP / Mutable Instruments Plaits

- **What:** the Kick (`SyntheticBassDrum`), Snare (`SyntheticSnareDrum`) and
  hi-hat (`HiHat`) models, with the filter and helpers they use. Included as
  source in [`Source/ThirdParty/DaisySP`](Source/ThirdParty/DaisySP), with
  FMBox's small changes listed in its README.
- **Copyright:** DaisySP © 2020 Electrosmith, Corp.; Plaits © 2016 Emilie
  Gillet; Soundpipe © 2015 Paul Batchelor.
- **License:** MIT - [`Source/ThirdParty/DaisySP/LICENSE`](Source/ThirdParty/DaisySP/LICENSE).
  MIT code may be combined into an AGPLv3 work; its copyright notice and
  permission text travel with it, which the LICENSE file does.

## Everything else

The Clap, Tom, Rim and Bell voices, the FM metallic source of the hats, the
engine (retrigger fades, choke, click, mix) and the panel are FMBox's own code.
The send effects (`Source/Engine/Fx`), the drive's soft clipper, the knob and
the palette are shared with its siblings Kitbox and Rackbox; the type buttons
with Rackbox Plus. The drive's antiderivative anti-aliasing and the frequency
shifter follow ideas published in Signalsmith Audio's open-source DSP, but
were written independently: no code or tuned constants were copied; the
shifter's Hilbert allpass is designed by `Scripts/HilbertDesign.cpp`. No other
library is linked, and nothing is downloaded at build or run time.
