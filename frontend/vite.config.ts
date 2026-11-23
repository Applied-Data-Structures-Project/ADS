import tailwindcss from '@tailwindcss/vite';
import adapter from '@sveltejs/adapter-auto';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

const apiTarget = process.env.BACKEND_URL || 'http://localhost:18080';

const apiProxy = {
	'/api': {
		target: apiTarget,
		bypass(req: { url?: string }) {
			const pathname = req.url?.split('?')[0];
			if (
				pathname === '/api/coach' ||
				pathname === '/api/internal/email' ||
				pathname === '/api/transfer-equivalencies'
			) {
				return req.url;
			}
		}
	}
};

export default defineConfig({
	plugins: [
		tailwindcss(),
		sveltekit({
			compilerOptions: {
				// Force runes mode for the project, except for libraries. Can be removed in svelte 6.
				runes: ({ filename }) =>
					filename.split(/[/\\]/).includes('node_modules') ? undefined : true
			},

			// adapter-auto only supports some environments, see https://svelte.dev/docs/kit/adapter-auto for a list.
			// If your environment is not supported, or you settled on a specific environment, switch out the adapter.
			// See https://svelte.dev/docs/kit/adapters for more information about adapters.
			adapter: adapter()
		})
	],
	server: {
		// Forward C++-backed /api/* requests to Crow during development, while
		// letting SvelteKit serve /api/coach and /api/internal/email directly.
		proxy: apiProxy
	},
	preview: {
		proxy: apiProxy
	}
});
