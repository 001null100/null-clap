#pragma once

#include "AudioPorts.hpp"
#include "Gui.hpp"
#include "NotePorts.hpp"
#include "ParameterStore.hpp"
#include "RemoteControls.hpp"
#include "State.hpp"

#include <array>
#include <atomic>
#include <clap/helpers/plugin.hh>
#include <clap/helpers/plugin.hxx>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace nullclap
{
#if defined(NDEBUG)
using HelperPlugin = clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore,
                                           clap::helpers::CheckingLevel::Minimal>;
#else
using HelperPlugin = clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Terminate,
                                           clap::helpers::CheckingLevel::Maximal>;
#endif

class Plugin : public HelperPlugin
{
public:
    Plugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~Plugin() override = default;

    ParameterStore& parameters() noexcept { return parameters_; }
    const ParameterStore& parameters() const noexcept { return parameters_; }
    AudioPorts& audioPorts() noexcept { return audioPorts_; }
    NotePorts& notePorts() noexcept { return notePorts_; }
    RemoteControls& remoteControls() noexcept { return remoteControls_; }

    void setGuiDelegate(std::unique_ptr<GuiDelegate> gui) noexcept;

    // Single-producer main/UI-thread API. Accepted values update locally at once.
    // False means the edit was not accepted; callers must retry rather than assume
    // delivery. In particular, do not abandon a rejected gesture-end request.
    bool beginParameterGesture(clap_id id) noexcept;
    bool setParameterFromGui(clap_id id, double value) noexcept;
    bool endParameterGesture(clap_id id) noexcept;

protected:
    virtual bool onInit() noexcept { return true; }
    virtual bool onActivate(double, std::uint32_t, std::uint32_t) noexcept { return true; }
    virtual void onDeactivate() noexcept {}
    virtual bool onStartProcessing() noexcept { return true; }
    virtual void onStopProcessing() noexcept {}
    virtual void onReset() noexcept {}
    virtual void onMainThreadCallback() noexcept {}

    // Called for each contiguous sample span between input events. Parameter state
    // already reflects every event at startFrame and remains constant until endFrame.
    virtual void processAudio(const clap_process_t& process,
                              std::uint32_t startFrame,
                              std::uint32_t endFrame) noexcept = 0;

    // Receives structurally valid core/non-core events after parameter handling.
    // This is also where polyphonic parameter events are intentionally surfaced.
    virtual void onEvent(const clap_event_header_t&) noexcept {}
    virtual clap_process_status processFinished() noexcept { return CLAP_PROCESS_CONTINUE; }

    virtual std::vector<std::byte> saveExtraState() const { return {}; }
    // New parameter values are visible here. Validate application state before
    // committing it; the framework rolls back parameters on false or exception.
    virtual bool loadExtraState(std::span<const std::byte>) { return true; }

    // Valid only while process(), processAudio(), onEvent(), or processFinished()
    // is executing. This exposes raw CLAP block context without wrapping transport
    // or other process-level data in a second framework abstraction.
    const clap_process_t* currentProcess() const noexcept { return currentProcess_; }

    bool requestGuiResize(std::uint32_t width, std::uint32_t height) noexcept;
    void markStateDirty() noexcept;

    // Audio-thread helper for plug-in-generated/read-only parameter telemetry.
    // sampleOffset must be strictly less than the current block's frames_count.
    bool emitParameterValue(clap_id id,
                            double value,
                            std::uint32_t sampleOffset,
                            std::uint32_t flags = CLAP_EVENT_DONT_RECORD) noexcept;

private:
    enum class GuiParamEventKind : std::uint8_t { begin, value, end };
    struct GuiParamEvent
    {
        GuiParamEventKind kind = GuiParamEventKind::value;
        clap_id id = CLAP_INVALID_ID;
        double value = 0.0;
    };

    class GuiParamQueue
    {
    public:
        bool canPush() const noexcept;
        bool push(const GuiParamEvent& event) noexcept;
        bool peek(GuiParamEvent& event) const noexcept;
        void consume() noexcept;
        std::size_t available() const noexcept;

    private:
        static constexpr std::size_t capacity = 256;
        std::array<GuiParamEvent, capacity> events_ {};
        std::atomic<std::size_t> head_ { 0 };
        std::atomic<std::size_t> tail_ { 0 };
    };

    bool init() noexcept override;
    bool activate(double sampleRate, std::uint32_t minFrames, std::uint32_t maxFrames) noexcept override;
    void deactivate() noexcept override;
    bool startProcessing() noexcept override;
    void stopProcessing() noexcept override;
    void reset() noexcept override;
    void onMainThread() noexcept override;
    clap_process_status process(const clap_process_t* process) noexcept override;

    bool implementsParams() const noexcept override { return true; }
    std::uint32_t paramsCount() const noexcept override;
    bool paramsInfo(std::uint32_t index, clap_param_info_t* info) const noexcept override;
    bool paramsValue(clap_id id, double* value) noexcept override;
    bool paramsValueToText(clap_id id, double value, char* display, std::uint32_t size) noexcept override;
    bool paramsTextToValue(clap_id id, const char* display, double* value) noexcept override;
    void paramsFlush(const clap_input_events_t* in, const clap_output_events_t* out) noexcept override;

    bool implementsState() const noexcept override { return true; }
    bool stateSave(const clap_ostream_t* stream) noexcept override;
    bool stateLoad(const clap_istream_t* stream) noexcept override;

    bool implementsAudioPorts() const noexcept override { return true; }
    std::uint32_t audioPortsCount(bool isInput) const noexcept override;
    bool audioPortsInfo(std::uint32_t index, bool isInput, clap_audio_port_info_t* info) const noexcept override;

    bool implementsNotePorts() const noexcept override { return true; }
    std::uint32_t notePortsCount(bool isInput) const noexcept override;
    bool notePortsInfo(std::uint32_t index, bool isInput, clap_note_port_info_t* info) const noexcept override;

    bool implementRemoteControls() const noexcept override { return true; }
    std::uint32_t remoteControlsPageCount() noexcept override;
    bool remoteControlsPageGet(std::uint32_t index, clap_remote_controls_page_t* page) noexcept override;

    bool implementsGui() const noexcept override;
    bool guiIsApiSupported(const char* api, bool floating) noexcept override;
    bool guiGetPreferredApi(const char** api, bool* floating) noexcept override;
    bool guiCreate(const char* api, bool floating) noexcept override;
    void guiDestroy() noexcept override;
    bool guiSetScale(double scale) noexcept override;
    bool guiShow() noexcept override;
    bool guiHide() noexcept override;
    bool guiGetSize(std::uint32_t* width, std::uint32_t* height) noexcept override;
    bool guiCanResize() const noexcept override;
    bool guiGetResizeHints(clap_gui_resize_hints_t* hints) noexcept override;
    bool guiAdjustSize(std::uint32_t* width, std::uint32_t* height) noexcept override;
    bool guiSetSize(std::uint32_t width, std::uint32_t height) noexcept override;
    void guiSuggestTitle(const char* title) noexcept override;
    bool guiSetParent(const clap_window_t* window) noexcept override;
    bool guiSetTransient(const clap_window_t* window) noexcept override;

    void applyInputEvent(const clap_event_header_t& event) noexcept;
    void drainGuiParameterEvents(const clap_output_events_t* out, std::uint32_t sampleOffset) noexcept;
    bool pushGuiParameterEvent(const GuiParamEvent& event) noexcept;
    bool pushParameterValue(const clap_output_events_t* out,
                            clap_id id,
                            double value,
                            std::uint32_t sampleOffset,
                            std::uint32_t flags) noexcept;

    ParameterStore parameters_;
    AudioPorts audioPorts_;
    NotePorts notePorts_;
    RemoteControls remoteControls_;
    std::unique_ptr<GuiDelegate> gui_;
    GuiParamQueue guiParamQueue_;
    std::atomic<bool> guiFlushRetryPending_ { false };
    const clap_process_t* currentProcess_ = nullptr;
    const clap_output_events_t* currentOutputEvents_ = nullptr;
    std::uint32_t currentFrameCount_ = 0;
};
} // namespace nullclap
