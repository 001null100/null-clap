#pragma once

#include <nullclap/NullClap.hpp>

class MinimalEffect final : public nullclap::Plugin
{
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept;
    explicit MinimalEffect(const clap_host_t* host);

private:
    static constexpr clap_id gainId = nullclap::stableId("minimal.gain-db");

    void processAudio(const clap_process_t& process,
                      std::uint32_t startFrame,
                      std::uint32_t endFrame) noexcept override;
};
