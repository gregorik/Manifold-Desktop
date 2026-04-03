import { Router } from 'express';
import { config } from '../config.js';
import { getQuota } from '../store/quota-store.js';

const router = Router();

router.get('/v1/quota', (req, res) => {
    const used = getQuota(req.deviceId);
    res.json({
        used,
        limit: config.dailyQuotaPerDevice,
        remaining: Math.max(0, config.dailyQuotaPerDevice - used)
    });
});

export default router;
