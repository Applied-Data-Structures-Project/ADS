<script lang="ts" module>
	import type { IconName } from './Icon.svelte';

	export interface NavItem {
		href: string;
		label: string;
		short: string;
		icon: IconName;
		badge?: number;
	}

	export interface NavGroup {
		section: string;
		items: NavItem[];
	}
</script>

<script lang="ts">
	// The student-side frame: same header, sidebar, mobile bottom bar, dark mode and toast
	// as the admin console (routes/admin/+page.svelte), so both sides look like one product.
	import { onMount, type Snippet } from 'svelte';
	import { page } from '$app/state';
	import { store } from '#lib/student/store.svelte.ts';
	import Icon from './Icon.svelte';

	interface Props {
		groups: NavGroup[];
		mobileTabs: NavItem[];
		cycleLabel: string;
		personName: string;
		personRole: string;
		children: Snippet;
	}

	let { groups, mobileTabs, cycleLabel, personName, personRole, children }: Props = $props();

	let sidebarCollapsed = $state(false);

	const allItems = $derived(groups.flatMap((g) => g.items));

	/** The nav item whose href is the longest prefix of the current path. */
	const activeHref = $derived.by(() => {
		const path = page.url.pathname;
		let best = '';
		for (const item of allItems) {
			const match = path === item.href || path.startsWith(item.href + '/');
			if (match && item.href.length > best.length) best = item.href;
		}
		return best;
	});

	const activeLabel = $derived(allItems.find((i) => i.href === activeHref)?.label ?? '');

	const initials = $derived(
		personName
			.replace(/^Dr\.\s*/, '')
			.split(/\s+/)
			.map((w) => w[0] ?? '')
			.join('')
			.slice(0, 2)
			.toUpperCase()
	);

	onMount(() => store.loadDarkMode());

	function handleKeydown(e: KeyboardEvent) {
		const tag = (e.target as HTMLElement | null)?.tagName ?? '';
		if (['INPUT', 'TEXTAREA', 'SELECT'].includes(tag) || e.metaKey || e.ctrlKey) return;
		if (e.key === 'm' || e.key === 'M') sidebarCollapsed = !sidebarCollapsed;
		if (e.key === 'd' || e.key === 'D') store.toggleDarkMode();
	}
</script>

<svelte:window onkeydown={handleKeydown} />

<div
	class="min-h-screen bg-[#f8f9ff] text-[#0b1c30] overflow-x-hidden transition-colors duration-200 {store.darkMode
		? 'admin-dark'
		: ''}"
