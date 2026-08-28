#pragma once

#include "ParameterStore.hpp"

#include <clap/ext/state.h>
#include <cstddef>
#include <span>
#include <vector>

namespace nullclap::state
{
constexpr std::uint32_t formatVersion = 1;
constexpr std::size_t maximumExtraStateBytes = 16u * 1024u * 1024u;

bool save(const ParameterStore& parameters,
          std::span<const std::byte> extraState,
          const clap_ostream_t* stream) noexcept;

bool load(ParameterStore& parameters,
          std::vector<std::byte>& extraState,
          const clap_istream_t* stream) noexcept;
} // namespace nullclap::state
