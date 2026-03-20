// Main orchestrator — wires together all components
import * as bridge from './services/bridge.js';
import * as settingsStore from './services/settings-store.js';
import * as providerApi from './services/provider-api.js';
import * as tabBar from './components/tab-bar.js';
import * as sidePanel from './components/side-panel.js';
import * as inputBar from './components/input-bar.js';
import * as settingsPanel from './components/settings-panel.js';
import * as homeTab from './components/home-tab.js';
import { create as createChatTab } from './components/chat-tab.js';
import { create as createTerminalTab } from './components/terminal-tab.js';
import { create as createCompareTab } from './components/compare-tab.js';
import { showToast } from './services/toast.js';
import { showConfirm } from './services/confirm.js';
import * as searchOverlay from './components/search-overlay.js';

const tabContent = document.getElementById('tab-content');
const tabPanes = new Map(); // tabId -> { pane, type, instance }

let tabCounter = 0;
let appStreaming = false;

// Initialize components
tabBar.create(document.getElementById('tab-bar'));
sidePanel.create(document.getElementById('side-panel'));
inputBar.create(document.getElementById('input-bar'));
settingsPanel.create();
searchOverlay.create({
    onOpenSession: (id) => openChatTab(id)
});

// Home tab (always present, not closable)
const homeEl = homeTab.create({
    newChat: () => openChatTab(),
    newTerminal: () => openTerminalTab(),
    newCompare: () => openCompareTab(),
    openSession: (id) => openChatTab(id),
    openSettings: () => settingsPanel.toggle()
});

const homeId = 'home';
tabPanes.set(homeId, { pane: homeEl, type: 'home', instance: null });
tabContent.appendChild(homeEl);
tabBar.addTab({ id: homeId, title: 'Home', type: 'home', closable: false });

// Tab selection
tabBar.onTabSelected(id => {
    tabPanes.forEach((val, key) => {
        val.pane.classList.toggle('active', key === id);
    });
    const entry = tabPanes.get(id);
    if (entry) {
        inputBar.setTabType(entry.type);
        if (entry.type === 'chat' && entry.instance) entry.instance.focus?.();
        if (entry.type === 'terminal' && entry.instance) entry.instance.focus();
        const tab = tabBar.getTab(id);
        document.title = 'Manifold \u2014 ' + (tab?.title || 'Home');
    }
});

// Tab close
tabBar.onTabClosed(async id => {
    const entry = tabPanes.get(id);
    if (!entry) return;

    if (entry.type === 'chat' && appStreaming) {
        const ok = await showConfirm('A response is still streaming. Close this tab?');
        if (!ok) return;
    }

    entry.instance?.destroy?.();
    entry.pane.remove();
    tabPanes.delete(id);
    tabBar.removeTab(id);
});

// New tab button
tabBar.onNewTab(() => openChatTab());

// Side panel events
sidePanel.onSessionClick(id => openChatTab(id));
sidePanel.onNewChat(() => openChatTab());
sidePanel.onNewTerminal(() => openTerminalTab());

// Input bar send
inputBar.onSend(({ text, providerId, modelId }) => {
    const activeId = tabBar.getActive();
    const entry = tabPanes.get(activeId);
    if (!entry) return;

    if (entry.type === 'compare') {
        entry.instance.addUserMessage(text);
        inputBar.setStreaming(true);
        appStreaming = true;

        const messages = entry.instance.getMessages();
        const settings = settingsStore.state.settings;

        bridge.send('COMPARE_CHAT', {
            slots: entry.instance.slots,
            messages,
            systemPrompt: settings.systemPrompt
        });
        return;
    }

    if (entry.type !== 'chat') return;

    entry.instance.addUserMessage(text);
    inputBar.setStreaming(true);
    appStreaming = true;

    const messages = entry.instance.getMessages();
    const settings = settingsStore.state.settings;

    providerApi.sendChat({
        providerId,
        modelId,
        messages,
        systemPrompt: settings.systemPrompt,
        temperature: settings.temperature
    });
});

