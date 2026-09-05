#include <nullclap/Factory.hpp>
#include <nullclap/Plugin.hpp>
#include "TestCheck.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace
{
constexpr clap_id gainId = 1;
constexpr clap_id meterId = 2;

struct Host
{
    bool audioThread = false;
    bool advertiseParams = true;
    unsigned flushRequests = 0, callbackRequests = 0, processRequests = 0, rescans = 0;
    clap_param_rescan_flags lastRescan = 0;
    clap_host_t api {};
    clap_host_params_t params {};
    clap_host_thread_check_t threads {};

    Host()
    {
        api = { CLAP_VERSION, this, "null-clap tests", "null-clap", "https://github.com/001null100/null-clap", "1",
            [](const clap_host_t* host, const char* id) -> const void* {
                auto& self = *static_cast<Host*>(host->host_data);
                if (std::strcmp(id, CLAP_EXT_PARAMS) == 0 && self.advertiseParams) return &self.params;
                if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0) return &self.threads;
                return nullptr;
            },
            [](const clap_host_t*) {},
            [](const clap_host_t* host) { ++static_cast<Host*>(host->host_data)->processRequests; },
            [](const clap_host_t* host) { ++static_cast<Host*>(host->host_data)->callbackRequests; }
        };
        params.rescan = [](const clap_host_t* host, clap_param_rescan_flags flags) {
            auto& self = *static_cast<Host*>(host->host_data);
            CHECK(!self.audioThread);
            ++self.rescans;
            self.lastRescan = flags;
        };
        params.clear = [](const clap_host_t*, clap_id, clap_param_clear_flags) {};
        params.request_flush = [](const clap_host_t* host) {
            auto& self = *static_cast<Host*>(host->host_data);
            CHECK(!self.audioThread);
            ++self.flushRequests;
        };
        threads.is_main_thread = [](const clap_host_t* host) { return !static_cast<Host*>(host->host_data)->audioThread; };
        threads.is_audio_thread = [](const clap_host_t* host) { return static_cast<Host*>(host->host_data)->audioThread; };
    }
};

struct Input
{
    std::array<const clap_event_header_t*, 8> events {};
    std::uint32_t count = 0;
    clap_input_events_t api {};
    Input()
    {
        api.ctx = this;
        api.size = [](const clap_input_events_t* list) { return static_cast<const Input*>(list->ctx)->count; };
        api.get = [](const clap_input_events_t* list, std::uint32_t index) -> const clap_event_header_t* {
            const auto& self = *static_cast<const Input*>(list->ctx);
            return index < self.count ? self.events[index] : nullptr;
        };
    }
};

struct Output
{
    struct Event { std::uint16_t type; std::uint32_t time, flags; clap_id id; double value; };
    std::array<Event, 512> events {};
    std::size_t count = 0, capacity = events.size(), attempts = 0;
    void (*onAccepted)(void*) = nullptr;
    void* callbackData = nullptr;
    clap_output_events_t api {};
    Output()
    {
        api.ctx = this;
        api.try_push = [](const clap_output_events_t* list, const clap_event_header_t* event) {
            auto& self = *static_cast<Output*>(list->ctx);
            ++self.attempts;
            if (self.count >= self.capacity) return false;
            auto& record = self.events[self.count++];
            record.type = event->type;
            record.time = event->time;
            record.flags = event->flags;
            if (event->type == CLAP_EVENT_PARAM_VALUE)
            {
                const auto& value = *reinterpret_cast<const clap_event_param_value_t*>(event);
                record.id = value.param_id;
                record.value = value.value;
                CHECK(value.note_id == -1 && value.port_index == -1 && value.channel == -1 && value.key == -1);
            }
            else
                record.id = reinterpret_cast<const clap_event_param_gesture_t*>(event)->param_id;
            if (self.onAccepted) self.onAccepted(self.callbackData);
            return true;
        };
    }
};

