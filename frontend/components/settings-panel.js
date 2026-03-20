// Settings drawer panel — multi-provider version
import * as settingsStore from '../services/settings-store.js';
import * as pluginStore from '../services/plugin-store.js';
import * as bridge from '../services/bridge.js';

let panel = null;
let visible = false;
let activeSection = 'general';
let dirty = false;

export function create() {
    panel = document.createElement('div');
    panel.className = 'settings-panel';
    render();
    document.body.appendChild(panel);

    settingsStore.onChange(sync);
    settingsStore.onReady(sync);
    pluginStore.onChange(renderPlugins);
}

function render() {
    if (!panel) return;
    panel.innerHTML = `
        <div class="settings-header">
            <h2>Settings</h2>
            <button class="settings-close" aria-label="Close">&times;</button>
        </div>
        <div class="settings-nav">
            <button class="settings-nav-btn active" data-section="general">General<span class="settings-dirty-dot"></span></button>
            <button class="settings-nav-btn" data-section="providers">Providers</button>
            <button class="settings-nav-btn" data-section="plugins">Plugins</button>
            <button class="settings-nav-btn" data-section="mcp">MCP</button>
            <button class="settings-nav-btn" data-section="prompts">Prompts</button>
            <button class="settings-nav-btn" data-section="usage">Usage</button>
        </div>
        <div class="settings-body">
            <div class="settings-section active" data-section="general">
                ${renderGeneralSection()}
            </div>
            <div class="settings-section" data-section="providers">
                <div id="providers-list"></div>
            </div>
            <div class="settings-section" data-section="plugins">
                <div id="plugins-list"></div>
            </div>
            <div class="settings-section" data-section="mcp">
                ${renderMcpSection()}
            </div>
            <div class="settings-section" data-section="prompts">
                <div id="prompts-list"></div>
                <div class="prompt-add-form" style="margin-top:16px;padding-top:16px;border-top:1px solid var(--border-subtle);">
                    <input type="text" id="promptTitleInput" placeholder="Prompt title" class="mcp-input" style="width:100%;padding:8px;background:var(--bg-tertiary);color:var(--text-primary);border:1px solid var(--border);border-radius:var(--radius-sm);">
                    <textarea id="promptContentInput" rows="4" placeholder="Prompt content..." style="width:100%;margin-top:8px;padding:8px;background:var(--bg-tertiary);color:var(--text-primary);border:1px solid var(--border);border-radius:var(--radius-sm);resize:vertical;"></textarea>
                    <label style="display:flex;align-items:center;gap:6px;margin-top:8px;font-size:0.85em;color:var(--text-secondary);">
                        <input type="checkbox" id="promptIsSystemInput"> System prompt
                    </label>
                    <button class="btn-sm" id="promptAddBtn" style="margin-top:8px;">Save Prompt</button>
                </div>
            </div>
            <div class="settings-section" data-section="usage">
                <div id="usage-stats"></div>
            </div>
        </div>
    `;

    // Close
    panel.querySelector('.settings-close').addEventListener('click', hide);

    // Nav tabs
    panel.querySelectorAll('.settings-nav-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            activeSection = btn.dataset.section;
            panel.querySelectorAll('.settings-nav-btn').forEach(b => b.classList.toggle('active', b.dataset.section === activeSection));
            panel.querySelectorAll('.settings-section').forEach(s => s.classList.toggle('active', s.dataset.section === activeSection));
            if (activeSection === 'providers') renderProviders();
            if (activeSection === 'plugins') { pluginStore.listPlugins(); }
            if (activeSection === 'mcp') { pluginStore.listMcpServers(); renderMcpServers(); }
            if (activeSection === 'prompts') { bridge.send('LIST_PROMPTS'); }
            if (activeSection === 'usage') renderUsage();
        });
    });

    // MCP add button
    const mcpAddBtn = panel.querySelector('#mcpAddBtn');
    if (mcpAddBtn) {
        mcpAddBtn.addEventListener('click', () => {
            const name = panel.querySelector('#mcpNameInput')?.value?.trim();
            const command = panel.querySelector('#mcpCommandInput')?.value?.trim();
            if (!name) return;
            pluginStore.addMcpServer({
                name,
                transportType: command ? 'stdio' : 'sse',
                command: command || '',
                url: ''
            });
            if (panel.querySelector('#mcpNameInput')) panel.querySelector('#mcpNameInput').value = '';
            if (panel.querySelector('#mcpCommandInput')) panel.querySelector('#mcpCommandInput').value = '';
        });
    }

    // Prompt add button
    const promptAddBtn = panel.querySelector('#promptAddBtn');
    if (promptAddBtn) {
        promptAddBtn.addEventListener('click', () => {
            const title = panel.querySelector('#promptTitleInput')?.value?.trim();
            const content = panel.querySelector('#promptContentInput')?.value?.trim();
            const isSystemPrompt = panel.querySelector('#promptIsSystemInput')?.checked || false;
            if (!title || !content) return;
            bridge.send('SAVE_PROMPT', {
                id: crypto.randomUUID(),
                title, content, isSystemPrompt,
                createdAt: new Date().toISOString()
            });
            if (panel.querySelector('#promptTitleInput')) panel.querySelector('#promptTitleInput').value = '';
            if (panel.querySelector('#promptContentInput')) panel.querySelector('#promptContentInput').value = '';
            if (panel.querySelector('#promptIsSystemInput')) panel.querySelector('#promptIsSystemInput').checked = false;
            // Refresh list
            setTimeout(() => bridge.send('LIST_PROMPTS'), 200);
        });
    }

    // General section bindings
    bindGeneralSection();
}

