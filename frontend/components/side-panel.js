import * as bridge from '../services/bridge.js';
import { showConfirm } from '../services/confirm.js';

let container = null;
let sessions = [];
let searchResults = null;
const callbacks = { sessionClick: [], newChat: [], newTerminal: [] };

export function create(el) {
    container = el;

    bridge.on('SESSIONS_LOADED', data => { sessions = data; render(); });
    bridge.on('SEARCH_RESULTS', data => { searchResults = data; render(); });
    bridge.on('SESSION_SAVED', () => bridge.send('LIST_SESSIONS'));
    bridge.on('SESSION_DELETED', () => bridge.send('LIST_SESSIONS'));

    render();
    bridge.send('LIST_SESSIONS');
}

export function refresh() { bridge.send('LIST_SESSIONS'); }

export function toggle() {
    if (container) container.classList.toggle('collapsed');
}

export function onSessionClick(cb) { callbacks.sessionClick.push(cb); }
export function onNewChat(cb) { callbacks.newChat.push(cb); }
export function onNewTerminal(cb) { callbacks.newTerminal.push(cb); }

function render() {
    if (!container) return;
    container.innerHTML = '';

    // Search box
    const searchBox = document.createElement('div');
    searchBox.style.cssText = 'padding: 8px;';
    searchBox.setAttribute('role', 'search');
    const searchInput = document.createElement('input');
    searchInput.type = 'text';
    searchInput.placeholder = 'Search sessions...';
    searchInput.style.cssText = 'width: 100%; padding: 6px 10px; border: 1px solid var(--border); border-radius: 6px; background: var(--bg-tertiary); color: var(--text-primary); font-size: 0.85em; outline: none;';
    searchInput.oninput = () => {
        const q = searchInput.value.trim();
        if (q) bridge.send('SEARCH_SESSIONS', { query: q });
        else { searchResults = null; render(); }
    };
    searchBox.appendChild(searchInput);
    container.appendChild(searchBox);

    // Action buttons
    const actions = document.createElement('div');
    actions.style.cssText = 'padding: 4px 8px; display: flex; gap: 4px;';
    const chatBtn = makeBtn('New Chat', () => callbacks.newChat.forEach(cb => cb()));
    const termBtn = makeBtn('Terminal', () => callbacks.newTerminal.forEach(cb => cb()));
    actions.appendChild(chatBtn);
    actions.appendChild(termBtn);
    container.appendChild(actions);

    // Divider
    container.appendChild(makeDivider());

    // Session list
    const list = document.createElement('div');
    list.style.cssText = 'flex: 1; overflow-y: auto; padding: 4px 8px;';

    const items = searchResults || sessions;
    if (items.length === 0) {
        const empty = document.createElement('div');
        empty.style.cssText = 'color: var(--text-muted); font-size: 0.8em; padding: 12px; text-align: center;';
        empty.textContent = searchResults ? 'No results' : 'No sessions yet';
        list.appendChild(empty);
    } else {
        for (const item of items) {
            const el = document.createElement('div');
            el.className = 'session-item';
            el.style.position = 'relative';

            const titleEl = document.createElement('div');
            titleEl.className = 'title';
            titleEl.textContent = item.title || item.sessionId || 'Untitled';

            // Double-click to rename
            titleEl.addEventListener('dblclick', e => {
                e.stopPropagation();
                const input = document.createElement('input');
                input.type = 'text';
                input.value = titleEl.textContent;
                input.style.cssText = 'width: 100%; padding: 2px 4px; font-size: inherit; background: var(--bg-tertiary); color: var(--text-primary); border: 1px solid var(--accent); border-radius: 4px; outline: none;';
                titleEl.replaceWith(input);
                input.focus();
                input.select();

                function finishRename() {
                    const newTitle = input.value.trim() || titleEl.textContent;
                    titleEl.textContent = newTitle;
                    input.replaceWith(titleEl);
                    bridge.send('SAVE_SESSION', { id: item.id || item.sessionId, data: { title: newTitle } });
                }
                input.addEventListener('blur', finishRename);
                input.addEventListener('keydown', e2 => {
                    if (e2.key === 'Enter') { e2.preventDefault(); input.blur(); }
                    if (e2.key === 'Escape') { input.replaceWith(titleEl); }
                });
            });

            el.appendChild(titleEl);

            const meta = document.createElement('div');
            meta.className = 'meta';
            meta.textContent = item.model || '';
            el.appendChild(meta);

            el.onclick = () => callbacks.sessionClick.forEach(cb => cb(item.id || item.sessionId));

            // Delete button
            const delBtn = document.createElement('button');
            delBtn.className = 'btn-sm session-delete';
            delBtn.textContent = '\u00D7';
            delBtn.title = 'Delete session';
            delBtn.style.cssText = 'position: absolute; right: 4px; top: 4px; opacity: 0; padding: 2px 6px; font-size: 0.75em;';
            delBtn.onclick = async e => {
                e.stopPropagation();
                const ok = await showConfirm('Delete this session?');
                if (!ok) return;
                bridge.send('DELETE_SESSION', { id: item.id || item.sessionId });
            };
            // Export Markdown button
            const exportBtn = document.createElement('button');
            exportBtn.className = 'btn-sm session-export';
            exportBtn.textContent = 'MD';
            exportBtn.title = 'Export as Markdown';
            exportBtn.style.cssText = 'position: absolute; right: 28px; top: 4px; opacity: 0; padding: 2px 6px; font-size: 0.65em;';
            exportBtn.onclick = e => {
                e.stopPropagation();
                bridge.send('EXPORT_MARKDOWN', { id: item.id || item.sessionId });
            };

            el.addEventListener('mouseenter', () => { delBtn.style.opacity = '1'; exportBtn.style.opacity = '1'; });
            el.addEventListener('mouseleave', () => { delBtn.style.opacity = '0'; exportBtn.style.opacity = '0'; });
            el.appendChild(delBtn);
            el.appendChild(exportBtn);

            list.appendChild(el);
        }
    }
    container.appendChild(list);
}

function makeBtn(text, onClick) {
    const btn = document.createElement('button');
    btn.className = 'btn-sm';
    btn.textContent = text;
    btn.style.cssText = 'flex: 1; padding: 6px; font-size: 0.8em;';
    btn.onclick = onClick;
    return btn;
}

function makeDivider() {
    const d = document.createElement('div');
    d.style.cssText = 'height: 1px; background: var(--border); margin: 0 8px;';
    return d;
}

function escHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
