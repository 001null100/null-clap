#include <nullclap/State.hpp>
#include "TestCheck.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <new>

namespace { int allocationsUntilFailure = -1; }

void* operator new(std::size_t size)
{
    if (allocationsUntilFailure == 0)
    {
        allocationsUntilFailure = -1;
        throw std::bad_alloc();
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
    nullclap::ParameterStore store;
    CHECK(store.add(nullclap::ParameterSpec::continuous(1, "Test", "", 0.0, 1.0, 0.75)));
    struct Buffer { std::array<std::byte, 128> bytes {}; std::size_t size = 0, cursor = 0; } buffer;
    clap_ostream_t output { &buffer, [](const clap_ostream_t* stream, const void* data, std::uint64_t size) -> std::int64_t {
        auto& self = *static_cast<Buffer*>(stream->ctx);
        if (size > self.bytes.size() - self.size) return -1;
        std::memcpy(self.bytes.data() + self.size, data, static_cast<std::size_t>(size));
        self.size += static_cast<std::size_t>(size);
        return static_cast<std::int64_t>(size);
    }};
    clap_istream_t input { &buffer, [](const clap_istream_t* stream, void* data, std::uint64_t size) -> std::int64_t {
        auto& self = *static_cast<Buffer*>(stream->ctx);
        const auto count = std::min<std::uint64_t>(size, self.size - self.cursor);
        if (count) std::memcpy(data, self.bytes.data() + self.cursor, static_cast<std::size_t>(count));
        self.cursor += static_cast<std::size_t>(count);
        return static_cast<std::int64_t>(count);
    }};
    const std::array payload { std::byte{0x42} };
    allocationsUntilFailure = 0;
    CHECK(!nullclap::state::save(store, payload, &output));
    CHECK(allocationsUntilFailure == -1);
    CHECK(buffer.size == 0);
    CHECK(nullclap::state::save(store, payload, &output));

    CHECK(store.setBaseValue(1, 0.25));
    std::vector<std::byte> extra { std::byte{0x7f} };
    for (int allocation : { 0, 1 })
    {
        buffer.cursor = 0;
        allocationsUntilFailure = allocation;
        CHECK(!nullclap::state::load(store, extra, &input));
        CHECK(allocationsUntilFailure == -1);
        CHECK(store.value(1) == 0.25);
        CHECK(extra.size() == 1 && extra[0] == std::byte{0x7f});
    }
    buffer.cursor = 0;
    CHECK(nullclap::state::load(store, extra, &input));
    CHECK(store.value(1) == 0.75);
    CHECK(extra.size() == 1 && extra[0] == std::byte{0x42});
    return 0;
}