function renderGeneralSection() {
    const s = settingsStore.state.settings;
    return `
        <label class="settings-field">
            <span>Theme</span>
            <select id="themeSelect">
                <option value="dark" ${s.theme === 'dark' ? 'selected' : ''}>Dark</option>
                <option value="light" ${s.theme === 'light' ? 'selected' : ''}>Light</option>
            </select>
        </label>
        <label class="settings-field">
            <span>Temperature: <span id="tempValue">${s.temperature}</span></span>
            <input type="range" id="tempSlider" min="0" max="2" step="0.1" value="${s.temperature}">
        </label>
        <label class="settings-field">
            <span>Font Size: <span id="fontSizeValue">${s.fontSize}</span>px</span>
            <input type="range" id="fontSizeSlider" min="10" max="24" step="1" value="${s.fontSize}">
        </label>
        <label class="settings-field">
            <span>System Prompt</span>
            <textarea id="systemPromptInput" rows="4" placeholder="Optional system instructions...">${esc(s.systemPrompt || '')}</textarea>
        </label>
        <label class="settings-field checkbox-field">
            <input type="checkbox" id="streamToggle" ${s.streamResponses ? 'checked' : ''}>
            <span>Stream responses</span>
        </label>
        <div style="margin-top: 16px; color: var(--text-muted); font-size: 0.8em;">Manifold Desktop v${settingsStore.state.version || '0.1.0'}</div>
    `;
}

function bindGeneralSection() {
    if (!panel) return;

    const themeSelect = panel.querySelector('#themeSelect');
    themeSelect?.addEventListener('change', () => {
        settingsStore.updateSettings({ theme: themeSelect.value });
        dirty = true;
        updateDirtyIndicator();
    });

    const tempSlider = panel.querySelector('#tempSlider');
    const tempValue = panel.querySelector('#tempValue');
    tempSlider?.addEventListener('input', () => { tempValue.textContent = tempSlider.value; });
    tempSlider?.addEventListener('change', () => {
        settingsStore.updateSettings({ temperature: parseFloat(tempSlider.value) });
        dirty = true;
        updateDirtyIndicator();
    });

    const fontSizeSlider = panel.querySelector('#fontSizeSlider');
    const fontSizeValue = panel.querySelector('#fontSizeValue');
    fontSizeSlider?.addEventListener('input', () => { fontSizeValue.textContent = fontSizeSlider.value; });
    fontSizeSlider?.addEventListener('change', () => {
        settingsStore.updateSettings({ fontSize: parseInt(fontSizeSlider.value) });
        dirty = true;
        updateDirtyIndicator();
    });

    const systemPromptInput = panel.querySelector('#systemPromptInput');
    systemPromptInput?.addEventListener('change', () => {
        settingsStore.updateSettings({ systemPrompt: systemPromptInput.value });
        dirty = true;
        updateDirtyIndicator();
    });

    const streamToggle = panel.querySelector('#streamToggle');
    streamToggle?.addEventListener('change', () => {
        settingsStore.updateSettings({ streamResponses: streamToggle.checked });
        dirty = true;
        updateDirtyIndicator();
    });
}

