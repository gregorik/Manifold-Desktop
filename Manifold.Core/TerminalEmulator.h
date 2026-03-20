#pragma once
#include "TerminalTypes.h"
#include <string>
#include <vector>

namespace Manifold::Core {

    struct TerminalCell {
        wchar_t ch = L' ';
        TextStyle style{};
    };

    // Result for a single row render
    struct RowRenderResult {
        int rowIndex;
        std::vector<TextSegment> segments;
    };

    // Result from incremental render
    struct RenderResult {
        std::vector<RowRenderResult> dirtyRows;
        bool fullRedrawNeeded;
        int scrollCount;
        uint64_t generation;
    };

    class TerminalEmulator {
    public:
        TerminalEmulator(int width = 120, int height = 30);

        void Reset();
        void Resize(int width, int height);
        void Feed(const std::wstring& input);
        std::vector<TextSegment> Render() const;

        // Incremental rendering API
        RenderResult RenderDirty();
        std::vector<TextSegment> RenderRow(int row) const;
        void AcknowledgeRender();
        bool HasChanges() const;
        uint64_t GetGeneration() const { return m_generation; }
        int GetHeight() const { return m_height; }

    private:
        enum class State {
            Normal,
            Escape,
            CSI,
            OSC,       // Operating System Command (\x1b] ... BEL/ST)
            OSC_Esc,   // Got ESC inside OSC, waiting for backslash (ST)
            Ignore     // Generic ignore-until-ST state (DCS, PM, APC)
        };

        State m_state{ State::Normal };
        std::wstring m_csiBuffer;

        int m_width{ 120 };
        int m_height{ 30 };
        int m_row{ 0 };
        int m_col{ 0 };
        int m_savedRow{ 0 };
        int m_savedCol{ 0 };
        TextStyle m_style{};

        std::vector<std::vector<TerminalCell>> m_buffer;

        // Dirty tracking
        std::vector<bool> m_dirtyRows;
        uint64_t m_generation{ 0 };
        bool m_scrollOccurred{ false };
        int m_scrollCount{ 0 };

        void MarkRowDirty(int row);
        void MarkAllDirty();

        void PutChar(wchar_t ch);
        void NewLine();
        void Scroll();
        void ClearLine(int row, int fromCol, int toCol);
        void ClearScreen(int mode);
        void HandleCsi(wchar_t finalChar, const std::wstring& params);
        std::vector<int> ParseParams(const std::wstring& params) const;
        void ApplySgr(const std::vector<int>& params);
        static TerminalColor Index256ToColor(int idx);
    };
}
