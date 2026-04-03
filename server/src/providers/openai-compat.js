export async function streamChat(endpoint, apiKey, model, messages, options = {}) {
    const body = {
        model,
        messages: messages.map(m => ({ role: m.role, content: m.content || m.text || '' })),
        temperature: options.temperature ?? 0.7,
        stream: true
    };

    const headers = { 'Content-Type': 'application/json' };
    if (apiKey) headers['Authorization'] = `Bearer ${apiKey}`;

    const resp = await fetch(`${endpoint}/v1/chat/completions`, {
        method: 'POST',
        headers,
        body: JSON.stringify(body)
    });

    if (!resp.ok) {
        const err = await resp.text();
        throw new Error(`API error ${resp.status}: ${err}`);
    }

    return resp.body;
}