function renderProviders() {
    const list = panel?.querySelector('#providers-list');
    if (!list) return;

    const providers = settingsStore.state.providers || [];
    if (providers.length === 0) {
        list.innerHTML = '<div class="settings-empty">No providers configured</div>';
        return;
    }

    list.innerHTML = '';

    // Ollama endpoint config
    const ollamaSection = document.createElement('div');
    ollamaSection.className = 'provider-settings-item';
    ollamaSection.innerHTML = `
        <div class="provider-info">
            <div class="provider-name">Ollama (Local)</div>
            <div class="provider-status">Endpoint URL</div>
        </div>
        <div class="provider-key-row">
            <input type="text" placeholder="http://localhost:11434" value="${esc(settingsStore.state.settings.ollamaEndpoint || 'http://localhost:11434')}" class="provider-key-input ollama-endpoint-input">
            <button class="btn-sm ollama-endpoint-save">Save</button>
        </div>
    `;
    ollamaSection.querySelector('.ollama-endpoint-save').onclick = () => {
        const val = ollamaSection.querySelector('.ollama-endpoint-input').value.trim();
        settingsStore.updateSettings({ ollamaEndpoint: val });
    };
    list.appendChild(ollamaSection);

    for (const p of providers) {
        const item = document.createElement('div');
        item.className = 'provider-settings-item';

        const statusClass = p.requiresApiKey ? 'needs-key' : 'connected';
        const statusText = p.requiresApiKey ? 'Needs API key' : 'Ready';

        item.innerHTML = `
            <div class="provider-info">
                <div class="provider-name">${esc(p.displayName)}</div>
                <div class="provider-status ${statusClass}">${statusText}</div>
            </div>
            <div class="provider-key-row">
                <input type="password" placeholder="API key for ${esc(p.displayName)}" autocomplete="off" class="provider-key-input">
                <button class="btn-sm provider-key-save">Save</button>
            </div>
        `;

        const input = item.querySelector('.provider-key-input');
        item.querySelector('.provider-key-save').onclick = () => {
            settingsStore.setApiKey(p.id, input.value);
            input.value = '';
            input.placeholder = 'Key saved';
            setTimeout(() => { input.placeholder = `API key for ${p.displayName}`; }, 2000);
        };

        list.appendChild(item);
    }
}

function renderPlugins(state) {
    const list = panel?.querySelector('#plugins-list');
    if (!list) return;

    const plugins = state?.plugins || pluginStore.state.plugins || [];
    if (plugins.length === 0) {
        list.innerHTML = '<div class="settings-empty">No plugins installed</div>';
        return;
    }

    list.innerHTML = '';
    for (const p of plugins) {
        const item = document.createElement('div');
        item.className = 'plugin-settings-item';
        item.innerHTML = `
            <div class="plugin-info">
                <div class="plugin-name">${esc(p.name)}</div>
                <div class="plugin-desc">${esc(p.description || '')}</div>
            </div>
            <label class="toggle-switch">
                <input type="checkbox" ${p.enabled ? 'checked' : ''}>
                <span class="toggle-slider"></span>
            </label>
        `;
        item.querySelector('input').onchange = (e) => {
            pluginStore.enablePlugin(p.id, e.target.checked);
        };
        list.appendChild(item);
    }
}

function renderMcpSection() {
    return `
        <div class="mcp-add-row">
            <input type="text" id="mcpNameInput" placeholder="Server name" class="mcp-input">
            <input type="text" id="mcpCommandInput" placeholder="Command (e.g. npx server)" class="mcp-input">
            <button class="btn-sm" id="mcpAddBtn">Add</button>
        </div>
        <div id="mcp-servers-list"></div>
    `;
}

function renderUsage() {
    const list = panel?.querySelector('#usage-stats');
    if (!list) return;

    import('../services/pricing.js').then(({ getUsage, resetUsage }) => {
        const usage = getUsage();
        const providers = Object.entries(usage);

        if (providers.length === 0) {
            list.innerHTML = '<div class="settings-empty">No usage data yet</div>';
            return;
        }

        let html = '';
        let grandTotal = 0;
        for (const [pid, data] of providers) {
            grandTotal += data.totalCost || 0;
            html += `<div class="usage-row" style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--border-subtle);">
                <div class="usage-provider" style="font-weight:600;">${esc(pid)}</div>
                <div class="usage-detail" style="color:var(--text-secondary);">${(data.totalTokens || 0).toLocaleString()} tokens &middot; $${(data.totalCost || 0).toFixed(4)}</div>
            </div>`;
        }
        html += `<div class="usage-total" style="margin-top:12px;font-weight:700;font-size:1.1em;">Total: $${grandTotal.toFixed(4)}</div>`;
        html += `<button class="btn-sm usage-reset" style="margin-top:12px;">Reset Usage Data</button>`;

        list.innerHTML = html;
        list.querySelector('.usage-reset')?.addEventListener('click', () => {
            resetUsage();
            renderUsage();
        });
    });
}

