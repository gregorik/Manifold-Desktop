// Model pricing data — cost per 1M tokens (USD)
const PRICING = {
    'gpt-4o': { input: 2.50, output: 10.00 },
    'gpt-4o-mini': { input: 0.15, output: 0.60 },
    'gpt-4-turbo': { input: 10.00, output: 30.00 },
    'gpt-3.5-turbo': { input: 0.50, output: 1.50 },
    'claude-3-5-sonnet': { input: 3.00, output: 15.00 },
    'claude-3-5-haiku': { input: 0.80, output: 4.00 },
    'claude-3-opus': { input: 15.00, output: 75.00 },
    'gemini-2.0-flash': { input: 0.10, output: 0.40 },
    'gemini-1.5-pro': { input: 1.25, output: 5.00 },
    'gemini-1.5-flash': { input: 0.075, output: 0.30 },
};

export function getModelCost(modelId, promptTokens, completionTokens) {
    // Fuzzy match: find longest matching key
    const key = Object.keys(PRICING)
        .filter(k => modelId.toLowerCase().includes(k.toLowerCase()))
        .sort((a, b) => b.length - a.length)[0];

    if (!key) return null;

    const p = PRICING[key];
    const inputCost = (promptTokens / 1_000_000) * p.input;
    const outputCost = (completionTokens / 1_000_000) * p.output;
    return {
        inputCost,
        outputCost,
        totalCost: inputCost + outputCost,
        model: key
    };
}

export function formatCost(cost) {
    if (!cost) return '';
    if (cost.totalCost < 0.001) return `<$0.001`;
    return `$${cost.totalCost.toFixed(4)}`;
}

// Usage tracking in localStorage
const USAGE_KEY = 'manifold_usage';

export function recordUsage(providerId, modelId, promptTokens, completionTokens) {
    const usage = JSON.parse(localStorage.getItem(USAGE_KEY) || '{}');
    if (!usage[providerId]) usage[providerId] = { totalTokens: 0, totalCost: 0 };

    usage[providerId].totalTokens += promptTokens + completionTokens;

    const cost = getModelCost(modelId, promptTokens, completionTokens);
    if (cost) usage[providerId].totalCost += cost.totalCost;

    usage[providerId].lastUpdated = new Date().toISOString();
    localStorage.setItem(USAGE_KEY, JSON.stringify(usage));
}

export function getUsage() {
    return JSON.parse(localStorage.getItem(USAGE_KEY) || '{}');
}

export function resetUsage() {
    localStorage.removeItem(USAGE_KEY);
}
