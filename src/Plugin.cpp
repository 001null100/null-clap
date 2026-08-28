#include <nullclap/Plugin.hpp>

#include <algorithm>
#include <cstring>

namespace nullclap
{
Plugin::Plugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : HelperPlugin(descriptor, host)
{
}

void Plugin::setGuiDelegate(std::unique_ptr<GuiDelegate> gui) noexcept
{
    gui_ = std::move(gui);
}

bool Plugin::GuiParamQueue::push(const GuiParamEvent& event) noexcept
{
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = (head + 1) % capacity;
    if (next == tail_.load(std::memory_order_acquire))
        return false;
    events_[head] = event;
    head_.store(next, std::memory_order_release);
    return true;
}

bool Plugin::GuiParamQueue::pop(GuiParamEvent& event) noexcept
{
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire))
        return false;
    event = events_[tail];
    tail_.store((tail + 1) % capacity, std::memory_order_release);
    return true;
}

bool Plugin::pushGuiParameterEvent(const GuiParamEvent& event) noexcept
{
    if (!guiParamQueue_.push(event))
        return false;
    if (_host.canUseParams())
        _host.paramsRequestFlush();
    return true;
}

bool Plugin::beginParameterGesture(clap_id id) noexcept
{
    return parameters_.contains(id) && pushGuiParameterEvent({ GuiParamEventKind::begin, id, 0.0 });
}

bool Plugin::setParameterFromGui(clap_id id, double value) noexcept
{
    if (parameters_.isReadOnly(id) || !parameters_.setBaseValue(id, value))
        return false;
    return pushGuiParameterEvent({ GuiParamEventKind::value, id, parameters_.value(id) });
}

bool Plugin::endParameterGesture(clap_id id) noexcept
{
    return parameters_.contains(id) && pushGuiParameterEvent({ GuiParamEventKind::end, id, 0.0 });
}

bool Plugin::init() noexcept
{
    return onInit();
}

bool Plugin::activate(double sampleRate, std::uint32_t minFrames, std::uint32_t maxFrames) noexcept
{
    return onActivate(sampleRate, minFrames, maxFrames);
}

void Plugin::deactivate() noexcept
{
    onDeactivate();
}

bool Plugin::startProcessing() noexcept
{
    return onStartProcessing();
}

void Plugin::stopProcessing() noexcept
{
    onStopProcessing();
}

void Plugin::reset() noexcept
{
    onReset();
}

void Plugin::onMainThread() noexcept
{
    onMainThreadCallback();
}

void Plugin::applyInputEvent(const clap_event_header_t& event) noexcept
{
    parameters_.applyInputEvent(event);
    onEvent(event);
}

clap_process_status Plugin::process(const clap_process_t* process) noexcept
{
    if (process == nullptr)
        return CLAP_PROCESS_ERROR;

    currentOutputEvents_ = process->out_events;
    currentFrameCount_ = process->frames_count;
    drainGuiParameterEvents(process->out_events, 0);

    std::uint32_t cursor = 0;
    const auto* events = process->in_events;
    const std::uint32_t eventCount = events != nullptr && events->size != nullptr ? events->size(events) : 0;

    for (std::uint32_t index = 0; index < eventCount; ++index)
    {
        const auto* event = events->get(events, index);
        if (event == nullptr)
            continue;

        const std::uint32_t eventTime = std::clamp(event->time, cursor, process->frames_count);
        if (eventTime > cursor)
            processAudio(*process, cursor, eventTime);

        applyInputEvent(*event);
        cursor = eventTime;
    }

    if (cursor < process->frames_count)
        processAudio(*process, cursor, process->frames_count);

    currentOutputEvents_ = nullptr;
    currentFrameCount_ = 0;
    return processFinished();
}

std::uint32_t Plugin::paramsCount() const noexcept
{
    return parameters_.count();
}

bool Plugin::paramsInfo(std::uint32_t index, clap_param_info_t* info) const noexcept
{
    return info != nullptr && parameters_.info(index, *info);
}

bool Plugin::paramsValue(clap_id id, double* value) noexcept
{
    if (value == nullptr || !parameters_.contains(id))
        return false;
    *value = parameters_.value(id);
    return true;
}

bool Plugin::paramsValueToText(clap_id id, double value, char* display, std::uint32_t size) noexcept
{
    return parameters_.valueToText(id, value, display, size);
}

bool Plugin::paramsTextToValue(clap_id id, const char* display, double* value) noexcept
{
    if (value == nullptr)
        return false;
    return parameters_.textToValue(id, display, *value);
}

void Plugin::paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept
{
    if (in != nullptr && in->size != nullptr)
    {
        const auto count = in->size(in);
        for (std::uint32_t index = 0; index < count; ++index)
            if (const auto* event = in->get(in, index))
                applyInputEvent(*event);
    }
    drainGuiParameterEvents(out, 0);
}

bool Plugin::stateSave(const clap_ostream_t* stream) noexcept
{
    try
    {
        const auto extra = saveExtraState();
        return state::save(parameters_, extra, stream);
    }
    catch (...)
    {
        return false;
    }
}

bool Plugin::stateLoad(const clap_istream_t* stream) noexcept
{
    try
    {
        std::vector<std::byte> extra;
        if (!state::load(parameters_, extra, stream))
            return false;
        return loadExtraState(extra);
    }
    catch (...)
    {
        return false;
    }
}

