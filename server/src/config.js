import 'dotenv/config';

export const config = {
    port: parseInt(process.env.PORT || '3000'),
    geminiApiKey: process.env.GEMINI_API_KEY || '',
    dailyQuotaPerDevice: parseInt(process.env.DAILY_QUOTA_PER_DEVICE || '100'),
    rpmLimit: parseInt(process.env.RPM_LIMIT || '10'),
};
