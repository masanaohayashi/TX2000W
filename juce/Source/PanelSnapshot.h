#pragma once
#include <array>

// Plain display data, independent of JUCE Components and drawing code.
struct PanelSnapshot
{
    std::array<unsigned char,2*24*8> dots{};
    unsigned short leds=0;
    unsigned char contrast=2;   // UTIL > SYS の Contrast（1-8）
    bool lcdOn=false;
};
