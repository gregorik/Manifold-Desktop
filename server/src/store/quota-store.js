import { readFileSync, writeFileSync, existsSync } from 'fs';

const QUOTA_FILE = './data/quotas.json';
let quotas = {};

function load() {
    try {
        if (existsSync(QUOTA_FILE)) {
            quotas = JSON.parse(readFileSync(QUOTA_FILE, 'utf8'));
        }
    } catch { quotas = {}; }
}

function save() {
    try {
        writeFileSync(QUOTA_FILE, JSON.stringify(quotas, null, 2));
    } catch { /* ignore */ }
}

function todayKey() {
    return new Date().toISOString().slice(0, 10);
}

export function checkQuota(deviceId, limit) {
    load();
    const day = todayKey();
    const key = `${deviceId}:${day}`;
    const used = quotas[key] || 0;
    return used < limit;
}

export function incrementQuota(deviceId) {
    load();
    const day = todayKey();
    const key = `${deviceId}:${day}`;
    quotas[key] = (quotas[key] || 0) + 1;
    save();
}

export function getQuota(deviceId) {
    load();
    const day = todayKey();
    const key = `${deviceId}:${day}`;
    return quotas[key] || 0;
}
