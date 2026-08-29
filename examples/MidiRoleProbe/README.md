# Bitwig MIDI role probe

This diagnostic bundle builds several otherwise-identical CLAP audio processors. Each has stereo audio input/output and one raw-MIDI input port. The only difference is descriptor feature classification.

Send CC20 into a probe. While CC20 is >= 64, the probe ducks its audio output to 25%. When CC20 is < 64, audio returns to unity. If the audio ducks, Bitwig delivered the CC to that CLAP instance.

Variants:

- `NullClapMidiProbeAudioEffect`: audio-effect + stereo
- `NullClapMidiProbeAudioNoteEffect`: audio-effect + note-effect + stereo
- `NullClapMidiProbeAudioInstrument`: audio-effect + instrument + stereo
- `NullClapMidiProbeAllRoles`: audio-effect + instrument + note-effect + stereo
- `NullClapMidiProbeInstrument`: instrument + stereo

All variants expose the same audio and MIDI ports and use the same process/event code.