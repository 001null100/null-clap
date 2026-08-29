#pragma once

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <nullclap/Gui.hpp>

#include <atomic>
#include <clap/ext/gui.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <windows.h>

namespace nullclap::midi_role_probe
{
class EventSnifferGui final : public nullclap::GuiDelegate
{
public:
    EventSnifferGui(std::atomic<std::uint32_t>& eventCount,
                    std::atomic<std::uint16_t>& lastEventType,
                    std::atomic<float>& gain) noexcept
        : eventCount_(eventCount), lastEventType_(lastEventType), gain_(gain)
    {
    }

    ~EventSnifferGui() override
    {
        destroy();
    }

    bool isApiSupported(const char* api, bool floating) const noexcept override
    {
        return !floating && api != nullptr && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
    }

    const char* preferredApi() const noexcept override
    {
        return CLAP_WINDOW_API_WIN32;
    }

    bool create(const char* api, bool floating) noexcept override
    {
        created_ = isApiSupported(api, floating);
        return created_;
    }

    void destroy() noexcept override
    {
        if (window_ != nullptr)
        {
            DestroyWindow(window_);
            window_ = nullptr;
        }
        created_ = false;
        parent_ = nullptr;
        title_ = nullptr;
        status_ = nullptr;
        count_ = nullptr;
        type_ = nullptr;
        gainText_ = nullptr;
        help_ = nullptr;
    }

    bool show() noexcept override
    {
        if (window_ == nullptr)
            return false;
        ShowWindow(window_, SW_SHOW);
        return true;
    }

    bool hide() noexcept override
    {
        if (window_ == nullptr)
            return false;
        ShowWindow(window_, SW_HIDE);
        return true;
    }

    bool getSize(std::uint32_t& width, std::uint32_t& height) noexcept override
    {
        width = width_;
        height = height_;
        return true;
    }

    bool canResize() const noexcept override
    {
        return false;
    }

    bool adjustSize(std::uint32_t& width, std::uint32_t& height) noexcept override
    {
        width = defaultWidth;
        height = defaultHeight;
        return true;
    }

