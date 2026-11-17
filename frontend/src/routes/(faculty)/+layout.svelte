<script lang="ts">
	// Frame for faculty pages (G3 Malak): doctors answer recommendation letter requests here.
	import { onMount, type Snippet } from 'svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import AppShell, { type NavGroup } from '#lib/components/student/AppShell.svelte';
	import SessionGate from '#lib/components/auth/SessionGate.svelte';
	import { store } from '#lib/student/store.svelte.ts';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { roleHome, roleLabel } from '#lib/auth/roles.ts';

	let { children }: { children: Snippet } = $props();

	const groups = $derived<NavGroup[]>([
		{
			section: 'Faculty',
			items: [
				{
					href: '/doctor',
					label: 'Letter Requests',
					short: 'Requests',
					icon: 'inbox',
					badge: store.doctorQueue.length
				}
			]
		},
		{
			section: 'Demo',
			items: [{ href: '/apply', label: 'Back to Student View', short: 'Student', icon: 'profile' }]
		}
	]);

	onMount(async () => {
		await auth.init();
		if (!auth.signedIn) {
			const next = page.url.pathname + page.url.search;
			await goto(`/login?next=${encodeURIComponent(next)}`, { replace: true });
		}
	});
</script>

{#if auth.status === 'loading'}
	<SessionGate message="Checking your session…" spinner />
{:else if !auth.signedIn}
	<SessionGate message="Taking you to Microsoft sign-in…" spinner />
{:else if auth.role === 'faculty'}
	<AppShell
		{groups}
		mobileTabs={groups.flatMap((g) => g.items)}
		cycleLabel="Fall 2027 · Recommendation letters"
		personName={auth.displayName}
		personRole="AUC Faculty"
	>
		{@render children()}
	</AppShell>
{:else}
	<SessionGate
		message={`You are signed in as ${roleLabel(auth.role)}. This page is for AUC faculty.`}
		href={roleHome(auth.role)}
		actionLabel="Go to my area"
		signOut
	/>
{/if}
