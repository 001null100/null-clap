# Bitwig MIDI routing probe

These CLAP modules are deliberately tiny diagnostics for the Bitwig -> CLAP MIDI boundary. Every module has the same stereo audio pass-through. Probes 1-6 vary only host-facing descriptor roles and note-port negotiation details. Probe 7 is a broad event sniffer. Probes 8-9 force raw-MIDI dialects so Bitwig cannot silently select native CLAP note events for a port that also needs arbitrary controller messages.

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

## Probe 7: broad event sniffer

`NullClapMidiProbe7EventSniffer.clap` advertises CLAP, MIDI1, and MIDI2 note dialects plus the audio-effect, instrument, note-effect, and stereo roles. Its native Windows GUI reports every non-parameter core event received by the plug-in.

The current Bitwig result is that Probe 7's counter stays at zero while the MIDI CC device emits CC20. That means the controller message is being dropped before it reaches the plug-in's CLAP input-event queue.

## Probes 8-9: forced raw-MIDI dialect test

These two probes use new plug-in IDs so Bitwig cannot reuse Probe 7's cached port metadata. They are otherwise the same event sniffer and use the same native Windows GUI.

8. `NullClapMidiProbe8Midi1Only.clap`
   - supports **only `CLAP_NOTE_DIALECT_MIDI`**
   - preferred dialect is MIDI1
   - forces the host to either deliver raw MIDI1 or provide no compatible note port

9. `NullClapMidiProbe9MidiMpeOnly.clap`
   - supports **only `CLAP_NOTE_DIALECT_MIDI_MPE`**
   - preferred dialect is MIDI-MPE
   - this matters because the official CLAP template advertises MIDI-MPE rather than plain MIDI1 alongside native CLAP notes, and MIDI-MPE still uses `CLAP_EVENT_MIDI` for controller messages

### Test procedure

1. Replace the previous diagnostic package and rescan plug-ins in Bitwig.
2. Place Bitwig's MIDI CC device immediately before Probe 8.
3. Open the probe GUI and move CC20 repeatedly between 0 and 127.
4. Record whether `Event count` stays at 0 or increments, and the `Last event` value if it increments.
5. Repeat with Probe 9.

Interpretation:

- **Probe 8 works, Probe 9 fails:** Bitwig accepts plain MIDI1 but not the MPE dialect for this route.
- **Probe 9 works, Probe 8 fails:** the missing MIDI-MPE dialect was the compatibility bug; GlitchDeck should advertise MIDI-MPE.
- **Both work:** Bitwig's negotiation of a multi-dialect port is what was dropping CC, so GlitchDeck should force a raw-MIDI dialect instead of advertising native CLAP notes on that port.
- **Both stay at 0:** dialect negotiation is not the cause. The next diagnostic should bypass null-clap's framework layer with a direct C-ABI probe and/or compare against a known Bitwig-working CLAP implementation.

For all event sniffers:

- any non-parameter core CLAP event sets the output to 50% gain;
- raw MIDI1/MPE CC20 maps 0-63 to unity and 64-127 to 25% gain;
- `Event count` increments for every non-parameter core input event;
- `Last event` identifies the CLAP core event type (`10` = raw MIDI1; `12` = raw MIDI2).

Do **not** adjust `Diagnostic Sentinel (ignore)`. It is an inert hidden parameter included only because clap-validator 0.4.1 crashes when a plug-in implements the parameter extension with zero parameters.

All probes are required to pass clap-validator before being packaged.