    bool setSize(std::uint32_t width, std::uint32_t height) noexcept override
    {
        width_ = width;
        height_ = height;
        if (window_ != nullptr)
        {
            SetWindowPos(window_, nullptr, 0, 0,
                         static_cast<int>(width_), static_cast<int>(height_),
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            layout();
        }
        return true;
    }

    bool setParent(const clap_window_t& window) noexcept override
    {
        if (!created_ || window.api == nullptr || std::strcmp(window.api, CLAP_WINDOW_API_WIN32) != 0
            || window.win32 == nullptr)
            return false;

        parent_ = static_cast<HWND>(window.win32);
        if (window_ == nullptr)
            return createWindow();

        SetParent(window_, parent_);
        return true;
    }

private:
    static constexpr std::uint32_t defaultWidth = 440;
    static constexpr std::uint32_t defaultHeight = 190;
    static constexpr UINT_PTR timerId = 1;

    static constexpr const wchar_t* windowClassName = L"NullClapEventSnifferWindow";

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<EventSnifferGui*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<EventSnifferGui*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (self != nullptr)
        {
            switch (message)
            {
            case WM_TIMER:
                if (wParam == timerId)
                {
                    self->refresh();
                    return 0;
                }
                break;
            case WM_SIZE:
                self->layout();
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd, timerId);
                return 0;
            default:
                break;
            }
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static bool ensureWindowClass() noexcept
    {
        static const ATOM atom = []() noexcept -> ATOM {
            WNDCLASSW wc {};
            wc.lpfnWndProc = &EventSnifferGui::windowProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = windowClassName;
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            return RegisterClassW(&wc);
        }();
        return atom != 0;
    }

    bool createWindow() noexcept
    {
        if (parent_ == nullptr || !ensureWindowClass())
            return false;

        window_ = CreateWindowExW(
            0,
            windowClassName,
            L"NullClap Event Sniffer",
            WS_CHILD | WS_CLIPCHILDREN,
            0,
            0,
            static_cast<int>(width_),
            static_cast<int>(height_),
            parent_,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
        if (window_ == nullptr)
            return false;

        const auto makeLabel = [this](const wchar_t* text) noexcept -> HWND {
            HWND label = CreateWindowExW(
                0,
                L"STATIC",
                text,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0,
                0,
                100,
                20,
                window_,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            if (label != nullptr)
                SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            return label;
        };

        title_ = makeLabel(L"Bitwig CLAP Event Sniffer");
        status_ = makeLabel(L"Status: waiting for a non-parameter input event");
        count_ = makeLabel(L"Event count: 0");
        type_ = makeLabel(L"Last event: none");
        gainText_ = makeLabel(L"Audio gain: 100%");
        help_ = makeLabel(L"Send CC20 from Bitwig's MIDI CC device. Any received event increments the counter.");

        layout();
        refresh(true);
        SetTimer(window_, timerId, 50, nullptr);
        return true;
    }

    void layout() noexcept
    {
        if (window_ == nullptr)
            return;

        const int margin = 14;
        const int width = static_cast<int>(width_) - margin * 2;
        int y = 12;

        place(title_, margin, y, width, 24);
        y += 30;
        place(status_, margin, y, width, 20);
        y += 24;
        place(count_, margin, y, width, 20);
        y += 22;
        place(type_, margin, y, width, 20);
        y += 22;
        place(gainText_, margin, y, width, 20);
        y += 28;
        place(help_, margin, y, width, 34);
    }

    static void place(HWND child, int x, int y, int width, int height) noexcept
    {
        if (child != nullptr)
            SetWindowPos(child, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    static const wchar_t* eventTypeName(std::uint16_t type) noexcept
    {
        switch (type)
        {
        case CLAP_EVENT_NOTE_ON: return L"CLAP note on";
        case CLAP_EVENT_NOTE_OFF: return L"CLAP note off";
        case CLAP_EVENT_NOTE_CHOKE: return L"CLAP note choke";
        case CLAP_EVENT_NOTE_END: return L"CLAP note end";
        case CLAP_EVENT_NOTE_EXPRESSION: return L"CLAP note expression";
        case CLAP_EVENT_TRANSPORT: return L"transport";
        case CLAP_EVENT_MIDI: return L"MIDI 1";
        case CLAP_EVENT_MIDI_SYSEX: return L"MIDI SysEx";
        case CLAP_EVENT_MIDI2: return L"MIDI 2";
        default: return L"other core event";
        }
    }

    void refresh(bool force = false) noexcept
    {
        if (window_ == nullptr)
            return;

        const auto count = eventCount_.load(std::memory_order_relaxed);
        const auto type = lastEventType_.load(std::memory_order_relaxed);
        const auto gain = gain_.load(std::memory_order_relaxed);
        const int gainPercent = static_cast<int>(std::lround(static_cast<double>(gain) * 100.0));

        if (force || count != displayedCount_)
        {
            wchar_t text[96] {};
            std::swprintf(text, std::size(text), L"Event count: %u", static_cast<unsigned>(count));
            SetWindowTextW(count_, text);
            SetWindowTextW(status_, count == 0
                ? L"Status: waiting for a non-parameter input event"
                : L"Status: receiving CLAP input events");
            displayedCount_ = count;
        }

        if (force || type != displayedType_)
        {
            wchar_t text[160] {};
            if (type == noEventType)
                std::swprintf(text, std::size(text), L"Last event: none");
            else
                std::swprintf(text, std::size(text), L"Last event: %u (%ls)",
                              static_cast<unsigned>(type), eventTypeName(type));
            SetWindowTextW(type_, text);
            displayedType_ = type;
        }

        if (force || gainPercent != displayedGainPercent_)
        {
            wchar_t text[96] {};
            std::swprintf(text, std::size(text), L"Audio gain: %d%%", gainPercent);
            SetWindowTextW(gainText_, text);
            displayedGainPercent_ = gainPercent;
        }
    }

    static constexpr std::uint16_t noEventType = 0xFFFFu;

    std::atomic<std::uint32_t>& eventCount_;
    std::atomic<std::uint16_t>& lastEventType_;
    std::atomic<float>& gain_;

    bool created_ = false;
    HWND parent_ = nullptr;
    HWND window_ = nullptr;
    HWND title_ = nullptr;
    HWND status_ = nullptr;
    HWND count_ = nullptr;
    HWND type_ = nullptr;
    HWND gainText_ = nullptr;
    HWND help_ = nullptr;
    std::uint32_t width_ = defaultWidth;
    std::uint32_t height_ = defaultHeight;
    std::uint32_t displayedCount_ = 0xFFFFFFFFu;
    std::uint16_t displayedType_ = 0xFFFEu;
    int displayedGainPercent_ = -1;
};
} // namespace nullclap::midi_role_probe
#endif
