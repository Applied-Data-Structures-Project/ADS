<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import BipartiteMatchModal from '#lib/components/BipartiteMatchModal.svelte';
	import {
		DEFAULT_WEIGHT_CONFIG,
		MOCK_MATCHED_ROWS,
		MOCK_SAVED_RUNS,
		MOCK_STUDENTS,
		MOCK_UNIVERSITIES,
		MOCK_UNPLACED_ROWS,
		type AdminStudentApplication,
		type AuditStatus,
		type PartnerUniversity,
		type SavedMatchRun,
		type WeightConfig
	} from '#lib/api/mocks.ts';

	type AdminPageId = 'applications' | 'matching' | 'universities' | 'runs';
	type StudentCohortTab = 'all' | 'eligible' | 'flagged';
	type MatchResultTab = 'matched' | 'unplaced' | 'unis';

	const ACADEMIC_CYCLES = [
		'2026–27 · Fall Term',
		'2026–27 · Spring Term',
		'2025–26 · Spring Term',
		'2025–26 · Fall Term'
	] as const;

	let selectedCycle = $state<string>(ACADEMIC_CYCLES[0]);
	let activePage = $state<AdminPageId>('applications');
	let sidebarCollapsed = $state(false);
	let darkMode = $state(false);
	let filterPanelOpen = $state(true);
	let mobileWeightsOpen = $state(true);

	// Applications filter & table state
	let students = $state<AdminStudentApplication[]>(
		MOCK_STUDENTS.map((s) => ({ ...s, prefs: [...s.prefs] as [string, string, string] }))
	);
	let studentTab = $state<StudentCohortTab>('all');
	let searchQuery = $state('');
	let standingFilter = $state('all');
	let gpaMin = $state(0);
	let gpaMax = $state(4);
	let uniFilter = $state('all');
	let priorFilter = $state('all');
	let majorFilter = $state('');
	let statusFilter = $state('all');
	let selectAllChecked = $state(false);
	let rowsPerPage = $state('25');

	// Partner Universities & Seat Quotas
	let universities = $state<PartnerUniversity[]>(MOCK_UNIVERSITIES.map((u) => ({ ...u })));

	// Matching Console state
	let weights = $state<WeightConfig>({ ...DEFAULT_WEIGHT_CONFIG });
	let matchTab = $state<MatchResultTab>('matched');
	let animOpen = $state(false);
	let activeRunLabel = $state('Draft Run #4 · Unsaved Simulation');

	// Saved Runs state
	let savedRuns = $state<SavedMatchRun[]>(
		MOCK_SAVED_RUNS.map((r) => ({ ...r, weights: { ...r.weights } }))
	);

	// Inspection drawer & toast
	let inspectedStudentId = $state<string | null>(null);
	let toastMessage = $state<string | null>(null);
	let toastTimer: ReturnType<typeof setTimeout> | null = null;

	const inspectedStudent = $derived(
		students.find((s) => s.id === inspectedStudentId) ?? null
	);

	const totalConfiguredSeats = $derived(
		universities.reduce((sum, u) => sum + u.seats, 0)
	);

	const filteredStudents = $derived.by(() => {
		const q = searchQuery.trim().toLowerCase();
		const maj = majorFilter.trim().toLowerCase();
		const minG = Number.isFinite(gpaMin) ? gpaMin : 0;
		const maxG = Number.isFinite(gpaMax) ? gpaMax : 4;

		return students.filter((s) => {
			if (studentTab === 'eligible' && s.gpa < 2.8) return false;
			if (studentTab === 'flagged' && s.status === 'Verified' && s.gpa >= 2.8) return false;
			if (standingFilter !== 'all' && s.standing !== standingFilter) return false;
			if (s.gpa < minG || s.gpa > maxG) return false;
			if (uniFilter !== 'all' && !s.prefs.includes(uniFilter)) return false;
			if (priorFilter === 'first' && s.prior) return false;
			if (priorFilter === 'prior' && !s.prior) return false;
			if (maj && !s.major.toLowerCase().includes(maj)) return false;
			if (statusFilter !== 'all' && s.status !== statusFilter) return false;
			if (
				q &&
				!(
					s.name.toLowerCase().includes(q) ||
					s.id.toLowerCase().includes(q) ||
					s.major.toLowerCase().includes(q) ||
					s.prefs.join(' ').toLowerCase().includes(q)
				)
			) {
				return false;
			}
			return true;
		});
	});

	const activeFilterCount = $derived.by(() => {
		let count = 0;
		if (standingFilter !== 'all') count++;
		if (gpaMin > 0 || gpaMax < 4) count++;
		if (uniFilter !== 'all') count++;
		if (priorFilter !== 'all') count++;
		if (majorFilter.trim() !== '') count++;
		if (statusFilter !== 'all') count++;
		return count;
	});

	// Simulation metrics derived reactively from current WeightConfig
	const simPlaced = $derived.by(() => {
		if (weights.mode === 'hungarian') return weights.allowUnranked ? 133 : 118;
		return weights.allowUnranked ? 138 : 121;
	});

	const simFirstPct = $derived(
		Math.min(79.5, Math.max(54.0, 58 + weights.pref * 16 - weights.gpa * 4))
	);

	const simFirstCount = $derived(Math.round((simPlaced * simFirstPct) / 100));

	const simChoiceSplit = $derived.by(() => {
		const rem = Math.max(0, simPlaced - simFirstCount);
		const second = Math.round(rem * 0.55);
		const third = Math.round(rem * 0.3);
		const alt = weights.allowUnranked ? Math.max(0, rem - second - third) : 0;
		return `2nd: ${second} · 3rd: ${third} · Alt: ${alt}`;
	});

	const simAvgWeight = $derived(
		(0.62 + weights.pref * 0.25 + weights.gpa * 0.14 + weights.llm * 0.1 + weights.equity * 0.08).toFixed(
			3
		)
	);

	const simUnplaced = $derived(164 - simPlaced);

	const HEADER_TITLES: Record<AdminPageId, string> = {
		applications: 'Student Applications',
		matching: 'Matching Console',
		universities: 'Partner Universities',
		runs: 'Saved Runs'
	};

	const NAV_GROUPS = [
		{
			section: 'Exchange Operations',
			items: [
				{
					id: 'applications' as AdminPageId,
					label: 'Student Applications',
					shortLabel: 'Apps'
				},
				{
					id: 'matching' as AdminPageId,
					label: 'Matching Console',
					shortLabel: 'Matching'
				},
				{
					id: 'universities' as AdminPageId,
					label: 'Partner Universities',
					shortLabel: 'Partners'
				}
			]
		},
		{
			section: 'Governance & Controls',
			items: [
				{
					id: 'runs' as AdminPageId,
					label: 'Saved Runs & Diffs',
					shortLabel: 'Runs'
				}
			]
		}
	];

	const MOBILE_TABS = NAV_GROUPS.flatMap((g) => g.items);

	function showToast(msg: string) {
		toastMessage = msg;
		if (toastTimer) clearTimeout(toastTimer);
		toastTimer = setTimeout(() => {
			toastMessage = null;
		}, 2800);
	}

	onDestroy(() => {
		if (toastTimer) clearTimeout(toastTimer);
	});

	function navigate(page: AdminPageId) {
		activePage = page;
	}

	function resetAppFilters() {
		standingFilter = 'all';
		gpaMin = 0;
		gpaMax = 4;
		uniFilter = 'all';
		priorFilter = 'all';
		majorFilter = '';
		statusFilter = 'all';
		searchQuery = '';
		studentTab = 'all';
	}

	function toggleAllStudents(checked: boolean) {
		selectAllChecked = checked;
		const visibleIds = new Set(filteredStudents.map((s) => s.id));
		students = students.map((s) => (visibleIds.has(s.id) ? { ...s, selected: checked } : s));
	}

	function markStudentVerified(id: string) {
		students = students.map((s) => (s.id === id ? { ...s, status: 'Verified' } : s));
		inspectedStudentId = null;
		showToast('Application audit status marked Verified.');
	}

	function adjustSeat(idx: number, delta: number) {
		universities = universities.map((u, i) =>
			i === idx ? { ...u, seats: Math.max(0, u.seats + delta) } : u
		);
	}

	function resetWeights() {
		weights = { ...DEFAULT_WEIGHT_CONFIG };
		activeRunLabel = 'Draft Run #4 · Unsaved Simulation';
		showToast('Weights reset to default WeightConfig.');
	}

	function triggerMatchAnimation() {
		animOpen = true;
	}

	function handleAnimationDone() {
		animOpen = false;
		activeRunLabel = 'Draft Run #4 · Simulated Just Now';
		showToast(`Matching complete: ${simPlaced}/164 placed across optimal bipartite paths.`);
	}

	function saveDraftRun() {
		const wLabel = `λ=${weights.pref.toFixed(2)} · GPA=${weights.gpa.toFixed(2)} · LLM=${weights.llm.toFixed(2)} · Eq=${weights.equity.toFixed(2)}`;
		savedRuns = savedRuns.map((r, i) =>
			i === 0
				? {
						...r,
						weightsLabel: wLabel,
						weights: { ...weights },
						placed: simPlaced,
						firstChoicePct: Number(simFirstPct.toFixed(1)),
						meta: 'Saved just now · Nadia Hassan'
					}
				: r
		);
		activeRunLabel = 'Draft Run #4 · Saved';
		showToast('Draft Run #4 saved to Run History.');
	}

	function publishResults() {
		savedRuns = savedRuns.map((r, i) => (i === 0 ? { ...r, status: 'Published' } : r));
		activeRunLabel = 'Run #4 · Published';
		showToast('Run #4 published! Matched students can now view their nominations.');
	}

	function loadSavedRun(run: SavedMatchRun) {
		weights = { ...run.weights };
		activeRunLabel = `${run.id} · Loaded (${run.status})`;
		activePage = 'matching';
		showToast(`Loaded ${run.id} (${run.title}) into Matching Console.`);
	}

	function statusBadgeClass(status: AuditStatus) {
		if (status === 'Verified') return 'bg-[#f0fdf4] text-[#15803d]';
		if (status === 'In Review') return 'bg-[#fffbeb] text-[#b45309]';
		return 'bg-[#fef2f2] text-[#ba1a1a]';
	}

	onMount(() => {
		darkMode = localStorage.getItem('auc-admin-dark-mode') === '1';
	});

	function toggleDarkMode() {
		darkMode = !darkMode;
		localStorage.setItem('auc-admin-dark-mode', darkMode ? '1' : '0');
		showToast(darkMode ? 'Switched to Dark Mode.' : 'Switched to Light Mode.');
	}

	function handleWindowKeydown(e: KeyboardEvent) {
		const tag = (e.target as HTMLElement | null)?.tagName ?? '';
		const inField = ['INPUT', 'TEXTAREA', 'SELECT'].includes(tag);
		if ((e.key === 'm' || e.key === 'M') && !inField && !e.metaKey && !e.ctrlKey) {
			sidebarCollapsed = !sidebarCollapsed;
		}
		if ((e.key === 'd' || e.key === 'D') && !inField && !e.metaKey && !e.ctrlKey) {
			toggleDarkMode();
		}
		if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'k') {
			const input = document.getElementById('student-search') as HTMLInputElement | null;
			if (input) {
				e.preventDefault();
				input.focus();
			}
		}
	}