struct Bytes
{
    std::vector<std::byte> data;
    std::size_t cursor = 0;
    clap_ostream_t output {};
    clap_istream_t input {};
    Bytes()
    {
        output.ctx = this;
        output.write = [](const clap_ostream_t* stream, const void* data, std::uint64_t count) -> std::int64_t {
            auto& self = *static_cast<Bytes*>(stream->ctx);
            const auto* bytes = static_cast<const std::byte*>(data);
            self.data.insert(self.data.end(), bytes, bytes + static_cast<std::size_t>(count));
            return static_cast<std::int64_t>(count);
        };
        input.ctx = this;
        input.read = [](const clap_istream_t* stream, void* data, std::uint64_t count) -> std::int64_t {
            auto& self = *static_cast<Bytes*>(stream->ctx);
            const auto available = std::min<std::uint64_t>(count, self.data.size() - self.cursor);
            if (available) std::memcpy(data, self.data.data() + self.cursor, static_cast<std::size_t>(available));
            self.cursor += static_cast<std::size_t>(available);
            return static_cast<std::int64_t>(available);
        };
    }
};

class Probe final : public nullclap::Plugin
{
public:
    static inline Probe* instance = nullptr;
    struct Span { std::uint32_t begin, end; double value; };
    std::array<Span, 32> spans {};
    std::size_t spanCount = 0, eventCount = 0;
    int loadMode = 0; // 0 accept, 1 reject, 2 throw
    double valueSeenWhileLoading = -1;
    bool emitAtEnd = false, lastFrameAccepted = false, pastEndAccepted = false;

    static const clap_plugin_descriptor_t& descriptor() noexcept
    {
        static const char* const features[] { CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr };
        static const clap_plugin_descriptor_t description {
            CLAP_VERSION, "dev.nullclap.tests.contract", "Contract probe", "null-clap",
            "https://github.com/001null100/null-clap", "", "", "1", "Contract regression probe", features
        };
        return description;
    }

    explicit Probe(const clap_host_t* host) : Plugin(&descriptor(), host)
    {
        instance = this;
        CHECK(parameters().add(nullclap::ParameterSpec::continuous(gainId, "Gain", "", 0.0, 1.0, 0.25)));
        auto meter = nullclap::ParameterSpec::continuous(meterId, "Meter", "", 0.0, 1.0, 0.0, CLAP_PARAM_IS_READONLY);
        meter.persistent = false;
        CHECK(parameters().add(std::move(meter)));
    }
    ~Probe() override { instance = nullptr; }
    const clap_process_t* context() const noexcept { return currentProcess(); }

private:
    void processAudio(const clap_process_t& process, std::uint32_t begin, std::uint32_t end) noexcept override
    {
        CHECK(currentProcess() == &process);
        CHECK(spanCount < spans.size());
        spans[spanCount++] = { begin, end, parameters().effectiveValue(gainId) };
    }
    void onEvent(const clap_event_header_t&) noexcept override { ++eventCount; }
    clap_process_status processFinished() noexcept override
    {
        if (emitAtEnd)
        {
            lastFrameAccepted = emitParameterValue(meterId, 0.5, currentProcess()->frames_count - 1);
            pastEndAccepted = emitParameterValue(meterId, 0.9, currentProcess()->frames_count);
        }
        return CLAP_PROCESS_CONTINUE;
    }
    std::vector<std::byte> saveExtraState() const override { return { std::byte{0x42} }; }
    bool loadExtraState(std::span<const std::byte> bytes) override
    {
        valueSeenWhileLoading = parameters().value(gainId);
        if (loadMode == 2) throw std::runtime_error("injected application-state failure");
        return loadMode == 0 && bytes.size() == 1 && bytes[0] == std::byte{0x42};
    }
};

struct Fixture
{
    Host host;
    const clap_plugin_t* plugin = nullptr;
    Probe* probe = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    explicit Fixture(bool hostParams = true)
    {
        host.advertiseParams = hostParams;
        const auto* factory = nullclap::SinglePluginFactory<Probe>::get();
        plugin = factory->create_plugin(factory, &host.api, Probe::descriptor().id);
        CHECK(plugin != nullptr);
        probe = Probe::instance;
        CHECK(plugin->init(plugin));
        params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        CHECK(params != nullptr && state != nullptr);
    }
    ~Fixture() { plugin->destroy(plugin); }
};

