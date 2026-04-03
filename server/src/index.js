import express from 'express';
import cors from 'cors';
import { config } from './config.js';
import { deviceId } from './middleware/device-id.js';
import { rateLimiter } from './middleware/rate-limiter.js';
import chatRouter from './routes/chat.js';
import modelsRouter from './routes/models.js';
import quotaRouter from './routes/quota.js';
import { mkdirSync } from 'fs';

// Ensure data directory exists
try { mkdirSync('./data', { recursive: true }); } catch {}

const app = express();

app.use(cors());
app.use(express.json());

// Health check (no auth required)
app.get('/health', (req, res) => res.json({ status: 'ok' }));

// All API routes require device ID and rate limiting
app.use('/v1', deviceId, rateLimiter(config.rpmLimit));
app.use(chatRouter);
app.use(modelsRouter);
app.use(quotaRouter);

app.listen(config.port, () => {
    console.log(`Manifold Proxy listening on port ${config.port}`);
});
