// Bridge between JS frontend and C++ host via WebView2 postMessage
const listeners = new Map();

export function send(type, payload = {}) {
    if (window.chrome?.webview) {
        window.chrome.webview.postMessage(JSON.stringify({ type, ...payload }));
    }
}

export function on(type, callback) {
    if (!listeners.has(type)) listeners.set(type, []);
    listeners.get(type).push(callback);
    return () => {
        const cbs = listeners.get(type);
        const idx = cbs.indexOf(callback);
        if (idx >= 0) cbs.splice(idx, 1);
    };
}

export function once(type) {
    return new Promise(resolve => {
        const unsub = on(type, payload => {
            unsub();
            resolve(payload);
        });
    });
}

// Listen for messages from C++ host
if (window.chrome?.webview) {
    window.chrome.webview.addEventListener('message', e => {
        try {
            const msg = JSON.parse(e.data);
            const { type, payload } = msg;
            const cbs = listeners.get(type);
            if (cbs) cbs.forEach(cb => cb(payload));
        } catch {
            // Non-JSON message, ignore
        }
    });
}
