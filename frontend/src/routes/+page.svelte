<script lang="ts">
	// Setup check page: proves frontend -> Vite proxy -> Crow -> SQLite all work.
	// Replace with the real landing page when building features.
	import { onMount } from 'svelte';
	import { api, type HealthResponse } from '#lib/api/client.ts';

	let health = $state<HealthResponse | null>(null);
	let error = $state<string | null>(null);

	onMount(async () => {
		try {
			health = await api.get<HealthResponse>('/api/health');
		} catch (e) {
			error = e instanceof Error ? e.message : String(e);
		}
	});
</script>

<main class="flex min-h-screen items-center justify-center bg-slate-50 p-6">
	<div class="w-full max-w-md rounded-2xl bg-white p-8 shadow-sm ring-1 ring-slate-200">
		<h1 class="text-2xl font-semibold text-slate-900">AUC Exchange Matcher</h1>
		<p class="mt-1 text-sm text-slate-500">Workspace setup check</p>

		<ul class="mt-6 space-y-3 text-sm">
			<li class="flex justify-between">
				<span class="text-slate-600">Frontend (SvelteKit + Tailwind)</span>
				<span class="font-medium text-emerald-600">ok</span>
			</li>
			<li class="flex justify-between">
				<span class="text-slate-600">Backend (Crow)</span>
				{#if health}
					<span class="font-medium text-emerald-600">{health.status}</span>
				{:else if error}
					<span class="font-medium text-rose-600">unreachable</span>
				{:else}
					<span class="text-slate-400">checking…</span>
				{/if}
			</li>
			<li class="flex justify-between">
				<span class="text-slate-600">Database (SQLite)</span>
				{#if health}
					<span class={health.db === 'ok' ? 'font-medium text-emerald-600' : 'font-medium text-rose-600'}>
						{health.db} <span class="text-slate-400">v{health.sqlite_version}</span>
					</span>
				{:else}
					<span class="text-slate-400">—</span>
				{/if}
			</li>
		</ul>

		{#if error}
			<p class="mt-6 rounded-lg bg-rose-50 p-3 text-xs text-rose-700">
				Could not reach the backend ({error}). Is <code>./build/ads_server</code> running on port 18080?
			</p>
		{/if}

		<div class="mt-6 pt-5 border-t border-slate-100">
			<a
				href="/admin"
				class="w-full h-10 px-4 bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-lg shadow-sm flex items-center justify-center transition-colors"
			>
				Open IPSO Admin Dashboard (/admin)
			</a>
		</div>
	</div>
</main>
