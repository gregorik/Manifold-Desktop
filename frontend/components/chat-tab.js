import * as bridge from '../services/bridge.js';
import { renderMessage, renderToolCall, renderToolResult } from './message-renderer.js';
import { getModelCost, formatCost, recordUsage } from '../services/pricing.js';

export function create(sessionId) {
    const element = document.createElement('div');
    element.className = 'tab-pane chat-pane';

    const messagesEl = document.createElement('div');
    messagesEl.className = 'chat-messages';
    element.appendChild(messagesEl);

    // Empty state
    const emptyState = document.createElement('div');
    emptyState.className = 'chat-empty';
    emptyState.textContent = 'Start a conversation';
    messagesEl.appendChild(emptyState);

    let streaming = false;
    let streamingEl = null;
    let streamingText = '';
    const messages = [];

    // Listen for chunks scoped to this session
    const unsubs = [];

    unsubs.push(bridge.on('CHAT_CHUNK', data => {
        if (!streaming) {
            const empty = messagesEl.querySelector('.chat-empty');
            if (empty) empty.remove();
            streaming = true;
            streamingText = '';
            streamingEl = document.createElement('div');
            streamingEl.className = 'message assistant';
            messagesEl.appendChild(streamingEl);
            // Add streaming indicator
            const indicator = document.createElement('span');
            indicator.className = 'streaming-indicator';
            streamingEl.appendChild(indicator);
        }

        if (data.text) {
            streamingText += data.text;
            streamingEl.innerHTML = renderMarkdownSafe(streamingText);
        }

        if (data.toolCall) {
            messagesEl.appendChild(renderToolCall(data.toolCall));
        }

        if (data.done) {
            streaming = false;
            if (streamingEl) {
                streamingEl.innerHTML = renderMarkdownSafe(streamingText);
            }
        }

        scrollToBottom(messagesEl);
    }));

    unsubs.push(bridge.on('CHAT_DONE', data => {
        streaming = false;

        // Display cost badge
        if (data.promptTokens || data.completionTokens) {
            const cost = getModelCost(
                document.querySelector('.input-model-select')?.value || '',
                data.promptTokens, data.completionTokens
            );
            const badge = document.createElement('div');
            badge.className = 'message-cost';
            const tokens = data.promptTokens + data.completionTokens;
            badge.textContent = cost
                ? `${formatCost(cost)} (${tokens} tokens)`
                : `${tokens} tokens`;
            if (streamingEl) streamingEl.appendChild(badge);

            // Record usage
            const providerId = document.querySelector('.input-provider-select')?.value || '';
            const modelId = document.querySelector('.input-model-select')?.value || '';
            recordUsage(providerId, modelId, data.promptTokens, data.completionTokens);
        }

        streamingEl = null;
    }));

    unsubs.push(bridge.on('CHAT_ERROR', data => {
        streaming = false;
        const errEl = document.createElement('div');
        errEl.className = 'message system error-message';
        const errText = (data.error || 'Unknown error').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        errEl.innerHTML = `<span>Error: ${errText}</span><button class="btn-sm error-retry">Retry</button>`;
        errEl.querySelector('.error-retry').onclick = () => {
            errEl.remove();
        };
        messagesEl.appendChild(errEl);
        scrollToBottom(messagesEl);
    }));

    // Load existing session data
    if (sessionId) {
        bridge.send('LOAD_SESSION', { id: sessionId });
        const unsubLoad = bridge.on('SESSION_DATA', data => {
            unsubLoad();
            if (data && data.messages) {
                for (const msg of data.messages) {
                    messages.push(msg);
                    messagesEl.appendChild(renderMessage(msg));
                }
                scrollToBottom(messagesEl);
            }
        });
        unsubs.push(unsubLoad);
    }

    return {
        element,
        sessionId,
        addUserMessage(text) {
            const empty = messagesEl.querySelector('.chat-empty');
            if (empty) empty.remove();
            const msg = { role: 'user', content: text };
            messages.push(msg);
            messagesEl.appendChild(renderMessage(msg));
            scrollToBottom(messagesEl);
        },
        getMessages() {
            return messages.map(m => ({ role: m.role, content: m.content || m.text || '' }));
        },
        destroy() {
            unsubs.forEach(u => u?.());
        }
    };
}

function scrollToBottom(el) {
    requestAnimationFrame(() => { el.scrollTop = el.scrollHeight; });
}

function renderMarkdownSafe(text) {
    if (typeof marked !== 'undefined') {
        try { return marked.parse(text); } catch {}
    }
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>');
}
