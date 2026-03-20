import * as bridge from '../services/bridge.js';

export function create(tabId) {
    const element = document.createElement('div');
    element.className = 'tab-pane terminal-pane';

    const grid = document.createElement('div');
    grid.className = 'terminal-grid';
    grid.tabIndex = 0;
    element.appendChild(grid);

    // Terminal buffer as rows of HTML
    const rows = [];
    let rowCount = 30;
    let colCount = 120;
    for (let i = 0; i < rowCount; i++) {
        const row = document.createElement('div');
        row.className = 'terminal-row';
        row.innerHTML = '&nbsp;';
        grid.appendChild(row);
        rows.push(row);
    }

    // Selection state
    let selecting = false;
    let selStart = null; // { row, col }
    let selEnd = null;

    // Listen for terminal output
    const unsubs = [];
    unsubs.push(bridge.on('TERMINAL_OUTPUT', data => {
        if (data.tabId !== tabId) return;
        for (const rowData of data.rows) {
            const rowEl = rows[rowData.line];
            if (!rowEl) continue;
            let html = '';
            for (const seg of rowData.segments) {
                let style = '';
                if (seg.fg) style += `color:${seg.fg};`;
                if (seg.bg) style += `background:${seg.bg};`;
                if (seg.bold) style += 'font-weight:bold;';
                if (seg.italic) style += 'font-style:italic;';
                if (seg.underline) style += 'text-decoration:underline;';
                const text = escHtml(seg.text);
                html += style ? `<span style="${style}">${text}</span>` : text;
            }
            rowEl.innerHTML = html || '&nbsp;';
        }
    }));

    unsubs.push(bridge.on('TERMINAL_STOPPED', data => {
        if (data.tabId !== tabId) return;
        const msg = document.createElement('div');
        msg.style.cssText = 'color: var(--text-muted); padding: 8px; text-align: center; font-size: 0.85em;';
        msg.textContent = 'Process exited';
        grid.appendChild(msg);
    }));

    // Keyboard input
    grid.addEventListener('keydown', e => {
        // Ctrl+Shift+C = copy selection
        if (e.ctrlKey && e.shiftKey && e.key === 'C') {
            e.preventDefault();
            copySelection();
            return;
        }
        // Ctrl+Shift+V = paste
        if (e.ctrlKey && e.shiftKey && e.key === 'V') {
            e.preventDefault();
            pasteClipboard();
            return;
        }

        e.preventDefault();
        let data = '';

        if (e.key.length === 1 && !e.ctrlKey && !e.altKey) {
            data = e.key;
        } else if (e.key === 'Enter') {
            data = '\r';
        } else if (e.key === 'Backspace') {
            data = '\x7f';
        } else if (e.key === 'Tab') {
            data = '\t';
        } else if (e.key === 'Escape') {
            data = '\x1b';
        } else if (e.key === 'ArrowUp') {
            data = '\x1b[A';
        } else if (e.key === 'ArrowDown') {
            data = '\x1b[B';
        } else if (e.key === 'ArrowRight') {
            data = '\x1b[C';
        } else if (e.key === 'ArrowLeft') {
            data = '\x1b[D';
        } else if (e.key === 'Home') {
            data = '\x1b[H';
        } else if (e.key === 'End') {
            data = '\x1b[F';
        } else if (e.key === 'Delete') {
            data = '\x1b[3~';
        } else if (e.ctrlKey && e.key.length === 1) {
            const code = e.key.toUpperCase().charCodeAt(0) - 64;
            if (code > 0 && code < 32) data = String.fromCharCode(code);
        }

        if (data) {
            clearSelection();
            bridge.send('TERMINAL_INPUT', { tabId, data });
        }
    });

    // Mouse selection
    grid.addEventListener('mousedown', e => {
        if (e.button !== 0) return;
        selecting = true;
        selStart = getCellFromMouse(e);
        selEnd = selStart;
        clearSelectionHighlight();
    });

    grid.addEventListener('mousemove', e => {
        if (!selecting) return;
        selEnd = getCellFromMouse(e);
        highlightSelection();
    });

    grid.addEventListener('mouseup', () => {
        selecting = false;
    });

    function getCellFromMouse(e) {
        const rect = grid.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        const cellW = rect.width / colCount;
        const cellH = rect.height / rowCount;
        return {
            row: Math.min(Math.max(Math.floor(y / cellH), 0), rowCount - 1),
            col: Math.min(Math.max(Math.floor(x / cellW), 0), colCount - 1)
        };
    }

    function highlightSelection() {
        clearSelectionHighlight();
        if (!selStart || !selEnd) return;

        let startRow = selStart.row, startCol = selStart.col;
        let endRow = selEnd.row, endCol = selEnd.col;

        if (startRow > endRow || (startRow === endRow && startCol > endCol)) {
            [startRow, startCol, endRow, endCol] = [endRow, endCol, startRow, startCol];
        }

        for (let r = startRow; r <= endRow; r++) {
            const rowEl = rows[r];
            if (!rowEl) continue;
            rowEl.classList.add('terminal-row-selected');
        }
    }

    function clearSelectionHighlight() {
        rows.forEach(r => r.classList.remove('terminal-row-selected'));
    }

    function clearSelection() {
        selStart = null;
        selEnd = null;
        clearSelectionHighlight();
    }

    function getSelectedText() {
        if (!selStart || !selEnd) return '';
        let startRow = selStart.row, endRow = selEnd.row;
        if (startRow > endRow) [startRow, endRow] = [endRow, startRow];

        const lines = [];
        for (let r = startRow; r <= endRow; r++) {
            const rowEl = rows[r];
            if (!rowEl) continue;
            lines.push(rowEl.textContent || '');
        }
        return lines.join('\n');
    }

    function copySelection() {
        const text = getSelectedText();
        if (!text) return;
        if (navigator.clipboard) {
            navigator.clipboard.writeText(text);
        } else {
            bridge.send('COPY_TO_CLIPBOARD', { text });
        }
        // Flash selected rows
        rows.forEach(r => {
            if (r.classList.contains('terminal-row-selected')) {
                r.classList.add('terminal-row-copied');
                setTimeout(() => r.classList.remove('terminal-row-copied'), 300);
            }
        });
    }

    function pasteClipboard() {
        if (navigator.clipboard) {
            navigator.clipboard.readText().then(text => {
                if (text) bridge.send('TERMINAL_INPUT', { tabId, data: text });
            });
        }
    }

    // Resize observer
    const resizeObserver = new ResizeObserver(() => {
        const rect = grid.getBoundingClientRect();
        if (rect.width === 0 || rect.height === 0) return;

        // Measure a single character to compute cols/rows
        const probe = document.createElement('span');
        probe.style.cssText = 'visibility:hidden;position:absolute;white-space:pre;';
        probe.textContent = 'M';
        grid.appendChild(probe);
        const charW = probe.offsetWidth || 8;
        const charH = probe.offsetHeight || 16;
        grid.removeChild(probe);

        const newCols = Math.max(10, Math.floor(rect.width / charW));
        const newRows = Math.max(5, Math.floor(rect.height / charH));

        if (newCols !== colCount || newRows !== rowCount) {
            colCount = newCols;

            // Adjust row elements if row count changed
            while (rows.length < newRows) {
                const row = document.createElement('div');
                row.className = 'terminal-row';
                row.innerHTML = '&nbsp;';
                grid.appendChild(row);
                rows.push(row);
            }
            while (rows.length > newRows) {
                const row = rows.pop();
                row.remove();
            }
            rowCount = newRows;

            bridge.send('TERMINAL_RESIZE', { tabId, cols: colCount, rows: rowCount });
        }
    });
    resizeObserver.observe(grid);

    // Focus grid when terminal becomes active
    requestAnimationFrame(() => grid.focus());

    // Start terminal process
    bridge.send('TERMINAL_START', { tabId, command: 'cmd.exe' });

    return {
        element,
        tabId,
        focus() { grid.focus(); },
        destroy() {
            resizeObserver.disconnect();
            unsubs.forEach(u => u?.());
            bridge.send('TERMINAL_STOP', { tabId });
        }
    };
}

function escHtml(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