function renderMcpServers() {
    const list = panel?.querySelector('#mcp-servers-list');
    if (!list) return;

    const servers = pluginStore.state.mcpServers || [];
    if (servers.length === 0) {
        list.innerHTML = '<div class="settings-empty">No MCP servers configured</div>';
        return;
    }

    list.innerHTML = '';
    for (const s of servers) {
        const item = document.createElement('div');
        item.className = 'mcp-server-item';
        item.innerHTML = `
            <div class="mcp-server-info">
                <div class="mcp-server-name">${esc(s.name || s.id)}</div>
                <div class="mcp-server-detail">${esc(s.transportType)} &middot; ${s.connected ? 'Connected' : 'Disconnected'} &middot; ${s.toolCount || 0} tools</div>
            </div>
            <button class="btn-sm mcp-server-remove">Remove</button>
        `;
        item.querySelector('.mcp-server-remove').onclick = () => {
            pluginStore.removeMcpServer(s.id);
        };
        list.appendChild(item);
    }
}

// Listen for MCP server list updates
bridge.on('MCP_SERVERS_LIST', () => renderMcpServers());

// Listen for prompt list updates
bridge.on('PROMPTS_LIST', data => {
    const list = panel?.querySelector('#prompts-list');
    if (!list) return;
    if (!data || data.length === 0) {
        list.innerHTML = '<div class="settings-empty">No saved prompts</div>';
        return;
    }
    list.innerHTML = '';
    for (const p of data) {
        const item = document.createElement('div');
        item.style.cssText = 'display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border-subtle);';
        const info = document.createElement('div');
        info.innerHTML = `<div style="font-weight:600;">${esc(p.title || 'Untitled')}</div>
            <div style="font-size:0.8em;color:var(--text-muted);">${p.isSystemPrompt ? 'System prompt' : 'User prompt'}</div>`;
        item.appendChild(info);
        const delBtn = document.createElement('button');
        delBtn.className = 'btn-sm';
        delBtn.textContent = '\u00D7';
        delBtn.onclick = () => {
            bridge.send('DELETE_PROMPT', { id: p.id });
            setTimeout(() => bridge.send('LIST_PROMPTS'), 200);
        };
        item.appendChild(delBtn);
        list.appendChild(item);
    }
});
bridge.on('PROMPT_SAVED', () => bridge.send('LIST_PROMPTS'));
bridge.on('PROMPT_DELETED', () => bridge.send('LIST_PROMPTS'));

function sync({ settings }) {
    if (!panel) return;
    const el = (id) => panel.querySelector(id);
    const themeSelect = el('#themeSelect');
    if (themeSelect) themeSelect.value = settings.theme;
    const tempSlider = el('#tempSlider');
    if (tempSlider) { tempSlider.value = settings.temperature; }
    const tempValue = el('#tempValue');
    if (tempValue) tempValue.textContent = settings.temperature;
    const fontSizeSlider = el('#fontSizeSlider');
    if (fontSizeSlider) fontSizeSlider.value = settings.fontSize;
    const fontSizeValue = el('#fontSizeValue');
    if (fontSizeValue) fontSizeValue.textContent = settings.fontSize;
    const systemPromptInput = el('#systemPromptInput');
    if (systemPromptInput) systemPromptInput.value = settings.systemPrompt || '';
    const streamToggle = el('#streamToggle');
    if (streamToggle) streamToggle.checked = settings.streamResponses;

    dirty = false;
    updateDirtyIndicator();
    renderProviders();
}

export function show() {
    if (!panel) return;
    visible = true;
    panel.classList.add('open');
    renderProviders();
    pluginStore.refresh();
}

export function hide() {
    if (!panel) return;
    visible = false;
    panel.classList.remove('open');
}

export function toggle() {
    visible ? hide() : show();
}

function esc(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function updateDirtyIndicator() {
    if (!panel) return;
    const dot = panel.querySelector('.settings-dirty-dot');
    if (dot) dot.style.display = dirty ? 'inline-block' : 'none';
}
