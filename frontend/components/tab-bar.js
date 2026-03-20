const tabs = [];
let activeTabId = null;
let container = null;
const callbacks = { select: [], close: [] };

export function create(el) {
    container = el;
    render();
}

export function addTab({ id, title, type, closable = true }) {
    tabs.push({ id, title, type, closable });
    render();
    setActive(id);
}

export function removeTab(id) {
    const idx = tabs.findIndex(t => t.id === id);
    if (idx < 0) return;
    tabs.splice(idx, 1);
    if (activeTabId === id) {
        const next = tabs[Math.min(idx, tabs.length - 1)];
        if (next) setActive(next.id);
    }
    render();
}

export function setActive(id) {
    activeTabId = id;
    render();
    callbacks.select.forEach(cb => cb(id));
}

export function getActive() { return activeTabId; }
export function getTab(id) { return tabs.find(t => t.id === id); }
export function getTabs() { return [...tabs]; }

export function updateTitle(id, title) {
    const tab = tabs.find(t => t.id === id);
    if (tab) { tab.title = title; render(); }
}

export function onTabSelected(cb) { callbacks.select.push(cb); }
export function onTabClosed(cb) { callbacks.close.push(cb); }

export function onNewTab(cb) { callbacks.newTab = cb; }

function render() {
    if (!container) return;
    container.innerHTML = '';
    container.setAttribute('role', 'tablist');

    for (const tab of tabs) {
        const el = document.createElement('div');
        el.className = 'tab' + (tab.id === activeTabId ? ' active' : '');
        el.setAttribute('role', 'tab');
        el.setAttribute('aria-selected', tab.id === activeTabId ? 'true' : 'false');
        el.innerHTML = `<span class="tab-title">${escHtml(tab.title)}</span>`;

        if (tab.closable) {
            const closeBtn = document.createElement('span');
            closeBtn.className = 'tab-close';
            closeBtn.textContent = '\u00D7';
            closeBtn.onclick = e => { e.stopPropagation(); callbacks.close.forEach(cb => cb(tab.id)); };
            el.appendChild(closeBtn);
        }

        el.onclick = () => setActive(tab.id);
        container.appendChild(el);
    }

    // New tab button
    const newBtn = document.createElement('div');
    newBtn.className = 'tab-new';
    newBtn.textContent = '+';
    newBtn.title = 'New tab';
    newBtn.onclick = () => {
        if (callbacks.newTab) callbacks.newTab();
    };
    container.appendChild(newBtn);
}

function escHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
