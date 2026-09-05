#include <nullclap/State.hpp>
#include "TestCheck.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <utility>

namespace
{
struct MemoryStream
{
    std::vector<std::byte> data;
    std::size_t cursor = 0;
    std::uint64_t chunk = 3;
    int failure = 0; // 1: no progress, 2: error, 3: impossible byte count
    clap_ostream_t output {};
    clap_istream_t input {};

    explicit MemoryStream(std::vector<std::byte> bytes = {}) : data(std::move(bytes))
    {
        output.ctx = this;
        output.write = [](const clap_ostream_t* stream, const void* source, std::uint64_t size) -> std::int64_t {
            auto& self = *static_cast<MemoryStream*>(stream->ctx);
            if (self.failure != 0)
                return self.failure == 1 ? 0 : self.failure == 2 ? -1 : static_cast<std::int64_t>(size + 1);
            const auto count = static_cast<std::size_t>(std::min(size, self.chunk));
            const auto* bytes = static_cast<const std::byte*>(source);
            self.data.insert(self.data.end(), bytes, bytes + count);
            return static_cast<std::int64_t>(count);
        };
        input.ctx = this;
        input.read = [](const clap_istream_t* stream, void* target, std::uint64_t size) -> std::int64_t {
            auto& self = *static_cast<MemoryStream*>(stream->ctx);
            if (self.failure != 0)
                return self.failure == 1 ? 0 : self.failure == 2 ? -1 : static_cast<std::int64_t>(size + 1);
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
                { size, self.chunk, self.data.size() - self.cursor }));
            if (count != 0)
                std::memcpy(target, self.data.data() + self.cursor, count);
            self.cursor += count;
            return static_cast<std::int64_t>(count);
        };
    }
};

void addParameter(nullclap::ParameterStore& store, clap_id id, double value)
{
    CHECK(store.add(nullclap::ParameterSpec::continuous(id, "Test", "", 0.0, 1.0, value)));
}

void putU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255u);
}
}

int main()
{
    nullclap::ParameterStore source;
    addParameter(source, 1, 0.75);
    addParameter(source, 99, 0.5); // Unknown to the destination: migration remains supported.
    const std::vector<std::byte> payload { std::byte{1}, std::byte{2}, std::byte{3} };
    MemoryStream saved;
    CHECK(nullclap::state::save(source, payload, &saved.output));
    CHECK(saved.data.size() == 16 + 2 * 12 + payload.size());
    CHECK(saved.data[0] == std::byte{'N'} && saved.data[3] == std::byte{'P'});
    CHECK(saved.data[4] == std::byte{1}); // Existing little-endian v1 envelope.

    nullclap::ParameterStore destination;
    addParameter(destination, 1, 0.125);
    addParameter(destination, 2, 0.25); // Missing in incoming state: keep its value.
    const std::vector<std::byte> sentinel { std::byte{0x7f} };
    std::vector<std::byte> extra = sentinel;

    // Fail at every byte boundary, including after all parameters but before the
    // last extra-state byte. Neither live parameters nor caller output may change.
    for (std::size_t length = 0; length < saved.data.size(); ++length)
    {
        MemoryStream truncated(std::vector<std::byte>(saved.data.begin(), saved.data.begin() + length));
        CHECK(!nullclap::state::load(destination, extra, &truncated.input));
        CHECK(destination.value(1) == 0.125);
        CHECK(destination.value(2) == 0.25);
        CHECK(extra == sentinel);
    }

    for (int fault = 0; fault < 6; ++fault)
    {
        auto malformed = saved.data;
        if (fault == 0) malformed[0] = std::byte{0};
        if (fault == 1) putU32(malformed, 4, 999);
        if (fault == 2) putU32(malformed, 8, 0xffffffffu);
        if (fault == 3) putU32(malformed, 12, 0xffffffffu);
        if (fault >= 4)
        {
            const double value = fault == 4 ? std::numeric_limits<double>::quiet_NaN()
                                            : std::numeric_limits<double>::infinity();
            const auto bits = std::bit_cast<std::uint64_t>(value);
            for (unsigned i = 0; i < 8; ++i)
                malformed[20 + i] = static_cast<std::byte>((bits >> (8 * i)) & 255u);
        }
        MemoryStream stream(std::move(malformed));
        CHECK(!nullclap::state::load(destination, extra, &stream.input));
        CHECK(destination.value(1) == 0.125);
        CHECK(extra == sentinel);
    }

    for (int failure = 1; failure <= 3; ++failure)
    {
        MemoryStream broken(saved.data);
        broken.failure = failure;
        CHECK(!nullclap::state::load(destination, extra, &broken.input));
        CHECK(!nullclap::state::save(source, payload, &broken.output));
        CHECK(destination.value(1) == 0.125);
        CHECK(extra == sentinel);
    }
    CHECK(!nullclap::state::load(destination, extra, nullptr));
    CHECK(!nullclap::state::save(source, payload, nullptr));

    MemoryStream restored(saved.data);
    restored.chunk = 1;
    CHECK(nullclap::state::load(destination, extra, &restored.input));
    CHECK(destination.value(1) == 0.75);
    CHECK(destination.value(2) == 0.25);
    CHECK(extra == payload);
    CHECK(destination.count() == 2);
    return 0;
}
