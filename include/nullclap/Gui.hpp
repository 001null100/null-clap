#pragma once

#include <clap/ext/gui.h>
#include <cstdint>

namespace nullclap
{
// Toolkit-neutral boundary between CLAP and a concrete UI implementation.
// A consuming plug-in can implement this with JUCE Components, a native view,
// a webview, or no GUI at all.
class GuiDelegate
{
public:
    virtual ~GuiDelegate() = default;

    virtual bool isApiSupported(const char* api, bool floating) const noexcept = 0;
    virtual const char* preferredApi() const noexcept = 0;
    virtual bool isFloating() const noexcept { return false; }

    virtual bool create(const char* api, bool floating) noexcept = 0;
    virtual void destroy() noexcept = 0;
    virtual bool setScale(double scale) noexcept { return scale > 0.0; }
    virtual bool show() noexcept = 0;
    virtual bool hide() noexcept = 0;
    virtual bool getSize(std::uint32_t& width, std::uint32_t& height) noexcept = 0;
    virtual bool canResize() const noexcept { return true; }
    virtual bool getResizeHints(clap_gui_resize_hints_t&) noexcept { return false; }
    virtual bool adjustSize(std::uint32_t&, std::uint32_t&) noexcept { return true; }
    virtual bool setSize(std::uint32_t width, std::uint32_t height) noexcept = 0;
    virtual void suggestTitle(const char*) noexcept {}
    virtual bool setParent(const clap_window_t& window) noexcept = 0;
    virtual bool setTransient(const clap_window_t&) noexcept { return false; }
};
} // namespace nullclap
