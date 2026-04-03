import { Router } from 'express';
import { getModels } from '../providers/gemini.js';

const router = Router();

router.get('/v1/models', (req, res) => {
    const models = getModels().map(m => ({
        ...m,
        isFree: true
    }));
    res.json({ data: models });
});

export default router;