std::uint32_t Plugin::audioPortsCount(bool isInput) const noexcept
{
    return audioPorts_.count(isInput);
}

bool Plugin::audioPortsInfo(std::uint32_t index, bool isInput, clap_audio_port_info_t* info) const noexcept
{
    return info != nullptr && audioPorts_.info(index, isInput, *info);
}

std::uint32_t Plugin::remoteControlsPageCount() noexcept
{
    return remoteControls_.count();
}

bool Plugin::remoteControlsPageGet(std::uint32_t index, clap_remote_controls_page_t* page) noexcept
{
    return page != nullptr && remoteControls_.get(index, *page);
}

bool Plugin::implementsGui() const noexcept
{
    return gui_ != nullptr;
}

bool Plugin::guiIsApiSupported(const char* api, bool floating) noexcept
{
    return gui_ != nullptr && api != nullptr && gui_->isApiSupported(api, floating);
}

bool Plugin::guiGetPreferredApi(const char** api, bool* floating) noexcept
{
    if (gui_ == nullptr || api == nullptr || floating == nullptr)
        return false;
    *api = gui_->preferredApi();
    *floating = gui_->isFloating();
    return *api != nullptr;
}

bool Plugin::guiCreate(const char* api, bool floating) noexcept
{
    return gui_ != nullptr && gui_->create(api, floating);
}

void Plugin::guiDestroy() noexcept
{
    if (gui_ != nullptr)
        gui_->destroy();
}

bool Plugin::guiSetScale(double scale) noexcept
{
    return gui_ != nullptr && gui_->setScale(scale);
}

bool Plugin::guiShow() noexcept
{
    return gui_ != nullptr && gui_->show();
}

bool Plugin::guiHide() noexcept
{
    return gui_ != nullptr && gui_->hide();
}

bool Plugin::guiGetSize(std::uint32_t* width, std::uint32_t* height) noexcept
{
    return gui_ != nullptr && width != nullptr && height != nullptr && gui_->getSize(*width, *height);
}

bool Plugin::guiCanResize() const noexcept
{
    return gui_ != nullptr && gui_->canResize();
}

bool Plugin::guiGetResizeHints(clap_gui_resize_hints_t* hints) noexcept
{
    return gui_ != nullptr && hints != nullptr && gui_->getResizeHints(*hints);
}

bool Plugin::guiAdjustSize(std::uint32_t* width, std::uint32_t* height) noexcept
{
    return gui_ != nullptr && width != nullptr && height != nullptr && gui_->adjustSize(*width, *height);
}

bool Plugin::guiSetSize(std::uint32_t width, std::uint32_t height) noexcept
{
    return gui_ != nullptr && gui_->setSize(width, height);
}

void Plugin::guiSuggestTitle(const char* title) noexcept
{
    if (gui_ != nullptr)
        gui_->suggestTitle(title);
}

bool Plugin::guiSetParent(const clap_window_t* window) noexcept
{
    return gui_ != nullptr && window != nullptr && gui_->setParent(*window);
}

bool Plugin::guiSetTransient(const clap_window_t* window) noexcept
{
    return gui_ != nullptr && window != nullptr && gui_->setTransient(*window);
}

bool Plugin::requestGuiResize(std::uint32_t width, std::uint32_t height) noexcept
{
    return _host.canUseGui() && _host.guiRequestResize(width, height);
}

void Plugin::markStateDirty() noexcept
{
    if (_host.canUseState())
        _host.stateMarkDirty();
}

bool Plugin::pushParameterValue(const clap_output_events_t* out,
                                clap_id id,
                                double value,
                                std::uint32_t sampleOffset,
                                std::uint32_t flags) noexcept
{
    if (out == nullptr || out->try_push == nullptr || !parameters_.contains(id))
        return false;

    clap_event_param_value_t event {};
    event.header.size = sizeof(event);
    event.header.time = sampleOffset;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.header.flags = flags;
    event.param_id = id;
    event.cookie = nullptr;
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = value;
    return out->try_push(out, &event.header);
}

bool Plugin::emitParameterValue(clap_id id, double value, std::uint32_t sampleOffset, std::uint32_t flags) noexcept
{
    if (currentOutputEvents_ == nullptr || sampleOffset > currentFrameCount_)
        return false;
    if (!parameters_.setInternalValue(id, value))
        return false;
    return pushParameterValue(currentOutputEvents_, id, parameters_.value(id), sampleOffset, flags);
}

void Plugin::drainGuiParameterEvents(const clap_output_events_t* out, std::uint32_t sampleOffset) noexcept
{
    if (out == nullptr || out->try_push == nullptr)
        return;

    GuiParamEvent queued;
    while (guiParamQueue_.pop(queued))
    {
        if (queued.kind == GuiParamEventKind::value)
        {
            pushParameterValue(out, queued.id, queued.value, sampleOffset, CLAP_EVENT_IS_LIVE);
            continue;
        }

        clap_event_param_gesture_t event {};
        event.header.size = sizeof(event);
        event.header.time = sampleOffset;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = queued.kind == GuiParamEventKind::begin
            ? CLAP_EVENT_PARAM_GESTURE_BEGIN
            : CLAP_EVENT_PARAM_GESTURE_END;
        event.header.flags = CLAP_EVENT_IS_LIVE;
        event.param_id = queued.id;
        out->try_push(out, &event.header);
    }
}
} // namespace nullclap
