// Provider API — routes all chat requests through the C++ bridge
import * as bridge from './bridge.js';

export function sendChat({ providerId, modelId, messages, systemPrompt, temperature, tools }) {
    bridge.send('CHAT_SEND', {
        provider: providerId,
        model: modelId,
        messages,
        systemPrompt: systemPrompt || '',
        temperature: temperature ?? 0.7,
        tools: tools || []
    });
}

export function cancelChat() {
    bridge.send('CHAT_CANCEL');
}

export function listProviders() {
    bridge.send('LIST_PROVIDERS');
    return bridge.once('PROVIDERS_LIST');
}

export function listModels(providerId) {
    bridge.send('LIST_MODELS', { providerId });
    return bridge.once('MODELS_LIST');
}

export function setApiKey(providerId, apiKey) {
    bridge.send('SET_API_KEY', { providerId, apiKey });
}

export function validateKey(providerId, apiKey) {
    bridge.send('VALIDATE_KEY', { providerId, apiKey });
    return bridge.once('KEY_VALIDATION');
}
