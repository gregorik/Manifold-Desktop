#include "pch.h"
#include "TerminalEmulator.h"

namespace Manifold::Core {

    TerminalEmulator::TerminalEmulator(int width, int height)
        : m_width(width), m_height(height) {
        Reset();
    }

    void TerminalEmulator::Reset() {
        m_state = State::Normal;
        m_csiBuffer.clear();
        m_style = TextStyle();
        m_row = 0;
        m_col = 0;
        m_savedRow = 0;
        m_savedCol = 0;
        m_buffer.assign(m_height, std::vector<TerminalCell>(m_width));
        m_dirtyRows.assign(m_height, true);
        m_generation++;
        m_scrollOccurred = false;
        m_scrollCount = 0;
    }

    void TerminalEmulator::Resize(int width, int height) {
        m_width = width;
        m_height = height;
        Reset();
    }

    void TerminalEmulator::Feed(const std::wstring& input) {
        for (wchar_t c : input) {
            switch (m_state) {
            case State::Normal:
                if (c == L'\x1b') {
                    m_state = State::Escape;
                }
                else if (c == L'\r') {
                    m_col = 0;
                }
                else if (c == L'\n') {
                    NewLine();
                }
                else if (c == L'\b') {
                    if (m_col > 0) m_col--;
                }
                else if (c == L'\t') {
                    int nextTab = std::min(((m_col / 8) + 1) * 8, m_width);
                    while (m_col < nextTab) {
                        if (m_row >= 0 && m_row < m_height && m_col >= 0 && m_col < m_width) {
                            m_buffer[m_row][m_col].ch = L' ';
                            m_buffer[m_row][m_col].style = m_style;
                            MarkRowDirty(m_row);
                        }
                        m_col++;
                    }
                    if (m_col >= m_width) {
                        NewLine();
                    }
                }
                else if (c >= L' ') {
                    // Only print visible characters (>= 0x20); ignore other C0 controls
                    PutChar(c);
                }
                // else: silently ignore BEL, NUL, and other C0 control chars
                break;

            case State::Escape:
                if (c == L'[') {
                    m_state = State::CSI;
                    m_csiBuffer.clear();
                }
                else if (c == L']') {
                    m_state = State::OSC; // Operating System Command
                }
                else if (c == L'P' || c == L'^' || c == L'_') {
                    m_state = State::Ignore; // DCS, PM, APC — consume until ST
                }
                else {
                    // Single-character ESC sequences (ESC 7, ESC 8, ESC =, ESC >, etc.)
                    m_state = State::Normal;
                }
                break;

            case State::CSI:
                if (c >= 0x40 && c <= 0x7E) {
                    HandleCsi(c, m_csiBuffer);
                    m_csiBuffer.clear();
                    m_state = State::Normal;
                }
                else {
                    m_csiBuffer += c;
                }
                break;

            case State::OSC:
                if (c == L'\x07') {
                    m_state = State::Normal; // BEL terminates OSC
                }
                else if (c == L'\x1b') {
                    m_state = State::OSC_Esc; // Might be ST (\x1b\\)
                }
                // else: consume and discard OSC payload
                break;

            case State::OSC_Esc:
                // Inside OSC, got ESC — if next is '\\' it's ST (end of OSC)
                if (c == L'\\') {
                    m_state = State::Normal;
                }
                else {
                    // Not ST — treat the ESC as starting a new sequence
                    m_state = State::Escape;
                    // Re-process this character in Escape state
                    if (c == L'[') {
                        m_state = State::CSI;
                        m_csiBuffer.clear();
                    }
                    else if (c == L']') {
                        m_state = State::OSC;
                    }
                    else {
                        m_state = State::Normal;
                    }
                }
                break;

            case State::Ignore:
                // Consume until ST (\x1b\\) or BEL
                if (c == L'\x1b') {
                    m_state = State::OSC_Esc; // Reuse OSC_Esc for ST detection
                }
                else if (c == L'\x07') {
                    m_state = State::Normal;
                }
                break;
            }
        }
    }

    void TerminalEmulator::PutChar(wchar_t ch) {
        if (m_row < 0 || m_row >= m_height) return;
        if (m_col < 0 || m_col >= m_width) {
            NewLine();
        }
        if (m_row < 0 || m_row >= m_height || m_col < 0 || m_col >= m_width) return;

        m_buffer[m_row][m_col].ch = ch;
        m_buffer[m_row][m_col].style = m_style;
        MarkRowDirty(m_row);
        m_col++;

        if (m_col >= m_width) {
            NewLine();
        }
    }