clap_event_param_value_t valueEvent(std::uint32_t time, double value)
{
    clap_event_param_value_t event {};
    event.header = { sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 };
    event.param_id = gainId;
    event.note_id = -1; event.port_index = -1; event.channel = -1; event.key = -1;
    event.value = value;
    return event;
}

void testQueue()
{
    Fixture f;
    Input empty;
    CHECK(!f.probe->beginParameterGesture(meterId));
    CHECK(!f.probe->setParameterFromGui(meterId, 0.5));
    CHECK(!f.probe->endParameterGesture(meterId));
    CHECK(f.probe->beginParameterGesture(gainId));
    CHECK(f.probe->setParameterFromGui(gainId, 0.75));
    CHECK(f.probe->endParameterGesture(gainId));

    Output first; first.capacity = 1;
    f.params->flush(f.plugin, &empty.api, &first.api);
    CHECK(first.count == 1 && first.attempts == 2);
    CHECK(first.events[0].type == CLAP_EVENT_PARAM_GESTURE_BEGIN);
    CHECK(f.host.callbackRequests == 1);
    const auto requests = f.host.flushRequests;
    f.plugin->on_main_thread(f.plugin);
    CHECK(f.host.flushRequests == requests + 1);

    Output second; second.capacity = 1;
    f.params->flush(f.plugin, &empty.api, &second.api);
    CHECK(second.count == 1 && second.attempts == 2);
    CHECK(second.events[0].type == CLAP_EVENT_PARAM_VALUE && second.events[0].value == 0.75);
    f.plugin->on_main_thread(f.plugin);
    Output third;
    f.params->flush(f.plugin, &empty.api, &third.api);
    CHECK(third.count == 1 && third.events[0].type == CLAP_EVENT_PARAM_GESTURE_END);
    Output noDuplicates;
    f.params->flush(f.plugin, &empty.api, &noDuplicates.api);
    CHECK(noDuplicates.count == 0);

    for (unsigned i = 0; i < 255; ++i) CHECK(f.probe->setParameterFromGui(gainId, 0.5));
    CHECK(!f.probe->setParameterFromGui(gainId, 0.875));
    CHECK(f.probe->parameters().value(gainId) == 0.5);
    Output full;
    f.params->flush(f.plugin, &empty.api, &full.api);
    CHECK(full.count == 255);
    CHECK(f.probe->beginParameterGesture(gainId));
    CHECK(f.probe->setParameterFromGui(gainId, 0.25));
    CHECK(f.probe->endParameterGesture(gainId));
    Output wrapped;
    f.params->flush(f.plugin, &empty.api, &wrapped.api);
    CHECK(wrapped.count == 3 && wrapped.events[1].value == 0.25);

    // Simulate a producer refilling during delivery. Only the initial work budget
    // may drain in this callback, even though another event becomes available.
    CHECK(f.probe->setParameterFromGui(gainId, 0.5));
    Output refill;
    refill.callbackData = f.probe;
    refill.onAccepted = [](void* data) { CHECK(static_cast<Probe*>(data)->setParameterFromGui(gainId, 0.75)); };
    f.params->flush(f.plugin, &empty.api, &refill.api);
    CHECK(refill.count == 1);
    Output remaining;
    f.params->flush(f.plugin, &empty.api, &remaining.api);
    CHECK(remaining.count == 1 && remaining.events[0].value == 0.75);
}

void testStateRollback()
{
    Fixture f;
    CHECK(f.probe->parameters().setBaseValue(gainId, 0.75));
    Bytes saved;
    CHECK(f.state->save(f.plugin, &saved.output));
    CHECK(f.probe->parameters().setBaseValue(gainId, 0.25));
    for (int mode : { 1, 2 })
    {
        f.probe->loadMode = mode;
        saved.cursor = 0;
        CHECK(!f.state->load(f.plugin, &saved.input));
        CHECK(f.probe->valueSeenWhileLoading == 0.75);
        CHECK(f.probe->parameters().value(gainId) == 0.25);
        CHECK(f.host.rescans == 0);
    }
    f.probe->loadMode = 0;
    saved.cursor = 0;
    CHECK(f.state->load(f.plugin, &saved.input));
    CHECK(f.probe->parameters().value(gainId) == 0.75);
    CHECK(f.host.rescans == 1 && f.host.lastRescan == CLAP_PARAM_RESCAN_VALUES);
}

