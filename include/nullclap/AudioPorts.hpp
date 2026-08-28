#pragma once

#include <clap/ext/audio-ports.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace nullclap
{
struct AudioPortSpec
{
    clap_id id = CLAP_INVALID_ID;
    std::string name;
    std::uint32_t channelCount = 2;
    std::uint32_t flags = 0;
    std::string portType = CLAP_PORT_STEREO;
    clap_id inPlacePair = CLAP_INVALID_ID;

    static AudioPortSpec stereo(clap_id id, std::string name, bool main = false)
    {
        AudioPortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.channelCount = 2;
        result.flags = main ? CLAP_AUDIO_PORT_IS_MAIN : 0u;
        result.portType = CLAP_PORT_STEREO;
        return result;
    }

    static AudioPortSpec mono(clap_id id, std::string name, bool main = false)
    {
        AudioPortSpec result;
        result.id = id;
        result.name = std::move(name);
        result.channelCount = 1;
        result.flags = main ? CLAP_AUDIO_PORT_IS_MAIN : 0u;
        result.portType = CLAP_PORT_MONO;
        return result;
    }
};

class AudioPorts
{
public:
    void addInput(AudioPortSpec spec) { inputs_.push_back(std::move(spec)); }
    void addOutput(AudioPortSpec spec) { outputs_.push_back(std::move(spec)); }

    std::uint32_t count(bool isInput) const noexcept
    {
        return static_cast<std::uint32_t>(isInput ? inputs_.size() : outputs_.size());
    }

    bool info(std::uint32_t index, bool isInput, clap_audio_port_info_t& out) const noexcept
    {
        const auto& list = isInput ? inputs_ : outputs_;
        if (index >= list.size())
            return false;

        const auto& spec = list[index];
        out = {};
        out.id = spec.id;
        std::strncpy(out.name, spec.name.c_str(), CLAP_NAME_SIZE - 1);
        out.name[CLAP_NAME_SIZE - 1] = '\0';
        out.flags = spec.flags;
        out.channel_count = spec.channelCount;
        out.port_type = spec.portType.empty() ? nullptr : spec.portType.c_str();
        out.in_place_pair = spec.inPlacePair;
        return true;
    }

private:
    std::vector<AudioPortSpec> inputs_;
    std::vector<AudioPortSpec> outputs_;
};
} // namespace nullclap
