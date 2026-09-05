#include <nullclap/PhysicalPixelGuiSizing.hpp>

#include "TestCheck.hpp"
#include <cstdint>

int main()
{
    nullclap::PhysicalPixelGuiSizing sizing(1320, 820, 1120, 700, 1800, 1100);

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    sizing.getPhysicalSize(width, height);
    CHECK(width == 1320 && height == 820);
    CHECK(sizing.logicalWidth() == 1320 && sizing.logicalHeight() == 820);

    // CLAP Win32 uses physical pixels. At 125%, the host client area must grow
    // while the toolkit layout remains 1320x820 logical units.
    CHECK(sizing.setScale(1.25));
    sizing.getPhysicalSize(width, height);
    CHECK(width == 1650 && height == 1025);
    CHECK(sizing.logicalWidth() == 1320 && sizing.logicalHeight() == 820);

    sizing.setPhysicalSize(1500, 875);
    CHECK(sizing.logicalWidth() == 1200);
    CHECK(sizing.logicalHeight() == 700);
    sizing.getPhysicalSize(width, height);
    CHECK(width == 1500 && height == 875);

    width = 1000;
    height = 700;
    sizing.adjustPhysicalSize(width, height);
    CHECK(width == 1400 && height == 875);

    width = 3000;
    height = 2000;
    sizing.adjustPhysicalSize(width, height);
    CHECK(width == 2250 && height == 1375);

    CHECK(!sizing.setScale(0.0));
    CHECK(!sizing.setScale(-1.0));
    CHECK(sizing.scale() == 1.25);

    CHECK(sizing.setScale(1.5));
    sizing.getPhysicalSize(width, height);
    CHECK(width == 1800 && height == 1050);
    return 0;
}
