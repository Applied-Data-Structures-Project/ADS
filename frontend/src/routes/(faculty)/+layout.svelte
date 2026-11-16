<script lang="ts">
	// Frame for faculty pages (G3 Malak): doctors answer recommendation letter requests here.
	import type { Snippet } from 'svelte';
	import AppShell, { type NavGroup } from '#lib/components/student/AppShell.svelte';
	import { store } from '#lib/student/store.svelte.ts';

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
</script>

<AppShell
	{groups}
	mobileTabs={groups.flatMap((g) => g.items)}
	cycleLabel="Fall 2027 · Recommendation letters"
	personName={store.doctor.name}
	personRole={store.doctor.major}
>
	{@render children()}
</AppShell>
