# Bitwig MIDI routing probe

These CLAP modules are deliberately tiny diagnostics for the Bitwig -> CLAP MIDI boundary. Every module has the same stereo audio pass-through and the same CC handler. The only differences are host-facing descriptor roles and note-port negotiation details.

## Audible result

Place Bitwig's MIDI CC device immediately before a probe and send **CC20**.

- CC20 >= 64: output is ducked to 25%.
- CC20 < 64: output returns to unity.
- If nothing happens, that probe did not receive the raw CC event.

The probe ignores MIDI channel so channel configuration cannot muddy the result.

## Variants

1. `NullClapMidiProbe1Current.clap`
   - audio-effect + stereo
   - MIDI | CLAP supported
   - MIDI preferred
   - non-zero stable note-port ID
   - represents GlitchDeck's original note-port negotiation without its DSP/UI

2. `NullClapMidiProbe2ClapPreferred.clap`
   - identical to #1 except CLAP is the preferred dialect
   - isolates dialect preference

3. `NullClapMidiProbe3NihPort.clap`
   - identical to #2 except note-port ID is 0
   - mirrors the important port metadata used by NIH-plug

4. `NullClapMidiProbe4AudioInstrument.clap`
   - NIH-style port metadata
   - audio-effect + instrument + stereo
   - isolates the hybrid role used by the latest GlitchDeck experiment

5. `NullClapMidiProbe5AudioNoteEffect.clap`
   - NIH-style port metadata
   - audio-effect + note-effect + stereo
   - exposes both a note input and note output so the note-effect classification is standards-valid

6. `NullClapMidiProbe6AllRoles.clap`
   - NIH-style port metadata
   - audio-effect + instrument + note-effect + stereo
   - exposes note input/output
   - closest to the historical VCV Rack compatibility profile

All six are required to pass clap-validator before being packaged. The numbered progression is intentional: the first probe that ducks identifies the smallest host-facing change that alters Bitwig routing.