</script>

<svelte:window onkeydown={handleWindowKeydown} />

<div class="min-h-screen bg-[#f8f9ff] text-[#0b1c30] overflow-x-hidden transition-colors duration-200 {darkMode ? 'admin-dark' : ''}">
	<!-- THICKER TOP NAVBAR (h-16 mobile, h-[72px] desktop) WITH CENTERED AY/TERM DROPDOWN & LARGER PROFILE -->
	<header
		class="fixed top-0 left-0 right-0 h-16 md:h-[72px] bg-white border-b border-[#c6c6cd]/70 z-40 grid grid-cols-[auto_1fr_auto] md:grid-cols-[1fr_auto_1fr] items-center px-3 sm:px-5 md:px-7 gap-2 sm:gap-3 md:gap-4"
	>
		<!-- Left: Larger AUC Logo + Context -->
		<div class="flex items-center gap-2.5 md:gap-3.5 min-w-0">
			<img
				src="/auc-logo.png"
				alt="The American University in Cairo"
				class="h-6 sm:h-7 md:h-9 w-auto max-w-[112px] sm:max-w-none object-contain shrink-0 transition-all {darkMode ? 'brightness-0 invert opacity-95' : ''}"
			/>

			<div class="hidden xl:block h-6 w-px bg-[#c6c6cd] shrink-0"></div>
			<span class="hidden xl:inline-block text-[15px] font-semibold text-[#0b1c30] tracking-tight truncate">
				IPSO Exchange
				<span class="text-[#515f74] font-normal">/ {HEADER_TITLES[activePage]}</span>
			</span>
		</div>

		<!-- Center: Interactive Academic Year & Term Dropdown -->
		<div class="flex justify-center min-w-0">
			<div class="relative flex items-center max-w-full">
				<svg
					class="hidden sm:block w-4 h-4 text-[#0b1c30] absolute left-3.5 pointer-events-none"
					viewBox="0 0 24 24"
					fill="none"
					stroke="currentColor"
					stroke-width="1.8"
				>
					<rect x="3" y="4" width="18" height="18" rx="2" />
					<line x1="16" y1="2" x2="16" y2="6" />
					<line x1="8" y1="2" x2="8" y2="6" />
					<line x1="3" y1="10" x2="21" y2="10" />
				</svg>
				<select
					aria-label="Academic Year and Term"
					bind:value={selectedCycle}
					onchange={() => showToast(`Switched active cycle to ${selectedCycle}.`)}
					class="stitch-select h-9 sm:h-10 md:h-11 max-w-full pl-2.5 sm:pl-10 pr-7 sm:pr-9 bg-[#eff4ff] hover:bg-[#e5eeff] border border-[#c6c6cd]/60 rounded-xl text-xs sm:text-sm font-semibold text-[#0b1c30] shadow-sm focus:outline-none focus:border-black focus:ring-0 cursor-pointer truncate"
				>
					{#each ACADEMIC_CYCLES as cycle (cycle)}
						<option value={cycle}>{cycle}</option>
					{/each}
				</select>
				<svg
					class="w-4 h-4 text-[#515f74] absolute right-2 sm:right-3 pointer-events-none"
					viewBox="0 0 24 24"
					fill="none"
					stroke="currentColor"
					stroke-width="2"
				>
					<polyline points="6 9 12 15 18 9" />
				</svg>
			</div>
		</div>

		<!-- Right: Dark Mode Toggle + Coordinator Profile Card -->
		<div class="flex items-center justify-end gap-2 sm:gap-2.5 shrink-0">
			<button
				type="button"
				onclick={toggleDarkMode}
				aria-label={darkMode ? 'Switch to light mode' : 'Switch to dark mode'}
				title="Toggle Dark Mode [D]"
				class="h-9 sm:h-10 px-2.5 sm:px-3.5 rounded-xl bg-[#eff4ff] hover:bg-[#e5eeff] border border-[#c6c6cd]/50 text-[#0b1c30] flex items-center gap-2 text-xs sm:text-sm font-semibold cursor-pointer transition-colors shrink-0"
			>
				{#if darkMode}
					<svg class="w-4 h-4 text-[#fbbf24]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<circle cx="12" cy="12" r="4" />
						<line x1="12" y1="2" x2="12" y2="4" />
						<line x1="12" y1="20" x2="12" y2="22" />
						<line x1="4.93" y1="4.93" x2="6.34" y2="6.34" />
						<line x1="17.66" y1="17.66" x2="19.07" y2="19.07" />
						<line x1="2" y1="12" x2="4" y2="12" />
						<line x1="20" y1="12" x2="22" y2="12" />
						<line x1="6.34" y1="17.66" x2="4.93" y2="19.07" />
						<line x1="19.07" y1="4.93" x2="17.66" y2="6.34" />
					</svg>
					<span class="hidden lg:inline">Light</span>
				{:else}
					<svg class="w-4 h-4 text-[#0b1c30]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
						<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z" />
					</svg>
					<span class="hidden lg:inline">Dark</span>
				{/if}
			</button>

			<div class="flex items-center gap-3 py-1.5 px-1.5 sm:px-3.5 rounded-xl sm:bg-[#f8f9ff] sm:border sm:border-[#c6c6cd]/50">
				<div
					class="w-9 h-9 md:w-10 md:h-10 rounded-full bg-[#dce9ff] border border-[#c6c6cd] flex items-center justify-center text-xs md:text-sm font-bold text-[#0b1c30] shrink-0"
				>
					NH
				</div>
				<div class="hidden md:flex flex-col text-left">
					<span class="text-sm font-semibold text-[#0b1c30] leading-tight">Nadia Hassan</span>
					<span class="text-xs font-medium text-[#515f74] leading-tight">IPSO Coordinator</span>
				</div>
			</div>
		</div>
	</header>

	<!-- DESKTOP LEFT SIDEBAR (hidden on < md) -->
	<aside
		class="hidden md:flex fixed left-0 top-[72px] bottom-0 {sidebarCollapsed
			? 'w-[72px]'
			: 'w-64'} bg-white border-r border-[#c6c6cd]/70 z-30 flex-col justify-between transition-all duration-150"
	>
		<div class="py-5 px-3 overflow-y-auto space-y-6">
			{#each NAV_GROUPS as group (group.section)}
				<div>
					{#if !sidebarCollapsed}
						<div class="px-3 pb-2 text-[11px] font-semibold text-[#515f74] uppercase tracking-wider">
							{group.section}
						</div>
					{/if}
					<nav class="flex flex-col gap-2">
						{#each group.items as item (item.id)}
							{@const active = activePage === item.id}
							<button
								type="button"
								onclick={() => navigate(item.id)}
								title={item.label}
								class="w-full flex items-center {sidebarCollapsed
									? 'justify-center px-0 py-3'
									: 'gap-3 px-3.5 py-3'} rounded-xl text-left transition-colors cursor-pointer {active
									? 'bg-[#dce9ff]/80 text-[#0b1c30] shadow-sm border border-[#c6c6cd]/50'
									: 'text-[#45464d] hover:bg-[#eff4ff] hover:text-[#0b1c30]'}"
							>
								<span
									class="w-9 h-9 rounded-lg flex items-center justify-center shrink-0 transition-colors {active
										? 'bg-[#131b2e] text-white'
										: 'bg-[#eff4ff] text-[#0b1c30]'}"
								>
									{#if item.id === 'applications'}
										<svg class="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
											<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2" />
											<circle cx="9" cy="7" r="4" />
											<path d="M23 21v-2a4 4 0 0 0-3-3.87" />
											<path d="M16 3.13a4 4 0 0 1 0 7.75" />
										</svg>
									{:else if item.id === 'matching'}
										<svg class="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
											<circle cx="18" cy="18" r="3" />
											<circle cx="6" cy="6" r="3" />
											<path d="M13 6h3a2 2 0 0 1 2 2v7" />
											<path d="M11 18H8a2 2 0 0 1-2-2V9" />
										</svg>
									{:else if item.id === 'universities'}
										<svg class="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
											<path d="M3 21h18" />
											<path d="M5 21V7l7-4 7 4v14" />
											<path d="M9 21v-6h6v6" />
										</svg>
									{:else}
										<svg class="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
											<path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" />
											<path d="M3 3v5h5" />
											<path d="M12 7v5l4 2" />
										</svg>
									{/if}
								</span>

								{#if !sidebarCollapsed}
									<span class="text-[14.5px] font-semibold text-[#0b1c30] truncate">
										{item.label}
									</span>
								{/if}
							</button>
						{/each}
					</nav>
				</div>
			{/each}
		</div>

		<!-- Collapse Toggle -->
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
					{#if !sidebarCollapsed}
						<span>Collapse Sidebar</span>
					{/if}
				</span>
				{#if !sidebarCollapsed}
					<span class="font-mono-num text-[11px] text-[#515f74]">[M]</span>
				{/if}
			</button>
		</div>
	</aside>

	<!-- MAIN WORKSPACE -->
	<div class="pt-16 md:pt-[72px] pb-20 md:pb-8 {sidebarCollapsed ? 'md:pl-[72px]' : 'md:pl-64'} transition-all duration-150">
		<main class="min-h-[calc(100vh-4.5rem)] bg-[#f8f9ff] px-4 sm:px-6 lg:px-8 pb-8 pt-5 md:pt-6 max-w-[1440px]">
			<!-- ========================================================= -->
			<!-- VIEW 1: STUDENT APPLICATIONS & ELIGIBILITY                -->
			<!-- ========================================================= -->
			{#if activePage === 'applications'}
				<section class="space-y-5 md:space-y-6">
					<!-- Breadcrumb & Header -->
					<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
						<div class="flex flex-col gap-1.5 min-w-0">
							<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74] flex-wrap">
								<span>AUC IPSO</span>
								<span class="text-[#c6c6cd]">&rsaquo;</span>
								<span>Outgoing Exchange</span>
								<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
								<span class="hidden sm:inline text-[#0b1c30] font-semibold"
									>Student Applications &amp; Eligibility</span
								>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Student Applications &amp; Eligibility
							</h1>
						</div>
						<button
							type="button"
							onclick={() => navigate('matching')}
							class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center justify-center gap-2 cursor-pointer transition-colors shrink-0 self-start sm:self-auto"
						>
							<svg class="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
								<polygon points="5 3 19 12 5 21 5 3" />
							</svg>
							<span>Open Matching Console</span>
						</button>
					</div>

					<!-- 4 Metric Counters Ribbon -->
					<div class="grid grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col justify-between min-w-0">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74] truncate">
									Total Applicants
								</span>
								<svg class="w-5 h-5 text-[#515f74] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">
									<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2" />
									<circle cx="9" cy="7" r="4" />
								</svg>
							</div>
							<div class="mt-2.5 sm:mt-3 flex items-baseline gap-2 flex-wrap">
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tracking-tight tnum">186</span>
								<span class="text-xs sm:text-sm font-medium text-[#515f74]">92% complete</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">+24</span> from Fall 2025
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col justify-between min-w-0">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74] truncate">
									Eligible Cohort
								</span>
								<svg class="w-5 h-5 text-[#515f74] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">
									<path d="M22 11.08V12a10 10 0 1 1-5.93-9.14" />
									<polyline points="22 4 12 14.01 9 11.01" />
								</svg>
							</div>
							<div class="mt-2.5 sm:mt-3 flex items-baseline gap-2 flex-wrap">
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tracking-tight tnum">164</span>
								<span class="text-xs sm:text-sm font-medium text-[#515f74]">88.2% (&ge;2.80)</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">112</span> first-time abroad
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col justify-between min-w-0">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74] truncate">
									Available Seats
								</span>
								<svg class="w-5 h-5 text-[#515f74] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">
									<path d="M3 21h18" />
									<path d="M5 21V7l7-4 7 4v14" />
								</svg>
							</div>
							<div class="mt-2.5 sm:mt-3 flex items-baseline gap-2 flex-wrap">
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tracking-tight tnum">
									{116 + totalConfiguredSeats}
								</span>
								<span class="text-xs sm:text-sm font-medium text-[#515f74]">38 partner unis</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">HK Max &nu; = 138</span> placeable
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col justify-between min-w-0">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#ba1a1a] truncate">
									Flagged Audits
								</span>
								<svg class="w-5 h-5 text-[#ba1a1a] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
									<circle cx="12" cy="12" r="10" />
									<line x1="12" y1="8" x2="12" y2="12" />
									<line x1="12" y1="16" x2="12.01" y2="16" />
								</svg>
							</div>
							<div class="mt-2.5 sm:mt-3 flex items-baseline gap-2 flex-wrap">
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#ba1a1a] tracking-tight tnum">22</span>
								<span class="text-xs sm:text-sm font-medium text-[#ba1a1a]">Review required</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#ba1a1a]">14 GPA &lt; 2.8</span> · 8 final sem.
							</div>
						</div>
					</div>

					<!-- Collapsible Advanced Application Filtration Panel -->
					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
						<div
							role="button"
							tabindex="0"
							onclick={() => (filterPanelOpen = !filterPanelOpen)}
							onkeydown={(e) => {
								if (e.key === 'Enter' || e.key === ' ') {
									e.preventDefault();
									filterPanelOpen = !filterPanelOpen;
								}
							}}
							class="px-4 sm:px-5 py-4 flex items-center justify-between bg-[#eff4ff] cursor-pointer select-none gap-2"
						>
							<div class="flex items-center gap-2.5 min-w-0">
								<svg class="w-5 h-5 text-[#0b1c30] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
									<line x1="4" y1="21" x2="4" y2="14" />
									<line x1="4" y1="10" x2="4" y2="3" />
									<line x1="12" y1="21" x2="12" y2="12" />
									<line x1="12" y1="8" x2="12" y2="3" />
									<line x1="20" y1="21" x2="20" y2="16" />
									<line x1="20" y1="12" x2="20" y2="3" />
									<line x1="1" y1="14" x2="7" y2="14" />
									<line x1="9" y1="8" x2="15" y2="8" />
									<line x1="17" y1="16" x2="23" y2="16" />
								</svg>
								<span class="text-base sm:text-lg font-semibold text-[#0b1c30] truncate">
									<span class="hidden sm:inline">Advanced Application Filtration</span>
									<span class="sm:hidden">Filters</span>
								</span>
								<span class="px-2.5 py-0.5 sm:py-1 bg-[#131b2e] text-white text-xs font-medium rounded-full shrink-0">
									{activeFilterCount} active
								</span>
							</div>
							<div class="flex items-center gap-3 sm:gap-4 shrink-0">
								<button
									type="button"
									onclick={(e) => {
										e.stopPropagation();
										resetAppFilters();
									}}
									class="text-xs sm:text-sm font-medium text-[#515f74] hover:text-[#0b1c30] underline cursor-pointer"
								>
									Reset All
								</button>
								<svg
									class="w-4 h-4 text-[#515f74] transition-transform {filterPanelOpen ? 'rotate-180' : ''}"
									viewBox="0 0 24 24"
									fill="none"
									stroke="currentColor"
									stroke-width="2"
								>
									<polyline points="6 9 12 15 18 9" />
								</svg>
							</div>
						</div>

						{#if filterPanelOpen}
							<div class="p-4 sm:p-5 flex flex-col gap-4 border-t border-[#c6c6cd]/25">
								<!-- Quick-Toggle Filter Chips -->
								<div class="flex items-center gap-2 overflow-x-auto no-scrollbar sm:flex-wrap pb-0.5">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74] shrink-0">
										Quick Filters:
									</span>
									<button
										type="button"
										onclick={() => (gpaMin = gpaMin >= 2.8 ? 0 : 2.8)}
										class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer transition-colors {gpaMin >=
										2.8
											? 'bg-[#131b2e] text-white'
											: 'bg-[#e5eeff] text-[#0b1c30] hover:bg-[#dce9ff]'}"
									>
										<span>GPA &ge; 2.80</span>
										{#if gpaMin >= 2.8}<span>&times;</span>{/if}
									</button>
									<button
										type="button"
										onclick={() => (priorFilter = priorFilter === 'first' ? 'all' : 'first')}
										class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer transition-colors {priorFilter ===
										'first'
											? 'bg-[#131b2e] text-white'
											: 'bg-[#e5eeff] text-[#0b1c30] hover:bg-[#dce9ff]'}"
									>
										<span>First-Time Abroad</span>
										{#if priorFilter === 'first'}<span>&times;</span>{/if}
									</button>
									<button
										type="button"
										onclick={() => (standingFilter = standingFilter === 'Junior' ? 'all' : 'Junior')}
										class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer transition-colors {standingFilter ===
										'Junior'
											? 'bg-[#131b2e] text-white'
											: 'bg-[#e5eeff] text-[#0b1c30] hover:bg-[#dce9ff]'}"
									>
										<span>Junior Standing</span>
										{#if standingFilter === 'Junior'}<span>&times;</span>{/if}
									</button>
									{#if uniFilter !== 'all'}
										<button
											type="button"
											onclick={() => (uniFilter = 'all')}
											class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full bg-[#131b2e] text-white text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer"
										>
											<span>Uni: {uniFilter}</span><span>&times;</span>
										</button>
									{/if}
									{#if statusFilter !== 'all'}
										<button
											type="button"
											onclick={() => (statusFilter = 'all')}
											class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full bg-[#131b2e] text-white text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer"
										>
											<span>Status: {statusFilter}</span><span>&times;</span>
										</button>
									{/if}
								</div>

								<!-- Responsive 6-Control Filter Grid -->
								<div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-6 gap-3.5 pt-1">
									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-standing" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											Class Standing
										</label>
										<div class="relative">
											<select
												id="filter-standing"
												bind:value={standingFilter}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Standings</option>
												<option value="Junior">Junior</option>
												<option value="Sophomore">Sophomore</option>
												<option value="Senior">Senior (Non-Grad)</option>
											</select>
											<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
												<polyline points="7 15 12 20 17 15" />
												<polyline points="7 9 12 4 17 9" />
											</svg>
										</div>
									</div>

									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-gpa-min" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											GPA Range
										</label>
										<div class="flex items-center gap-1.5 min-w-0">
											<input
												id="filter-gpa-min"
												bind:value={gpaMin}
												class="min-w-0 flex-1 h-11 px-2.5 bg-white text-[#0b1c30] text-sm font-mono-num rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm"
												max="4"
												min="0"
												step="0.1"
												type="number"
												aria-label="Minimum GPA"
											/>
											<span class="text-[#515f74] font-medium shrink-0">-</span>
											<input
												id="filter-gpa-max"
												bind:value={gpaMax}
												class="min-w-0 flex-1 h-11 px-2.5 bg-white text-[#0b1c30] text-sm font-mono-num rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm"
												max="4"
												min="0"
												step="0.1"
												type="number"
												aria-label="Maximum GPA"
											/>
										</div>
									</div>

									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-uni" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											Preferred Uni
										</label>
										<div class="relative">
											<select
												id="filter-uni"
												bind:value={uniFilter}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Partner Unis</option>
												<option value="ETH Zurich">ETH Zurich</option>
												<option value="Univ. of Amsterdam">Univ. of Amsterdam</option>
												<option value="Sciences Po">Sciences Po Paris</option>
												<option value="UC Berkeley">UC Berkeley</option>
												<option value="Bocconi Univ.">Bocconi University</option>
												<option value="Yonsei Univ.">Yonsei University</option>
											</select>
											<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
												<polyline points="7 15 12 20 17 15" />
												<polyline points="7 9 12 4 17 9" />
											</svg>
										</div>
									</div>

									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-prior" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											Prior Exchange
										</label>
										<div class="relative">
											<select
												id="filter-prior"
												bind:value={priorFilter}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Applicants</option>
												<option value="first">First-Time (Priority)</option>
												<option value="prior">Prior Intl Study</option>
											</select>
											<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
												<polyline points="7 15 12 20 17 15" />
												<polyline points="7 9 12 4 17 9" />
											</svg>
										</div>
									</div>

									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-major" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											AUC Major
										</label>
										<div class="relative">
											<input
												id="filter-major"
												bind:value={majorFilter}
												class="w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm"
												placeholder="CSCE, ECON..."
												type="text"
											/>
											<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
												<circle cx="11" cy="11" r="8" />
												<line x1="21" y1="21" x2="16.65" y2="16.65" />
											</svg>
										</div>
									</div>

									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-status" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											Audit Status
										</label>
										<div class="relative">
											<select
												id="filter-status"
												bind:value={statusFilter}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Statuses</option>
												<option value="Verified">Verified Eligible</option>
												<option value="In Review">In Review</option>
												<option value="Ineligible">Ineligible / Flagged</option>
											</select>
											<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
												<polyline points="7 15 12 20 17 15" />
												<polyline points="7 9 12 4 17 9" />
											</svg>
										</div>
									</div>
								</div>
							</div>
						{/if}
					</div>

					<!-- Cohort Sub-tabs + Search Toolbar + Desktop Table / Mobile Cards -->
					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col overflow-hidden">
						<div class="p-4 flex flex-col lg:flex-row lg:items-center justify-between gap-3 bg-[#eff4ff] border-b border-[#c6c6cd]/30">
							<div class="flex items-center gap-1.5 bg-[#d3e4fe]/40 p-1 rounded-xl max-w-full overflow-x-auto no-scrollbar">
								<button
									type="button"
									onclick={() => (studentTab = 'all')}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'all'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									All Applicants ({students.length})
								</button>
								<button
									type="button"
									onclick={() => (studentTab = 'eligible')}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'eligible'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									Eligible ({students.filter((s) => s.gpa >= 2.8).length})
								</button>
								<button
									type="button"
									onclick={() => (studentTab = 'flagged')}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'flagged'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									Flagged ({students.filter((s) => s.status !== 'Verified' || s.gpa < 2.8).length})
								</button>
							</div>

							<div class="flex items-center gap-2.5 flex-wrap">
								<div class="relative flex items-center flex-1 sm:flex-initial min-w-0">
									<svg class="w-4 h-4 text-[#515f74] absolute left-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
										<circle cx="11" cy="11" r="8" />
										<line x1="21" y1="21" x2="16.65" y2="16.65" />
									</svg>
									<input
										id="student-search"
										bind:value={searchQuery}
										class="h-10 sm:h-11 pl-10 pr-14 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm placeholder:text-[#515f74] w-full sm:w-64 lg:w-72"
										placeholder="Search applicants..."
										type="search"
									/>
									<span class="hidden sm:inline-block absolute right-3 text-xs text-[#515f74] bg-[#e5eeff] px-1.5 py-0.5 rounded font-mono-num">
										⌘K
									</span>
								</div>
								<button
									type="button"
									onclick={() => showToast('Exported filtered applicants to CSV.')}
									class="h-10 sm:h-11 px-4 bg-white text-[#515f74] hover:text-[#0b1c30] rounded-xl border border-[#c6c6cd]/40 flex items-center gap-2 shadow-sm text-xs sm:text-sm font-semibold cursor-pointer shrink-0"
								>
									<svg class="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
										<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
										<polyline points="7 10 12 15 17 10" />
										<line x1="12" y1="15" x2="12" y2="3" />
									</svg>
									<span>Export CSV</span>
								</button>
							</div>
						</div>

						<!-- Desktop Data Table (md+) -->
						<div class="hidden md:block overflow-x-auto w-full">
							<table class="w-full text-left border-collapse">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider select-none border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-4 px-4 w-12 text-center">
											<input
												type="checkbox"
												checked={selectAllChecked}
												onchange={(e) => toggleAllStudents((e.currentTarget as HTMLInputElement).checked)}
												class="rounded w-4 h-4 accent-[#131b2e] cursor-pointer"
												aria-label="Select all"
											/>
										</th>
										<th class="py-4 px-4">Applicant</th>
										<th class="py-4 px-4">Standing &amp; Major</th>
										<th class="py-4 px-4 text-right">GPA</th>
										<th class="py-4 px-4">Ranked Top 3 Preferences</th>
										<th class="py-4 px-4 text-right">LLM Fit</th>
										<th class="py-4 px-4">Intl. Equity</th>
										<th class="py-4 px-4">Audit Status</th>
										<th class="py-4 px-5 text-right">Actions</th>
									</tr>
								</thead>
								<tbody class="text-[#0b1c30] divide-y divide-[#c6c6cd]/20 text-sm sm:text-[15px]">
									{#if filteredStudents.length > 0}
										{#each filteredStudents as s (s.id)}
											<tr class="hover:bg-[#eff4ff]/50 transition-colors">
												<td class="py-4 px-4 text-center">
													<input
														type="checkbox"
														bind:checked={s.selected}
														class="rounded w-4 h-4 accent-[#131b2e] cursor-pointer"
														aria-label={`Select ${s.name}`}
													/>
												</td>
												<td class="py-4 px-4">
													<div class="flex flex-col">
														<span class="font-semibold text-[15px] text-[#0b1c30]">{s.name}</span>
														<span class="text-xs text-[#515f74] font-mono-num">
															#{s.id} · {s.email}
														</span>
													</div>
												</td>
												<td class="py-4 px-4">
													<div class="flex flex-col gap-0.5">
														<span class="font-medium text-[#0b1c30]">{s.standing}</span>
														<span class="text-xs sm:text-[13px] text-[#515f74]">{s.major}</span>
													</div>
												</td>
												<td class="py-4 px-4 text-right">
													<div class="flex flex-col items-end">
														<div class="flex items-center gap-1.5">
															<span
																class="text-[15px] font-bold font-mono-num {s.gpa < 2.8
																	? 'text-[#ba1a1a]'
																	: 'text-[#0b1c30]'}"
															>
																{s.gpa.toFixed(2)}
															</span>
															{#if s.gpa >= 3.7}
																<span class="w-2 h-2 rounded-full bg-[#131b2e]" title="Honors Tier"></span>
															{/if}
														</div>
														<span class="text-xs text-[#515f74]">
															{s.gpa >= 3.7 ? 'Honors' : s.gpa >= 2.8 ? 'Eligible' : 'Below Min'}
														</span>
													</div>
												</td>
												<td class="py-4 px-4">
													<div class="flex items-center gap-1.5 flex-wrap max-w-[290px]">
														{#each s.prefs as p, i (p)}
															<span
																class="px-2.5 py-0.5 rounded-full text-xs whitespace-nowrap {i === 0
																	? 'bg-[#e5eeff] text-[#0b1c30] font-semibold'
																	: 'bg-[#eff4ff] text-[#515f74]'}"
															>
																<span class="font-mono-num">#{i + 1}</span>
																{p}
															</span>
														{/each}
													</div>
												</td>
												<td class="py-4 px-4 text-right font-mono-num font-semibold">
													{s.llm.toFixed(1)}<span class="text-xs text-[#515f74] font-normal">/10</span>
												</td>
												<td class="py-4 px-4">
													{#if !s.prior}
														<span class="px-2.5 py-1 rounded-full bg-[#e5eeff] text-[#0b1c30] text-xs font-semibold whitespace-nowrap">
															First-Time
														</span>
													{:else}
														<span class="px-2.5 py-1 rounded-full bg-[#eff4ff] text-[#515f74] text-xs font-medium whitespace-nowrap">
															Prior Abroad
														</span>
													{/if}
												</td>
												<td class="py-4 px-4">
													<span class="px-2.5 py-1 rounded-full text-xs font-semibold whitespace-nowrap {statusBadgeClass(s.status)}">
														{s.status}
													</span>
												</td>
												<td class="py-4 px-5 text-right">
													<button
														type="button"
														onclick={() => (inspectedStudentId = s.id)}
														title="Inspect application"
														class="h-9 w-9 rounded-lg inline-flex items-center justify-center text-[#515f74] hover:bg-[#dce9ff] hover:text-[#0b1c30] transition-colors cursor-pointer"
													>
														<svg class="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
															<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z" />
															<circle cx="12" cy="12" r="3" />
														</svg>
													</button>
												</td>
											</tr>
										{/each}
									{:else}
										<tr>
											<td colspan="9" class="py-10 text-center text-sm text-[#515f74]">
												No student applications match the current filters.
											</td>
										</tr>
									{/if}
								</tbody>
							</table>
						</div>

						<!-- Mobile Sister Card List (< md) -->
						<div class="md:hidden p-3 space-y-3 bg-[#f8f9ff]">
							{#if filteredStudents.length > 0}
								{#each filteredStudents as s (s.id)}
									<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col gap-3">
										<div class="flex items-start justify-between gap-2">
											<div class="min-w-0">
												<div class="flex items-center gap-1.5 flex-wrap">
													<span class="text-[15px] font-semibold text-[#0b1c30]">{s.name}</span>
													<span class="px-2 py-0.5 rounded-full text-[11px] font-semibold bg-[#dce9ff] text-[#0b1c30]">
														{s.standing}
													</span>
												</div>
												<div class="text-xs text-[#515f74] mt-0.5 font-mono-num truncate">
													#{s.id} · {s.major}
												</div>
											</div>
											<div class="text-right shrink-0">
												<div
													class="flex items-center justify-end gap-1 font-bold text-[15px] font-mono-num {s.gpa <
													2.8
														? 'text-[#ba1a1a]'
														: 'text-[#0b1c30]'}"
												>
													<span>{s.gpa.toFixed(2)}</span>
													{#if s.gpa >= 3.7}
														<span class="h-2 w-2 rounded-full bg-[#131b2e]"></span>
													{/if}
												</div>
												<span class="text-[11px] text-[#515f74]">
													{s.gpa >= 3.7 ? 'Honors' : s.gpa >= 2.8 ? 'Eligible' : 'Flagged'}
												</span>
											</div>
										</div>
										<div class="pt-2.5 border-t border-[#c6c6cd]/20 space-y-1.5 text-xs sm:text-sm">
											<div class="flex justify-between gap-2">
												<span class="text-[#515f74] shrink-0">Top Choices</span>
												<span class="font-medium text-[#0b1c30] text-right truncate">
													{s.prefs.slice(0, 2).join(' · ')}
												</span>
											</div>
											<div class="flex justify-between gap-2">
												<span class="text-[#515f74]">LLM Fit / Equity</span>
												<span class="font-mono-num">
													{s.llm.toFixed(1)}/10 · {s.prior ? 'Prior' : '1st-Time'}
												</span>
											</div>
										</div>
										<div class="pt-2.5 border-t border-[#c6c6cd]/20 flex items-center justify-between">
											<span class="px-2.5 py-1 rounded-full text-xs font-semibold {statusBadgeClass(s.status)}">
												{s.status}
											</span>
											<button
												type="button"
												onclick={() => (inspectedStudentId = s.id)}
												class="h-8 px-3.5 rounded-lg bg-[#eff4ff] hover:bg-[#dce9ff] text-xs font-semibold text-[#0b1c30] cursor-pointer"
											>
												View
											</button>
										</div>
									</div>
								{/each}
							{:else}
								<div class="bg-white p-6 rounded-xl text-center text-sm text-[#515f74] border border-[#c6c6cd]/40">
									No student applications match the current filters.
								</div>
							{/if}
						</div>

						<!-- Pagination Footer -->
						<div class="p-4 bg-[#eff4ff] flex flex-col sm:flex-row items-center justify-between gap-3 border-t border-[#c6c6cd]/30">
							<div class="flex items-center gap-3 flex-wrap justify-center sm:justify-start">
								<span class="text-xs sm:text-sm text-[#515f74]">
									Showing <span class="font-semibold text-[#0b1c30]"
										>{filteredStudents.length > 0 ? `1 to ${filteredStudents.length}` : '0'}</span
									>
									of <span class="font-semibold text-[#0b1c30]">186</span> applications
								</span>
								<div class="flex items-center gap-2">
									<label for="rows-per-page" class="text-xs text-[#515f74] font-medium">Rows:</label>
									<select
										id="rows-per-page"
										bind:value={rowsPerPage}
										class="stitch-select h-9 pl-2.5 pr-6 bg-white text-[#0b1c30] text-xs font-medium rounded-lg border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:ring-0"
									>
										<option value="25">25</option>
										<option value="50">50</option>
										<option value="100">100</option>
									</select>
								</div>
							</div>
							<div class="flex items-center gap-2">
								<button
									type="button"
									class="h-9 px-3.5 bg-white text-[#515f74] rounded-lg border border-[#c6c6cd]/40 shadow-sm text-xs font-semibold opacity-60 cursor-not-allowed"
								>
									Previous
								</button>
								<span class="h-9 w-9 rounded-lg bg-[#131b2e] text-white font-semibold text-xs flex items-center justify-center shadow-sm">
									1
								</span>
								<button
									type="button"
									onclick={() => showToast('Demo dataset shows cohort sample page 1.')}
									class="h-9 px-3.5 bg-white text-[#0b1c30] hover:bg-[#dce9ff] rounded-lg border border-[#c6c6cd]/40 shadow-sm text-xs font-semibold cursor-pointer"
								>
									Next
								</button>
							</div>
						</div>
					</div>
				</section>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 2: BIPARTITE MATCHING CONSOLE                        -->
			<!-- ========================================================= -->
			{#if activePage === 'matching'}
				<section class="space-y-5 md:space-y-6">
					<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
						<div class="flex flex-col gap-1.5 min-w-0">
							<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74]">
								<span>AUC IPSO</span>
								<span class="text-[#c6c6cd]">&rsaquo;</span>
								<span>Matching &amp; Allocation</span>
								<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
								<span class="hidden sm:inline text-[#0b1c30] font-semibold"
									>Bipartite Simulation Console</span
								>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Bipartite Matching Console
							</h1>
						</div>
						<div class="flex items-center gap-2.5 flex-wrap">
							<span class="px-3.5 py-2 bg-[#eff4ff] border border-[#c6c6cd]/40 rounded-full text-xs sm:text-[13px] font-mono-num text-[#0b1c30] shadow-sm">
								{activeRunLabel}
							</span>
							<button
								type="button"
								onclick={saveDraftRun}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-white hover:bg-[#eff4ff] text-[#0b1c30] border border-[#c6c6cd]/50 text-xs sm:text-sm font-semibold rounded-xl shadow-sm cursor-pointer"
							>
								Save Draft Run
							</button>
							<button
								type="button"
								onclick={publishResults}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm cursor-pointer"
							>
								Publish Results
							</button>
						</div>
					</div>

					<!-- TOP FULL-WIDTH: Algorithm & Weight Configuration Card -->
					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
						<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between flex-wrap gap-3">
							<div class="flex items-center gap-2.5 min-w-0 flex-wrap">
								<svg class="w-5 h-5 text-[#0b1c30] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
									<circle cx="18" cy="18" r="3" />
									<circle cx="6" cy="6" r="3" />
									<path d="M13 6h3a2 2 0 0 1 2 2v7" />
									<path d="M11 18H8a2 2 0 0 1-2-2V9" />
								</svg>
								<span class="text-base sm:text-lg font-semibold text-[#0b1c30]">
									Algorithm &amp; Weight Configuration
								</span>
								<span class="hidden sm:inline-block font-mono-num text-xs text-[#0b1c30] bg-[#e5eeff] px-3 py-1 rounded-full font-medium">
									W = Pref·(λ + β·Merit) + γ·R
								</span>
							</div>
							<div class="flex items-center gap-3 shrink-0">
								<button
									type="button"
									onclick={resetWeights}
									class="text-xs sm:text-sm font-medium text-[#515f74] hover:text-[#0b1c30] underline cursor-pointer"
								>
									Reset Defaults
								</button>
								<button
									type="button"
									onclick={triggerMatchAnimation}
									class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center gap-2 cursor-pointer"
								>
									<svg class="w-4 h-4" viewBox="0 0 24 24" fill="currentColor">
										<polygon points="6 4 20 12 6 20 6 4" />
									</svg>
									<span>Re-run Simulation</span>
								</button>
								<button
									type="button"
									onclick={() => (mobileWeightsOpen = !mobileWeightsOpen)}
									aria-label="Toggle weight sliders"
									class="md:hidden h-9 w-9 rounded-lg border border-[#c6c6cd]/40 bg-white flex items-center justify-center text-[#515f74] cursor-pointer"
								>
									<svg
										class="w-4 h-4 transition-transform {mobileWeightsOpen ? 'rotate-180' : ''}"
										viewBox="0 0 24 24"
										fill="none"
										stroke="currentColor"
										stroke-width="2"
									>
										<polyline points="6 9 12 15 18 9" />
									</svg>
								</button>
							</div>
						</div>

						<div class="{mobileWeightsOpen ? 'block' : 'hidden md:block'} p-4 sm:p-5 space-y-5">
							<!-- Top Row: Strategy + Fallback Toggle + HK Feasibility Pill -->
							<div class="grid grid-cols-1 md:grid-cols-12 gap-4 items-center">
								<div class="md:col-span-5 flex flex-col gap-1.5 min-w-0">
									<label for="algo-mode" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										Allocation Strategy
									</label>
									<div class="relative">
										<select
											id="algo-mode"
											bind:value={weights.mode}
											class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:border-black focus:ring-0 truncate"
										>
											<option value="lex">Cardinality-First + Hungarian (Max placed, then weight)</option>
											<option value="hungarian">Pure Max-Weight (Hungarian O(K³))</option>
											<option value="hk">Feasibility Only (Hopcroft-Karp O(E√V))</option>
										</select>
										<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
											<polyline points="7 15 12 20 17 15" />
											<polyline points="7 9 12 4 17 9" />
										</svg>
									</div>
								</div>

								<div class="md:col-span-4 flex items-center gap-2.5 md:pt-5">
									<label class="flex items-start gap-2.5 cursor-pointer">
										<input
											bind:checked={weights.allowUnranked}
											type="checkbox"
											class="mt-0.5 rounded w-4 h-4 accent-[#131b2e]"
										/>
										<span class="text-xs sm:text-sm text-[#0b1c30]">
											<span class="font-semibold">Allow eligible unranked fallback (ε = 0.08)</span>
											<span class="block text-xs text-[#515f74]">
												Place in other eligible partners if all Top 3 are full.
											</span>
										</span>
									</label>
								</div>

								<div class="md:col-span-3 md:text-right md:pt-5">
									<div class="inline-flex items-center gap-2 px-3.5 py-2 bg-[#eff4ff] rounded-full text-xs sm:text-sm text-[#0b1c30]">
										<span class="text-[#515f74]">HK Feasibility Max:</span>
										<span class="font-mono-num font-bold">138 / 164</span>
									</div>
								</div>
							</div>

							<!-- Bottom Row: 4 Horizontal Weight Sliders -->
							<div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-5 pt-4 border-t border-[#c6c6cd]/25">
								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">Preference Dominance (λ)</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.pref.toFixed(2)}</span>
									</div>
									<input
										bind:value={weights.pref}
										type="range"
										min="0.2"
										max="1.0"
										step="0.05"
										class="w-full h-1.5 cursor-pointer"
									/>
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">Academic GPA Weight</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.gpa.toFixed(2)}</span>
									</div>
									<input
										bind:value={weights.gpa}
										type="range"
										min="0.0"
										max="1.0"
										step="0.05"
										class="w-full h-1.5 cursor-pointer"
									/>
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">LLM Qualitative Fit</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.llm.toFixed(2)}</span>
									</div>
									<input
										bind:value={weights.llm}
										type="range"
										min="0.0"
										max="1.0"
										step="0.05"
										class="w-full h-1.5 cursor-pointer"
									/>
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">First-Time Equity Bonus</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.equity.toFixed(2)}</span>
									</div>
									<input
										bind:value={weights.equity}
										type="range"
										min="0.0"
										max="0.5"
										step="0.05"
										class="w-full h-1.5 cursor-pointer"
									/>
								</div>
							</div>
						</div>
					</div>

					<!-- BELOW: Full-width Simulation KPIs, Diff Strip, and Results -->
					<div class="space-y-4 sm:space-y-5">
						<div class="grid grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
							<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
								<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
									Placed Students
								</div>
								<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
									<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">{simPlaced}</span>
									<span class="text-xs sm:text-sm text-[#515f74] tnum">/ 164 ({((simPlaced / 164) * 100).toFixed(1)}%)</span>
								</div>
								<div class="mt-2 text-xs sm:text-sm text-[#15803d] font-medium">
									Matches HK max (&nu;=138)
								</div>
							</div>

							<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
								<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
									1st Choice Rate
								</div>
								<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
									<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">{simFirstPct.toFixed(1)}%</span>
									<span class="text-xs sm:text-sm text-[#515f74] tnum">({simFirstCount})</span>
								</div>
								<div class="mt-2 text-xs sm:text-sm text-[#515f74] tnum">{simChoiceSplit}</div>
							</div>

							<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
								<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
									Avg Edge Weight
								</div>
								<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2">
									<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] font-mono-num">{simAvgWeight}</span>
								</div>
								<div class="mt-2 text-xs sm:text-sm text-[#515f74]">Normalized [0–1.50]</div>
							</div>

							<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
								<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#ba1a1a]">
									Unplaced Eligible
								</div>
								<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
									<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#ba1a1a] tnum">{simUnplaced}</span>
									<span class="text-xs sm:text-sm text-[#ba1a1a] font-medium">students</span>
								</div>
								<div class="mt-2 text-xs sm:text-sm text-[#515f74]">19 contention · 7 no seats</div>
							</div>
						</div>

						<!-- Run Diff Strip -->
						<div class="bg-[#eff4ff] border border-[#c6c6cd]/40 rounded-xl shadow-sm px-4 sm:px-5 py-3.5 flex flex-wrap items-center justify-between gap-2.5 text-xs sm:text-sm">
							<div class="flex items-center gap-2.5 flex-wrap">
								<span class="px-3 py-0.5 bg-white border border-[#c6c6cd]/40 rounded-full font-mono-num font-semibold text-[#0b1c30]">
									DIFF vs Run #3
								</span>
								<span class="text-[#0b1c30]">
									<span class="font-semibold text-[#15803d]">+4 placed</span> ·
									<span class="font-semibold text-[#15803d]">+7 got 1st choice</span> ·
									<span class="font-semibold text-[#515f74]">11 assignments shifted</span>
								</span>
							</div>
							<button
								type="button"
								onclick={() => navigate('runs')}
								class="text-xs sm:text-sm font-semibold text-[#003ea8] hover:underline cursor-pointer"
							>
								View Run History &rarr;
							</button>
						</div>

						<!-- Results Table / Mobile Cards -->
						<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
							<div class="p-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between flex-wrap gap-3">
								<div class="flex items-center gap-1.5 bg-[#d3e4fe]/40 p-1 rounded-xl max-w-full overflow-x-auto no-scrollbar">
									<button
										type="button"
										onclick={() => (matchTab = 'matched')}
										class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
										'matched'
											? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
											: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
									>
										Matched Allocations ({simPlaced})
									</button>
									<button
										type="button"
										onclick={() => (matchTab = 'unplaced')}
										class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
										'unplaced'
											? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
											: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
									>
										Unplaced Analysis ({simUnplaced})
									</button>
									<button
										type="button"
										onclick={() => (matchTab = 'unis')}
										class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
										'unis'
											? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
											: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
									>
										University Fill Rate (38)
									</button>
								</div>
								<span class="text-xs sm:text-sm font-mono-num text-[#515f74]">Solved in 18 ms</span>
							</div>

							<!-- Desktop Tables (md+) -->
							<div class="hidden md:block overflow-x-auto">
								{#if matchTab === 'matched'}
									<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
										<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
											<tr>
												<th class="py-4 px-5">Student</th>
												<th class="py-4 px-4 text-right">Merit Score</th>
												<th class="py-4 px-4">Assigned University (Slot)</th>
												<th class="py-4 px-4">Preference Rank</th>
												<th class="py-4 px-5 text-right">Edge Weight W(s,u)</th>
											</tr>
										</thead>
										<tbody class="divide-y divide-[#c6c6cd]/20">
											{#each MOCK_MATCHED_ROWS as r (r.studentId)}
												<tr class="hover:bg-[#eff4ff]/50">
													<td class="py-4 px-5 font-semibold text-[#0b1c30]">{r.studentName}</td>
													<td class="py-4 px-4 text-right font-mono-num">{r.merit.toFixed(2)}</td>
													<td class="py-4 px-4 font-medium">
														{r.assignedUni} <span class="text-xs text-[#515f74]">({r.slotLabel})</span>
													</td>
													<td class="py-4 px-4">
														<span
															class="px-2.5 py-1 rounded-full text-xs font-semibold {r.rankLabel ===
															'#1 Choice'
																? 'bg-[#f0fdf4] text-[#15803d]'
																: 'bg-[#e5eeff] text-[#0b1c30]'}"
														>
															{r.rankLabel}
														</span>
													</td>
													<td class="py-4 px-5 text-right font-mono-num font-semibold">
														{r.weight.toFixed(3)}
													</td>
												</tr>
											{/each}
										</tbody>
									</table>
								{:else if matchTab === 'unplaced'}
									<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
										<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
											<tr>
												<th class="py-4 px-5">Student</th>
												<th class="py-4 px-4 text-right">GPA</th>
												<th class="py-4 px-4">Requested Preferences</th>
												<th class="py-4 px-4">Bottleneck Category</th>
												<th class="py-4 px-5">Graph Explanation</th>
											</tr>
										</thead>
										<tbody class="divide-y divide-[#c6c6cd]/20">
											{#each MOCK_UNPLACED_ROWS as u (u.studentName)}
												<tr class="hover:bg-[#eff4ff]/50">
													<td class="py-4 px-5 font-semibold text-[#0b1c30]">{u.studentName}</td>
													<td class="py-4 px-4 text-right font-mono-num">{u.gpa.toFixed(2)}</td>
													<td class="py-4 px-4 text-xs sm:text-sm text-[#515f74]">{u.prefsLabel}</td>
													<td class="py-4 px-4">
														<span
															class="px-2.5 py-1 rounded-full text-xs font-semibold {u.bottleneck ===
															'Capacity Contention'
																? 'bg-[#fffbeb] text-[#b45309]'
																: 'bg-[#fef2f2] text-[#ba1a1a]'}"
														>
															{u.bottleneck}
														</span>
													</td>
													<td class="py-4 px-5 text-xs sm:text-sm text-[#515f74]">{u.reason}</td>
												</tr>
											{/each}
										</tbody>
									</table>
								{:else}
									<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
										<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
											<tr>
												<th class="py-4 px-5">Partner University</th>
												<th class="py-4 px-4">Country</th>
												<th class="py-4 px-4 text-right">Filled / Seats</th>
												<th class="py-4 px-4 text-right">Demand Ratio</th>
												<th class="py-4 px-5 text-right">Cutoff Merit</th>
											</tr>
										</thead>
										<tbody class="divide-y divide-[#c6c6cd]/20">
											{#each universities as u (u.id)}
												{@const ratio = u.seats > 0 ? u.demand / u.seats : u.demand}
												<tr class="hover:bg-[#eff4ff]/50">
													<td class="py-4 px-5 font-semibold text-[#0b1c30]">{u.name}</td>
													<td class="py-4 px-4 text-[#515f74]">{u.country}</td>
													<td class="py-4 px-4 text-right font-mono-num font-semibold">
														{Math.min(u.seats, u.demand)} / {u.seats}
													</td>
													<td class="py-4 px-4 text-right font-mono-num {ratio >= 4 ? 'text-[#ba1a1a] font-semibold' : ''}">
														{ratio.toFixed(1)}x ({u.demand} req)
													</td>
													<td class="py-4 px-5 text-right font-mono-num">
														{(0.72 + (u.minGpa - 3.0) * 0.35).toFixed(2)}
													</td>
												</tr>
											{/each}
										</tbody>
									</table>
								{/if}
							</div>

							<!-- Mobile Cards (< md) -->
							<div class="md:hidden p-3 space-y-3 bg-[#f8f9ff]">
								{#if matchTab === 'matched'}
									{#each MOCK_MATCHED_ROWS as r (r.studentId)}
										<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 space-y-2 text-xs sm:text-sm">
											<div class="flex justify-between items-center gap-2">
												<span class="font-semibold text-[15px] text-[#0b1c30]">{r.studentName}</span>
												<span
													class="px-2.5 py-0.5 rounded-full font-semibold text-xs shrink-0 {r.rankLabel ===
													'#1 Choice'
														? 'bg-[#f0fdf4] text-[#15803d]'
														: 'bg-[#e5eeff] text-[#0b1c30]'}"
												>
													{r.rankLabel}
												</span>
											</div>
											<div class="flex justify-between gap-2 text-[#515f74]">
												<span>{r.assignedUni} ({r.slotLabel})</span>
												<span class="font-mono-num font-semibold text-[#0b1c30] shrink-0">
													W = {r.weight.toFixed(3)}
												</span>
											</div>
										</div>
									{/each}
								{:else if matchTab === 'unplaced'}
									{#each MOCK_UNPLACED_ROWS as u (u.studentName)}
										<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 space-y-2 text-xs sm:text-sm">
											<div class="flex justify-between items-center gap-2">
												<span class="font-semibold text-[15px] text-[#0b1c30]">{u.studentName}</span>
												<span
													class="px-2.5 py-0.5 rounded-full font-semibold text-xs shrink-0 {u.bottleneck ===
													'Capacity Contention'
														? 'bg-[#fffbeb] text-[#b45309]'
														: 'bg-[#fef2f2] text-[#ba1a1a]'}"
												>
													{u.bottleneck}
												</span>
											</div>
											<div class="text-[#515f74]">{u.reason}</div>
										</div>
									{/each}
								{:else}
									{#each universities as u (u.id)}
										{@const ratio = u.seats > 0 ? u.demand / u.seats : u.demand}
										<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex justify-between items-center gap-2 text-xs sm:text-sm">
											<div class="min-w-0">
												<div class="font-semibold text-[15px] text-[#0b1c30] truncate">{u.name}</div>
												<div class="text-[#515f74]">{u.demand} requests · {u.seats} seats</div>
											</div>
											<span class="font-mono-num font-bold text-sm shrink-0 {ratio >= 4 ? 'text-[#ba1a1a]' : 'text-[#0b1c30]'}">
												{ratio.toFixed(1)}x
											</span>
										</div>
									{/each}
								{/if}
							</div>
						</div>
					</div>
				</section>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 3: PARTNER UNIVERSITIES & SEAT QUOTAS                -->
			<!-- ========================================================= -->
			{#if activePage === 'universities'}
				<section class="space-y-5 md:space-y-6">
					<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
						<div class="flex flex-col gap-1.5 min-w-0">
							<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74]">
								<span>AUC IPSO</span>
								<span class="text-[#c6c6cd]">&rsaquo;</span>
								<span>Partner Registry</span>
								<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
								<span class="hidden sm:inline text-[#0b1c30] font-semibold"
									>Partner Universities &amp; Seat Quotas</span
								>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Partner Universities &amp; Seat Quotas
							</h1>
						</div>
						<button
							type="button"
							onclick={() => showToast('Partner onboarding form connects to POST /api/admin/universities.')}
							class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm cursor-pointer shrink-0 self-start sm:self-auto"
						>
							+ Add Partner University
						</button>
					</div>

					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
						<div class="px-4 sm:px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3 flex-wrap">
							<span class="text-xs sm:text-sm font-medium text-[#515f74]">
								Adjust seat quotas to test how slot-node expansion affects Hopcroft-Karp capacity.
							</span>
							<span class="px-3.5 py-1.5 bg-white border border-[#c6c6cd]/40 rounded-full text-xs sm:text-sm font-mono-num font-semibold text-[#0b1c30] shadow-sm">
								Configured Seats: {totalConfiguredSeats}
							</span>
						</div>

						<!-- Desktop Table -->
						<div class="hidden md:block overflow-x-auto">
							<table class="w-full text-left border-collapse">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-4 px-5">Partner University</th>
										<th class="py-4 px-4">Country / Region</th>
										<th class="py-4 px-4 text-right">Min GPA</th>
										<th class="py-4 px-4 text-right">Est. Living/Sem</th>
										<th class="py-4 px-4">Eligible Majors</th>
										<th class="py-4 px-4 text-center">Seats (Slots)</th>
										<th class="py-4 px-5 text-right">1st-Choice Demand</th>
									</tr>
								</thead>
								<tbody class="divide-y divide-[#c6c6cd]/20 text-sm sm:text-[15px]">
									{#each universities as u, idx (u.id)}
										<tr class="hover:bg-[#eff4ff]/50">
											<td class="py-4 px-5 font-semibold text-[#0b1c30]">{u.name}</td>
											<td class="py-4 px-4 text-[#515f74]">{u.country} · {u.region}</td>
											<td class="py-4 px-4 text-right font-mono-num">{u.minGpa.toFixed(2)}</td>
											<td class="py-4 px-4 text-right font-mono-num">${u.cost.toLocaleString()}</td>
											<td class="py-4 px-4 text-[#515f74] text-xs sm:text-sm">{u.majors}</td>
											<td class="py-4 px-4 text-center">
												<div class="inline-flex items-center border border-[#c6c6cd]/40 rounded-xl bg-white shadow-sm">
													<button
														type="button"
														onclick={() => adjustSeat(idx, -1)}
														class="w-8 h-8 text-[#515f74] hover:text-[#0b1c30] font-semibold cursor-pointer"
														aria-label={`Decrease seats for ${u.name}`}
													>
														-
													</button>
													<span class="w-9 text-center font-mono-num font-semibold text-sm">{u.seats}</span>
													<button
														type="button"
														onclick={() => adjustSeat(idx, 1)}
														class="w-8 h-8 text-[#515f74] hover:text-[#0b1c30] font-semibold cursor-pointer"
														aria-label={`Increase seats for ${u.name}`}
													>
														+
													</button>
												</div>
											</td>
											<td class="py-4 px-5 text-right font-mono-num text-xs sm:text-sm">{u.demand} applicants</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>

						<!-- Mobile Partner Cards -->
						<div class="md:hidden p-3 space-y-3 bg-[#f8f9ff]">
							{#each universities as u, idx (u.id)}
								<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex items-center justify-between gap-3 text-xs sm:text-sm">
									<div class="min-w-0">
										<div class="font-semibold text-[15px] text-[#0b1c30] truncate">{u.name}</div>
										<div class="text-[#515f74] mt-0.5">
											{u.country} · Min GPA {u.minGpa.toFixed(2)} · {u.demand} req
										</div>
									</div>
									<div class="inline-flex items-center border border-[#c6c6cd]/40 rounded-xl bg-[#f8f9ff] shrink-0">
										<button
											type="button"
											onclick={() => adjustSeat(idx, -1)}
											class="w-9 h-9 cursor-pointer font-semibold text-base"
											aria-label={`Decrease seats for ${u.name}`}
										>
											-
										</button>
										<span class="w-8 text-center font-mono-num font-bold text-sm">{u.seats}</span>
										<button
											type="button"
											onclick={() => adjustSeat(idx, 1)}
											class="w-9 h-9 cursor-pointer font-semibold text-base"
											aria-label={`Increase seats for ${u.name}`}
										>
											+
										</button>
									</div>
								</div>
							{/each}
						</div>
					</div>
				</section>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 4: SAVED RUNS & AUDIT HISTORY                        -->
			<!-- ========================================================= -->
			{#if activePage === 'runs'}
				<section class="space-y-5 md:space-y-6">
					<div class="flex flex-col gap-1.5">
						<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74]">
							<span>AUC IPSO</span>
							<span class="text-[#c6c6cd]">&rsaquo;</span>
							<span>Governance</span>
							<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
							<span class="hidden sm:inline text-[#0b1c30] font-semibold"
								>Saved Runs &amp; Audit History</span
							>
						</div>
						<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
							Saved Matching Runs
						</h1>
					</div>

					<!-- Desktop Table -->
					<div class="hidden md:block bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
						<div class="overflow-x-auto">
							<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-4 px-5">Run ID</th>
										<th class="py-4 px-4">Configuration Notes</th>
										<th class="py-4 px-4">Weight Profile</th>
										<th class="py-4 px-4 text-right">Placed / HK Max</th>
										<th class="py-4 px-4 text-right">1st Choice %</th>
										<th class="py-4 px-4">Status</th>
										<th class="py-4 px-5 text-right">Actions</th>
									</tr>
								</thead>
								<tbody class="divide-y divide-[#c6c6cd]/20">
									{#each savedRuns as run, i (run.id)}
										<tr class="hover:bg-[#eff4ff]/50">
											<td class="py-4 px-5 font-mono-num font-semibold">{run.id}</td>
											<td class="py-4 px-4">
												<span class="font-semibold text-[#0b1c30]">{run.title}</span>
												<span class="block text-xs text-[#515f74] mt-0.5">{run.meta}</span>
											</td>
											<td class="py-4 px-4 font-mono-num text-xs sm:text-sm text-[#515f74]">
												{run.weightsLabel}
											</td>
											<td class="py-4 px-4 text-right font-mono-num font-semibold">
												{run.placed} / {run.hkMax}
											</td>
											<td class="py-4 px-4 text-right font-mono-num font-semibold {run.firstChoicePct >= 65 ? 'text-[#15803d]' : ''}">
												{run.firstChoicePct.toFixed(1)}%
											</td>
											<td class="py-4 px-4">
												<span
													class="px-2.5 py-1 rounded-full text-xs font-semibold {run.status ===
													'Published'
														? 'bg-[#f0fdf4] text-[#15803d]'
														: run.status === 'Draft'
															? 'bg-[#e5eeff] text-[#0b1c30]'
															: 'bg-[#eff4ff] text-[#515f74]'}"
												>
													{run.status}
												</span>
											</td>
											<td class="py-4 px-5 text-right">
												<button
													type="button"
													onclick={() => loadSavedRun(run)}
													class="text-xs sm:text-sm font-semibold text-[#003ea8] hover:underline cursor-pointer"
												>
													{i === 0 ? 'Load in Console' : 'Compare'}
												</button>
											</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>
					</div>

					<!-- Mobile Run Cards -->
					<div class="md:hidden space-y-3 text-xs sm:text-sm">
						{#each savedRuns as run, i (run.id)}
							<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 space-y-2">
								<div class="flex justify-between items-center">
									<span class="font-mono-num font-bold text-[15px]">{run.id}</span>
									<span
										class="px-2.5 py-0.5 rounded-full font-semibold text-xs {run.status ===
										'Published'
											? 'bg-[#f0fdf4] text-[#15803d]'
											: run.status === 'Draft'
												? 'bg-[#e5eeff] text-[#0b1c30]'
												: 'bg-[#eff4ff] text-[#515f74]'}"
									>
										{run.status}
									</span>
								</div>
								<div class="font-semibold text-[15px] text-[#0b1c30]">{run.title}</div>
								<div class="font-mono-num text-xs text-[#515f74]">{run.weightsLabel}</div>
								<div class="flex justify-between items-center text-[#515f74] pt-2.5 border-t border-[#c6c6cd]/20">
									<span>
										Placed: <strong class="text-[#0b1c30]">{run.placed}/{run.hkMax}</strong> · 1st:
										<strong class="text-[#15803d]">{run.firstChoicePct.toFixed(1)}%</strong>
									</span>
									<button
										type="button"
										onclick={() => loadSavedRun(run)}
										class="h-8 px-3.5 rounded-lg bg-[#eff4ff] text-xs font-semibold text-[#003ea8] cursor-pointer"
									>
										{i === 0 ? 'Load' : 'Compare'}
									</button>
								</div>
							</div>
						{/each}
					</div>
				</section>
			{/if}
		</main>
	</div>

	<!-- MOBILE STICKY BOTTOM NAV (< md) -->
	<nav
		class="md:hidden fixed bottom-0 left-0 right-0 h-15 bg-white border-t border-[#c6c6cd]/70 flex items-center justify-around px-2 z-30 shadow-lg"
	>
		{#each MOBILE_TABS as tab (tab.id)}
			{@const active = activePage === tab.id}
			<button
				type="button"
				onclick={() => navigate(tab.id)}
				class="flex flex-col items-center justify-center gap-0.5 px-3 py-1 cursor-pointer {active
					? 'text-[#0b1c30] font-semibold'
					: 'text-[#515f74]'}"
			>
				{#if tab.id === 'applications'}
					<svg class="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width={active ? '2' : '1.6'}>
						<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2" />
						<circle cx="9" cy="7" r="4" />
					</svg>
				{:else if tab.id === 'matching'}
					<svg class="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width={active ? '2' : '1.6'}>
						<circle cx="18" cy="18" r="3" />
						<circle cx="6" cy="6" r="3" />
						<path d="M13 6h3a2 2 0 0 1 2 2v7" />
						<path d="M11 18H8a2 2 0 0 1-2-2V9" />
					</svg>
				{:else if tab.id === 'universities'}
					<svg class="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width={active ? '2' : '1.6'}>
						<path d="M3 21h18" />
						<path d="M5 21V7l7-4 7 4v14" />
					</svg>
				{:else}
					<svg class="w-5 h-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width={active ? '2' : '1.6'}>
						<path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" />
						<path d="M3 3v5h5" />
					</svg>
				{/if}
				<span class="text-[11px]">{tab.shortLabel}</span>
			</button>
		{/each}
	</nav>

	<!-- SLIDE-OVER STUDENT INSPECTION DRAWER -->
	{#if inspectedStudent}
		<div class="fixed inset-0 z-50 flex justify-end">
			<button
				type="button"
				aria-label="Close inspection drawer"
				onclick={() => (inspectedStudentId = null)}
				class="fixed inset-0 bg-[#0b1c30]/35"
			></button>
			<aside class="relative w-full max-w-md bg-white border-l border-[#c6c6cd]/60 shadow-xl z-10 flex flex-col h-full">
				<div class="px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-3">
					<div class="min-w-0">
						<span class="font-mono-num text-xs text-[#515f74] block truncate">
							#{inspectedStudent.id} · {inspectedStudent.email}
						</span>
						<h2 class="text-xl font-semibold text-[#0b1c30] truncate">{inspectedStudent.name}</h2>
					</div>
					<button
						type="button"
						onclick={() => (inspectedStudentId = null)}
						class="h-9 w-9 rounded-lg border border-[#c6c6cd]/40 bg-white text-[#515f74] hover:text-[#0b1c30] shadow-sm flex items-center justify-center cursor-pointer shrink-0 text-lg"
					>
						&times;
					</button>
				</div>

				<div class="p-5 overflow-y-auto space-y-4 text-sm flex-1">
					<div class="grid grid-cols-2 gap-3.5 p-4 bg-[#eff4ff] rounded-xl">
						<div>
							<span class="text-[#515f74] block text-xs">Major &amp; Standing</span>
							<span class="font-semibold text-[#0b1c30]"
								>{inspectedStudent.standing} · {inspectedStudent.major}</span
							>
						</div>
						<div>
							<span class="text-[#515f74] block text-xs">Cumulative GPA</span>
							<span class="font-mono-num font-bold text-[#0b1c30]">
								{inspectedStudent.gpa.toFixed(2)} / 4.00
							</span>
						</div>
						<div>
							<span class="text-[#515f74] block text-xs">Max Budget</span>
							<span class="font-mono-num font-semibold text-[#0b1c30]">
								${inspectedStudent.budget.toLocaleString()}/sem
							</span>
						</div>
						<div>
							<span class="text-[#515f74] block text-xs">Intl Equity Priority</span>
							<span class="font-semibold text-[#0b1c30]">
								{inspectedStudent.prior ? 'Prior Abroad' : 'First-Time (+Bonus)'}
							</span>
						</div>
					</div>

					<div class="space-y-2">
						<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
							Ranked Top 3 Preferences
						</div>
						{#each inspectedStudent.prefs as pref, i (pref)}
							<div class="flex justify-between items-center px-3.5 py-2.5 bg-white border border-[#c6c6cd]/40 rounded-lg shadow-sm">
								<span><strong class="font-mono-num">#{i + 1}</strong> {pref}</span>
								<span class="text-[#15803d] font-semibold text-xs">Eligible</span>
							</div>
						{/each}
					</div>

					<div class="space-y-2">
						<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
							Statement &amp; Cached LLM Assessment ({inspectedStudent.llm.toFixed(1)}/10)
						</div>
						<p class="p-3.5 bg-[#eff4ff] rounded-xl text-[#0b1c30] text-sm leading-relaxed">
							{inspectedStudent.statement}
						</p>
					</div>
				</div>

				<div class="px-5 py-4 border-t border-[#c6c6cd]/30 bg-[#eff4ff] flex justify-end gap-2.5">
					<button
						type="button"
						onclick={() => (inspectedStudentId = null)}
						class="h-10 px-4 bg-white border border-[#c6c6cd]/40 rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer"
					>
						Close
					</button>
					<button
						type="button"
						onclick={() => markStudentVerified(inspectedStudent.id)}
						class="h-10 px-4 bg-[#131b2e] text-white rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer"
					>
						Mark Verified
					</button>
				</div>
			</aside>
		</div>
	{/if}

	<!-- VISUAL-FIRST BIPARTITE GRAPH MATCHING SOLVER ANIMATION -->
	<BipartiteMatchModal
		open={animOpen}
		allowUnranked={weights.allowUnranked}
		{darkMode}
		onDone={handleAnimationDone}
	/>

	<!-- NON-INTRUSIVE TOAST -->
	{#if toastMessage}
		<div
			class="fixed bottom-18 md:bottom-5 left-4 right-4 md:left-auto md:right-5 bg-[#131b2e] text-white px-4 py-3 rounded-xl shadow-md text-xs sm:text-sm font-medium z-50 text-center md:text-left"
		>
			{toastMessage}
		</div>
	{/if}
</div>
