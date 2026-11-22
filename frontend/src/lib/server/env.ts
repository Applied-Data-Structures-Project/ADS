import { createHash, timingSafeEqual } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

interface CachedEnvFile {
	mtimeMs: number;
	values: Record<string, string>;
}

const envFileCache = new Map<string, CachedEnvFile>();

/**
 * Parses KEY=value lines from a .env file using the same rules as C++ loadEnvFile().
 */
export function parseEnvContent(content: string): Record<string, string> {
	const out: Record<string, string> = {};
	const lines = content.split('\n');
	for (let rawLine of lines) {
		if (rawLine.endsWith('\r')) {
			rawLine = rawLine.slice(0, -1);
		}
		if (!rawLine || rawLine.startsWith('#')) continue;
		const eq = rawLine.indexOf('=');
		if (eq === -1) continue;
		const name = rawLine.slice(0, eq);
		let value = rawLine.slice(eq + 1);
		if (value.length >= 2 && value.startsWith('"') && value.endsWith('"')) {
			value = value.slice(1, -1);
		}
		out[name] = value;
	}
	return out;
}

function readEnvFile(filePath: string): Record<string, string> {
	try {
		const stat = fs.statSync(filePath);
		const cached = envFileCache.get(filePath);
		if (cached && cached.mtimeMs === stat.mtimeMs) {
			return cached.values;
		}
		const content = fs.readFileSync(filePath, 'utf8');
		const values = parseEnvContent(content);
		envFileCache.set(filePath, { mtimeMs: stat.mtimeMs, values });
		return values;
	} catch {
		envFileCache.delete(filePath);
		return {};
	}
}

/**
 * Reads an environment variable from process.env first, falling back to <repo>/.env
 * and then <cwd>/.env so one root .env works for both C++ and SvelteKit.
 */
export function getServerEnv(name: string, fallback = ''): string {
	const direct = process.env[name];
	if (direct && direct.length > 0) return direct;

	const candidateFiles = [
		path.resolve(process.cwd(), '..', '.env'),
		path.resolve(process.cwd(), '.env')
	];
	for (const filePath of candidateFiles) {
		const parsed = readEnvFile(filePath);
		const value = parsed[name];
		if (value && value.length > 0) return value;
	}
	return fallback;
}

/** Returns lowercase 64-char SHA-256 hex digest of UTF-8 input. */
export function sha256Hex(input: string): string {
	return createHash('sha256').update(input, 'utf8').digest('hex');
}

/**
 * Computes the shared internal relay token matching C++ SmtpNotifier::computeRelayToken.
 */
export function computeRelayToken(secret: string, smtpUser: string, smtpPass: string): string {
	if (secret.length > 0) {
		return sha256Hex('ads-email-relay-secret:' + secret);
	}
	if (smtpUser.length > 0) {
		return sha256Hex('ads-email-relay:' + smtpUser + ':' + smtpPass);
	}
	return '';
}

/** Constant-time token verification for the internal localhost email relay. */
export function verifyRelayToken(provided: string | null, expected: string): boolean {
	if (!provided || !expected) return false;
	const a = Buffer.from(provided, 'utf8');
	const b = Buffer.from(expected, 'utf8');
	if (a.length !== b.length) return false;
	return timingSafeEqual(a, b);
}

/** Checks whether the caller IP is a local loopback address. */
export function isLoopbackAddress(addr: string): boolean {
	return (
		addr === '127.0.0.1' ||
		addr === '::1' ||
		addr === '::ffff:127.0.0.1' ||
		addr === 'localhost'
	);
}