    void TerminalEmulator::NewLine() {
        m_col = 0;
        m_row++;
        if (m_row >= m_height) {
            Scroll();
            m_row = m_height - 1;
        }
    }

    void TerminalEmulator::Scroll() {
        if (m_buffer.empty()) return;
        m_buffer.erase(m_buffer.begin());
        m_buffer.push_back(std::vector<TerminalCell>(m_width));

        // Rotate dirty flags: shift up and mark last row dirty
        m_dirtyRows.erase(m_dirtyRows.begin());
        m_dirtyRows.push_back(true);
        m_scrollOccurred = true;
        m_scrollCount++;
        m_generation++;
    }

    void TerminalEmulator::ClearLine(int row, int fromCol, int toCol) {
        if (row < 0 || row >= m_height) return;
        if (fromCol < 0) fromCol = 0;
        if (toCol >= m_width) toCol = m_width - 1;
        if (fromCol > toCol) return;

        for (int c = fromCol; c <= toCol; ++c) {
            m_buffer[row][c] = TerminalCell{};
        }
        MarkRowDirty(row);
    }

    void TerminalEmulator::ClearScreen(int mode) {
        if (mode == 2) {
            for (int r = 0; r < m_height; ++r) {
                ClearLine(r, 0, m_width - 1);
            }
            return;
        }

        if (mode == 0) {
            ClearLine(m_row, m_col, m_width - 1);
            for (int r = m_row + 1; r < m_height; ++r) {
                ClearLine(r, 0, m_width - 1);
            }
        }
        else if (mode == 1) {
            for (int r = 0; r < m_row; ++r) {
                ClearLine(r, 0, m_width - 1);
            }
            ClearLine(m_row, 0, m_col);
        }
    }

    std::vector<int> TerminalEmulator::ParseParams(const std::wstring& params) const {
        std::vector<int> out;
        std::wstring token;
        for (wchar_t c : params) {
            if (c == L';') {
                if (token.empty()) {
                    out.push_back(0);
                }
                else {
                    try { out.push_back(std::stoi(token)); }
                    catch (...) { out.push_back(0); }
                }
                token.clear();
            }
            else if (c >= L'0' && c <= L'9') {
                token += c;
            }
            // Skip non-digit intermediate bytes (?, >, space, etc.)
        }
        if (!token.empty()) {
            try { out.push_back(std::stoi(token)); }
            catch (...) { out.push_back(0); }
        }
        else if (!params.empty() && params.back() == L';') {
            out.push_back(0);
        }
        return out;
    }

    void TerminalEmulator::HandleCsi(wchar_t finalChar, const std::wstring& params) {
        auto p = ParseParams(params);

        auto getParam = [&](size_t idx, int defaultValue) -> int {
            if (idx >= p.size()) return defaultValue;
            return p[idx] == 0 ? defaultValue : p[idx];
        };

        switch (finalChar) {
        case L'A': { // Cursor up
            int n = getParam(0, 1);
            m_row = std::max(0, m_row - n);
            break;
        }
        case L'B': { // Cursor down
            int n = getParam(0, 1);
            m_row = std::min(m_height - 1, m_row + n);
            break;
        }
        case L'C': { // Cursor forward
            int n = getParam(0, 1);
            m_col = std::min(m_width - 1, m_col + n);
            break;
        }
        case L'D': { // Cursor back
            int n = getParam(0, 1);
            m_col = std::max(0, m_col - n);
            break;
        }
        case L'H':
        case L'f': { // Cursor position
            int r = getParam(0, 1) - 1;
            int c = getParam(1, 1) - 1;
            m_row = std::clamp(r, 0, m_height - 1);
            m_col = std::clamp(c, 0, m_width - 1);
            break;
        }
        case L'J': { // Clear screen
            int mode = p.empty() ? 0 : p[0];
            ClearScreen(mode);
            break;
        }
        case L'K': { // Clear line
            int mode = p.empty() ? 0 : p[0];
            if (mode == 0) {
                ClearLine(m_row, m_col, m_width - 1);
            }
            else if (mode == 1) {
                ClearLine(m_row, 0, m_col);
            }
            else if (mode == 2) {
                ClearLine(m_row, 0, m_width - 1);
            }
            break;
        }
        case L's': { // Save cursor
            m_savedRow = m_row;
            m_savedCol = m_col;
            break;
        }
        case L'u': { // Restore cursor
            m_row = m_savedRow;
            m_col = m_savedCol;
            break;
        }
        case L'm': { // SGR
            ApplySgr(p);
            break;
        }
        default:
            break;
        }
    }

