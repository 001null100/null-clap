#include <nullclap/Plugin.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nullclap {

Plugin::Plugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : BasePlugin(descriptor, host)
{
}

void Plugin::configureParameters(ParameterList parameters)
{
    parameters_.configure(std::move(parameters));
}

void Plugin::configureAudioPorts(AudioPorts audioPorts)
{
    audioPorts_ = std::move(audioPorts);
}

void Plugin::configureRemoteControls(RemoteControls remoteControls)
{
    remoteControls_ = std::move(remoteControls);
}

void Plugin::setGuiDelegate(std::unique_ptr<GuiDelegate> guiDelegate)
{
    guiDelegate_ = std::move(guiDelegate);
}

bool Plugin::init() noexcept
{
    try
    {
        if (!onInit())
            return false;

        guiHost_.parameterValue = [this](clap_id id) { return parameterValue(id); };
        guiHost_.beginEdit = [this](clap_id id) { guiEvents_.beginGesture(id); };
        guiHost_.setValue = [this](clap_id id, double value) { guiEvents_.setValue(id, value); };
        guiHost_.endEdit = [this](clap_id id) { guiEvents_.endGesture(id); };

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool Plugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept
{
    try
    {
        sampleRate_ = sampleRate;
        minFrameCount_ = minFrameCount;
        maxFrameCount_ = maxFrameCount;
        return onActivate(sampleRate, minFrameCount, maxFrameCount);
    }
    catch (...)
    {
        return false;
    }
}

void Plugin::deactivate() noexcept
{
    try
    {
        onDeactivate();
    }
    catch (...)
    {
    }
}

bool Plugin::startProcessing() noexcept
{
    try
    {
        return onStartProcessing();
    }
    catch (...)
    {
        return false;
    }
}

void Plugin::stopProcessing() noexcept
{
    try
    {
        onStopProcessing();
    }
    catch (...)
    {
    }
}

void Plugin::reset() noexcept
{
    try
    {
        parameters_.clearModulation();
        onReset();
    }
    catch (...)
    {
    }
}

clap_process_status Plugin::process(const clap_process_t* process) noexcept
{
    if (!process)
        return CLAP_PROCESS_ERROR;

    try
    {
        flushGuiEvents(process->out_events);

        uint32_t cursor = 0;
        const auto eventCount = process->in_events ? process->in_events->size(process->in_events) : 0u;

        for (uint32_t eventIndex = 0; eventIndex < eventCount; ++eventIndex)
        {
            const auto* event = process->in_events->get(process->in_events, eventIndex);
            if (!event)
                continue;

            const auto eventTime = std::min(event->time, process->frames_count);
            if (eventTime > cursor)
            {
                processAudio(*process, cursor, eventTime);
                cursor = eventTime;
            }

            handleEvent(*event);
        }

        if (cursor < process->frames_count)
            processAudio(*process, cursor, process->frames_count);

        return CLAP_PROCESS_CONTINUE;
    }
    catch (...)
    {
        return CLAP_PROCESS_ERROR;
    }
}

void Plugin::onMainThread() noexcept
{
    try
    {
        onMainThreadCallback();
    }
    catch (...)
    {
    }
}

uint32_t Plugin::paramsCount() const noexcept
{
    return parameters_.size();
}

bool Plugin::paramsInfo(uint32_t paramIndex, clap_param_info_t* info) const noexcept
{
    return info && parameters_.fillInfo(paramIndex, *info);
}

bool Plugin::paramsValue(clap_id paramId, double* value) noexcept
{
    if (!value)
        return false;
    *value = parameters_.baseValue(paramId);
    return parameters_.find(paramId) != nullptr;
}

bool Plugin::paramsValueToText(clap_id paramId, double value, char* display, uint32_t size) noexcept
{
    if (!display || size == 0)
        return false;

    const auto* parameter = parameters_.find(paramId);
    if (!parameter)
        return false;

    if (parameter->definition.valueToText)
        return parameter->definition.valueToText(value, display, size);

    std::snprintf(display, size, "%.3f", value);
    return true;
}

bool Plugin::paramsTextToValue(clap_id paramId, const char* display, double* value) noexcept
{
    if (!display || !value)
        return false;

    const auto* parameter = parameters_.find(paramId);
    if (!parameter)
        return false;

    if (parameter->definition.textToValue)
        return parameter->definition.textToValue(display, *value);

    char* end = nullptr;
    const auto parsed = std::strtod(display, &end);
    if (end == display)
        return false;

    *value = parsed;
    return true;
}

void Plugin::paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept
{
    try
    {
        flushGuiEvents(out);
        if (!in)
            return;

        const auto count = in->size(in);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (const auto* event = in->get(in, i))
                handleEvent(*event);
        }
    }
    catch (...)
    {
    }
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
        if (!loadExtraState(extra))
            return false;

        // Loading state may change any persistent parameter value. CLAP hosts cache
        // parameter values, so they must be explicitly told to query them again.
        if (_host.canUseParams())
            _host.paramsRescan(CLAP_PARAM_RESCAN_VALUES);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

uint32_t Plugin::audioPortsCount(bool isInput) const noexcept
{
    return audioPorts_.count(isInput);
}

bool Plugin::audioPortsInfo(uint32_t index, bool isInput, clap_audio_port_info_t* info) const noexcept
{
    return info && audioPorts_.fillInfo(index, isInput, *info);
}

uint32_t Plugin::remoteControlsPageCount() noexcept
{
    return remoteControls_.count();
}

bool Plugin::remoteControlsPageGet(uint32_t pageIndex, clap_remote_controls_page_t* page) noexcept
{
    return page && remoteControls_.fillPage(pageIndex, *page);
}

bool Plugin::guiIsApiSupported(const char* api, bool isFloating) noexcept
{
    return guiDelegate_ && guiDelegate_->isApiSupported(api, isFloating);
}

bool Plugin::guiGetPreferredApi(const char** api, bool* isFloating) noexcept
{
    return guiDelegate_ && guiDelegate_->getPreferredApi(api, isFloating);
}

bool Plugin::guiCreate(const char* api, bool isFloating) noexcept
{
    return guiDelegate_ && guiDelegate_->create(api, isFloating, guiHost_);
}

void Plugin::guiDestroy() noexcept
{
    if (guiDelegate_)
        guiDelegate_->destroy();
}

bool Plugin::guiSetScale(double scale) noexcept
{
    return guiDelegate_ && guiDelegate_->setScale(scale);
}

bool Plugin::guiGetSize(uint32_t* width, uint32_t* height) noexcept
{
    return guiDelegate_ && width && height && guiDelegate_->getSize(*width, *height);
}

bool Plugin::guiCanResize() noexcept
{
    return guiDelegate_ && guiDelegate_->canResize();
}

bool Plugin::guiGetResizeHints(clap_gui_resize_hints_t* hints) noexcept
{
    return guiDelegate_ && hints && guiDelegate_->getResizeHints(*hints);
}

bool Plugin::guiAdjustSize(uint32_t* width, uint32_t* height) noexcept
{
    return guiDelegate_ && width && height && guiDelegate_->adjustSize(*width, *height);
}

bool Plugin::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    return guiDelegate_ && guiDelegate_->setSize(width, height);
}

bool Plugin::guiSetParent(const clap_window_t* window) noexcept
{
    return guiDelegate_ && window && guiDelegate_->setParent(*window);
}

bool Plugin::guiSetTransient(const clap_window_t* window) noexcept
{
    return guiDelegate_ && window && guiDelegate_->setTransient(*window);
}

void Plugin::guiSuggestTitle(const char* title) noexcept
{
    if (guiDelegate_)
        guiDelegate_->suggestTitle(title ? title : "");
}

bool Plugin::guiShow() noexcept
{
    return guiDelegate_ && guiDelegate_->show();
}

bool Plugin::guiHide() noexcept
{
    return guiDelegate_ && guiDelegate_->hide();
}

double Plugin::parameterValue(clap_id id) const noexcept
{
    return parameters_.baseValue(id);
}

double Plugin::parameterModulation(clap_id id) const noexcept
{
    return parameters_.modulation(id);
}

double Plugin::effectiveParameterValue(clap_id id) const noexcept
{
    return parameters_.effectiveValue(id);
}

void Plugin::setInternalParameterValue(clap_id id, double value) noexcept
{
    parameters_.setInternalValue(id, value);
}

void Plugin::processAudio(const clap_process_t& process, uint32_t startFrame, uint32_t endFrame) noexcept
{
    onProcessAudio(process, startFrame, endFrame);
}

void Plugin::handleEvent(const clap_event_header_t& event) noexcept
{
    if (event.space_id != CLAP_CORE_EVENT_SPACE_ID)
    {
        onEvent(event);
        return;
    }

    switch (event.type)
    {
        case CLAP_EVENT_PARAM_VALUE:
        {
            const auto& paramEvent = reinterpret_cast<const clap_event_param_value_t&>(event);
            parameters_.setBaseValue(paramEvent.param_id, paramEvent.value);
            break;
        }
        case CLAP_EVENT_PARAM_MOD:
        {
            const auto& modEvent = reinterpret_cast<const clap_event_param_mod_t&>(event);
            if (modEvent.note_id == -1 && modEvent.port_index == -1 && modEvent.channel == -1 && modEvent.key == -1)
                parameters_.setModulation(modEvent.param_id, modEvent.amount);
            else
                onEvent(event);
            break;
        }
        default:
            onEvent(event);
            break;
    }
}

void Plugin::flushGuiEvents(const clap_output_events_t* out) noexcept
{
    if (!out)
        return;

    guiEvents_.drain([&](const GuiParameterEvent& event) {
        switch (event.kind)
        {
            case GuiParameterEventKind::BeginGesture:
            {
                clap_event_param_gesture_t gesture{};
                gesture.header.size = sizeof(gesture);
                gesture.header.time = 0;
                gesture.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                gesture.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
                gesture.header.flags = 0;
                gesture.param_id = event.paramId;
                out->try_push(out, &gesture.header);
                break;
            }
            case GuiParameterEventKind::Value:
            {
                if (!parameters_.setBaseValue(event.paramId, event.value))
                    break;

                clap_event_param_value_t value{};
                value.header.size = sizeof(value);
                value.header.time = 0;
                value.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                value.header.type = CLAP_EVENT_PARAM_VALUE;
                value.header.flags = 0;
                value.param_id = event.paramId;
                value.cookie = nullptr;
                value.note_id = -1;
                value.port_index = -1;
                value.channel = -1;
                value.key = -1;
                value.value = parameters_.baseValue(event.paramId);
                out->try_push(out, &value.header);
                break;
            }
            case GuiParameterEventKind::EndGesture:
            {
                clap_event_param_gesture_t gesture{};
                gesture.header.size = sizeof(gesture);
                gesture.header.time = 0;
                gesture.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                gesture.header.type = CLAP_EVENT_PARAM_GESTURE_END;
                gesture.header.flags = 0;
                gesture.param_id = event.paramId;
                out->try_push(out, &gesture.header);
                break;
            }
        }
    });
}

} // namespace nullclap
