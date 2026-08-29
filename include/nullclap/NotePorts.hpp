#pragma once

#include <clap/ext/note-ports.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace nullclap
{
struct NotePortSpec
{
    clap_id id = CLAP_INVALID_ID;
    std::string name;
    std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI;
    std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_MIDI;

    static NotePortSpec midi(clap_id id, std::string name)
    {
        NotePortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.supportedDialects = CLAP_NOTE_DIALECT_MIDI;
        result.preferredDialect = CLAP_NOTE_DIALECT_MIDI;
        return result;
    }

    // Controller-oriented input for applications that also accept native CLAP
    // note events. Raw-MIDI input ports are normalized to advertise MIDI_MPE in
    // NotePorts::info(), so callers do not need to opt into that compatibility
    // flag individually.
    static NotePortSpec controllerInput(clap_id id, std::string name)
    {
        NotePortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
        result.preferredDialect = CLAP_NOTE_DIALECT_MIDI;
        return result;
    }

    static NotePortSpec dialects(clap_id id,
                                 std::string name,
                                 std::uint32_t supported,
                                 std::uint32_t preferred)
    {
        NotePortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.supportedDialects = supported;
        result.preferredDialect = preferred;
        return result;
    }
};

class NotePorts
{
public:
    void addInput(NotePortSpec spec) { inputs_.push_back(std::move(spec)); }
    void addOutput(NotePortSpec spec) { outputs_.push_back(std::move(spec)); }

    std::uint32_t count(bool isInput) const noexcept
    {
        return static_cast<std::uint32_t>(isInput ? inputs_.size() : outputs_.size());
    }

    bool info(std::uint32_t index, bool isInput, clap_note_port_info_t& out) const noexcept
    {
        const auto& list = isInput ? inputs_ : outputs_;
        if (index >= list.size())
            return false;

        const auto& spec = list[index];
        auto supportedDialects = spec.supportedDialects;

        // MIDI-MPE uses the same raw MIDI channel-voice wire format. Hosts such
        // as Bitwig have historically gated some controller traffic on this flag,
        // so a raw-MIDI input can safely advertise it without changing application
        // event parsing. Output ports remain exactly as declared by the consumer.
        if (isInput && (supportedDialects & CLAP_NOTE_DIALECT_MIDI) != 0)
            supportedDialects |= CLAP_NOTE_DIALECT_MIDI_MPE;

        if (spec.id == CLAP_INVALID_ID || supportedDialects == 0
            || (supportedDialects & spec.preferredDialect) == 0)
            return false;

        out = {};
        out.id = spec.id;
        out.supported_dialects = supportedDialects;
        out.preferred_dialect = spec.preferredDialect;
        std::strncpy(out.name, spec.name.c_str(), CLAP_NAME_SIZE - 1);
        out.name[CLAP_NAME_SIZE - 1] = '\0';
        return true;
    }

private:
    std::vector<NotePortSpec> inputs_;
    std::vector<NotePortSpec> outputs_;
};
} // namespace nullclap