    TerminalColor TerminalEmulator::Index256ToColor(int idx) {
        // Standard 16 colors (0-15)
        static const TerminalColor basic16[] = {
            {0,0,0,false}, {197,15,31,false}, {19,161,14,false}, {193,156,0,false},
            {0,55,218,false}, {136,23,152,false}, {58,150,221,false}, {204,204,204,false},
            {118,118,118,false}, {231,72,86,false}, {22,198,12,false}, {249,241,165,false},
            {59,120,255,false}, {180,0,158,false}, {97,214,214,false}, {242,242,242,false},
        };
        if (idx >= 0 && idx < 16) return basic16[idx];

        // 216-color cube (16-231): 6x6x6
        if (idx >= 16 && idx < 232) {
            int ci = idx - 16;
            uint8_t r = static_cast<uint8_t>(ci / 36 * 51);
            uint8_t g = static_cast<uint8_t>((ci / 6 % 6) * 51);
            uint8_t b = static_cast<uint8_t>((ci % 6) * 51);
            return { r, g, b, false };
        }

        // Grayscale (232-255)
        if (idx >= 232 && idx <= 255) {
            uint8_t v = static_cast<uint8_t>(8 + (idx - 232) * 10);
            return { v, v, v, false };
        }
        return TerminalColor::Default();
    }

    void TerminalEmulator::ApplySgr(const std::vector<int>& params) {
        if (params.empty()) {
            m_style = TextStyle();
            return;
        }

        for (size_t i = 0; i < params.size(); ++i) {
            int code = params[i];

            // Handle 256-color: 38;5;N (fg) or 48;5;N (bg)
            if ((code == 38 || code == 48) && i + 1 < params.size() && params[i + 1] == 5) {
                if (i + 2 < params.size()) {
                    int colorIdx = params[i + 2];
                    auto color = Index256ToColor(colorIdx);
                    if (code == 38) m_style.foreground = color;
                    else            m_style.background = color;
                    i += 2; // skip ;5;N
                }
                else {
                    i += 1; // malformed, skip what we can
                }
                continue;
            }
            // Handle 24-bit color: 38;2;R;G;B (fg) or 48;2;R;G;B (bg)
            if ((code == 38 || code == 48) && i + 1 < params.size() && params[i + 1] == 2) {
                if (i + 4 < params.size()) {
                    uint8_t r = static_cast<uint8_t>(std::clamp(params[i + 2], 0, 255));
                    uint8_t g = static_cast<uint8_t>(std::clamp(params[i + 3], 0, 255));
                    uint8_t b = static_cast<uint8_t>(std::clamp(params[i + 4], 0, 255));
                    if (code == 38) m_style.foreground = { r, g, b, false };
                    else            m_style.background = { r, g, b, false };
                    i += 4; // skip ;2;R;G;B
                }
                else {
                    i = params.size() - 1; // malformed, skip rest
                }
                continue;
            }

            switch (code) {
            case 0:  m_style = TextStyle(); break;
            case 1:  m_style.isBold = true; break;
            case 2:  break; // dim/faint — ignore
            case 3:  m_style.isItalic = true; break;
            case 4:  m_style.isUnderline = true; break;
            case 7:  break; // reverse video — ignore
            case 8:  break; // hidden — ignore
            case 9:  break; // strikethrough — ignore
            case 22: m_style.isBold = false; break;
            case 23: m_style.isItalic = false; break;
            case 24: m_style.isUnderline = false; break;
            case 27: break; // reverse off
            case 28: break; // hidden off
            case 29: break; // strikethrough off

            case 30: m_style.foreground = { 0, 0, 0, false }; break;
            case 31: m_style.foreground = { 197, 15, 31, false }; break;
            case 32: m_style.foreground = { 19, 161, 14, false }; break;
            case 33: m_style.foreground = { 193, 156, 0, false }; break;
            case 34: m_style.foreground = { 0, 55, 218, false }; break;
            case 35: m_style.foreground = { 136, 23, 152, false }; break;
            case 36: m_style.foreground = { 58, 150, 221, false }; break;
            case 37: m_style.foreground = { 204, 204, 204, false }; break;
            case 39: m_style.foreground = TerminalColor::Default(); break;

            case 40: m_style.background = { 0, 0, 0, false }; break;
            case 41: m_style.background = { 197, 15, 31, false }; break;
            case 42: m_style.background = { 19, 161, 14, false }; break;
            case 43: m_style.background = { 193, 156, 0, false }; break;
            case 44: m_style.background = { 0, 55, 218, false }; break;
            case 45: m_style.background = { 136, 23, 152, false }; break;
            case 46: m_style.background = { 58, 150, 221, false }; break;
            case 47: m_style.background = { 204, 204, 204, false }; break;
            case 49: m_style.background = TerminalColor::Default(); break;

            case 90: m_style.foreground = { 118, 118, 118, false }; break;
            case 91: m_style.foreground = { 231, 72, 86, false }; break;
            case 92: m_style.foreground = { 22, 198, 12, false }; break;
            case 93: m_style.foreground = { 249, 241, 165, false }; break;
            case 94: m_style.foreground = { 59, 120, 255, false }; break;
            case 95: m_style.foreground = { 180, 0, 158, false }; break;
            case 96: m_style.foreground = { 97, 214, 214, false }; break;
            case 97: m_style.foreground = { 242, 242, 242, false }; break;

            case 100: m_style.background = { 118, 118, 118, false }; break;
            case 101: m_style.background = { 231, 72, 86, false }; break;
            case 102: m_style.background = { 22, 198, 12, false }; break;
            case 103: m_style.background = { 249, 241, 165, false }; break;
            case 104: m_style.background = { 59, 120, 255, false }; break;
            case 105: m_style.background = { 180, 0, 158, false }; break;
            case 106: m_style.background = { 97, 214, 214, false }; break;
            case 107: m_style.background = { 242, 242, 242, false }; break;

            default:
                break;
            }
        }
    }

