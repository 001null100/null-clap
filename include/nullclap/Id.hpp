#pragma once

#include <clap/id.h>
#include <cstdint>
#include <string_view>

namespace nullclap
{
constexpr clap_id stableId(std::string_view text) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char c : text)
    {
        hash ^= c;
        hash *= 16777619u;
    }

    return hash == CLAP_INVALID_ID ? hash - 1u : hash;
}
} // namespace nullclap
