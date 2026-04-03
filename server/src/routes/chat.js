import { Router } from 'express';
import { config } from '../config.js';
import { streamChat } from '../providers/gemini.js';
import { checkQuota, incrementQuota } from '../store/quota-store.js';

const router = Router();

router.post('/v1/chat', async (req, res) => {
    const { model, messages, temperature, systemPrompt } = req.body;

    if (!checkQuota(req.deviceId, config.dailyQuotaPerDevice)) {
        return res.status(429).json({ error: 'Daily quota exceeded' });
    }

    if (!config.geminiApiKey) {
        return res.status(500).json({ error: 'Server API key not configured' });
    }

    try {
        const stream = await streamChat(config.geminiApiKey, model || 'gemini-2.0-flash', messages || [], {
            temperature, systemPrompt
        });

        res.setHeader('Content-Type', 'text/event-stream');
        res.setHeader('Cache-Control', 'no-cache');
        res.setHeader('Connection', 'keep-alive');

        incrementQuota(req.deviceId);

        const reader = stream.getReader();
        const decoder = new TextDecoder();

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            res.write(decoder.decode(value, { stream: true }));
        }

        res.write('data: [DONE]\n\n');
        res.end();
    } catch (err) {
        if (!res.headersSent) {
            res.status(500).json({ error: err.message });
        } else {
            res.end();
        }
    }
});

export default router;