void testProcess()
{
    Fixture f;
    CHECK(f.plugin->activate(f.plugin, 48000.0, 1, 64));
    f.host.audioThread = true;
    CHECK(f.plugin->start_processing(f.plugin));
    auto first = valueEvent(3, 0.5);
    auto sameTime = valueEvent(3, 0.75);
    auto last = valueEvent(7, 0.875);
    auto outside = valueEvent(16, 0.125);
    Input input;
    input.events = { &first.header, &sameTime.header, &last.header, &outside.header };
    input.count = 3;
#if defined(NDEBUG)
    // Deliberately malformed host traffic is tested with the production helper
    // policy; maximal Debug helper checks may reject it before our boundary.
    input.count = 4;
#endif
    Output output;
    clap_process_t block {};
    block.frames_count = 16;
    block.in_events = &input.api;
    block.out_events = &output.api;
    f.probe->emitAtEnd = true;
    CHECK(f.plugin->process(f.plugin, &block) == CLAP_PROCESS_CONTINUE);
    CHECK(f.probe->context() == nullptr);
    CHECK(f.probe->spanCount == 3 && f.probe->eventCount == 3);
    CHECK(f.probe->spans[0].begin == 0 && f.probe->spans[0].end == 3 && f.probe->spans[0].value == 0.25);
    CHECK(f.probe->spans[1].begin == 3 && f.probe->spans[1].end == 7 && f.probe->spans[1].value == 0.75);
    CHECK(f.probe->spans[2].begin == 7 && f.probe->spans[2].end == 16 && f.probe->spans[2].value == 0.875);
    CHECK(f.probe->lastFrameAccepted && !f.probe->pastEndAccepted);
    CHECK(f.probe->parameters().value(meterId) == 0.5);
    CHECK(output.count == 1 && output.events[0].time == 15);

#if defined(NDEBUG)
    clap_event_header_t shortMidi { sizeof(clap_event_header_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0 };
    input.events[0] = &shortMidi;
    input.count = 1;
    f.probe->emitAtEnd = false;
    f.probe->spanCount = 0;
    f.probe->eventCount = 0;
    CHECK(f.plugin->process(f.plugin, &block) == CLAP_PROCESS_CONTINUE);
    CHECK(f.probe->eventCount == 0 && f.probe->spanCount == 1);
    input.api.get = nullptr;
    f.probe->spanCount = 0;
    CHECK(f.plugin->process(f.plugin, &block) == CLAP_PROCESS_CONTINUE);
    CHECK(f.probe->spanCount == 1);
#endif

    // Reject output on the audio thread: retry must request a main-thread
    // callback, never call the host's main/non-audio request_flush directly.
    f.host.audioThread = false;
    CHECK(f.probe->setParameterFromGui(gainId, 0.5));
    f.host.audioThread = true;
    Input empty;
    Output blocked; blocked.capacity = 0;
    block.in_events = &empty.api;
    block.out_events = &blocked.api;
    f.probe->emitAtEnd = false;
    f.probe->spanCount = 0;
    const auto flushes = f.host.flushRequests;
    CHECK(f.plugin->process(f.plugin, &block) == CLAP_PROCESS_CONTINUE);
    CHECK(f.host.callbackRequests == 1 && f.host.flushRequests == flushes);
    f.plugin->stop_processing(f.plugin);
    f.host.audioThread = false;
    f.plugin->deactivate(f.plugin);
    f.plugin->on_main_thread(f.plugin);
    CHECK(f.host.flushRequests == flushes + 1);
    Output retry;
    f.params->flush(f.plugin, &empty.api, &retry.api);
    CHECK(retry.count == 1 && retry.events[0].value == 0.5);
}
}

int main()
{
    testQueue();
    testStateRollback();
    testProcess();
    {
        Fixture f(false);
        CHECK(f.probe->setParameterFromGui(gainId, 0.5));
        CHECK(f.host.processRequests == 1);
    }
    return 0;
}