// Input bar cancel
inputBar.onCancel(() => {
    providerApi.cancelChat();
    inputBar.setStreaming(false);
});

// Chat lifecycle events
bridge.on('CHAT_DONE', () => { inputBar.setStreaming(false); appStreaming = false; });
bridge.on('CHAT_ERROR', () => { inputBar.setStreaming(false); appStreaming = false; });
bridge.on('CHAT_CANCELLED', () => { inputBar.setStreaming(false); appStreaming = false; });
bridge.on('COMPARE_CANCELLED', () => { inputBar.setStreaming(false); appStreaming = false; });

// Open a chat tab
function openChatTab(sessionId) {
    const id = 'chat-' + (++tabCounter);
    const instance = createChatTab(sessionId || null);
    const pane = instance.element;
    pane.classList.remove('active');

    tabPanes.set(id, { pane, type: 'chat', instance });
    tabContent.appendChild(pane);
    tabBar.addTab({ id, title: sessionId ? 'Session' : 'New Chat', type: 'chat' });
}

// Open a terminal tab
function openTerminalTab() {
    const id = 'term-' + (++tabCounter);
    const instance = createTerminalTab(id);
    const pane = instance.element;
    pane.classList.remove('active');

    tabPanes.set(id, { pane, type: 'terminal', instance });
    tabContent.appendChild(pane);
    tabBar.addTab({ id, title: 'Terminal', type: 'terminal' });
}

function openCompareTab() {
    const id = 'compare-' + (++tabCounter);
    const defaultSlots = [
        { providerId: 'gemini', modelId: 'gemini-2.0-flash', providerName: 'Gemini' },
        { providerId: 'openai', modelId: 'gpt-4o-mini', providerName: 'OpenAI' }
    ];
    const instance = createCompareTab(defaultSlots);
    const pane = instance.element;
    pane.classList.remove('active');

    tabPanes.set(id, { pane, type: 'compare', instance });
    tabContent.appendChild(pane);
    tabBar.addTab({ id, title: 'Compare', type: 'compare' });
}

// Keyboard shortcuts
document.addEventListener('keydown', e => {
    if (e.ctrlKey && !e.shiftKey && e.key === 'n') {
        e.preventDefault();
        openChatTab();
    }
    else if (e.ctrlKey && !e.shiftKey && e.key === 't') {
        e.preventDefault();
        openTerminalTab();
    }
    else if (e.ctrlKey && !e.shiftKey && e.key === 'w') {
        e.preventDefault();
        const activeId = tabBar.getActive();
        const entry = tabPanes.get(activeId);
        if (entry && activeId !== 'home') {
            entry.instance?.destroy?.();
            entry.pane.remove();
            tabPanes.delete(activeId);
            tabBar.removeTab(activeId);
        }
    }
    else if (e.ctrlKey && e.key === ',') {
        e.preventDefault();
        settingsPanel.toggle();
    }
    else if (e.ctrlKey && e.key === 'k') {
        e.preventDefault();
        const searchInput = document.querySelector('#side-panel input[type="text"]');
        if (searchInput) searchInput.focus();
    }
    else if (e.ctrlKey && e.key === 'f') {
        e.preventDefault();
        searchOverlay.toggle();
    }
    else if (e.ctrlKey && e.key >= '1' && e.key <= '9') {
        e.preventDefault();
        const allTabs = tabBar.getTabs();
        const idx = parseInt(e.key) - 1;
        if (idx < allTabs.length) {
            tabBar.setActive(allTabs[idx].id);
        }
    }
});

// Signal host that frontend is ready
bridge.send('FRONTEND_READY');

// Global error handlers
window.onerror = (msg) => { showToast(String(msg), 'error'); };
window.addEventListener('unhandledrejection', e => {
    showToast(String(e.reason || 'Unhandled error'), 'error');
});
