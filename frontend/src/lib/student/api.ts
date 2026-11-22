// Group 3's calls to the C++ server (see docs/api.md).
// Every request says who is signed in with the X-User-Email header, which the
// server reads with currentUser(). Errors come back as {"error": "..."}.

import { auth } from '#lib/auth/store.svelte.ts';

/** A sentence the page can show, e.g. "Your application is submitted and can no longer be edited." */
export class ServerError extends Error {
	constructor(
		public status: number,
		message: string
	) {
		super(message);
	}
}

/**
 * Sends one request to the C++ server and returns its JSON answer.
 * body: an object (sent as JSON) or a File (sent as it is, for PDF uploads).
 */
export async function callServer<T>(method: string, path: string, body?: unknown): Promise<T> {
	const headers: Record<string, string> = { 'X-User-Email': auth.email };
	let payload: BodyInit | undefined;
	if (body instanceof File) {
		headers['Content-Type'] = 'application/pdf';
		payload = body;
	} else if (body !== undefined) {
		headers['Content-Type'] = 'application/json';
		payload = JSON.stringify(body);
	}

	let res: Response;
	try {
		res = await fetch(path, { method, headers, body: payload });
	} catch {
		throw new ServerError(0, 'The server is not reachable. Is ./build/ads_server running?');
	}
	const text = await res.text();
	let data: unknown = null;
	try {
		data = text ? JSON.parse(text) : null;
	} catch {
		data = null;
	}
	if (!res.ok) {
		const message = (data as { error?: string } | null)?.error;
		throw new ServerError(res.status, message ?? `The server answered ${res.status}.`);
	}
	return data as T;
}

/** The text of any error, for a toast. */
export function errorText(e: unknown): string {
	return e instanceof Error ? e.message : 'Something went wrong. Please try again.';
}
