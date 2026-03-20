import * as bridge from '../services/bridge.js';
import { state } from '../services/settings-store.js';

let container = null;
let textarea = null;
let providerSelect = null;
let modelSelect = null;
let sendBtn = null;
let cancelBtn = null;
let streaming = false;
let activeTabType = 'home'; // 'chat' | 'terminal' | 'home'
const callbacks = { send: [], cancel: [] };

export function create(el) {
    container = el;
    render();
}

export function onSend(cb) { callbacks.send.push(cb); }
export function onCancel(cb) { callbacks.cancel.push(cb); }

export function setStreaming(isStreaming) {
    streaming = isStreaming;
    if (sendBtn) {
        sendBtn.style.display = streaming ? 'none' : '';
        sendBtn.disabled = streaming;
    }
    if (cancelBtn) cancelBtn.style.display = streaming ? '' : 'none';
    if (textarea) textarea.disabled = streaming;
}

export function setTabType(type) {
    activeTabType = type;
    render();
}

function render() {
    if (!container) return;
    container.innerHTML = '';

    if (activeTabType === 'home') return;
    if (activeTabType === 'terminal') return;

    // Chat input bar
    const row = document.createElement('div');
    row.className = 'input-row';

    // Provider/model selectors
    const selectors = document.createElement('div');
    selectors.className = 'input-selectors';

    providerSelect = document.createElement('select');
    providerSelect.className = 'input-provider-select';
    providerSelect.title = 'Provider';
    providerSelect.onchange = () => {
        bridge.send('LIST_MODELS', { providerId: providerSelect.value });
    };
    selectors.appendChild(providerSelect);

    modelSelect = document.createElement('select');
    modelSelect.className = 'input-model-select';
    modelSelect.title = 'Model';
    selectors.appendChild(modelSelect);

    row.appendChild(selectors);

    // Text area
    textarea = document.createElement('textarea');
    textarea.className = 'input-textarea';
    textarea.placeholder = 'Type a message...';
    textarea.setAttribute('aria-label', 'Chat message input');
    textarea.rows = 1;
    textarea.addEventListener('keydown', e => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            doSend();
        }
    });
    textarea.addEventListener('input', autoResize);

    // Token estimate
    const tokenEstimate = document.createElement('div');
    tokenEstimate.style.cssText = 'font-size: 0.75em; color: var(--text-muted); padding: 0 8px 4px; text-align: right;';
    textarea.addEventListener('input', () => {
        const count = Math.ceil((textarea.value.length || 0) / 4);
        tokenEstimate.textContent = count > 0 ? `~${count} tokens` : '';
    });

    row.appendChild(textarea);

    // Send button
    sendBtn = document.createElement('button');
    sendBtn.className = 'input-send-btn';
    sendBtn.textContent = 'Send';
    sendBtn.setAttribute('aria-label', 'Send message');
    sendBtn.onclick = doSend;
    row.appendChild(sendBtn);

    // Cancel button (hidden by default)
    cancelBtn = document.createElement('button');
    cancelBtn.className = 'input-cancel-btn';
    cancelBtn.textContent = 'Cancel';
    cancelBtn.setAttribute('aria-label', 'Cancel streaming');
    cancelBtn.style.display = 'none';
    cancelBtn.onclick = () => callbacks.cancel.forEach(cb => cb());
    row.appendChild(cancelBtn);

    // Prompt library button
    const promptBtn = document.createElement('button');
    promptBtn.className = 'btn-sm input-prompt-btn';
    promptBtn.textContent = '\u{1F4CB}';
    promptBtn.title = 'Insert saved prompt';
    promptBtn.style.cssText = 'font-size: 1.1em; padding: 4px 8px; margin-left: 4px;';
    promptBtn.onclick = () => {
        bridge.send('LIST_PROMPTS');
        const unsub = bridge.on('PROMPTS_LIST', data => {
            unsub();
            // Build dropdown
            const existing = document.querySelector('.prompt-dropdown');
            if (existing) existing.remove();
            const dropdown = document.createElement('div');
            dropdown.className = 'prompt-dropdown';
            dropdown.style.cssText = 'position:absolute;bottom:100%;right:0;background:var(--bg-secondary);border:1px solid var(--border);border-radius:var(--radius, 6px);max-height:200px;overflow-y:auto;min-width:200px;z-index:100;box-shadow:0 4px 12px rgba(0,0,0,0.3);';
            if (!data || data.length === 0) {
                dropdown.innerHTML = '<div style="padding:12px;color:var(--text-muted);font-size:0.85em;">No saved prompts</div>';
            } else {
                for (const p of data) {
                    const item = document.createElement('div');
                    item.style.cssText = 'padding:8px 12px;cursor:pointer;font-size:0.85em;';
                    item.textContent = p.title || 'Untitled';
                    item.onmouseenter = () => { item.style.background = 'var(--bg-hover)'; };
                    item.onmouseleave = () => { item.style.background = ''; };
                    item.onclick = () => {
                        if (p.isSystemPrompt) {
                            import('../services/settings-store.js').then(s => {
                                s.updateSettings({ systemPrompt: p.content });
                            });
                        } else {
                            textarea.value += p.content;
                            autoResize();
                        }
                        dropdown.remove();
                    };
                    dropdown.appendChild(item);
                }
            }
            promptBtn.style.position = 'relative';
            promptBtn.appendChild(dropdown);
            setTimeout(() => {
                document.addEventListener('click', () => dropdown.remove(), { once: true });
            }, 0);
        });
    };
    row.appendChild(promptBtn);

    container.appendChild(row);
    container.appendChild(tokenEstimate);

    // Populate providers from current state
    populateProviders();

    // Listen for provider/model list updates
    bridge.on('PROVIDERS_LIST', populateProviderOptions);
    bridge.on('MODELS_LIST', populateModelOptions);

    bridge.send('LIST_PROVIDERS');
}

