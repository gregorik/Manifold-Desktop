// Reactive settings store, initialized from HOST_READY
import * as bridge from './bridge.js';

const changeListeners = [];
const readyListeners = [];

export const state = {
    providerKeys: {},
    providers: [],
    version: '',
    settings: {
        model: 'gemini-2.0-flash',
        temperature: 0.7,
        theme: 'dark',
        fontSize: 14,
        systemPrompt: '',
        streamResponses: true
    },
    ready: false
};

export function onReady(cb) {
    if (state.ready) cb(state);
    else readyListeners.push(cb);
}

export function onChange(cb) {
    changeListeners.push(cb);
}

export function updateSettings(partial) {
    Object.assign(state.settings, partial);
    bridge.send('SAVE_SETTINGS', state.settings);
    notify();
}

export function setApiKey(providerId, apiKey) {
    bridge.send('SET_API_KEY', { providerId, apiKey });
}

function notify() {
    changeListeners.forEach(cb => cb(state));
}

// Initialize from host
bridge.on('HOST_READY', payload => {
    state.providerKeys = payload.providerKeys || {};
    state.providers = payload.providers || [];
    state.version = payload.version || '';
    Object.assign(state.settings, payload.settings || {});
    state.ready = true;
    applyTheme(state.settings.theme);
    applyFontSize(state.settings.fontSize);
    notify();
    readyListeners.forEach(cb => cb(state));
    readyListeners.length = 0;
});

bridge.on('SETTINGS_UPDATED', payload => {
    Object.assign(state.settings, payload);
    applyTheme(state.settings.theme);
    applyFontSize(state.settings.fontSize);
    notify();
});

bridge.on('PROVIDERS_LIST', data => {
    state.providers = data || [];
    notify();
});

function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme || 'dark');
}

function applyFontSize(size) {
    document.documentElement.style.setProperty('--font-size', (size || 14) + 'px');
}
