import * as bridge from '../services/bridge.js';

let overlay = null;
let visible = false;
let onOpen = null;

export function create(opts = {}) {
    onOpen = opts.onOpenSession || (() => {});

    overlay = document.createElement('div');
    overlay.className = 'search-overlay';
    overlay.innerHTML = `
        <div class="search-dialog">
            <input type="text" class="search-input" placeholder="Search all conversations..." autofocus>
            <div class="search-results"></div>
        </div>
    `;

    overlay.addEventListener('click', e => { if (e.target === overlay) hide(); });

    const input = overlay.querySelector('.search-input');
    const results = overlay.querySelector('.search-results');

    let debounce = null;
    input.addEventListener('input', () => {
        clearTimeout(debounce);
        const q = input.value.trim();
        if (!q) { results.innerHTML = ''; return; }
        debounce = setTimeout(() => {
            bridge.send('SEARCH_SESSIONS', { query: q });
        }, 300);
    });

    input.addEventListener('keydown', e => {
        if (e.key === 'Escape') hide();
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            const items = results.querySelectorAll('.search-result-item');
            const active = results.querySelector('.search-result-item.active');
            const idx = active ? [...items].indexOf(active) : -1;
            if (active) active.classList.remove('active');
            const next = items[idx + 1] || items[0];
            if (next) { next.classList.add('active'); next.scrollIntoView({ block: 'nearest' }); }
        }
        if (e.key === 'ArrowUp') {
            e.preventDefault();
            const items = results.querySelectorAll('.search-result-item');
            const active = results.querySelector('.search-result-item.active');
            const idx = active ? [...items].indexOf(active) : items.length;
            if (active) active.classList.remove('active');
            const prev = items[idx - 1] || items[items.length - 1];
            if (prev) { prev.classList.add('active'); prev.scrollIntoView({ block: 'nearest' }); }
        }
        if (e.key === 'Enter') {
            const active = results.querySelector('.search-result-item.active');
            if (active) { active.click(); }
        }
    });

    bridge.on('SEARCH_RESULTS', data => {
        if (!visible) return;
        results.innerHTML = '';
        if (!data || data.length === 0) {
            results.innerHTML = '<div class="search-no-results">No results found</div>';
            return;
        }
        const query = input.value.trim().toLowerCase();
        for (const item of data) {
            const el = document.createElement('div');
            el.className = 'search-result-item';
            const snippet = highlightMatch(item.snippet || '', query);
            el.innerHTML = `
                <div class="search-result-title">${esc(item.title || item.sessionId || 'Untitled')}</div>
                <div class="search-result-snippet">${snippet}</div>
            `;
            el.onclick = () => {
                hide();
                onOpen(item.sessionId);
            };
            results.appendChild(el);
        }
    });

    document.body.appendChild(overlay);
}

export function show() {
    if (!overlay) return;
    visible = true;
    overlay.classList.add('open');
    const input = overlay.querySelector('.search-input');
    if (input) { input.value = ''; input.focus(); }
    overlay.querySelector('.search-results').innerHTML = '';
}

export function hide() {
    if (!overlay) return;
    visible = false;
    overlay.classList.remove('open');
}

export function toggle() { visible ? hide() : show(); }

function esc(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function highlightMatch(text, query) {
    if (!query) return esc(text);
    const escaped = esc(text);
    const re = new RegExp(`(${query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')})`, 'gi');
    return escaped.replace(re, '<mark>$1</mark>');
}
