import { json } from '@sveltejs/kit';
import nodemailer from 'nodemailer';
import type { RequestHandler } from './$types';
import {
	computeRelayToken,
	getServerEnv,
	isLoopbackAddress,
	verifyRelayToken
} from '#lib/server/env.ts';

function jsonError(status: number, error: string) {
	return json({ error }, { status });
}

export const POST: RequestHandler = async ({ request, getClientAddress }) => {
	let clientAddr = '';
	try {
		clientAddr = getClientAddress();
	} catch {
		clientAddr = '';
	}
	if (!isLoopbackAddress(clientAddr)) {
		return jsonError(403, 'Forbidden');
	}

	const smtpUser = getServerEnv('SMTP_USER');
	const smtpPass = getServerEnv('SMTP_PASS');
	const relaySecret = getServerEnv('EMAIL_RELAY_SECRET');
	const expectedToken = computeRelayToken(relaySecret, smtpUser, smtpPass);

	if (!smtpUser && !relaySecret) {
		return jsonError(503, 'SMTP_USER is not set.');
	}
	if (!verifyRelayToken(request.headers.get('x-internal-email-token'), expectedToken)) {
		return jsonError(403, 'Forbidden');
	}
	if (!smtpUser) {
		return jsonError(503, 'SMTP_USER is not set.');
	}

	let body: unknown;
	try {
		body = await request.json();
	} catch {
		return jsonError(400, 'Invalid email payload.');
	}
	if (!body || typeof body !== 'object' || Array.isArray(body)) {
		return jsonError(400, 'Invalid email payload.');
	}

	const payload = body as Record<string, unknown>;
	const to = typeof payload.to === 'string' ? payload.to.trim() : '';
	const subject = typeof payload.subject === 'string' ? payload.subject : '';
	const textBody = typeof payload.body === 'string' ? payload.body : '';
	if (!to || !subject || !textBody) {
		return jsonError(400, 'Invalid email payload.');
	}

	const smtpUrlRaw = getServerEnv('SMTP_URL', 'smtps://smtp.gmail.com:465');
	let parsedUrl: URL;
	try {
		parsedUrl = new URL(smtpUrlRaw);
	} catch {
		return jsonError(500, 'Invalid SMTP_URL configuration.');
	}

	if (parsedUrl.protocol !== 'smtps:' && parsedUrl.protocol !== 'smtp:') {
		return jsonError(500, 'Unsupported SMTP protocol.');
	}

	const secure = parsedUrl.protocol === 'smtps:';
	const host = parsedUrl.hostname;
	const port = parsedUrl.port ? Number(parsedUrl.port) : secure ? 465 : 587;
	const isLocal = host === 'localhost' || host === '127.0.0.1' || host === '::1';
	const from = getServerEnv('SMTP_FROM', smtpUser);

	const transporter = nodemailer.createTransport({
		host,
		port,
		secure,
		ignoreTLS: isLocal && !secure,
		auth: smtpUser ? { user: smtpUser, pass: smtpPass } : undefined,
		connectionTimeout: 10_000,
		greetingTimeout: 10_000,
		socketTimeout: 15_000
	});

	try {
		await transporter.sendMail({
			from: `AUC Exchange Matcher <${from}>`,
			to,
			subject,
			text: textBody
		});
		return json({ ok: true }, { status: 200 });
	} catch (err) {
		console.error(`Email to ${to} failed:`, err);
		return jsonError(502, 'Failed to send email.');
	}
};