>
	<!-- TOP BAR -->
	<header
		class="fixed top-0 left-0 right-0 h-16 md:h-[72px] bg-white border-b border-[#c6c6cd]/70 z-40 grid grid-cols-[auto_1fr_auto] md:grid-cols-[1fr_auto_1fr] items-center px-3 sm:px-5 md:px-7 gap-2 sm:gap-3 md:gap-4"
	>
		<div class="flex items-center gap-2.5 md:gap-3.5 min-w-0">
			<img
				src="/auc-logo.png"
				alt="The American University in Cairo"
				class="h-6 sm:h-7 md:h-9 w-auto max-w-[112px] sm:max-w-none object-contain shrink-0 {store.darkMode
					? 'brightness-0 invert opacity-95'
					: ''}"
			/>
			<div class="hidden xl:block h-6 w-px bg-[#c6c6cd] shrink-0"></div>
			<span class="hidden xl:inline-block text-[15px] font-semibold text-[#0b1c30] tracking-tight truncate">
				IPSO Exchange
				<span class="text-[#515f74] font-normal">/ {activeLabel}</span>
			</span>
		</div>

		<div class="flex justify-center min-w-0">
			<span
				class="h-9 sm:h-10 md:h-11 px-3 sm:px-4 inline-flex items-center gap-2 bg-[#eff4ff] border border-[#c6c6cd]/60 rounded-xl text-xs sm:text-sm font-semibold text-[#0b1c30] shadow-sm truncate max-w-full"
			>
				<svg class="hidden sm:block w-4 h-4 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">
					<rect x="3" y="4" width="18" height="18" rx="2" />
					<line x1="16" y1="2" x2="16" y2="6" />
					<line x1="8" y1="2" x2="8" y2="6" />
					<line x1="3" y1="10" x2="21" y2="10" />
				</svg>
				<span class="truncate">{cycleLabel}</span>
			</span>
		</div>

		<div class="flex items-center justify-end gap-2 sm:gap-2.5 shrink-0">
			<button
				type="button"
				onclick={() => store.toggleDarkMode()}
				aria-label={store.darkMode ? 'Switch to light mode' : 'Switch to dark mode'}
				title="Toggle Dark Mode [D]"
				class="h-9 sm:h-10 px-2.5 sm:px-3.5 rounded-xl bg-[#eff4ff] hover:bg-[#e5eeff] border border-[#c6c6cd]/50 text-[#0b1c30] flex items-center gap-2 text-xs sm:text-sm font-semibold cursor-pointer transition-colors shrink-0"
			>
				<Icon name={store.darkMode ? 'sun' : 'moon'} class="w-4 h-4" strokeWidth={2} />
				<span class="hidden lg:inline">{store.darkMode ? 'Light' : 'Dark'}</span>
			</button>

			<div class="flex items-center gap-3 py-1.5 px-1.5 sm:px-3.5 rounded-xl sm:bg-[#f8f9ff] sm:border sm:border-[#c6c6cd]/50">
				<div
					class="w-9 h-9 md:w-10 md:h-10 rounded-full bg-[#dce9ff] border border-[#c6c6cd] flex items-center justify-center text-xs md:text-sm font-bold text-[#0b1c30] shrink-0"
				>
					{initials}
				</div>
				<div class="hidden md:flex flex-col text-left">
					<span class="text-sm font-semibold text-[#0b1c30] leading-tight">{personName}</span>
					<span class="text-xs font-medium text-[#515f74] leading-tight">{personRole}</span>
				</div>
			</div>
		</div>
	</header>

	<!-- DESKTOP SIDEBAR -->
	<aside
		class="hidden md:flex fixed left-0 top-[72px] bottom-0 {sidebarCollapsed
			? 'w-[72px]'
			: 'w-64'} bg-white border-r border-[#c6c6cd]/70 z-30 flex-col justify-between transition-all duration-150"
	>
		<div class="py-5 px-3 overflow-y-auto space-y-6">
			{#each groups as group (group.section)}
				<div>
					{#if !sidebarCollapsed}
						<div class="px-3 pb-2 text-[11px] font-semibold text-[#515f74] uppercase tracking-wider">
							{group.section}
						</div>
					{/if}
					<nav class="flex flex-col gap-2">
						{#each group.items as item (item.href)}
							{@const active = activeHref === item.href}
							<a
								href={item.href}
								title={item.label}
								aria-current={active ? 'page' : undefined}
								class="w-full flex items-center {sidebarCollapsed
									? 'justify-center px-0 py-3'
									: 'gap-3 px-3.5 py-3'} rounded-xl transition-colors {active
									? 'bg-[#dce9ff]/80 text-[#0b1c30] shadow-sm border border-[#c6c6cd]/50'
									: 'text-[#45464d] hover:bg-[#eff4ff] hover:text-[#0b1c30]'}"
							>
								<span
									class="relative w-9 h-9 rounded-lg flex items-center justify-center shrink-0 transition-colors {active
										? 'bg-[#131b2e] text-white'
										: 'bg-[#eff4ff] text-[#0b1c30]'}"
								>
									<Icon name={item.icon} />
									{#if sidebarCollapsed && item.badge}
										<span class="absolute -top-1 -right-1 w-2.5 h-2.5 rounded-full bg-[#131b2e] border-2 border-white"></span>
									{/if}
								</span>
								{#if !sidebarCollapsed}
									<span class="text-[14.5px] font-semibold text-[#0b1c30] truncate flex-1">{item.label}</span>
									{#if item.badge}
										<span class="px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#0b1c30] text-xs font-semibold font-mono-num">
											{item.badge}
										</span>
									{/if}
								{/if}
							</a>
						{/each}
					</nav>
				</div>
			{/each}
		</div>

		<div class="p-2.5 border-t border-[#c6c6cd]/60 bg-white">
			<button
				type="button"
				onclick={() => (sidebarCollapsed = !sidebarCollapsed)}
				class="w-full flex items-center {sidebarCollapsed
					? 'justify-center px-0'
					: 'justify-between px-3'} h-10 text-[#45464d] hover:bg-[#eff4ff] hover:text-[#0b1c30] rounded-xl transition-colors cursor-pointer"
				title="Toggle Sidebar [M]"
			>
				<span class="flex items-center gap-2.5 text-xs font-semibold">
					<svg
						class="w-4 h-4 shrink-0 transition-transform {sidebarCollapsed ? 'rotate-180' : ''}"
						viewBox="0 0 24 24"
						fill="none"
						stroke="currentColor"
						stroke-width="1.8"
					>
						<polyline points="11 17 6 12 11 7" />
						<polyline points="18 17 13 12 18 7" />
					</svg>
					{#if !sidebarCollapsed}<span>Collapse Sidebar</span>{/if}
				</span>
				{#if !sidebarCollapsed}
					<span class="font-mono-num text-[11px] text-[#515f74]">[M]</span>
				{/if}
			</button>
		</div>
	</aside>

	<!-- PAGE -->
	<div class="pt-16 md:pt-[72px] pb-20 md:pb-8 {sidebarCollapsed ? 'md:pl-[72px]' : 'md:pl-64'} transition-all duration-150">
		<main class="min-h-[calc(100vh-4.5rem)] bg-[#f8f9ff] px-4 sm:px-6 lg:px-8 pb-8 pt-5 md:pt-6 max-w-[1440px]">
			{@render children()}
		</main>
	</div>

	<!-- MOBILE BOTTOM BAR -->
	<nav
		class="md:hidden fixed bottom-0 left-0 right-0 h-15 bg-white border-t border-[#c6c6cd]/70 flex items-center justify-around px-2 z-30 shadow-lg"
	>
		{#each mobileTabs as tab (tab.href)}
			{@const active = activeHref === tab.href}
			<a
				href={tab.href}
				class="relative flex flex-col items-center justify-center gap-0.5 px-3 py-1 {active
					? 'text-[#0b1c30] font-semibold'
					: 'text-[#515f74]'}"
			>
				<Icon name={tab.icon} class="w-5 h-5" strokeWidth={active ? 2 : 1.6} />
				<span class="text-[11px]">{tab.short}</span>
				{#if tab.badge}
					<span class="absolute top-0 right-1.5 min-w-4 h-4 px-1 rounded-full bg-[#131b2e] text-white text-[10px] font-semibold flex items-center justify-center">
						{tab.badge}
					</span>
				{/if}
			</a>
		{/each}
	</nav>

	{#if store.toast}
		<div
			role="status"
			class="fixed bottom-18 md:bottom-5 left-4 right-4 md:left-auto md:right-5 bg-[#131b2e] text-white px-4 py-3 rounded-xl shadow-md text-xs sm:text-sm font-medium z-50 text-center md:text-left"
		>
			{store.toast}
		</div>
	{/if}
</div>
