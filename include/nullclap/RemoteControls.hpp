#pragma once

#include <clap/ext/remote-controls.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace nullclap
{
struct RemoteControlPage
{
    clap_id id = CLAP_INVALID_ID;
    std::string section;
    std::string name;
    std::array<clap_id, CLAP_REMOTE_CONTROLS_COUNT> parameters {};
    bool forPreset = false;

    RemoteControlPage()
    {
        parameters.fill(CLAP_INVALID_ID);
    }
};

class RemoteControls
{
public:
    void add(RemoteControlPage page) { pages_.push_back(std::move(page)); }
    std::uint32_t count() const noexcept { return static_cast<std::uint32_t>(pages_.size()); }

    bool get(std::uint32_t index, clap_remote_controls_page_t& out) const noexcept
    {
        if (index >= pages_.size())
            return false;

        const auto& source = pages_[index];
        out = {};
        out.page_id = source.id;
        out.is_for_preset = source.forPreset;
        std::strncpy(out.section_name, source.section.c_str(), CLAP_NAME_SIZE - 1);
        std::strncpy(out.page_name, source.name.c_str(), CLAP_NAME_SIZE - 1);
        out.section_name[CLAP_NAME_SIZE - 1] = '\0';
        out.page_name[CLAP_NAME_SIZE - 1] = '\0';
        std::copy(source.parameters.begin(), source.parameters.end(), out.param_ids);
        return true;
    }

private:
    std::vector<RemoteControlPage> pages_;
};
} // namespace nullclap
