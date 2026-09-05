#include <nullclap/State.hpp>

#include <bit>
#include <cmath>
#include <cstdint>

namespace nullclap::state
{
namespace
{
constexpr std::byte magic[] {
    static_cast<std::byte>('N'), static_cast<std::byte>('C'),
    static_cast<std::byte>('L'), static_cast<std::byte>('P')
};
// Bound both allocation and work from the untrusted count in a state stream.
constexpr std::uint32_t maximumParameterCount = 1u << 20;

bool writeBytes(const clap_ostream_t* stream, const std::byte* data, std::size_t size) noexcept
{
    if (stream == nullptr || stream->write == nullptr)
        return false;

    std::size_t written = 0;
    while (written < size)
    {
        const auto remaining = size - written;
        const auto result = stream->write(stream, data + written, static_cast<std::uint64_t>(remaining));
        if (result <= 0 || static_cast<std::uint64_t>(result) > remaining)
            return false;
        written += static_cast<std::size_t>(result);
    }
    return true;
}

bool readBytes(const clap_istream_t* stream, std::byte* data, std::size_t size) noexcept
{
    if (stream == nullptr || stream->read == nullptr)
        return false;

    std::size_t read = 0;
    while (read < size)
    {
        const auto remaining = size - read;
        const auto result = stream->read(stream, data + read, static_cast<std::uint64_t>(remaining));
        if (result <= 0 || static_cast<std::uint64_t>(result) > remaining)
            return false;
        read += static_cast<std::size_t>(result);
    }
    return true;
}

bool writeU32(const clap_ostream_t* stream, std::uint32_t value) noexcept
{
    std::byte bytes[4];
    for (int i = 0; i < 4; ++i)
        bytes[i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
    return writeBytes(stream, bytes, 4);
}

bool readU32(const clap_istream_t* stream, std::uint32_t& value) noexcept
{
    std::byte bytes[4];
    if (!readBytes(stream, bytes, 4))
        return false;
    value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(std::to_integer<unsigned int>(bytes[i])) << (i * 8);
    return true;
}

bool writeU64(const clap_ostream_t* stream, std::uint64_t value) noexcept
{
    std::byte bytes[8];
    for (int i = 0; i < 8; ++i)
        bytes[i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
    return writeBytes(stream, bytes, 8);
}

bool readU64(const clap_istream_t* stream, std::uint64_t& value) noexcept
{
    std::byte bytes[8];
    if (!readBytes(stream, bytes, 8))
        return false;
    value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(std::to_integer<unsigned int>(bytes[i])) << (i * 8);
    return true;
}
}

bool save(const ParameterStore& parameters,
          std::span<const std::byte> extraState,
          const clap_ostream_t* stream) noexcept
{
    // This public noexcept boundary owns allocations: callers cannot catch an
    // allocation failure outside it once noexcept has called std::terminate.
    try
    {
        if (extraState.size() > maximumExtraStateBytes)
            return false;

        const auto values = parameters.persistentValues();
        if (values.size() > maximumParameterCount)
            return false;

        if (!writeBytes(stream, magic, sizeof(magic))
            || !writeU32(stream, formatVersion)
            || !writeU32(stream, static_cast<std::uint32_t>(values.size()))
            || !writeU32(stream, static_cast<std::uint32_t>(extraState.size())))
            return false;

        for (const auto& value : values)
        {
            if (!writeU32(stream, value.id)
                || !writeU64(stream, std::bit_cast<std::uint64_t>(value.value)))
                return false;
        }

        return extraState.empty() || writeBytes(stream, extraState.data(), extraState.size());
    }
    catch (...)
    {
        return false;
    }
}

bool load(ParameterStore& parameters,
          std::vector<std::byte>& extraState,
          const clap_istream_t* stream) noexcept
{
    try
    {
        std::byte incomingMagic[4];
        if (!readBytes(stream, incomingMagic, sizeof(incomingMagic)))
            return false;
        for (std::size_t i = 0; i < sizeof(magic); ++i)
            if (incomingMagic[i] != magic[i])
                return false;

        std::uint32_t version = 0;
        std::uint32_t parameterCount = 0;
        std::uint32_t extraSize = 0;
        if (!readU32(stream, version) || version != formatVersion
            || !readU32(stream, parameterCount) || parameterCount > maximumParameterCount
            || !readU32(stream, extraSize) || extraSize > maximumExtraStateBytes)
            return false;

        // Decode completely before touching live parameters or the caller's extra
        // state. A truncated stream must not leave a half-loaded preset behind.
        std::vector<ParameterStore::SavedValue> incomingValues;
        incomingValues.reserve(parameterCount);
        for (std::uint32_t i = 0; i < parameterCount; ++i)
        {
            std::uint32_t id = 0;
            std::uint64_t valueBits = 0;
            if (!readU32(stream, id) || !readU64(stream, valueBits))
                return false;
            const double value = std::bit_cast<double>(valueBits);
            if (!std::isfinite(value))
                return false;
            incomingValues.push_back({ id, value });
        }

        std::vector<std::byte> incomingExtra(extraSize);
        if (!incomingExtra.empty() && !readBytes(stream, incomingExtra.data(), incomingExtra.size()))
            return false;

        for (const auto& value : incomingValues)
            parameters.restorePersistentValue(value.id, value.value);
        extraState.swap(incomingExtra);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace nullclap::state
