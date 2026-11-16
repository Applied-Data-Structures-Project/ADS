<script lang="ts">
	// Frame for the student pages (same look as /admin). Group 3 (Malak): Application, Status, Letters.
	// Maya and Muhammad add their pages to the "Discover" and "Account" groups when they build them.
	import type { Snippet } from 'svelte';
	import AppShell, { type NavGroup, type NavItem } from '#lib/components/student/AppShell.svelte';
	import { store } from '#lib/student/store.svelte.ts';
	import { RULES } from '#lib/student/mocks.ts';

	let { children }: { children: Snippet } = $props();

	const deadline = new Date(RULES.deadline).toLocaleDateString('en-GB', { day: 'numeric', month: 'short' });

	const pendingLetters = $derived(store.myLetters.filter((l) => l.status === 'Pending').length);

	const groups = $derived<NavGroup[]>([
		{
			section: 'My Application',
			items: [
				{ href: '/apply', label: 'Application Form', short: 'Apply', icon: 'apply' },
				{ href: '/apply/status', label: 'Application Status', short: 'Status', icon: 'status' },
				{ href: '/letters', label: 'Recommendation Letter', short: 'Letters', icon: 'letters', badge: pendingLetters }
			]
		}
	]);

	const mobileTabs = $derived<NavItem[]>(
		['/apply', '/apply/status', '/letters']
			.map((href) => groups.flatMap((g) => g.items).find((i) => i.href === href))
			.filter((i): i is NavItem => i !== undefined)
	);
</script>

{#if store.user && store.user.role === 'student'}
	<AppShell
		{groups}
		{mobileTabs}
		cycleLabel={`Fall 2027 · Applications close ${deadline}`}
		personName={store.user.name}
		personRole={`${store.user.level} · ${store.user.major}`}
	>
		{@render children()}
	</AppShell>
{:else}
	<main class="min-h-screen bg-[#f8f9ff] flex items-center justify-center p-6">
		<div class="w-full max-w-sm bg-white rounded-2xl shadow-sm border border-[#c6c6cd]/40 p-6 text-center space-y-4">
			<img src="/auc-logo.png" alt="The American University in Cairo" class="h-8 mx-auto" />
			<p class="text-sm text-[#515f74]">
				{store.user ? 'This page is for students. You are signed in as faculty.' : 'Log in to see your study abroad pages.'}
			</p>
			<a
				href={store.user ? '/doctor' : '/login'}
				class="h-11 w-full bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-xl shadow-sm flex items-center justify-center"
			>
				{store.user ? 'Go to letter requests' : 'Log in'}
			</a>
		</div>
	</main>
{/if}
