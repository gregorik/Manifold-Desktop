const API_BASE = 'https://generativelanguage.googleapis.com/v1beta/models';

export async function streamChat(apiKey, model, messages, options = {}) {
    const contents = messages.map(m => ({
        role: m.role === 'user' ? 'user' : 'model',
        parts: [{ text: m.content || m.text || '' }]
    }));

    const body = {
        contents,
        generationConfig: { temperature: options.temperature ?? 0.7 }
    };
    if (options.systemPrompt) {
        body.systemInstruction = { parts: [{ text: options.systemPrompt }] };
    }

    const url = `${API_BASE}/${model}:streamGenerateContent?key=${apiKey}&alt=sse`;
    const resp = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });

    if (!resp.ok) {
        const err = await resp.text();
        throw new Error(`Gemini API error ${resp.status}: ${err}`);
    }

    return resp.body;
}

export function getModels() {
    return [
        { id: 'gemini-2.0-flash', displayName: 'Gemini 2.0 Flash', isFree: true },
        { id: 'gemini-2.0-flash-lite', displayName: 'Gemini 2.0 Flash Lite', isFree: true },
        { id: 'gemini-1.5-flash', displayName: 'Gemini 1.5 Flash', isFree: true }
    ];
}