function doSend() {
    if (!textarea) return;
    const text = textarea.value.trim();
    if (!text) return;
    textarea.value = '';
    autoResize();

    const providerId = providerSelect?.value || '';
    const modelId = modelSelect?.value || '';

    callbacks.send.forEach(cb => cb({
        text,
        providerId,
        modelId
    }));
}

function autoResize() {
    if (!textarea) return;
    textarea.style.height = 'auto';
    textarea.style.height = Math.min(textarea.scrollHeight, 200) + 'px';
    textarea.style.overflowY = textarea.scrollHeight > 200 ? 'auto' : 'hidden';
}

function populateProviders() {
    if (!providerSelect) return;
    const providers = state.providers || [];
    providerSelect.innerHTML = '';
    for (const p of providers) {
        const opt = document.createElement('option');
        opt.value = p.id;
        opt.textContent = p.displayName;
        providerSelect.appendChild(opt);
    }
    if (providers.length > 0) {
        bridge.send('LIST_MODELS', { providerId: providers[0].id });
    }
}

function populateProviderOptions(data) {
    if (!providerSelect) return;
    providerSelect.innerHTML = '';
    for (const p of data) {
        const opt = document.createElement('option');
        opt.value = p.id;
        opt.textContent = p.displayName;
        providerSelect.appendChild(opt);
    }
    if (data.length > 0) {
        bridge.send('LIST_MODELS', { providerId: data[0].id });
    }
}

function populateModelOptions(data) {
    if (!modelSelect) return;
    modelSelect.innerHTML = '';
    const models = data.models || data || [];
    for (const m of models) {
        const opt = document.createElement('option');
        opt.value = m.id || m;
        opt.textContent = m.displayName || m.id || m;
        modelSelect.appendChild(opt);
    }
    // Select the model from settings if it matches
    if (state.settings.model) {
        modelSelect.value = state.settings.model;
    }
}

export function focus() {
    textarea?.focus();
}
