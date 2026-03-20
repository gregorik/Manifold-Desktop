import * as bridge from '../services/bridge.js';
import { getModelCost, formatCost, recordUsage } from '../services/pricing.js';

export function create(slots) {
    const element = document.createElement('div');
    element.className = 'tab-pane compare-pane';

    // Shared messages area
    const shared = document.createElement('div');
    shared.className = 'compare-shared';
    element.appendChild(shared);

    // Columns
    const columnsEl = document.createElement('div');
    columnsEl.className = 'compare-columns';
    element.appendChild(columnsEl);

    const columns = [];
    for (let i = 0; i < slots.length; i++) {
        const col = document.createElement('div');
        col.className = 'compare-column';

        const header = document.createElement('div');
        header.className = 'compare-column-header';
        header.textContent = `${slots[i].providerName || slots[i].providerId} / ${slots[i].modelId}`;
        col.appendChild(header);

        const messages = document.createElement('div');
        messages.className = 'compare-column-messages';
        col.appendChild(messages);

        const footer = document.createElement('div');
        footer.className = 'compare-column-footer';
        col.appendChild(footer);

        columnsEl.appendChild(col);
        columns.push({ el: col, messages, footer, text: '', streaming: false });
    }

    const allMessages = [];
    const unsubs = [];

    unsubs.push(bridge.on('COMPARE_CHUNK', data => {
        const col = columns[data.slotIndex];
        if (!col) return;
        if (!col.streaming) {
            col.streaming = true;
            col.text = '';
            col.msgEl = document.createElement('div');
            col.msgEl.className = 'message assistant';
            col.messages.appendChild(col.msgEl);
        }
        col.text += data.text || '';
        col.msgEl.innerHTML = renderMarkdownSafe(col.text);
        col.messages.scrollTop = col.messages.scrollHeight;
    }));

    unsubs.push(bridge.on('COMPARE_DONE', data => {
        const col = columns[data.slotIndex];
        if (!col) return;
        col.streaming = false;
        const tokens = (data.promptTokens || 0) + (data.completionTokens || 0);
        const slot = slots[data.slotIndex];
        const cost = getModelCost(slot.modelId, data.promptTokens || 0, data.completionTokens || 0);
        col.footer.textContent = cost
            ? `${tokens} tokens \u00B7 ${formatCost(cost)}`
            : `${tokens} tokens`;
        if (slot.providerId) {
            recordUsage(slot.providerId, slot.modelId, data.promptTokens || 0, data.completionTokens || 0);
        }
    }));

    unsubs.push(bridge.on('COMPARE_ERROR', data => {
        const col = columns[data.slotIndex];
        if (!col) return;
        col.streaming = false;
        const errEl = document.createElement('div');
        errEl.className = 'message system';
        errEl.textContent = 'Error: ' + (data.error || 'Unknown');
        col.messages.appendChild(errEl);
    }));

    return {
        element,
        slots,
        addUserMessage(text) {
            const msgEl = document.createElement('div');
            msgEl.className = 'compare-user-message message user';
            msgEl.textContent = text;
            shared.appendChild(msgEl);
            allMessages.push({ role: 'user', content: text });
            // Reset columns for new response
            columns.forEach(c => { c.streaming = false; c.text = ''; c.footer.textContent = ''; });
        },
        getMessages() {
            return allMessages.map(m => ({ role: m.role, content: m.content }));
        },
        destroy() {
            unsubs.forEach(u => u?.());
            bridge.send('COMPARE_CANCEL');
        }
    };
}

function renderMarkdownSafe(text) {
    if (typeof marked !== 'undefined') {
        try { return marked.parse(text); } catch {}
    }
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>');
}
