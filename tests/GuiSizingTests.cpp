#include <nullclap/PhysicalPixelGuiSizing.hpp>

#include <cassert>
#include <cstdint>

int main()
{
    nullclap::PhysicalPixelGuiSizing sizing(1320, 820, 1120, 700, 1800, 1100);

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    sizing.getPhysicalSize(width, height);
    assert(width == 1320 && height == 820);
    assert(sizing.logicalWidth() == 1320 && sizing.logicalHeight() == 820);

    // CLAP Win32 uses physical pixels. At 125%, the host client area must grow
    // while the toolkit layout remains 1320x820 logical units.
    assert(sizing.setScale(1.25));
    sizing.getPhysicalSize(width, height);
    assert(width == 1650 && height == 1025);
    assert(sizing.logicalWidth() == 1320 && sizing.logicalHeight() == 820);

    // Host resize requests are converted back to logical toolkit coordinates.
    sizing.setPhysicalSize(1500, 875);
    assert(sizing.logicalWidth() == 1200);
    assert(sizing.logicalHeight() == 700);
    sizing.getPhysicalSize(width, height);
    assert(width == 1500 && height == 875);

    // Resize constraints are logical plugin constraints exposed in physical pixels.
    width = 1000;
    height = 700;
    sizing.adjustPhysicalSize(width, height);
    assert(width == 1400 && height == 875);

    width = 3000;
    height = 2000;
    sizing.adjustPhysicalSize(width, height);
    assert(width == 2250 && height == 1375);

    // Invalid host scale requests must not corrupt the current scale.
    assert(!sizing.setScale(0.0));
    assert(!sizing.setScale(-1.0));
    assert(sizing.scale() == 1.25);

    // Changing scale preserves logical size and recomputes physical size.
    assert(sizing.setScale(1.5));
    sizing.getPhysicalSize(width, height);
    assert(width == 1800 && height == 1050);

    return 0;
}
