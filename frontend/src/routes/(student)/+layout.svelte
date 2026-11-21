<script lang="ts">
	// Frame for the student pages (same look as /admin). Group 3 (Malak): Application, Status, Letters.
	// Maya and Muhammad add their pages to the "Discover" and "Account" groups when they build them.
	import { onMount, type Snippet } from 'svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import AppShell, { type NavGroup, type NavItem } from '#lib/components/student/AppShell.svelte';
	import SessionGate from '#lib/components/auth/SessionGate.svelte';
	import { store } from '#lib/student/store.svelte.ts';
	import { RULES } from '#lib/student/mocks.ts';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { roleHome, roleLabel } from '#lib/auth/roles.ts';

	let { children }: { children: Snippet } = $props();

	const deadline = new Date(RULES.deadline).toLocaleDateString('en-GB', { day: 'numeric', month: 'short' });

	const pendingLetters = $derived(store.myLetters.filter((l) => l.status === 'Pending').length);

	const groups = $derived<NavGroup[]>([
		{
        section: 'Discover',
        items: [
            {
                href: '/explore',
                label: 'Explore Universities',
                short: 'Explore',
                icon: 'explore'
            }
        ]
    },
		
		{
			section: 'My Application',
			items: [
				{ href: '/apply', label: 'Application Form', short: 'Apply', icon: 'apply' },
				{ href: '/apply/status', label: 'Application Status', short: 'Status', icon: 'status' },
				{ href: '/letters', label: 'Recommendation Letter', short: 'Letters', icon: 'letters', badge: pendingLetters }
			]
		},
		{
			section: 'Account',
			items: [
				{ href: '/profile', label: 'Eligibility Profile', short: 'Profile', icon: 'profile' }
			]
		}
	]);

	const mobileTabs = $derived<NavItem[]>(
			['/explore', '/apply', '/apply/status', '/letters', '/profile']
			.map((href) => groups.flatMap((g) => g.items).find((i) => i.href === href))
			.filter((i): i is NavItem => i !== undefined)
	);

	const personRole = $derived(
		auth.profile ? `${auth.profile.level} · ${auth.profile.major}` : roleLabel(auth.role)
	);

	onMount(async () => {
		await auth.init();
		if (!auth.signedIn) {
			const next = page.url.pathname + page.url.search;
			await goto(`/login?next=${encodeURIComponent(next)}`, { replace: true });
			return;
		}
		if (auth.needsProfile) {
			await goto('/signup', { replace: true });
			return;
		}

		store.loadEligibility(auth.account?.homeAccountId ?? auth.email);
		if (!store.hasEligibilityProfile && page.url.pathname !== '/profile') {
			const next = page.url.pathname + page.url.search;
			await goto(`/profile?next=${encodeURIComponent(next)}`, { replace: true });
		}
	});
</script>

{#if auth.status === 'loading'}
	<SessionGate message="Checking your session…" spinner />
{:else if !auth.signedIn}
	<SessionGate message="Taking you to Microsoft sign-in…" spinner />
{:else if auth.role === 'student'}
	<AppShell
		{groups}
		{mobileTabs}
		cycleLabel={`Fall 2027 · Applications close ${deadline}`}
		personName={auth.displayName}
		personRole={personRole}
	>
		{@render children()}
	</AppShell>
{:else}
	<SessionGate
		message={`You are signed in as ${roleLabel(auth.role)}. This page is for students.`}
		href={roleHome(auth.role)}
		actionLabel="Go to my area"
		signOut
	/>
{/if}
