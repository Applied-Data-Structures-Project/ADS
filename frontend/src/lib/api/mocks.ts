// Mock responses for frontend work before a backend endpoint exists.
// Shapes must match docs/api.md. Swap a mock for `api.get(...)` once the route lands.

import type { HealthResponse } from './client.ts';

export const mockHealth: HealthResponse = {
	status: 'ok',
	db: 'ok',
	sqlite_version: 'mock'
};
