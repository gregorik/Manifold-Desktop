const windows = new Map(); // deviceId -> { count, resetAt }

export function rateLimiter(rpmLimit) {
    return (req, res, next) => {
        const id = req.deviceId;
        if (!id) return next();

        const now = Date.now();
        let entry = windows.get(id);

        if (!entry || now > entry.resetAt) {
            entry = { count: 0, resetAt: now + 60000 };
            windows.set(id, entry);
        }

        entry.count++;
        if (entry.count > rpmLimit) {
            return res.status(429).json({
                error: 'Rate limit exceeded',
                retryAfter: Math.ceil((entry.resetAt - now) / 1000)
            });
        }

        next();
    };
}
