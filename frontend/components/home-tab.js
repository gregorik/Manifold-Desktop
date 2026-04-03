import * as bridge from '../services/bridge.js';
import { state } from '../services/settings-store.js';

let element = null;
let sessions = [];
let providers = [];
const callbacks = { newChat: null, newTerminal: null, newCompare: null, openSession: null, openSettings: null };

export function create(opts = {}) {
    Object.assign(callbacks, opts);
    element = document.createElement('div');
    element.className = 'tab-pane home-pane';

    bridge.on('SESSIONS_LOADED', data => { sessions = data; render(); });
    bridge.on('PROVIDERS_LIST', data => { providers = data; render(); });
    bridge.on('UPDATE_AVAILABLE', data => {
        if (!data.updateAvailable || !element) return;
        const banner = document.createElement('div');
        banner.className = 'update-banner';
        banner.innerHTML = `Update available: v${esc(data.latestVersion)} <a href="#" class="update-link">Download</a>`;
        banner.querySelector('.update-link').onclick = e => {
            e.preventDefault();
            if (data.downloadUrl) bridge.send('OPEN_EXTERNAL_URL', { url: data.downloadUrl });
        };
        element.insertBefore(banner, element.firstChild);
    });

    bridge.send('LIST_SESSIONS');
    bridge.send('LIST_PROVIDERS');
    render();
    return element;
}

export function getElement() { return element; }

export function refresh() {
    bridge.send('LIST_SESSIONS');
    bridge.send('LIST_PROVIDERS');
}

function render() {
    if (!element) return;
    element.innerHTML = '';

    // Header
    const header = document.createElement('div');
    header.className = 'home-header';
    header.innerHTML = `<h1>Manifold Desktop</h1><p>Multi-provider AI chat client</p>`;
    element.appendChild(header);
    bridge.send('CHECK_UPDATES');

    // Onboarding overlay
    const noKeys = Object.keys(state.providerKeys).length === 0 ||
                   Object.values(state.providerKeys).every(v => !v);
    const hasKeyRequired = providers.some(p => p.requiresApiKey);
    const dismissed = localStorage.getItem('manifold_onboarded');

    if (noKeys && hasKeyRequired && !dismissed) {
        const overlay = document.createElement('div');
        overlay.className = 'home-onboarding';
        overlay.innerHTML = `
            <div class="onboarding-card">
                <h2>Welcome to Manifold</h2>
                <p>Add an API key to get started</p>
                <button class="home-action-btn onboarding-btn">Open Settings</button>
                <button class="btn-sm onboarding-dismiss">Dismiss</button>
            </div>
        `;
        overlay.querySelector('.onboarding-btn').onclick = () => {
            localStorage.setItem('manifold_onboarded', '1');
            overlay.remove();
            callbacks.openSettings?.();
        };
        overlay.querySelector('.onboarding-dismiss').onclick = () => {
            localStorage.setItem('manifold_onboarded', '1');
            overlay.remove();
        };
        element.appendChild(overlay);
    }

    // Quick actions
    const actions = document.createElement('div');
    actions.className = 'home-actions';
    actions.appendChild(makeAction('New Chat', () => callbacks.newChat?.()));
    actions.appendChild(makeAction('Terminal', () => callbacks.newTerminal?.()));
    actions.appendChild(makeAction('Compare', () => callbacks.newCompare?.()));
    actions.appendChild(makeAction('Settings', () => callbacks.openSettings?.()));
    actions.appendChild(makeAction('Import Session', () => bridge.send('IMPORT_SESSION')));
    element.appendChild(actions);

    // Recent sessions
    if (sessions.length > 0) {
        const title = document.createElement('div');
        title.className = 'home-section-title';
        title.textContent = 'Recent Sessions';
        element.appendChild(title);

        const grid = document.createElement('div');
        grid.className = 'sessions-grid';
        for (const s of sessions.slice(0, 12)) {
            const card = document.createElement('div');
            card.className = 'session-card';
            card.innerHTML = `<div class="title">${esc(s.title || 'Untitled')}</div><div class="meta">${s.model || ''} &middot; ${formatDate(s.updatedAt)}</div>`;
            card.onclick = () => callbacks.openSession?.(s.id);
            grid.appendChild(card);
        }
        element.appendChild(grid);
    }

    // Providers
    if (providers.length > 0) {
        const title = document.createElement('div');
        title.className = 'home-section-title';
        title.textContent = 'Providers';
        element.appendChild(title);

        const grid = document.createElement('div');
        grid.className = 'providers-grid';
        for (const p of providers) {
            const card = document.createElement('div');
            card.className = 'provider-card';
            const statusClass = p.requiresApiKey ? 'needs-key' : 'connected';
            const statusText = p.requiresApiKey ? 'Needs API key' : 'Ready';
            card.innerHTML = `<div class="name">${esc(p.displayName)}</div><div class="status ${statusClass}">${statusText}</div>`;
            grid.appendChild(card);
        }
        element.appendChild(grid);
    }

    // Version footer
    const footer = document.createElement('div');
    footer.style.cssText = 'text-align: center; padding: 24px; color: var(--text-muted); font-size: 0.8em;';
    footer.textContent = `Manifold Desktop v${state.version || '0.1.0'}`;
    element.appendChild(footer);
}

function makeAction(text, onClick) {
    const btn = document.createElement('button');
    btn.className = 'home-action-btn';
    btn.textContent = text;
    btn.onclick = onClick;
    return btn;
}

function formatDate(d) {
    if (!d) return '';
    try { return new Date(d).toLocaleDateString(); } catch { return d; }
}

function esc(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
