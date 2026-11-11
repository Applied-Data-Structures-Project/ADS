// Tiny fetch wrapper for talking to the C++ backend.
// Always use relative paths ('/api/...'); in dev, Vite proxies them to localhost:18080.
//
// Usage:
//   const health = await api.get<HealthResponse>('/api/health');
//   await api.post('/api/auth/login', { email, password });

export class ApiError extends Error {
	constructor(
		public status: number,
		message: string
	) {
		super(message);
	}
}

let authToken: string | null = null;

/** Set after login; sent as `Authorization: Bearer <token>` on every request. */
export function setAuthToken(token: string | null) {
	authToken = token;
}

async function request<T>(method: string, path: string, body?: unknown): Promise<T> {
	const headers: Record<string, string> = {};
	if (body !== undefined) headers['Content-Type'] = 'application/json';
	if (authToken) headers['Authorization'] = `Bearer ${authToken}`;

	const res = await fetch(path, {
		method,
		headers,
		body: body === undefined ? undefined : JSON.stringify(body)
	});

	const text = await res.text();
	const data = text ? JSON.parse(text) : null;
	if (!res.ok) throw new ApiError(res.status, data?.error ?? res.statusText);
	return data as T;
}

export const api = {
	get: <T>(path: string) => request<T>('GET', path),
	post: <T>(path: string, body?: unknown) => request<T>('POST', path, body),
	put: <T>(path: string, body?: unknown) => request<T>('PUT', path, body),
	del: <T>(path: string) => request<T>('DELETE', path)
};

// ---- Response types (keep in sync with docs/api.md) ----

export interface HealthResponse {
	status: string;
	db: string;
	sqlite_version: string;
}

