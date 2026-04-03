export function deviceId(req, res, next) {
    const id = req.headers['x-manifold-device-id'];
    if (!id) {
        return res.status(400).json({ error: 'Missing X-Manifold-Device-ID header' });
    }
    req.deviceId = id;
    next();
}
