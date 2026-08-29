#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace nullclap
{
// CLAP's Win32 and X11 GUI APIs express sizes in physical pixels. Most desktop
// UI toolkits, including JUCE, lay out components in logical coordinates and
// apply the monitor scale when creating the native peer. Feeding a CLAP physical
// size directly into such a toolkit double-applies the display scale and makes
// the child window larger than the host viewport.
//
// PhysicalPixelGuiSizing keeps the plug-in's canonical size in logical units,
// converts it to physical pixels for the CLAP host, and converts host resize
// requests back to logical units for the toolkit. Calling setScale() preserves
// the logical layout size, so a 1320x820 editor becomes 1650x1025 physical at
// 125% scaling instead of being cropped inside a 1320x820 host client area.
class PhysicalPixelGuiSizing final
{
public:
    PhysicalPixelGuiSizing(std::uint32_t logicalWidth,
                           std::uint32_t logicalHeight,
                           std::uint32_t minimumLogicalWidth,
                           std::uint32_t minimumLogicalHeight,
                           std::uint32_t maximumLogicalWidth,
                           std::uint32_t maximumLogicalHeight) noexcept
        : logicalWidth_(std::max<std::uint32_t>(1, logicalWidth)),
          logicalHeight_(std::max<std::uint32_t>(1, logicalHeight)),
          minimumLogicalWidth_(std::max<std::uint32_t>(1, minimumLogicalWidth)),
          minimumLogicalHeight_(std::max<std::uint32_t>(1, minimumLogicalHeight)),
          maximumLogicalWidth_(std::max(maximumLogicalWidth, minimumLogicalWidth_)),
          maximumLogicalHeight_(std::max(maximumLogicalHeight, minimumLogicalHeight_))
    {
    }

    bool setScale(double scale) noexcept
    {
        if (!std::isfinite(scale) || scale <= 0.0)
            return false;
        scale_ = scale;
        return true;
    }

    double scale() const noexcept { return scale_; }

    std::uint32_t logicalWidth() const noexcept { return logicalWidth_; }
    std::uint32_t logicalHeight() const noexcept { return logicalHeight_; }

    std::uint32_t physicalWidth() const noexcept { return toPhysical(logicalWidth_); }
    std::uint32_t physicalHeight() const noexcept { return toPhysical(logicalHeight_); }

    void getPhysicalSize(std::uint32_t& width, std::uint32_t& height) const noexcept
    {
        width = physicalWidth();
        height = physicalHeight();
    }

    // set_size() is authoritative: use the exact size supplied by the host and
    // convert it back to toolkit coordinates. adjust_size() is where constraints
    // belong. This also makes old host-restored sizes degrade gracefully instead
    // of silently changing a size the host believes was accepted.
    void setPhysicalSize(std::uint32_t width, std::uint32_t height) noexcept
    {
        logicalWidth_ = toLogical(std::max<std::uint32_t>(1, width));
        logicalHeight_ = toLogical(std::max<std::uint32_t>(1, height));
    }

    void adjustPhysicalSize(std::uint32_t& width, std::uint32_t& height) const noexcept
    {
        width = std::clamp(width, toPhysical(minimumLogicalWidth_), toPhysical(maximumLogicalWidth_));
        height = std::clamp(height, toPhysical(minimumLogicalHeight_), toPhysical(maximumLogicalHeight_));
    }

private:
    std::uint32_t toPhysical(std::uint32_t logical) const noexcept
    {
        return roundedPositive(static_cast<double>(logical) * scale_);
    }

    std::uint32_t toLogical(std::uint32_t physical) const noexcept
    {
        return roundedPositive(static_cast<double>(physical) / scale_);
    }

    static std::uint32_t roundedPositive(double value) noexcept
    {
        constexpr auto maximum = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
        value = std::clamp(value, 1.0, maximum);
        return static_cast<std::uint32_t>(std::llround(value));
    }

    double scale_ = 1.0;
    std::uint32_t logicalWidth_ = 1;
    std::uint32_t logicalHeight_ = 1;
    std::uint32_t minimumLogicalWidth_ = 1;
    std::uint32_t minimumLogicalHeight_ = 1;
    std::uint32_t maximumLogicalWidth_ = 1;
    std::uint32_t maximumLogicalHeight_ = 1;
};
} // namespace nullclap
