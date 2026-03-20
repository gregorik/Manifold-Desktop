#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Manifold::Core {

    struct TerminalColor {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        bool isDefault = true;

        static TerminalColor Default() { return { 255, 255, 255, true }; }

        bool operator==(const TerminalColor& other) const {
            return r == other.r && g == other.g && b == other.b && isDefault == other.isDefault;
        }
    };

    struct TextStyle {
        bool isBold = false;
        bool isItalic = false;
        bool isUnderline = false;
        TerminalColor foreground = TerminalColor::Default();
        TerminalColor background = TerminalColor::Default();

        bool operator==(const TextStyle& other) const {
            return isBold == other.isBold &&
                isItalic == other.isItalic &&
                isUnderline == other.isUnderline &&
                foreground == other.foreground &&
                background == other.background;
        }
    };

    // A contiguous block of text with a single style.
    // The UI ViewModel will map this to a XAML <Run>.
    struct TextSegment {
        std::wstring text;
        TextStyle style;
    };
}
