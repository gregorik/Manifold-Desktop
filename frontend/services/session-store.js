// In-memory session cache, syncs to C++ host
import * as bridge from './bridge.js';

const sessions = new Map(); // id -> session data
const changeListeners = [];
let sessionList = []; // [{id, title, model, createdAt, updatedAt}]

export function onChange(cb) { changeListeners.push(cb); }
function notify() { changeListeners.forEach(cb => cb()); }

export function getList() { return sessionList; }
export function get(id) { return sessions.get(id); }

export function generateId() {
    return Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
}

export function createSession(id, { title = 'New Chat', model = 'gemini-2.0-flash' } = {}) {
    const now = new Date().toISOString();
    const session = {
        id,
        title,
        model,
        messages: [],
        createdAt: now,
        updatedAt: now
    };
    sessions.set(id, session);
    save(id);
    refreshList();
    return session;
}

export function addMessage(sessionId, role, text, parts) {
    const session = sessions.get(sessionId);
    if (!session) return;
    session.messages.push({ role, text, parts, timestamp: new Date().toISOString() });
    session.updatedAt = new Date().toISOString();

    // Auto-title from first user message
    if (session.messages.length === 1 && role === 'user') {
        session.title = text.slice(0, 60) + (text.length > 60 ? '...' : '');
    }
    save(sessionId);
    notify();
}

export function updateModelMessage(sessionId, text) {
    const session = sessions.get(sessionId);
    if (!session || session.messages.length === 0) return;
    const last = session.messages[session.messages.length - 1];
    if (last.role === 'model') {
        last.text = text;
        session.updatedAt = new Date().toISOString();
    }
}

export function save(id) {
    const session = sessions.get(id);
    if (!session) return;
    bridge.send('SAVE_SESSION', { id, data: session });
}

export function deleteSession(id) {
    sessions.delete(id);
    bridge.send('DELETE_SESSION', { id });
    refreshList();
    notify();
}

export async function load(id) {
    if (sessions.has(id)) return sessions.get(id);
    bridge.send('LOAD_SESSION', { id });
    const data = await bridge.once('SESSION_DATA');
    if (data && data !== 'null') {
        sessions.set(id, data);
        return data;
    }
    return null;
}

export function refreshList() {
    bridge.send('LIST_SESSIONS');
}

bridge.on('SESSIONS_LOADED', list => {
    sessionList = list || [];
    notify();
});

bridge.on('SESSION_DATA', data => {
    if (data && data.id) {
        sessions.set(data.id, data);
    }
});