    std::vector<TextSegment> TerminalEmulator::Render() const {
        std::vector<TextSegment> segments;

        for (int r = 0; r < m_height; ++r) {
            // Find last non-empty cell to avoid rendering trailing whitespace
            int lastCol = m_width - 1;
            while (lastCol >= 0 && (m_buffer[r][lastCol].ch == L'\0' || m_buffer[r][lastCol].ch == L' ')) {
                --lastCol;
            }

            TextStyle currentStyle{};
            bool haveSegment = false;

            for (int c = 0; c <= lastCol; ++c) {
                const auto& cell = m_buffer[r][c];
                if (!haveSegment || !(currentStyle == cell.style)) {
                    segments.push_back(TextSegment{ std::wstring(), cell.style });
                    currentStyle = cell.style;
                    haveSegment = true;
                }
                wchar_t ch = cell.ch ? cell.ch : L' ';
                segments.back().text.push_back(ch);
            }

            if (r < m_height - 1) {
                segments.push_back(TextSegment{ L"\n", TextStyle() });
            }
        }

        return segments;
    }

    std::vector<TextSegment> TerminalEmulator::RenderRow(int row) const {
        std::vector<TextSegment> segments;
        if (row < 0 || row >= m_height) return segments;

        int lastCol = m_width - 1;
        while (lastCol >= 0 && (m_buffer[row][lastCol].ch == L'\0' || m_buffer[row][lastCol].ch == L' ')) {
            --lastCol;
        }

        TextStyle currentStyle{};
        bool haveSegment = false;

        for (int c = 0; c <= lastCol; ++c) {
            const auto& cell = m_buffer[row][c];
            if (!haveSegment || !(currentStyle == cell.style)) {
                segments.push_back(TextSegment{ std::wstring(), cell.style });
                currentStyle = cell.style;
                haveSegment = true;
            }
            wchar_t ch = cell.ch ? cell.ch : L' ';
            segments.back().text.push_back(ch);
        }

        return segments;
    }

    // Render only dirty rows
    RenderResult TerminalEmulator::RenderDirty() {
        RenderResult result;
        result.fullRedrawNeeded = m_scrollOccurred;
        result.scrollCount = m_scrollCount;
        result.generation = m_generation;

        for (int r = 0; r < m_height; ++r) {
            if (m_dirtyRows[r]) {
                RowRenderResult rowResult;
                rowResult.rowIndex = r;
                rowResult.segments = RenderRow(r);
                result.dirtyRows.push_back(std::move(rowResult));
            }
        }

        return result;
    }

    void TerminalEmulator::AcknowledgeRender() {
        m_dirtyRows.assign(m_height, false);
        m_scrollOccurred = false;
        m_scrollCount = 0;
    }

    bool TerminalEmulator::HasChanges() const {
        for (bool dirty : m_dirtyRows) {
            if (dirty) return true;
        }
        return false;
    }

    void TerminalEmulator::MarkRowDirty(int row) {
        if (row >= 0 && row < m_height) {
            m_dirtyRows[row] = true;
            m_generation++;
        }
    }

    void TerminalEmulator::MarkAllDirty() {
        m_dirtyRows.assign(m_height, true);
        m_generation++;
    }
}
