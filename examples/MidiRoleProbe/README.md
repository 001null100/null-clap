# Bitwig MIDI routing probe

These CLAP modules are deliberately tiny diagnostics for the Bitwig -> CLAP MIDI boundary. Every module has the same stereo audio pass-through. Probes 1-6 vary only host-facing descriptor roles and note-port negotiation details. Probe 7 is a broader event sniffer used when none of the role variants receives the expected raw MIDI CC.

## Probes 1-6: CC20 role matrix

Place Bitwig's MIDI CC device immediately before a probe and send **CC20**.

- CC20 >= 64: output is ducked to 25%.
- CC20 < 64: output returns to unity.
- MIDI channel is ignored.
- If nothing happens, that probe did not receive the raw MIDI1 CC event.

### Variants

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

## Probe 7: Event Sniffer

`NullClapMidiProbe7EventSniffer.clap` uses a new plug-in ID so Bitwig cannot reuse a cached instance of the earlier probes. It advertises CLAP, MIDI1, and MIDI2 note dialects plus the audio-effect, instrument, note-effect, and stereo roles.

The goal is no longer to guess which role Bitwig wants. It answers a more basic question: **does any non-parameter CLAP event reach the plug-in at all?**

With continuous audio running through Probe 7:

- any non-parameter core CLAP event sets the output to 50% gain;
- raw MIDI1 CC20 still maps 0-63 to unity and 64-127 to 25% gain;
- MIDI2 CC20 does the same when Bitwig chooses MIDI2 delivery;
- `Event Count` increments for every non-parameter core input event;
- `Last Event Type` shows the CLAP core event type number (`10` = raw MIDI1, `12` = raw MIDI2; note/native events use the other CLAP core type values).

Do **not** adjust `Diagnostic Sentinel (ignore)`. It is an inert hidden parameter included only because clap-validator 0.4.1 crashes when a plug-in implements the parameter extension with zero parameters.

### Recommended second-stage test

1. Replace the old diagnostic package with the latest one and rescan plug-ins in Bitwig.
2. Load only `NullClapMidiProbe7EventSniffer.clap` after Bitwig's MIDI CC device.
3. Run continuous audio through it.
4. Move CC20 repeatedly between 0 and 127.
5. Watch `Event Count` and `Last Event Type`, and listen for any gain change.

If the count never changes and the audio never moves, Bitwig is not putting the generated controller message into this CLAP audio processor's event queue at all. If the count changes, the reported event type tells us which parser/path GlitchDeck needs to support.

All probes are required to pass clap-validator before being packaged.
