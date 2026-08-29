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

    // Controller-oriented input for hosts that gate channel-voice controller
    // traffic on MIDI-MPE capability. The wire format remains ordinary raw MIDI;
    // advertising MIDI_MPE simply tells the host that channelized MIDI controller
    // traffic is safe to deliver. CLAP note events are accepted as well so hosts
    // remain free to use their preferred representation for note on/off traffic.
    static NotePortSpec controllerInput(clap_id id, std::string name)
    {
        NotePortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.supportedDialects = CLAP_NOTE_DIALECT_MIDI
            | CLAP_NOTE_DIALECT_MIDI_MPE
            | CLAP_NOTE_DIALECT_CLAP;
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
        if (spec.id == CLAP_INVALID_ID || spec.supportedDialects == 0
            || (spec.supportedDialects & spec.preferredDialect) == 0)
            return false;

        out = {};
        out.id = spec.id;
        out.supported_dialects = spec.supportedDialects;
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
