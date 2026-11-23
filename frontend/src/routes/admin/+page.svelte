<script lang="ts">
	import { onDestroy, onMount } from 'svelte';
	import { auth } from '#lib/auth/store.svelte.ts';
	import { theme } from '#lib/theme.svelte.ts';
	import { adminApi, AdminApiError } from '#lib/admin/api.ts';
	import {
		filterAndSortPlacements,
		filterAndSortUniversityFills,
		filterAndSortUnplaced,
		filterShortlists,
		formatShortSeatLabel,
		groupPlacementsByPartner,
		type FillSortKey,
		type FillStatusFilter,
		type PlacementSortKey,
		type ShortlistQuotaFilter
	} from '#lib/admin/matchingFilters.ts';
	import {
		DEFAULT_WEIGHT_CONFIG,
		type AdminApplicationDetail,
		type AdminApplicationSummary,
		type AdminOverviewStats,
		type AdminPartnerDetail,
		type AdminPartnerStats,
		type AdminPartnerSummary,
		type AdminPlacementRow,
		type AdminRunDiff,
		type AdminSavedRunSummary,
		type AdminSimulationResult,
		type AllocationStrategy,
		type AuditStatus,
		type WeightConfig
	} from '#lib/admin/types.ts';
	import BipartiteMatchModal from '#lib/components/BipartiteMatchModal.svelte';
	import PartnerDossierDrawer from '#lib/components/admin/PartnerDossierDrawer.svelte';
	import StudentAuditDrawer from '#lib/components/admin/StudentAuditDrawer.svelte';
	import RunDiffModal from '#lib/components/admin/RunDiffModal.svelte';

	type AdminPageId = 'applications' | 'matching' | 'universities' | 'runs';
	type StudentCohortTab = 'all' | 'eligible' | 'flagged';
	type MatchResultTab = 'matched' | 'unplaced' | 'unis' | 'shortlists';

	const ACADEMIC_CYCLES = [
		'2026–27 · Fall Term',
		'2026–27 · Spring Term',
		'2025–26 · Spring Term',
		'2025–26 · Fall Term'
	] as const;

	const NAV_GROUPS = [
		{
			section: 'Exchange Operations',
			items: [
				{ id: 'applications' as AdminPageId, label: 'Student Applications', shortLabel: 'Apps' },
				{ id: 'matching' as AdminPageId, label: 'Matching Console', shortLabel: 'Matching' },
				{ id: 'universities' as AdminPageId, label: 'Partner Universities', shortLabel: 'Partners' }
			]
		},
		{
			section: 'Governance & Controls',
			items: [{ id: 'runs' as AdminPageId, label: 'Saved Runs & Diffs', shortLabel: 'Runs' }]
		}
	];
	const MOBILE_TABS = NAV_GROUPS.flatMap((g) => g.items);

	// Shell & global state
	let selectedCycle = $state<string>(ACADEMIC_CYCLES[0]);
	let activePage = $state<AdminPageId>('applications');
	let sidebarCollapsed = $state(false);
	const darkMode = $derived(theme.dark);
	let filterPanelOpen = $state(true);
	let mobileWeightsOpen = $state(true);

	// Backend data state
	let loadingInitial = $state(true);
	let backendError = $state<string | null>(null);
	let demoBusy = $state(false);
	let demoMenuOpen = $state(false);
	let overview = $state<AdminOverviewStats>({
		totalApplicants: 0,
		eligibleCount: 0,
		firstTimeAbroadCount: 0,
		flaggedCount: 0,
		lowGpaFlaggedCount: 0,
		reviewQueueWaiting: 0,
		demoApplicantCount: 0,
		totalPartners: 0,
		totalConfiguredSeats: 0,
		customQuotaCount: 0,
		hkMaxFeasible: 0
	});

	// View 1: Applications state
	let applications = $state<AdminApplicationSummary[]>([]);
	let studentTab = $state<StudentCohortTab>('all');
	let searchQuery = $state('');
	let standingFilter = $state('all');
	let gpaMin = $state(0);
	let gpaMax = $state(4);
	let uniFilter = $state('all');
	let priorFilter = $state('all');
	let majorFilter = $state('');
	let statusFilter = $state('all');
	let rowsPerPage = $state(25);
	let appPage = $state(1);
	let selectedMap = $state<Record<string, boolean>>({});

	// View 1 Audit Drawer
	let inspectedStudent = $state<AdminApplicationDetail | null>(null);
	let loadingStudent = $state(false);
	let savingReview = $state(false);

	// View 3: Partner Universities state
	let partners = $state<AdminPartnerSummary[]>([]);
	let partnerStats = $state<AdminPartnerStats>({
		totalPartners: 0,
		activePartners: 0,
		checkSlotsPartners: 0,
		loiPartners: 0,
		totalConfiguredSeats: 0,
		customQuotaCount: 0,
		highDemandCount: 0,
		factSheetMatchedCount: 0,
		knownMinGpaCount: 0
	});
	let partnerSearch = $state('');
	let partnerRegion = $state('all');
	let partnerStatus = $state('all');
	let partnerTerm = $state('all');
	let partnerLevel = $state('all');
	let partnerQuotaState = $state('all');
	let partnerRowsPerPage = $state(25);
	let partnerPage = $state(1);
	let savingSeatSlug = $state<string | null>(null);
	let inspectedPartner = $state<AdminPartnerDetail | null>(null);
	let loadingPartner = $state(false);

	// View 2: Matching Console state
	let weights = $state<WeightConfig>({ ...DEFAULT_WEIGHT_CONFIG });
	let matchTab = $state<MatchResultTab>('matched');
	let matchedLayout = $state<'grouped' | 'roster'>('grouped');
	let mobileMatchFiltersOpen = $state(false);
	let animOpen = $state(false);
	let runningSim = $state(false);
	let savingRun = $state(false);
	let publishingRun = $state(false);
	let simulation = $state<AdminSimulationResult | null>(null);
	let loadedRunSummary = $state<AdminSavedRunSummary | null>(null);
	let compareRunId = $state(0);
	let shortlistLimit = $state(10);
	let runTitleDraft = $state('');
	let activeRunLabel = $state('Unsaved Configuration');
	let matchPage = $state(1);
	const MATCH_PAGE_SIZE = 25;
	const MATCH_GROUP_PAGE_SIZE = 12;

	// Matching results filtration state (matched / unplaced / fill rates / shortlists)
	let matchSearch = $state('');
	let matchRankFilter = $state<'all' | '1' | '2' | '3' | '0'>('all');
	let matchPartnerSlug = $state('all');
	let matchMajorFilter = $state('all');
	let matchMinGpa = $state(0);
	let matchSortBy = $state<PlacementSortKey>('score_desc');

	let unplacedSearch = $state('');
	let unplacedBottleneck = $state('all');
	let unplacedMajor = $state('all');
	let unplacedSortBy = $state<'gpa_desc' | 'name_asc'>('gpa_desc');

	let fillSearch = $state('');
	let fillRegion = $state('all');
	let fillStatusFilter = $state<FillStatusFilter>('all');
	let fillSortBy = $state<FillSortKey>('demand_desc');

	let shortlistSearch = $state('');
	let shortlistPartnerSlug = $state('all');
	let shortlistMajor = $state('all');
	let shortlistQuotaFilter = $state<ShortlistQuotaFilter>('all');
	let shortlistMinGpa = $state(0);
	let shortlistHideEmpty = $state(true);

	// View 4: Saved Runs & Diff Modal state
	interface AdminReturnPoint {
		label: string;
		kind: 'modal' | 'page';
		page: AdminPageId;
		matchTab?: MatchResultTab;
		matchedLayout?: 'grouped' | 'roster';
		matchPartnerSlug?: string;
		matchPage?: number;
		studentEmail?: string | null;
		partnerSlug?: string | null;
		diffPair?: { a: number; b: number } | null;
	}

	let savedRuns = $state<AdminSavedRunSummary[]>([]);
	let diffModalOpen = $state(false);
	let diffRunA = $state(0);
	let diffRunB = $state(0);
	let diffData = $state<AdminRunDiff | null>(null);
	let loadingDiff = $state(false);
	let exportingRunId = $state<number | null>(null);
	let confirmDeleteRunId = $state<number | null>(null);
	let deletingRunId = $state<number | null>(null);
	let returnStack = $state<AdminReturnPoint[]>([]);

	// Toast
	let toastMessage = $state<string | null>(null);
	let toastTimer: ReturnType<typeof setTimeout> | null = null;

	function showToast(msg: string) {
		toastMessage = msg;
		if (toastTimer) clearTimeout(toastTimer);
		toastTimer = setTimeout(() => {
			toastMessage = null;
		}, 3200);
	}

	function msgOf(e: unknown): string {
		return e instanceof Error ? e.message : 'Request failed.';
	}

	onDestroy(() => {
		if (toastTimer) clearTimeout(toastTimer);
	});

	onMount(async () => {
		theme.init();
		await refreshAllData();
	});

	async function refreshAllData() {
		loadingInitial = true;
		backendError = null;
		try {
			const [ov, appRes, uniRes, runRes] = await Promise.all([
				adminApi.getOverview(selectedCycle),
				adminApi.getApplications(),
				adminApi.getUniversities(),
				adminApi.getRuns()
			]);
			overview = ov;
			applications = appRes.applications;
			partners = uniRes.universities;
			partnerStats = uniRes.stats;
			savedRuns = runRes.runs;
			if (savedRuns.length > 0 && compareRunId === 0) {
				compareRunId = savedRuns[0].id;
			}
		} catch (e) {
			backendError = msgOf(e);
		} finally {
			loadingInitial = false;
		}
	}

	async function handleCycleChange() {
		showToast(`Switched active cycle to ${selectedCycle}.`);
		try {
			overview = await adminApi.getOverview(selectedCycle);
			if (simulation) {
				await executeSimulation(false);
			}
		} catch (e) {
			showToast(msgOf(e));
		}
	}

	function toggleDarkMode() {
		const next = theme.toggle();
		showToast(next ? 'Switched to Dark Mode.' : 'Switched to Light Mode.');
	}

	const modalReturnPoint = $derived(
		returnStack.length > 0 && returnStack[returnStack.length - 1].kind === 'modal'
			? returnStack[returnStack.length - 1]
			: null
	);
	const pageReturnPoint = $derived.by(() => {
		for (let i = returnStack.length - 1; i >= 0; i--) {
			if (returnStack[i].kind === 'page') return returnStack[i];
		}
		return null;
	});

	function pushReturnPoint(entry: AdminReturnPoint) {
		const top = returnStack[returnStack.length - 1];
		if (
			top &&
			top.kind === entry.kind &&
			top.page === entry.page &&
			top.label === entry.label &&
			top.studentEmail === entry.studentEmail &&
			top.partnerSlug === entry.partnerSlug
		) {
			return;
		}
		returnStack = [...returnStack.slice(-11), entry];
	}

	function clearModalReturns() {
		if (returnStack.length === 0) return;
		let end = returnStack.length;
		while (end > 0 && returnStack[end - 1].kind === 'modal') {
			end--;
		}
		if (end !== returnStack.length) {
			returnStack = returnStack.slice(0, end);
		}
	}

	async function goBackOneStep() {
		if (returnStack.length === 0) return;
		const entry = returnStack[returnStack.length - 1];
		returnStack = returnStack.slice(0, -1);

		if (entry.studentEmail) {
			inspectedPartner = null;
			loadingPartner = false;
			activePage = entry.page;
			await inspectStudent(entry.studentEmail);
			return;
		}
		if (entry.partnerSlug) {
			inspectedStudent = null;
			loadingStudent = false;
			activePage = entry.page;
			if (entry.matchTab) matchTab = entry.matchTab;
			if (entry.matchedLayout) matchedLayout = entry.matchedLayout;
			if (entry.matchPartnerSlug !== undefined) matchPartnerSlug = entry.matchPartnerSlug;
			if (entry.matchPage !== undefined) matchPage = entry.matchPage;
			await inspectPartnerDossier(entry.partnerSlug);
			return;
		}
		if (entry.diffPair) {
			inspectedStudent = null;
			inspectedPartner = null;
			activePage = entry.page;
			await openDiffComparison(entry.diffPair.a, entry.diffPair.b);
			return;
		}

		inspectedStudent = null;
		inspectedPartner = null;
		activePage = entry.page;
		if (entry.matchTab) matchTab = entry.matchTab;
		if (entry.matchedLayout) matchedLayout = entry.matchedLayout;
		if (entry.matchPartnerSlug !== undefined) matchPartnerSlug = entry.matchPartnerSlug;
		if (entry.matchPage !== undefined) matchPage = entry.matchPage;
	}

	async function navigate(page: AdminPageId, clearHistory = false) {
		demoMenuOpen = false;
		confirmDeleteRunId = null;
		if (clearHistory) {
			returnStack = [];
		}
		activePage = page;
		if (page === 'matching' && !simulation && matchableApplicantsCount > 0 && !runningSim) {
			await executeSimulation(false);
		}
	}

	function openMatchingFromApplications() {
		pushReturnPoint({ label: 'Student Applications', kind: 'page', page: 'applications' });
		navigate('matching');
	}

	function openRunsFromMatching() {
		pushReturnPoint({
			label: 'Matching Console',
			kind: 'page',
			page: 'matching',
			matchTab,
			matchedLayout,
			matchPartnerSlug,
			matchPage
		});
		navigate('runs');
	}

	const matchableApplicantsCount = $derived(
		applications.filter((a) => a.status !== 'Draft').length
	);
	const realApplicantCount = $derived(
		Math.max(0, overview.totalApplicants - overview.demoApplicantCount)
	);

	// ---- Derived filtered applications (C++-matching semantics on fetched cohort + live responsive paging) ----
	const filteredStudents = $derived.by(() => {
		const q = searchQuery.trim().toLowerCase();
		const maj = majorFilter.trim().toLowerCase();
		const minG = Number.isFinite(gpaMin) ? gpaMin : 0;
		const maxG = Number.isFinite(gpaMax) ? gpaMax : 4;

		return applications.filter((s) => {
			if (studentTab === 'eligible' && (s.status === 'Ineligible' || s.status === 'Draft' || s.gpa < 2.8))
				return false;
			if (studentTab === 'flagged' && !s.flagged) return false;
			if (standingFilter !== 'all' && s.standing !== standingFilter) return false;
			if (s.gpa < minG || s.gpa > maxG) return false;
			if (uniFilter !== 'all' && !s.prefSlugs.includes(uniFilter)) return false;
			if (priorFilter === 'first' && s.prior) return false;
			if (priorFilter === 'prior' && !s.prior) return false;
			if (maj && !s.major.toLowerCase().includes(maj)) return false;
			if (statusFilter !== 'all' && s.status !== statusFilter) return false;
			if (
				q &&
				!(
					s.name.toLowerCase().includes(q) ||
					s.email.toLowerCase().includes(q) ||
					s.aucId.toLowerCase().includes(q) ||
					s.major.toLowerCase().includes(q) ||
					s.prefNames.join(' ').toLowerCase().includes(q)
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

	const appTotalPages = $derived(Math.max(1, Math.ceil(filteredStudents.length / rowsPerPage)));
	const clampedAppPage = $derived(Math.min(appPage, appTotalPages));
	const pagedStudents = $derived(
		filteredStudents.slice((clampedAppPage - 1) * rowsPerPage, clampedAppPage * rowsPerPage)
	);

	// University filter options sorted by demand
	const universityFilterOptions = $derived.by(() => {
		return [...partners].sort((a, b) => b.totalDemand - a.totalDemand || a.officialName.localeCompare(b.officialName));
	});

	const pageAllSelected = $derived(
		pagedStudents.length > 0 && pagedStudents.every((s) => !!selectedMap[s.email])
	);

	function togglePageStudents(checked: boolean) {
		const next = { ...selectedMap };
		for (const s of pagedStudents) {
			next[s.email] = checked;
		}
		selectedMap = next;
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
		appPage = 1;
	}

	// ---- Demo cohort controls (only shown when auth.demoEnabled) ----
	async function handleSeedDemo() {
		demoMenuOpen = false;
		demoBusy = true;
		try {
			const res = await adminApi.seedDemoCohort();
			await refreshAllData();
			simulation = null;
			loadedRunSummary = null;
			inspectedStudent = null;
			loadingStudent = false;
			selectedMap = {};
			activeRunLabel = 'Unsaved Configuration';
			resetAppFilters();
			resetMatchResultsFilters();
			showToast(
				`Loaded ${res.seeded} realistic sample applicants across ${partners.length} partner universities.`
			);
			if (activePage === 'matching') {
				await executeSimulation(false);
			}
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			demoBusy = false;
		}
	}

	async function handleClearDemo() {
		demoMenuOpen = false;
		demoBusy = true;
		try {
			const res = await adminApi.clearDemoCohort();
			await refreshAllData();
			simulation = null;
			loadedRunSummary = null;
			inspectedStudent = null;
			loadingStudent = false;
			selectedMap = {};
			activeRunLabel = 'Unsaved Configuration';
			resetAppFilters();
			resetMatchResultsFilters();
			const remainingReal = Math.max(0, overview.totalApplicants - overview.demoApplicantCount);
			showToast(
				`Cleared ${res.removed} demo applicants (${remainingReal} real preserved). Reinstate anytime via Demo controls.`
			);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			demoBusy = false;
		}
	}

	// ---- Student Audit Drawer ----
	async function inspectStudent(email: string) {
		loadingStudent = true;
		inspectedStudent = null;
		try {
			inspectedStudent = await adminApi.getApplicationDetail(email);
		} catch (e) {
			if (
				e instanceof AdminApiError &&
				e.status === 404 &&
				auth.demoEnabled &&
				overview.demoApplicantCount === 0
			) {
				showToast(
					'Applicant record was from cleared demo data. Reinstate the 500 demo cohort from the Demo menu to inspect details.'
				);
			} else {
				showToast(msgOf(e));
			}
		} finally {
			loadingStudent = false;
		}
	}

	async function handleSaveReview(
		email: string,
		payload: {
			status?: AuditStatus;
			goodStanding?: boolean | null;
			statementRating?: number | null;
			notes?: string;
		}
	) {
		savingReview = true;
		try {
			const updated = await adminApi.updateReview(email, payload);
			inspectedStudent = updated;
			const [ov, appRes] = await Promise.all([
				adminApi.getOverview(selectedCycle),
				adminApi.getApplications()
			]);
			overview = ov;
			applications = appRes.applications;
			showToast(`Saved audit review for ${updated.name} (${updated.status}).`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			savingReview = false;
		}
	}

	// ---- View 3: Partner Universities filtering & seat quotas ----
	const partnerRegions = $derived.by(() => {
		const set = new Set<string>();
		for (const p of partners) {
			if (p.region) set.add(p.region);
		}
		return Array.from(set).sort();
	});

	const filteredPartners = $derived.by(() => {
		const q = partnerSearch.trim().toLowerCase();
		return partners.filter((p) => {
			if (
				q &&
				!(
					p.officialName.toLowerCase().includes(q) ||
					p.shortName.toLowerCase().includes(q) ||
					p.city.toLowerCase().includes(q) ||
					p.country.toLowerCase().includes(q) ||
					p.slug.toLowerCase().includes(q)
				)
			) {
				return false;
			}
			if (partnerRegion !== 'all' && p.region !== partnerRegion) return false;
			if (partnerStatus !== 'all' && p.agreementStatus !== partnerStatus) return false;
			if (partnerLevel !== 'all' && !p.studentLevels.includes(partnerLevel)) return false;
			if (partnerTerm !== 'all') {
				const hasTerm =
					p.availableTerms.includes(partnerTerm) ||
					((partnerTerm === 'Fall' || partnerTerm === 'Spring') &&
						p.availableTerms.includes('Academic Year'));
				if (!hasTerm) return false;
			}
			if (partnerQuotaState === 'custom' && p.seatsAssumed) return false;
			if (partnerQuotaState === 'default' && !p.seatsAssumed) return false;
			if (partnerQuotaState === 'demanded' && p.totalDemand === 0) return false;
			return true;
		});
	});

	const partnerTotalPages = $derived(
		Math.max(1, Math.ceil(filteredPartners.length / partnerRowsPerPage))
	);
	const clampedPartnerPage = $derived(Math.min(partnerPage, partnerTotalPages));
	const pagedPartners = $derived(
		filteredPartners.slice(
			(clampedPartnerPage - 1) * partnerRowsPerPage,
			clampedPartnerPage * partnerRowsPerPage
		)
	);

	function resetPartnerFilters() {
		partnerSearch = '';
		partnerRegion = 'all';
		partnerStatus = 'all';
		partnerTerm = 'all';
		partnerLevel = 'all';
		partnerQuotaState = 'all';
		partnerPage = 1;
	}

	async function inspectPartnerDossier(slug: string) {
		loadingPartner = true;
		inspectedPartner = null;
		try {
			inspectedPartner = await adminApi.getUniversityDetail(slug);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			loadingPartner = false;
		}
	}

	async function updatePartnerSeatQuota(slug: string, nextSeats: number) {
		const clamped = Math.max(0, Math.min(100, Math.round(nextSeats)));
		savingSeatSlug = slug;
		try {
			const updated = await adminApi.setUniversitySeats(slug, clamped);
			partners = partners.map((p) => (p.slug === slug ? updated : p));
			if (inspectedPartner && inspectedPartner.slug === slug) {
				inspectedPartner = { ...inspectedPartner, seats: updated.seats, seatsAssumed: updated.seatsAssumed };
			}
			const [uniRes, ov] = await Promise.all([
				adminApi.getUniversities(),
				adminApi.getOverview(selectedCycle)
			]);
			partnerStats = uniRes.stats;
			overview = ov;
			showToast(`Seat quota for ${updated.officialName} saved as ${updated.seats}.`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			savingSeatSlug = null;
		}
	}

	// ---- View 2: Bipartite Matching Console ----
	let pendingSimPromise: Promise<AdminSimulationResult | null> | null = null;

	function strategyLabel(mode: AllocationStrategy): string {
		if (mode === 'best_fit') return 'Cardinality + Best Fit (Hungarian)';
		if (mode === 'highest_score') return 'Pure Max Score (Hungarian)';
		if (mode === 'most_placed') return 'Max Placed Only (Hopcroft–Karp)';
		return 'Ranked Shortlists (Heap)';
	}

	function resetMatchResultsFilters(tab?: MatchResultTab) {
		if (!tab || tab === 'matched') {
			matchSearch = '';
			matchRankFilter = 'all';
			matchPartnerSlug = 'all';
			matchMajorFilter = 'all';
			matchMinGpa = 0;
			matchSortBy = 'score_desc';
		}
		if (!tab || tab === 'unplaced') {
			unplacedSearch = '';
			unplacedBottleneck = 'all';
			unplacedMajor = 'all';
			unplacedSortBy = 'gpa_desc';
		}
		if (!tab || tab === 'unis') {
			fillSearch = '';
			fillRegion = 'all';
			fillStatusFilter = 'all';
			fillSortBy = 'demand_desc';
		}
		if (!tab || tab === 'shortlists') {
			shortlistSearch = '';
			shortlistPartnerSlug = 'all';
			shortlistMajor = 'all';
			shortlistQuotaFilter = 'all';
			shortlistMinGpa = 0;
			shortlistHideEmpty = true;
		}
		matchPage = 1;
	}

	function jumpToPartnerMatched(slug: string, recordReturn = false) {
		if (!simulation || simulation.mode === 'shortlists') {
			inspectPartnerDossier(slug);
			return;
		}
		if (recordReturn) {
			pushReturnPoint({
				label: 'Fill Rates',
				kind: 'page',
				page: 'matching',
				matchTab: 'unis',
				matchedLayout,
				matchPartnerSlug,
				matchPage
			});
		}
		resetMatchResultsFilters('matched');
		matchPartnerSlug = slug;
		matchedLayout = 'grouped';
		matchTab = 'matched';
		matchPage = 1;
		activePage = 'matching';
		const partnerName =
			partners.find((p) => p.slug === slug)?.officialName ??
			simulation.universityFills.find((u) => u.slug === slug)?.name ??
			slug;
		showToast(`Filtered Matched Allocations to ${partnerName}.`);
	}

	async function executeSimulation(withAnimation: boolean): Promise<AdminSimulationResult | null> {
		if (overview.totalApplicants === 0) {
			showToast('No applicants in SQLite yet. Load demo applicants or await student submissions.');
			return null;
		}
		if (matchableApplicantsCount === 0) {
			showToast(
				'No submitted applicants are ready for matching (only Draft records exist). Reinstate demo data or verify a submission.'
			);
			return null;
		}

		loadedRunSummary = null;
		runningSim = true;
		if (withAnimation && weights.mode !== 'shortlists') {
			animOpen = true;
		}

		const req = {
			cycle: selectedCycle,
			weights: { ...weights },
			shortlistLimit,
			compareRunId
		};

		const promise = adminApi
			.simulate(req)
			.then((res) => {
				simulation = res;
				matchPage = 1;
				if (res.mode === 'shortlists') {
					matchTab = 'shortlists';
				} else if (matchTab === 'shortlists') {
					matchTab = 'matched';
				}
				activeRunLabel = `Simulated · ${strategyLabel(res.mode)} (${res.solveTimeMs.toFixed(1)} ms)`;
				return res;
			})
			.catch((e) => {
				showToast(msgOf(e));
				return null;
			})
			.finally(() => {
				runningSim = false;
			});

		pendingSimPromise = promise;
		if (!animOpen) {
			const res = await promise;
			if (res && withAnimation) {
				if (res.mode === 'shortlists') {
					showToast(
						`Generated ranked Heap shortlists for ${res.shortlists.length} partners in ${res.solveTimeMs.toFixed(1)} ms.`
					);
				}
			}
			return res;
		}
		return null;
	}

	async function handleAnimationDone() {
		animOpen = false;
		const res = pendingSimPromise ? await pendingSimPromise : simulation;
		if (res) {
			showToast(
				`Matching complete: ${res.placed}/${res.eligibleCount} placed in ${res.solveTimeMs.toFixed(1)} ms.`
			);
		}
	}

	function resetWeights() {
		weights = { ...DEFAULT_WEIGHT_CONFIG };
		loadedRunSummary = null;
		activeRunLabel = 'Default Weights · Click Re-run Simulation';
		showToast('Weights reset to default WeightConfig.');
	}

	async function saveDraftRun() {
		if (weights.mode === 'shortlists') {
			showToast('Shortlists is an inspection mode. Switch to an allocation mode to save a run.');
			return;
		}
		savingRun = true;
		try {
			const title =
				runTitleDraft.trim() ||
				`${strategyLabel(weights.mode)} · λ=${weights.pref.toFixed(2)}`;
			const created = await adminApi.saveRun({
				title,
				cycle: selectedCycle,
				weights: { ...weights }
			});
			savedRuns = [created, ...savedRuns];
			loadedRunSummary = created;
			compareRunId = created.id;
			runTitleDraft = created.title;
			activeRunLabel = `${created.runCode} · Saved (Draft)`;
			if (simulation && simulation.mode === created.mode) {
				simulation = {
					...simulation,
					placements: created.placements,
					unplaced: created.unplaced,
					universityFills: created.universityFills ?? simulation.universityFills
				};
			}
			showToast(`${created.runCode} saved to SQLite Run History.`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			savingRun = false;
		}
	}

	async function publishCurrentOrSelectedRun(targetRunId?: number) {
		publishingRun = true;
		try {
			let runId = targetRunId;
			if (!runId) {
				if (weights.mode === 'shortlists') {
					showToast('Switch to an allocation strategy before publishing nominations.');
					return;
				}
				const title =
					runTitleDraft.trim() ||
					`${strategyLabel(weights.mode)} · Final Publish`;
				const created = await adminApi.saveRun({
					title,
					cycle: selectedCycle,
					weights: { ...weights }
				});
				runId = created.id;
			}
			const published = await adminApi.publishRun(runId);
			const runRes = await adminApi.getRuns();
			savedRuns = runRes.runs;
			if (loadedRunSummary && loadedRunSummary.id === published.id) {
				loadedRunSummary = published;
			}
			activeRunLabel = `${published.runCode} · Published`;
			showToast(`${published.runCode} published! Matched students notified.`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			publishingRun = false;
		}
	}

	async function loadSavedRun(run: AdminSavedRunSummary, returnEntry?: AdminReturnPoint) {
		try {
			const detail = await adminApi.getRunDetail(run.id);
			if (returnEntry) {
				pushReturnPoint(returnEntry);
			}
			weights = { ...detail.weights };
			loadedRunSummary = detail;
			runTitleDraft = detail.title;
			simulation = {
				mode: detail.mode,
				cycle: detail.cycle,
				allowUnranked: detail.allowUnranked,
				weights: detail.weights,
				placed: detail.placed,
				eligibleCount: detail.eligibleCount,
				unplacedCount: detail.unplaced.length,
				hkMax: detail.hkMax,
				firstChoiceCount: detail.firstChoiceCount,
				secondChoiceCount: detail.secondChoiceCount,
				thirdChoiceCount: detail.thirdChoiceCount,
				unrankedCount: detail.unrankedCount,
				firstChoicePct: detail.firstChoicePct,
				avgWeight: detail.avgScore,
				solveTimeMs: 0,
				placements: detail.placements,
				unplaced: detail.unplaced,
				universityFills: detail.universityFills ?? [],
				shortlists: [],
				diff: {
					hasBaseline: false,
					baselineRunId: 0,
					baselineRunCode: '',
					baselineTitle: '',
					placedDelta: 0,
					firstChoiceDelta: 0,
					gainedCount: 0,
					lostCount: 0,
					shiftedCount: 0,
					changes: []
				}
			};
			selectedCycle = detail.cycle;
			activeRunLabel = `${detail.runCode} · Loaded (${detail.status})`;
			resetMatchResultsFilters();
			matchTab = 'matched';
			matchPage = 1;
			activePage = 'matching';
			showToast(`Loaded ${detail.runCode} (${detail.title}) into Matching Console.`);
		} catch (e) {
			showToast(msgOf(e));
		}
	}

	async function handleDeleteRun(run: AdminSavedRunSummary) {
		deletingRunId = run.id;
		try {
			await adminApi.deleteRun(run.id);
			savedRuns = savedRuns.filter((r) => r.id !== run.id);
			if (loadedRunSummary?.id === run.id) {
				loadedRunSummary = null;
				simulation = null;
				activeRunLabel = 'Unsaved Configuration';
			}
			if (compareRunId === run.id) {
				compareRunId = savedRuns[0]?.id ?? 0;
			}
			if (diffModalOpen && (diffRunA === run.id || diffRunB === run.id)) {
				diffModalOpen = false;
			}
			confirmDeleteRunId = null;
			showToast(`Deleted ${run.runCode} (${run.title}).`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			deletingRunId = null;
		}
	}

	async function handleSelectLoadedRun(runId: number) {
		if (runId === 0) {
			loadedRunSummary = null;
			runTitleDraft = '';
			if (overview.totalApplicants > 0) {
				await executeSimulation(false);
			} else {
				simulation = null;
				activeRunLabel = 'Unsaved Configuration';
			}
			return;
		}
		const target = savedRuns.find((r) => r.id === runId);
		if (target) {
			await loadSavedRun(target);
		}
	}

	async function openDiffComparison(runA: number, runB?: number) {
		if (savedRuns.length < 2) {
			showToast('Save at least two runs to compare assignments side by side.');
			return;
		}
		const fallbackB = savedRuns.find((r) => r.id !== runA)?.id ?? runA;
		const b = runB && runB !== runA ? runB : fallbackB;
		diffRunA = runA;
		diffRunB = b;
		diffModalOpen = true;
		await fetchDiff(runA, b);
	}

	async function fetchDiff(runA: number, runB: number) {
		diffRunA = runA;
		diffRunB = runB;
		loadingDiff = true;
		try {
			diffData = await adminApi.diffRuns(runA, runB);
		} catch (e) {
			showToast(msgOf(e));
			diffData = null;
		} finally {
			loadingDiff = false;
		}
	}

	// ---- CSV Exports ----
	function csvCell(val: unknown): string {
		const s = String(val ?? '');
		if (/[",\n\r]/.test(s)) return `"${s.replace(/"/g, '""')}"`;
		return s;
	}

	function downloadCsv(filename: string, headers: string[], rows: unknown[][]) {
		const lines = [headers.map(csvCell).join(','), ...rows.map((r) => r.map(csvCell).join(','))];
		const blob = new Blob([lines.join('\n')], { type: 'text/csv;charset=utf-8;' });
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = filename;
		document.body.appendChild(a);
		a.click();
		document.body.removeChild(a);
		URL.revokeObjectURL(url);
	}

	function exportApplicationsCsv() {
		downloadCsv(
			`auc_exchange_applicants_${filteredStudents.length}.csv`,
			['AUC ID', 'Name', 'Email', 'Level', 'Standing', 'Major', 'GPA', 'Choice 1', 'Choice 2', 'Choice 3', 'Statement Rating', 'Prior Abroad', 'Status'],
			filteredStudents.map((s) => [
				s.aucId,
				s.name,
				s.email,
				s.level,
				s.standing,
				s.major,
				s.gpa.toFixed(2),
				s.prefNames[0],
				s.prefNames[1],
				s.prefNames[2],
				s.hasStatementRating && s.statementRating !== null ? s.statementRating.toFixed(1) : 'Unrated',
				s.prior ? 'Prior Abroad' : 'First-Time',
				s.status
			])
		);
		showToast(`Exported ${filteredStudents.length} filtered applicants to CSV.`);
	}

	function exportPartnersCsv() {
		downloadCsv(
			`auc_partner_universities_${filteredPartners.length}.csv`,
			['Slug', 'University', 'Short', 'Country', 'City', 'Region', 'Status', 'Levels', 'Host Min GPA', 'Seats', 'Seat Source', '1st Choice Demand', 'Total Demand', 'Terms'],
			filteredPartners.map((p) => [
				p.slug,
				p.officialName,
				p.shortName,
				p.country,
				p.city,
				p.region,
				p.agreementStatus,
				p.studentLevels.join('/'),
				p.minGpa !== null ? p.minGpa.toFixed(2) : 'AUC 2.80*',
				p.seats,
				p.seatsAssumed ? 'Default (2)' : 'Confirmed',
				p.firstChoiceDemand,
				p.totalDemand,
				p.availableTerms.join('/')
			])
		);
		showToast(`Exported ${filteredPartners.length} partner universities to CSV.`);
	}

	function exportActiveSimulationCsv() {
		if (!simulation) return;
		const runTag = (loadedRunSummary?.runCode ?? `live_${simulation.mode}`).toLowerCase();

		if (simulation.mode === 'shortlists' || matchTab === 'shortlists') {
			const rows: unknown[][] = [];
			for (const sl of filteredShortlists) {
				for (const c of sl.candidates) {
					rows.push([
						sl.name,
						sl.slug,
						sl.country,
						sl.totalSeats,
						c.rank,
						c.rank <= sl.totalSeats ? 'In Quota' : 'Waitlist',
						c.studentName,
						c.studentAucId,
						c.studentEmail,
						c.studentMajor,
						c.studentGpa.toFixed(2),
						c.score.toFixed(4)
					]);
				}
			}
			downloadCsv(
				`auc_matching_shortlists_${rows.length}.csv`,
				['Partner', 'Slug', 'Country', 'Seats', 'Rank', 'Tier', 'Student', 'AUC ID', 'Email', 'Major', 'GPA', 'Fit Score'],
				rows
			);
			showToast(`Exported ${rows.length} shortlist entries to CSV.`);
			return;
		}

		if (matchTab === 'unplaced') {
			downloadCsv(
				`auc_${runTag}_unplaced_${filteredUnplaced.length}.csv`,
				['Student Name', 'AUC ID', 'Email', 'Major', 'GPA', 'Preferences', 'Bottleneck', 'Reason'],
				filteredUnplaced.map((u) => [
					u.studentName,
					u.studentAucId,
					u.studentEmail,
					u.studentMajor,
					u.gpa.toFixed(2),
					u.prefsLabel,
					u.bottleneck,
					u.reason
				])
			);
			showToast(`Exported ${filteredUnplaced.length} unplaced records to CSV.`);
			return;
		}

		if (matchTab === 'unis') {
			downloadCsv(
				`auc_${runTag}_fill_rates_${filteredUniversityFills.length}.csv`,
				['University', 'Slug', 'Country', 'Region', 'Filled Seats', 'Total Seats', 'Open Seats', 'Total Demand', 'Demand Ratio', 'Cutoff Score'],
				filteredUniversityFills.map((u) => [
					u.name,
					u.slug,
					u.country,
					u.region,
					u.filledSeats,
					u.totalSeats,
					u.leftSeats,
					u.demandCount,
					u.demandRatio.toFixed(2),
					u.cutoffScore !== null ? u.cutoffScore.toFixed(4) : ''
				])
			);
			showToast(`Exported ${filteredUniversityFills.length} university fill rates to CSV.`);
			return;
		}

		downloadCsv(
			`auc_${runTag}_matched_${filteredPlacements.length}.csv`,
			['Run', 'Cycle', 'Strategy', 'Student Name', 'AUC ID', 'Email', 'Major', 'GPA', 'Assigned University', 'University Slug', 'Seat Slot', 'Choice Rank', 'Fit Score'],
			filteredPlacements.map((r) => [
				loadedRunSummary?.runCode ?? 'LIVE-SIM',
				simulation!.cycle,
				strategyLabel(simulation!.mode),
				r.studentName,
				r.studentAucId,
				r.studentEmail,
				r.studentMajor,
				r.studentGpa.toFixed(2),
				r.universityName,
				r.universitySlug,
				r.seatLabel,
				r.rankLabel,
				r.weight.toFixed(4)
			])
		);
		showToast(`Exported ${filteredPlacements.length} matched placements to CSV.`);
	}

	async function exportSavedRunCsv(run: AdminSavedRunSummary) {
		exportingRunId = run.id;
		try {
			const detail = await adminApi.getRunDetail(run.id);
			downloadCsv(
				`${detail.runCode.toLowerCase()}_matched_placements.csv`,
				['Run Code', 'Run Title', 'Cycle', 'Strategy', 'Status', 'Student Name', 'AUC ID', 'Email', 'Major', 'GPA', 'Assigned University', 'University Slug', 'Seat Slot', 'Choice Rank', 'Fit Score'],
				detail.placements.map((r) => [
					detail.runCode,
					detail.title,
					detail.cycle,
					strategyLabel(detail.mode),
					detail.status,
					r.studentName,
					r.studentAucId,
					r.studentEmail,
					r.studentMajor,
					r.studentGpa.toFixed(2),
					r.universityName,
					r.universitySlug,
					r.seatLabel,
					r.rankLabel,
					r.weight.toFixed(4)
				])
			);
			showToast(`Exported ${detail.placements.length} matched placements from ${detail.runCode} to CSV.`);
		} catch (e) {
			showToast(msgOf(e));
		} finally {
			exportingRunId = null;
		}
	}

	// ---- UI Helpers ----
	function statusBadgeClass(status: string) {
		if (status === 'Verified') return 'bg-[#f0fdf4] text-[#15803d]';
		if (status === 'Ineligible') return 'bg-[#fef2f2] text-[#ba1a1a]';
		if (status === 'Draft') return 'bg-[#eff4ff] text-[#515f74]';
		return 'bg-[#fffbeb] text-[#b45309]';
	}

	function partnerStatusBadge(status: string) {
		if (status === 'active') return { cls: 'bg-[#f0fdf4] text-[#15803d]', label: 'Active' };
		if (status === 'check_slots') return { cls: 'bg-[#fffbeb] text-[#b45309]', label: 'Check Slots' };
		return { cls: 'bg-[#fef2f2] text-[#ba1a1a]', label: 'LOI' };
	}

	// Filtered & grouped matching results (pure TS helpers in matchingFilters.ts)
	const filteredPlacements = $derived.by(() => {
		if (!simulation) return [];
		return filterAndSortPlacements(simulation.placements, {
			search: matchSearch,
			prefRank: matchRankFilter,
			partnerSlug: matchPartnerSlug,
			major: matchMajorFilter,
			minGpa: matchMinGpa,
			sort: matchSortBy
		});
	});

	const groupedPlacements = $derived.by(() => {
		if (!simulation) return [];
		return groupPlacementsByPartner(
			filteredPlacements,
			simulation.placements,
			simulation.universityFills,
			partners,
			matchSortBy
		);
	});

	const filteredUnplaced = $derived.by(() => {
		if (!simulation) return [];
		return filterAndSortUnplaced(simulation.unplaced, {
			search: unplacedSearch,
			bottleneck: unplacedBottleneck,
			major: unplacedMajor,
			sort: unplacedSortBy
		});
	});

	const filteredUniversityFills = $derived.by(() => {
		if (!simulation) return [];
		return filterAndSortUniversityFills(simulation.universityFills, {
			search: fillSearch,
			region: fillRegion,
			status: fillStatusFilter,
			sort: fillSortBy
		});
	});

	const filteredShortlists = $derived.by(() => {
		if (!simulation) return [];
		return filterShortlists(simulation.shortlists, {
			search: shortlistSearch,
			partnerSlug: shortlistPartnerSlug,
			major: shortlistMajor,
			quota: shortlistQuotaFilter,
			minGpa: shortlistMinGpa,
			hideEmpty: shortlistHideEmpty
		});
	});

	const shortlistFilteredCandidatesCount = $derived(
		filteredShortlists.reduce((acc, sl) => acc + sl.candidates.length, 0)
	);

	// Dynamic filter dropdown / chip options derived from current simulation data
	const matchedPartnerOptions = $derived.by(() => {
		if (!simulation) return [];
		const map = new Map<string, string>();
		for (const p of simulation.placements) {
			if (!map.has(p.universitySlug)) map.set(p.universitySlug, p.universityName);
		}
		return Array.from(map.entries())
			.map(([slug, name]) => ({ slug, name }))
			.sort((a, b) => a.name.localeCompare(b.name));
	});

	const matchedMajorOptions = $derived.by(() => {
		if (!simulation) return [];
		const set = new Set<string>();
		for (const p of simulation.placements) {
			if (p.studentMajor) set.add(p.studentMajor);
		}
		return Array.from(set).sort();
	});

	const unplacedBottleneckOptions = $derived.by(() => {
		if (!simulation) return [];
		const set = new Set<string>();
		for (const u of simulation.unplaced) {
			if (u.bottleneck) set.add(u.bottleneck);
		}
		return Array.from(set).sort();
	});

	const unplacedMajorOptions = $derived.by(() => {
		if (!simulation) return [];
		const set = new Set<string>();
		for (const u of simulation.unplaced) {
			if (u.studentMajor) set.add(u.studentMajor);
		}
		return Array.from(set).sort();
	});

	const fillRegionOptions = $derived.by(() => {
		if (!simulation) return [];
		const set = new Set<string>();
		for (const u of simulation.universityFills) {
			if (u.region) set.add(u.region);
		}
		return Array.from(set).sort();
	});

	const shortlistMajorOptions = $derived.by(() => {
		if (!simulation) return [];
		const set = new Set<string>();
		for (const sl of simulation.shortlists) {
			for (const c of sl.candidates) {
				if (c.studentMajor) set.add(c.studentMajor);
			}
		}
		return Array.from(set).sort();
	});

	const matchedActiveFilterCount = $derived(
		(matchSearch.trim() ? 1 : 0) +
			(matchRankFilter !== 'all' ? 1 : 0) +
			(matchPartnerSlug !== 'all' ? 1 : 0) +
			(matchMajorFilter !== 'all' ? 1 : 0) +
			(matchMinGpa > 0 ? 1 : 0)
	);

	// Paginated active match list
	const matchCurrentPageSize = $derived(
		matchTab === 'shortlists' || (matchTab === 'matched' && matchedLayout === 'grouped')
			? MATCH_GROUP_PAGE_SIZE
			: MATCH_PAGE_SIZE
	);
	const matchActiveTotal = $derived.by(() => {
		if (!simulation) return 0;
		if (matchTab === 'matched') {
			return matchedLayout === 'grouped' ? groupedPlacements.length : filteredPlacements.length;
		}
		if (matchTab === 'unplaced') return filteredUnplaced.length;
		if (matchTab === 'unis') return filteredUniversityFills.length;
		return filteredShortlists.length;
	});
	const matchTotalPages = $derived(
		Math.max(1, Math.ceil(matchActiveTotal / matchCurrentPageSize))
	);
	const clampedMatchPage = $derived(Math.min(matchPage, matchTotalPages));
	const pagedGroupedPartners = $derived(
		groupedPlacements.slice(
			(clampedMatchPage - 1) * MATCH_GROUP_PAGE_SIZE,
			clampedMatchPage * MATCH_GROUP_PAGE_SIZE
		)
	);
	const pagedMatchedRows = $derived(
		filteredPlacements.slice(
			(clampedMatchPage - 1) * MATCH_PAGE_SIZE,
			clampedMatchPage * MATCH_PAGE_SIZE
		)
	);
	const pagedUnplacedRows = $derived(
		filteredUnplaced.slice(
			(clampedMatchPage - 1) * MATCH_PAGE_SIZE,
			clampedMatchPage * MATCH_PAGE_SIZE
		)
	);
	const pagedFillRows = $derived(
		filteredUniversityFills.slice(
			(clampedMatchPage - 1) * MATCH_PAGE_SIZE,
			clampedMatchPage * MATCH_PAGE_SIZE
		)
	);
	const pagedShortlists = $derived(
		filteredShortlists.slice(
			(clampedMatchPage - 1) * MATCH_GROUP_PAGE_SIZE,
			clampedMatchPage * MATCH_GROUP_PAGE_SIZE
		)
	);

	// Active modal cross-links
	const inspectedStudentPlacement = $derived.by<AdminPlacementRow | null>(() => {
		if (!inspectedStudent || !simulation) return null;
		return simulation.placements.find((p) => p.studentEmail === inspectedStudent?.email) ?? null;
	});
	const inspectedPartnerPlacedCount = $derived.by(() => {
		if (!inspectedPartner || !simulation) return 0;
		return simulation.placements.filter((p) => p.universitySlug === inspectedPartner?.slug).length;
	});

	function handleWindowKeydown(e: KeyboardEvent) {
		if (e.key === 'Escape' && demoMenuOpen) {
			demoMenuOpen = false;
		}
		if (e.altKey && e.key === 'ArrowLeft' && returnStack.length > 0) {
			e.preventDefault();
			goBackOneStep();
			return;
		}
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

<div
	class="min-h-screen bg-[#f8f9ff] text-[#0b1c30] overflow-x-hidden transition-colors duration-200 {darkMode
		? 'admin-dark'
		: ''}"
>
	<!-- TOP NAVBAR -->
	<header
		class="fixed top-0 left-0 right-0 h-16 md:h-[72px] bg-white border-b border-[#c6c6cd]/70 z-40 grid grid-cols-[auto_1fr_auto] md:grid-cols-[1fr_auto_1fr] items-center px-3 sm:px-5 md:px-7 gap-2 sm:gap-3 md:gap-4"
	>
		<!-- Left: AUC Logo + Context -->
		<div class="flex items-center gap-2.5 md:gap-3.5 min-w-0">
			<img
				src="/auc-logo.png"
				alt="The American University in Cairo"
				class="h-6 sm:h-7 md:h-9 w-auto max-w-[112px] sm:max-w-none object-contain shrink-0 transition-all {darkMode
					? 'brightness-0 invert opacity-95'
					: ''}"
			/>
			<div class="hidden xl:block h-6 w-px bg-[#c6c6cd] shrink-0"></div>
			<span class="hidden xl:inline-block text-[15px] font-semibold text-[#0b1c30] tracking-tight truncate">
				IPSO Exchange
				<span class="text-[#515f74] font-normal">
					/ {NAV_GROUPS.flatMap((g) => g.items).find((i) => i.id === activePage)?.label ?? 'Admin'}
				</span>
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
					onchange={handleCycleChange}
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

		<!-- Right: Interactive Demo Controls + Dark Mode Toggle + Profile + Sign Out -->
		<div class="flex items-center justify-end gap-2 sm:gap-2.5 shrink-0">
			{#if auth.demoEnabled}
				<div class="relative shrink-0">
					<button
						type="button"
						onclick={() => (demoMenuOpen = !demoMenuOpen)}
						aria-expanded={demoMenuOpen}
						aria-label="Demo cohort controls"
						title="Manage demo applicant cohort"
						class="h-9 sm:h-10 px-2.5 sm:px-3 rounded-xl border text-xs font-semibold inline-flex items-center gap-1.5 whitespace-nowrap shrink-0 cursor-pointer transition-colors {overview.demoApplicantCount >
						0
							? 'bg-[#fffbeb] border-[#c6c6cd]/50 text-[#b45309]'
							: 'bg-[#fef2f2] border-[#c6c6cd]/50 text-[#ba1a1a]'}"
					>
						<svg class="w-3.5 h-3.5 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
							<path d="M12 9v4" />
							<path d="M12 17h.01" />
							<path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z" />
						</svg>
						<span class="hidden lg:inline">
							{overview.demoApplicantCount > 0 ? 'Demo (500)' : 'Demo Cleared'}
						</span>
						<svg class="w-3.5 h-3.5 opacity-75" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
							<polyline points="6 9 12 15 18 9" />
						</svg>
					</button>

					{#if demoMenuOpen}
						<button
							type="button"
							aria-label="Close demo controls menu"
							onclick={() => (demoMenuOpen = false)}
							class="fixed inset-0 z-40 cursor-default"
						></button>
						<div class="absolute right-0 mt-2 w-72 rounded-2xl bg-white border border-[#c6c6cd]/60 shadow-2xl p-4 space-y-3 z-50">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs font-bold uppercase tracking-wider text-[#0b1c30]">
									Demo Stress Cohort
								</span>
								<span
									class="px-2.5 py-0.5 rounded-full text-xs font-semibold whitespace-nowrap {overview.demoApplicantCount >
									0
										? 'bg-[#f0fdf4] text-[#15803d]'
										: 'bg-[#fffbeb] text-[#b45309]'}"
								>
									{overview.demoApplicantCount > 0 ? 'Active' : 'Cleared'}
								</span>
							</div>

							<div class="px-3 py-2 rounded-xl bg-[#eff4ff] text-xs text-[#515f74] font-mono-num flex justify-between">
								<span>Demo: <strong class="text-[#0b1c30]">{overview.demoApplicantCount}</strong></span>
								<span>Real: <strong class="text-[#0b1c30]">{realApplicantCount}</strong></span>
							</div>

							<p class="text-xs text-[#515f74] leading-relaxed">
								{overview.demoApplicantCount > 0
									? 'Clearing removes only flagged demo rows and preserves real student submissions & saved runs.'
									: 'Demo cohort is currently cleared. You can reinstate all 500 realistic applicants at any time.'}
							</p>

							<div class="flex flex-col gap-2 pt-1">
								{#if overview.demoApplicantCount === 0}
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleSeedDemo}
										class="h-9 px-3.5 rounded-xl bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
									>
										{demoBusy ? 'Seeding 500 Applicants…' : 'Reinstate 500 Demo Applicants'}
									</button>
								{:else}
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleSeedDemo}
										class="h-9 px-3.5 rounded-xl bg-[#eff4ff] hover:bg-[#dce9ff] text-[#0b1c30] text-xs font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
									>
										{demoBusy ? 'Working…' : 'Reset 500 Demo Applicants'}
									</button>
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleClearDemo}
										class="h-9 px-3.5 rounded-xl bg-[#fef2f2] hover:bg-[#fee2e2] text-[#ba1a1a] text-xs font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
									>
										Clear Demo Data Only
									</button>
								{/if}
							</div>
						</div>
					{/if}
				</div>
			{/if}

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
					{auth.initials || 'NH'}
				</div>
				<div class="hidden md:flex flex-col text-left">
					<span class="text-sm font-semibold text-[#0b1c30] leading-tight">
						{auth.displayName || 'Nadia Hassan'}
					</span>
					<span class="text-xs font-medium text-[#515f74] leading-tight">IPSO Coordinator</span>
				</div>
			</div>

			<button
				type="button"
				onclick={() => auth.signOut()}
				aria-label="Sign out"
				title="Sign out"
				class="h-9 sm:h-10 w-9 sm:w-10 rounded-xl bg-[#eff4ff] hover:bg-[#e5eeff] border border-[#c6c6cd]/50 text-[#0b1c30] flex items-center justify-center cursor-pointer transition-colors shrink-0"
			>
				<svg class="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
					<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4" />
					<polyline points="16 17 21 12 16 7" />
					<line x1="21" y1="12" x2="9" y2="12" />
				</svg>
			</button>
		</div>
	</header>

	<!-- DESKTOP LEFT SIDEBAR -->
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
								onclick={() => navigate(item.id, true)}
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
			{#if pageReturnPoint && !inspectedStudent && !inspectedPartner}
				<div class="mb-4 px-4 py-2.5 rounded-xl bg-[#e5eeff] border border-[#c6c6cd]/50 flex items-center justify-between gap-2 shadow-sm">
					<button
						type="button"
						onclick={goBackOneStep}
						class="inline-flex items-center gap-2 text-xs sm:text-sm font-semibold text-[#003ea8] hover:underline whitespace-nowrap cursor-pointer"
					>
						<span>&larr;</span>
						<span>Back to {pageReturnPoint.label}</span>
						{#if returnStack.length > 1}
							<span class="px-2 py-0.5 rounded-full bg-white text-[#0b1c30] text-xs font-mono-num">
								{returnStack.length} steps
							</span>
						{/if}
					</button>
					<div class="flex items-center gap-2.5 shrink-0">
						<span class="hidden sm:inline text-xs font-mono-num text-[#515f74]">Alt + &larr;</span>
						<button
							type="button"
							onclick={() => (returnStack = [])}
							aria-label="Dismiss return bar"
							class="h-7 w-7 rounded-lg bg-white text-[#515f74] hover:text-[#0b1c30] flex items-center justify-center cursor-pointer text-sm"
						>
							&times;
						</button>
					</div>
				</div>
			{/if}

			{#if backendError}
				<div class="mb-5 p-4 rounded-xl bg-[#fef2f2] border border-[#c6c6cd]/40 flex flex-wrap items-center justify-between gap-3">
					<div class="text-xs sm:text-sm font-medium text-[#ba1a1a]">
						Backend Connection Notice: {backendError}
					</div>
					<button
						type="button"
						onclick={refreshAllData}
						class="h-9 px-3.5 rounded-lg bg-white border border-[#c6c6cd]/50 text-xs font-semibold text-[#0b1c30] cursor-pointer"
					>
						Retry Sync
					</button>
				</div>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 1: STUDENT APPLICATIONS & ELIGIBILITY                -->
			<!-- ========================================================= -->
			{#if activePage === 'applications'}
				<section class="space-y-5 md:space-y-6">
					<!-- Header -->
					<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
						<div class="flex flex-col gap-1.5 min-w-0">
							<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74] flex-wrap">
								<span>AUC IPSO</span>
								<span class="text-[#c6c6cd]">&rsaquo;</span>
								<span>Outgoing Exchange</span>
								<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
								<span class="hidden sm:inline text-[#0b1c30] font-semibold">
									Student Applications &amp; Eligibility
								</span>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Student Applications &amp; Eligibility
							</h1>
						</div>

						<div class="flex items-center gap-2.5 flex-wrap">
							{#if auth.demoEnabled}
								{#if overview.demoApplicantCount > 0}
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleSeedDemo}
										class="h-10 sm:h-11 px-3.5 bg-white hover:bg-[#eff4ff] text-[#0b1c30] border border-[#c6c6cd]/50 text-xs sm:text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
									>
										{demoBusy ? 'Working…' : 'Reset 500 Demo'}
									</button>
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleClearDemo}
										class="h-10 sm:h-11 px-3.5 bg-[#fef2f2] hover:bg-[#fee2e2] text-[#ba1a1a] border border-[#c6c6cd]/40 text-xs sm:text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
									>
										Clear Demo Data
									</button>
								{:else}
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleSeedDemo}
										class="h-10 sm:h-11 px-4 bg-[#fffbeb] hover:bg-[#eff4ff] text-[#b45309] border border-[#c6c6cd]/50 text-xs sm:text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
									>
										{demoBusy ? 'Seeding 500…' : 'Reinstate 500 Demo'}
									</button>
								{/if}
							{/if}
							<button
								type="button"
								onclick={openMatchingFromApplications}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center justify-center gap-2 whitespace-nowrap cursor-pointer transition-colors shrink-0"
							>
								<svg class="w-4 h-4 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
									<polygon points="5 3 19 12 5 21 5 3" />
								</svg>
								<span>Open Matching Console</span>
							</button>
						</div>
					</div>

					<!-- Live Metric Counters Ribbon -->
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
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tracking-tight tnum">
									{overview.totalApplicants}
								</span>
								{#if overview.demoApplicantCount > 0}
									<span class="text-xs sm:text-sm font-medium text-[#515f74]">
										{overview.demoApplicantCount} demo
									</span>
								{/if}
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">{overview.reviewQueueWaiting}</span> waiting in queue
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
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tracking-tight tnum">
									{overview.eligibleCount}
								</span>
								<span class="text-xs sm:text-sm font-medium text-[#515f74] tnum">
									{overview.totalApplicants > 0
										? `${((overview.eligibleCount / overview.totalApplicants) * 100).toFixed(1)}%`
										: '0%'}
								</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">{overview.firstTimeAbroadCount}</span> first-time abroad
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
									{overview.totalConfiguredSeats}
								</span>
								<span class="text-xs sm:text-sm font-medium text-[#515f74]">
									{overview.totalPartners} partner unis
								</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#0b1c30]">HK Max &nu; = {overview.hkMaxFeasible}</span> placeable
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col justify-between min-w-0">
							<div class="flex items-center justify-between gap-2">
								<span class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#ba1a1a] truncate">
									Flagged / Review
								</span>
								<svg class="w-5 h-5 text-[#ba1a1a] shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
									<circle cx="12" cy="12" r="10" />
									<line x1="12" y1="8" x2="12" y2="12" />
									<line x1="12" y1="16" x2="12.01" y2="16" />
								</svg>
							</div>
							<div class="mt-2.5 sm:mt-3 flex items-baseline gap-2 flex-wrap">
								<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#ba1a1a] tracking-tight tnum">
									{overview.flaggedCount}
								</span>
								<span class="text-xs sm:text-sm font-medium text-[#ba1a1a]">Attention needed</span>
							</div>
							<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
								<span class="font-semibold text-[#ba1a1a]">{overview.lowGpaFlaggedCount} GPA &lt; 2.8</span> · Pending audit
							</div>
						</div>
					</div>

					<!-- Demo Mode Empty / Cleared State Banner (always offers Reinstate when demoApplicantCount === 0) -->
					{#if auth.demoEnabled && !loadingInitial && overview.demoApplicantCount === 0}
						<div class="bg-white p-5 sm:p-6 rounded-2xl shadow-sm border border-[#c6c6cd]/50 flex flex-col md:flex-row md:items-center justify-between gap-4">
							<div class="space-y-1.5 max-w-2xl">
								<div class="inline-flex items-center gap-2 px-2.5 py-1 rounded-full bg-[#e5eeff] text-[#003ea8] text-xs font-semibold">
									{applications.length === 0 ? 'Demo Mode · Stress Test Ready' : 'Demo Cohort Cleared'}
								</div>
								<h2 class="text-lg font-bold text-[#0b1c30]">
									{applications.length === 0
										? 'No Student Applications in SQLite Yet'
										: `Showing ${applications.length} Real Student Record${applications.length === 1 ? '' : 's'} Only`}
								</h2>
								<p class="text-xs sm:text-sm text-[#515f74]">
									All {partners.length} partner universities remain loaded in SQLite. Reinstate the 500 realistic demo applicants anytime (with varied GPAs, majors, essays, documents, and faculty letters) to benchmark the C++ matching engine, or test strictly against real student submissions.
								</p>
							</div>
							<button
								type="button"
								disabled={demoBusy}
								onclick={handleSeedDemo}
								class="h-11 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
							>
								{demoBusy
									? 'Seeding 500 Applicants…'
									: applications.length === 0
										? 'Load 500 Demo Applicants'
										: 'Reinstate 500 Demo Applicants'}
							</button>
						</div>
					{/if}

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
								<div class="flex items-center gap-2 overflow-x-auto no-scrollbar sm:flex-wrap pb-0.5">
									<span class="text-xs font-semibold uppercase tracking-wider text-[#515f74] shrink-0">
										Quick Filters:
									</span>
									<button
										type="button"
										onclick={() => {
											gpaMin = gpaMin >= 2.8 ? 0 : 2.8;
											appPage = 1;
										}}
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
										onclick={() => {
											priorFilter = priorFilter === 'first' ? 'all' : 'first';
											appPage = 1;
										}}
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
										onclick={() => {
											standingFilter = standingFilter === 'Junior' ? 'all' : 'Junior';
											appPage = 1;
										}}
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
											onclick={() => {
												uniFilter = 'all';
												appPage = 1;
											}}
											class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full bg-[#131b2e] text-white text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer"
										>
											<span>Uni: {uniFilter}</span><span>&times;</span>
										</button>
									{/if}
									{#if statusFilter !== 'all'}
										<button
											type="button"
											onclick={() => {
												statusFilter = 'all';
												appPage = 1;
											}}
											class="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-full bg-[#131b2e] text-white text-xs sm:text-[13px] font-medium whitespace-nowrap shrink-0 cursor-pointer"
										>
											<span>Status: {statusFilter}</span><span>&times;</span>
										</button>
									{/if}
								</div>

								<div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-6 gap-3.5 pt-1">
									<div class="flex flex-col gap-1.5 min-w-0">
										<label for="filter-standing" class="text-xs font-semibold text-[#515f74] uppercase tracking-wider whitespace-nowrap">
											Class Standing
										</label>
										<div class="relative">
											<select
												id="filter-standing"
												bind:value={standingFilter}
												onchange={() => (appPage = 1)}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Standings</option>
												<option value="Sophomore">Sophomore</option>
												<option value="Junior">Junior</option>
												<option value="Senior">Senior</option>
												<option value="Graduate">Graduate (GR)</option>
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
												oninput={() => (appPage = 1)}
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
												oninput={() => (appPage = 1)}
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
												onchange={() => (appPage = 1)}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Partner Unis ({partners.length})</option>
												{#each universityFilterOptions as u (u.slug)}
													<option value={u.slug}>
														{u.shortName ? `${u.shortName} · ` : ''}{u.officialName}{u.totalDemand > 0 ? ` (${u.totalDemand})` : ''}
													</option>
												{/each}
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
												onchange={() => (appPage = 1)}
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
												oninput={() => (appPage = 1)}
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
												onchange={() => (appPage = 1)}
												class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 focus:outline-none focus:border-black focus:ring-0 shadow-sm truncate"
											>
												<option value="all">All Statuses</option>
												<option value="Verified">Verified Eligible</option>
												<option value="Submitted">Submitted</option>
												<option value="In Review">In Review</option>
												<option value="Ineligible">Ineligible / Flagged</option>
												<option value="Draft">Draft (Unsubmitted)</option>
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

					<!-- Cohort Sub-tabs + Search + Paginated Data Table / Mobile Cards -->
					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 flex flex-col overflow-hidden">
						<div class="p-4 flex flex-col lg:flex-row lg:items-center justify-between gap-3 bg-[#eff4ff] border-b border-[#c6c6cd]/30">
							<div class="flex items-center gap-1.5 bg-[#d3e4fe]/40 p-1 rounded-xl max-w-full overflow-x-auto no-scrollbar">
								<button
									type="button"
									onclick={() => {
										studentTab = 'all';
										appPage = 1;
									}}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'all'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									All Applicants ({applications.length})
								</button>
								<button
									type="button"
									onclick={() => {
										studentTab = 'eligible';
										appPage = 1;
									}}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'eligible'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									Eligible ({applications.filter(
										(s) => s.status !== 'Ineligible' && s.status !== 'Draft' && s.gpa >= 2.8
									).length})
								</button>
								<button
									type="button"
									onclick={() => {
										studentTab = 'flagged';
										appPage = 1;
									}}
									class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {studentTab ===
									'flagged'
										? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
										: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
								>
									Flagged / Review ({applications.filter((s) => s.flagged).length})
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
										oninput={() => (appPage = 1)}
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
									onclick={exportApplicationsCsv}
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

						<!-- Desktop Data Table -->
						<div class="hidden md:block overflow-x-auto w-full">
							<table class="w-full text-left border-collapse">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider select-none border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-4 px-4 w-12 text-center">
											<input
												type="checkbox"
												checked={pageAllSelected}
												onchange={(e) => togglePageStudents((e.currentTarget as HTMLInputElement).checked)}
												class="rounded w-4 h-4 accent-[#131b2e] cursor-pointer"
												aria-label="Select page applicants"
											/>
										</th>
										<th class="py-4 px-4">Applicant</th>
										<th class="py-4 px-4">Standing &amp; Major</th>
										<th class="py-4 px-4 text-right">GPA</th>
										<th class="py-4 px-4">Ranked Top 3 Preferences</th>
										<th class="py-4 px-4 text-right">Statement</th>
										<th class="py-4 px-4">Intl. Equity</th>
										<th class="py-4 px-4">Audit Status</th>
										<th class="py-4 px-5 text-right">Inspect</th>
									</tr>
								</thead>
								<tbody class="text-[#0b1c30] divide-y divide-[#c6c6cd]/20 text-sm sm:text-[15px]">
									{#if loadingInitial}
										<tr>
											<td colspan="9" class="py-12 text-center text-sm text-[#515f74]">
												Loading applications from SQLite…
											</td>
										</tr>
									{:else if pagedStudents.length > 0}
										{#each pagedStudents as s (s.email)}
											<tr class="hover:bg-[#eff4ff]/50 transition-colors">
												<td class="py-4 px-4 text-center">
													<input
														type="checkbox"
														checked={!!selectedMap[s.email]}
														onchange={(e) =>
															(selectedMap = {
																...selectedMap,
																[s.email]: (e.currentTarget as HTMLInputElement).checked
															})}
														class="rounded w-4 h-4 accent-[#131b2e] cursor-pointer"
														aria-label={`Select ${s.name}`}
													/>
												</td>
												<td class="py-4 px-4">
													<div class="flex flex-col">
														<span class="font-semibold text-[15px] text-[#0b1c30]">{s.name}</span>
														<span class="text-xs text-[#515f74] font-mono-num">
															{s.aucId ? `#${s.aucId} · ` : ''}{s.email}
														</span>
													</div>
												</td>
												<td class="py-4 px-4">
													<div class="flex flex-col gap-0.5">
														<span class="font-medium text-[#0b1c30]">{s.level} · {s.standing}</span>
														<span class="text-xs sm:text-[13px] text-[#515f74]">{s.major || 'Undeclared'}</span>
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
													<div class="flex items-center gap-1.5 flex-wrap max-w-[300px]">
														{#each [0, 1, 2] as idx (idx)}
															{#if s.prefNames[idx]}
																<span
																	class="px-2.5 py-0.5 rounded-full text-xs max-w-[220px] truncate {idx === 0
																		? 'bg-[#e5eeff] text-[#0b1c30] font-semibold'
																		: 'bg-[#eff4ff] text-[#515f74]'}"
																	title={s.prefNames[idx]}
																>
																	<span class="font-mono-num">#{idx + 1}</span>
																	{s.prefNames[idx]}
																</span>
															{/if}
														{/each}
														{#if !s.prefNames[0]}
															<span class="text-xs text-[#515f74]">No choices selected</span>
														{/if}
													</div>
												</td>
												<td class="py-4 px-4 text-right font-mono-num font-semibold">
													{#if s.hasStatementRating && s.statementRating !== null}
														{s.statementRating.toFixed(1)}<span class="text-xs text-[#515f74] font-normal">/10</span>
													{:else}
														<span class="text-xs font-normal text-[#515f74]">Unrated</span>
													{/if}
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
														onclick={() => inspectStudent(s.email)}
														title="Inspect application dossier"
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

						<!-- Mobile Card List -->
						<div class="md:hidden p-3 space-y-3 bg-[#f8f9ff]">
							{#if pagedStudents.length > 0}
								{#each pagedStudents as s (s.email)}
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
													{s.aucId ? `#${s.aucId} · ` : ''}{s.major}
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
													{s.prefNames.filter(Boolean).slice(0, 2).join(' · ') || 'None'}
												</span>
											</div>
											<div class="flex justify-between gap-2">
												<span class="text-[#515f74]">Statement / Equity</span>
												<span class="font-mono-num">
													{s.hasStatementRating && s.statementRating !== null
														? `${s.statementRating.toFixed(1)}/10`
														: 'Unrated'} · {s.prior ? 'Prior' : '1st-Time'}
												</span>
											</div>
										</div>
										<div class="pt-2.5 border-t border-[#c6c6cd]/20 flex items-center justify-between">
											<span class="px-2.5 py-1 rounded-full text-xs font-semibold {statusBadgeClass(s.status)}">
												{s.status}
											</span>
											<button
												type="button"
												onclick={() => inspectStudent(s.email)}
												class="h-8 px-3.5 rounded-lg bg-[#eff4ff] hover:bg-[#dce9ff] text-xs font-semibold text-[#0b1c30] cursor-pointer"
											>
												Inspect
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
									Showing
									<span class="font-semibold text-[#0b1c30]">
										{filteredStudents.length > 0
											? `${(clampedAppPage - 1) * rowsPerPage + 1}–${Math.min(
													clampedAppPage * rowsPerPage,
													filteredStudents.length
												)}`
											: '0'}
									</span>
									of <span class="font-semibold text-[#0b1c30]">{filteredStudents.length}</span> matching ({applications.length} total)
								</span>
								<div class="flex items-center gap-2">
									<label for="rows-per-page" class="text-xs text-[#515f74] font-medium">Rows:</label>
									<select
										id="rows-per-page"
										bind:value={rowsPerPage}
										onchange={() => (appPage = 1)}
										class="stitch-select h-9 pl-2.5 pr-6 bg-white text-[#0b1c30] text-xs font-medium rounded-lg border border-[#c6c6cd]/40 shadow-sm"
									>
										<option value={25}>25</option>
										<option value={50}>50</option>
										<option value={100}>100</option>
									</select>
								</div>
							</div>

							<div class="flex items-center gap-2">
								<button
									type="button"
									disabled={clampedAppPage <= 1}
									onclick={() => (appPage = Math.max(1, clampedAppPage - 1))}
									class="h-9 px-3.5 bg-white text-[#0b1c30] hover:bg-[#dce9ff] rounded-lg border border-[#c6c6cd]/40 shadow-sm text-xs font-semibold cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
								>
									Previous
								</button>
								<span class="px-2.5 h-9 rounded-lg bg-[#131b2e] text-white font-semibold text-xs flex items-center justify-center shadow-sm font-mono-num">
									{clampedAppPage} / {appTotalPages}
								</span>
								<button
									type="button"
									disabled={clampedAppPage >= appTotalPages}
									onclick={() => (appPage = Math.min(appTotalPages, clampedAppPage + 1))}
									class="h-9 px-3.5 bg-white text-[#0b1c30] hover:bg-[#dce9ff] rounded-lg border border-[#c6c6cd]/40 shadow-sm text-xs font-semibold cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
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
								<span class="hidden sm:inline text-[#0b1c30] font-semibold">
									Bipartite Simulation Console
								</span>
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
								disabled={savingRun || matchableApplicantsCount === 0}
								onclick={saveDraftRun}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-white hover:bg-[#eff4ff] text-[#0b1c30] border border-[#c6c6cd]/50 text-xs sm:text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
							>
								{savingRun ? 'Saving…' : 'Save Draft Run'}
							</button>
							<button
								type="button"
								disabled={publishingRun || matchableApplicantsCount === 0}
								onclick={() => publishCurrentOrSelectedRun()}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
							>
								{publishingRun ? 'Publishing…' : 'Publish Results'}
							</button>
						</div>
					</div>

					<!-- TOP: Algorithm & Weight Configuration Card -->
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
									Score = (wP·P + wG·G + wS·S + wF·F) / Σw
								</span>
							</div>
							<div class="flex items-center gap-3 shrink-0">
								<button
									type="button"
									onclick={resetWeights}
									class="text-xs sm:text-sm font-medium text-[#515f74] hover:text-[#0b1c30] underline whitespace-nowrap cursor-pointer"
								>
									Reset Defaults
								</button>
								<button
									type="button"
									disabled={runningSim || matchableApplicantsCount === 0}
									onclick={() => executeSimulation(true)}
									class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center gap-2 whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
								>
									<svg class="w-4 h-4 shrink-0" viewBox="0 0 24 24" fill="currentColor">
										<polygon points="6 4 20 12 6 20 6 4" />
									</svg>
									<span>{runningSim ? 'Running C++ Solver…' : 'Re-run Simulation'}</span>
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
							<div class="grid grid-cols-1 md:grid-cols-12 gap-3.5 items-end">
								<div class="md:col-span-4 flex flex-col gap-1.5 min-w-0">
									<label for="algo-mode" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										C++ Allocation Strategy
									</label>
									<div class="relative">
										<select
											id="algo-mode"
											bind:value={weights.mode}
											class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:border-[#003ea8] focus:ring-0 truncate"
										>
											<option value="best_fit">Cardinality-First + Best Fit (Hungarian + Bonus)</option>
											<option value="highest_score">Pure Max Total Score (Hungarian O(K³))</option>
											<option value="most_placed">Max Placed Only (Hopcroft-Karp O(E√V))</option>
											<option value="shortlists">Ranked Candidate Shortlists per University (Heap)</option>
										</select>
										<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
											<polyline points="7 15 12 20 17 15" />
											<polyline points="7 9 12 4 17 9" />
										</svg>
									</div>
								</div>

								<div class="md:col-span-5 flex flex-col gap-1.5 min-w-0">
									<label for="load-saved-run-select" class="text-xs font-semibold uppercase tracking-wider text-[#003ea8] flex items-center gap-1.5">
										<span>Load Saved Run into Console</span>
									</label>
									<div class="relative">
										<select
											id="load-saved-run-select"
											value={loadedRunSummary?.id ?? 0}
											onchange={(e) =>
												handleSelectLoadedRun(Number((e.currentTarget as HTMLSelectElement).value))}
											class="stitch-select w-full h-11 pl-3.5 pr-8 bg-white text-[#0b1c30] text-sm font-medium rounded-xl border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:border-[#003ea8] truncate"
										>
											<option value={0}>Live Unsaved Simulation</option>
											{#each savedRuns as r (r.id)}
												<option value={r.id}>{r.runCode} · {r.title} [{r.status}]</option>
											{/each}
										</select>
										<svg class="w-4 h-4 text-[#515f74] absolute right-2.5 top-3.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
											<polyline points="7 15 12 20 17 15" />
											<polyline points="7 9 12 4 17 9" />
										</svg>
									</div>
								</div>

								<div class="md:col-span-3 flex flex-col gap-1.5 min-w-0">
									<label for="compare-run-select" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										{weights.mode === 'shortlists' ? 'Shortlist Limit / Uni' : 'Compare Baseline'}
									</label>
									{#if weights.mode === 'shortlists'}
										<select
											id="compare-run-select"
											bind:value={shortlistLimit}
											class="stitch-select w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm"
										>
											<option value={5}>Top 5 candidates</option>
											<option value={10}>Top 10 candidates</option>
											<option value={25}>Top 25 candidates</option>
											<option value={0}>All eligible candidates</option>
										</select>
									{:else}
										<select
											id="compare-run-select"
											bind:value={compareRunId}
											class="stitch-select w-full h-11 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm truncate"
										>
											<option value={0}>Latest Saved Run</option>
											{#each savedRuns as r (r.id)}
												<option value={r.id}>{r.runCode} · {r.title}</option>
											{/each}
										</select>
									{/if}
								</div>
							</div>

							<div class="grid grid-cols-1 md:grid-cols-12 gap-4 items-center">
								<div class="md:col-span-4 flex flex-col gap-1.5 min-w-0">
									<label for="run-title-draft" class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">
										Run Label (for Saving)
									</label>
									<input
										id="run-title-draft"
										type="text"
										bind:value={runTitleDraft}
										placeholder="e.g. Balanced Fall 2027 Simulation..."
										class="w-full h-10 px-3.5 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:border-[#003ea8]"
									/>
								</div>

								<label class="md:col-span-5 flex items-start gap-2.5 cursor-pointer pt-1">
									<input
										bind:checked={weights.allowUnranked}
										type="checkbox"
										class="mt-0.5 rounded w-4 h-4 accent-[#131b2e]"
									/>
									<span class="text-xs sm:text-sm text-[#0b1c30]">
										<span class="font-semibold">Allow eligible unranked fallback (pref = 0.10)</span>
										<span class="block text-xs text-[#515f74]">
											Permit matching into other eligible partners if all 3 ranked choices are full.
										</span>
									</span>
								</label>

								<div class="md:col-span-3 flex md:justify-end">
									<div class="inline-flex items-center gap-2 px-3.5 py-2 bg-[#eff4ff] rounded-full text-xs sm:text-sm text-[#0b1c30]">
										<span class="text-[#515f74]">HK Max:</span>
										<span class="font-mono-num font-bold">
											{simulation ? simulation.hkMax : overview.hkMaxFeasible} / {simulation ? simulation.eligibleCount : overview.eligibleCount}
										</span>
									</div>
								</div>
							</div>

							<!-- 4 Weight Sliders -->
							<div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-5 pt-4 border-t border-[#c6c6cd]/25">
								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">Preference Weight</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.pref.toFixed(2)}</span>
									</div>
									<input bind:value={weights.pref} type="range" min="0.0" max="1.0" step="0.05" class="w-full h-1.5 cursor-pointer" />
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">Academic GPA Weight</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.gpa.toFixed(2)}</span>
									</div>
									<input bind:value={weights.gpa} type="range" min="0.0" max="1.0" step="0.05" class="w-full h-1.5 cursor-pointer" />
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">Statement Rating Weight</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.llm.toFixed(2)}</span>
									</div>
									<input bind:value={weights.llm} type="range" min="0.0" max="1.0" step="0.05" class="w-full h-1.5 cursor-pointer" />
								</div>

								<div class="space-y-2">
									<div class="flex justify-between text-xs sm:text-sm">
										<span class="font-medium text-[#0b1c30]">First-Time Fairness</span>
										<span class="font-mono-num font-semibold text-[#0b1c30]">{weights.equity.toFixed(2)}</span>
									</div>
									<input bind:value={weights.equity} type="range" min="0.0" max="1.0" step="0.05" class="w-full h-1.5 cursor-pointer" />
								</div>
							</div>
						</div>
					</div>

					<!-- Matching Results Area -->
					{#if !simulation}
						<div class="bg-white rounded-2xl p-8 border border-[#c6c6cd]/40 shadow-sm text-center space-y-3.5">
							<h2 class="text-lg font-semibold text-[#0b1c30]">
								{#if overview.totalApplicants === 0}
									No Applicants Available for Simulation
								{:else if matchableApplicantsCount === 0}
									No Submitted Applicants Ready for Matching ({overview.totalApplicants} Draft Only)
								{:else}
									Configure Weights &amp; Run C++ Matching Engine
								{/if}
							</h2>
							<p class="text-xs sm:text-sm text-[#515f74] max-w-xl mx-auto">
								{#if overview.totalApplicants === 0}
									{auth.demoEnabled
										? 'Load the 500-applicant demo cohort to benchmark live bipartite matching, or submit real applications via the Student Portal.'
										: 'Applicants submitted via the Student Portal will appear here automatically for matching.'}
								{:else if matchableApplicantsCount === 0}
									Draft applications do not enter matching until submitted. {auth.demoEnabled &&
									overview.demoApplicantCount === 0
										? 'You can reinstate the 500-applicant demo cohort anytime or load a previously saved snapshot above.'
										: 'Have an applicant submit via /apply or load a saved run above.'}
								{:else}
									Click Re-run Simulation above to execute the selected C++ bipartite matching engine against SQLite applicants and configured seat quotas.
								{/if}
							</p>
							<div class="flex flex-wrap items-center justify-center gap-2.5 pt-1">
								{#if auth.demoEnabled && overview.demoApplicantCount === 0}
									<button
										type="button"
										disabled={demoBusy}
										onclick={handleSeedDemo}
										class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
									>
										{demoBusy
											? 'Seeding 500 Applicants…'
											: overview.totalApplicants === 0
												? 'Load 500 Demo Applicants'
												: 'Reinstate 500 Demo Applicants'}
									</button>
								{/if}
								{#if matchableApplicantsCount > 0}
									<button
										type="button"
										disabled={runningSim}
										onclick={() => executeSimulation(true)}
										class="h-10 px-5 bg-[#eff4ff] hover:bg-[#dce9ff] text-[#0b1c30] rounded-xl text-xs sm:text-sm font-semibold whitespace-nowrap cursor-pointer disabled:opacity-50"
									>
										Run Simulation ({matchableApplicantsCount} active)
									</button>
								{/if}
							</div>
						</div>
					{:else}
						<div class="space-y-4 sm:space-y-5">
							<!-- Active Loaded Saved Snapshot Banner -->
							{#if loadedRunSummary && simulation.mode !== 'shortlists'}
								<div class="bg-[#eff4ff] border border-[#c6c6cd]/50 rounded-2xl px-4 sm:px-5 py-3.5 flex flex-col sm:flex-row sm:items-center justify-between gap-3 shadow-sm">
									<div class="flex items-start sm:items-center gap-3 min-w-0">
										<span class="px-2.5 py-1 rounded-lg bg-white border border-[#c6c6cd]/40 font-mono-num text-xs font-bold text-[#0b1c30] shrink-0">
											{loadedRunSummary.runCode}
										</span>
										<div class="min-w-0">
											<div class="flex items-center gap-2 flex-wrap">
												<span class="text-xs sm:text-sm font-bold text-[#0b1c30] truncate">
													Loaded Snapshot: {loadedRunSummary.title}
												</span>
												<span
													class="px-2 py-0.5 rounded-full text-xs font-semibold {loadedRunSummary.status ===
													'Published'
														? 'bg-[#f0fdf4] text-[#15803d]'
														: loadedRunSummary.status === 'Draft'
															? 'bg-[#e5eeff] text-[#003ea8]'
															: 'bg-white text-[#515f74]'}"
												>
													{loadedRunSummary.status}
												</span>
											</div>
											<div class="text-xs text-[#515f74] font-mono-num mt-0.5">
												{loadedRunSummary.createdAt} · Pref={loadedRunSummary.weights.pref.toFixed(2)} · GPA={loadedRunSummary.weights.gpa.toFixed(2)} · Stmt={loadedRunSummary.weights.llm.toFixed(2)} · Eq={loadedRunSummary.weights.equity.toFixed(2)}
											</div>
										</div>
									</div>
									<div class="flex items-center gap-2 flex-wrap shrink-0">
										{#if loadedRunSummary.status !== 'Published'}
											<button
												type="button"
												disabled={publishingRun}
												onclick={() => publishCurrentOrSelectedRun(loadedRunSummary?.id)}
												class="h-9 px-3 rounded-xl bg-[#f0fdf4] text-[#15803d] text-xs font-semibold whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
											>
												Publish {loadedRunSummary.runCode}
											</button>
										{/if}
										{#if savedRuns.length >= 2}
											<button
												type="button"
												onclick={() => openDiffComparison(loadedRunSummary!.id)}
												class="h-9 px-3 rounded-xl bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#0b1c30] hover:bg-[#dce9ff] whitespace-nowrap shrink-0 cursor-pointer"
											>
												Compare
											</button>
										{/if}
										<button
											type="button"
											onclick={() => exportSavedRunCsv(loadedRunSummary!)}
											disabled={exportingRunId === loadedRunSummary.id}
											class="h-9 px-3 rounded-xl bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8] hover:bg-[#dce9ff] whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
										>
											{exportingRunId === loadedRunSummary.id ? 'Exporting…' : 'Export CSV'}
										</button>
										<button
											type="button"
											onclick={openRunsFromMatching}
											class="h-9 px-3 rounded-xl bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] whitespace-nowrap shrink-0 cursor-pointer"
										>
											All Runs
										</button>
										<button
											type="button"
											disabled={matchableApplicantsCount === 0}
											onclick={() => executeSimulation(true)}
											class="h-9 px-3.5 rounded-xl bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs font-semibold whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
										>
											Re-run Live
										</button>
									</div>
								</div>
							{/if}

							<!-- 4 Simulation KPIs -->
							{#if simulation.mode !== 'shortlists'}
								<div class="grid grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
									<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
										<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
											Placed Students
										</div>
										<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
											<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">
												{simulation.placed}
											</span>
											<span class="text-xs sm:text-sm text-[#515f74] tnum">
												/ {simulation.eligibleCount} eligible
											</span>
										</div>
										<div class="mt-2 text-xs sm:text-sm text-[#15803d] font-medium">
											{simulation.placed === simulation.hkMax
												? `Matches HK max (ν=${simulation.hkMax})`
												: `HK max ν=${simulation.hkMax} (${simulation.hkMax - simulation.placed} trade-off)`}
										</div>
									</div>

									<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
										<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
											1st Choice Rate
										</div>
										<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
											<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] tnum">
												{simulation.firstChoicePct.toFixed(1)}%
											</span>
											<span class="text-xs sm:text-sm text-[#515f74] tnum">
												({simulation.firstChoiceCount})
											</span>
										</div>
										<div class="mt-2 text-xs sm:text-sm text-[#515f74] tnum">
											2nd: {simulation.secondChoiceCount} · 3rd: {simulation.thirdChoiceCount} · Alt: {simulation.unrankedCount}
										</div>
									</div>

									<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
										<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#515f74]">
											Avg Fit Score
										</div>
										<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2">
											<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#0b1c30] font-mono-num">
												{simulation.avgWeight.toFixed(3)}
											</span>
										</div>
										<div class="mt-2 text-xs sm:text-sm text-[#515f74]">Blended range [0.00–1.00]</div>
									</div>

									<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
										<div class="text-xs sm:text-[13px] font-semibold uppercase tracking-wider text-[#ba1a1a]">
											Unplaced Applicants
										</div>
										<div class="mt-2 sm:mt-2.5 flex items-baseline gap-2 flex-wrap">
											<span class="text-2xl sm:text-3xl md:text-4xl font-bold text-[#ba1a1a] tnum">
												{simulation.unplacedCount}
											</span>
											<span class="text-xs sm:text-sm text-[#ba1a1a] font-medium">unplaced</span>
										</div>
										<div class="mt-2 text-xs sm:text-sm text-[#515f74]">
											See Unplaced Analysis for reasons
										</div>
									</div>
								</div>

								<!-- Real C++ Diff Strip -->
								{#if simulation.diff.hasBaseline}
									<div class="bg-[#eff4ff] border border-[#c6c6cd]/40 rounded-xl shadow-sm px-4 sm:px-5 py-3.5 flex flex-wrap items-center justify-between gap-2.5 text-xs sm:text-sm">
										<div class="flex items-center gap-2.5 flex-wrap">
											<span class="px-3 py-0.5 bg-white border border-[#c6c6cd]/40 rounded-full font-mono-num font-semibold text-[#0b1c30]">
												DIFF vs {simulation.diff.baselineRunCode}
											</span>
											<span class="text-[#0b1c30]">
												<span class="font-semibold {simulation.diff.placedDelta >= 0 ? 'text-[#15803d]' : 'text-[#ba1a1a]'}">
													{simulation.diff.placedDelta >= 0
														? `+${simulation.diff.placedDelta}`
														: simulation.diff.placedDelta} placed
												</span> ·
												<span class="font-semibold {simulation.diff.firstChoiceDelta >= 0 ? 'text-[#15803d]' : 'text-[#ba1a1a]'}">
													{simulation.diff.firstChoiceDelta >= 0
														? `+${simulation.diff.firstChoiceDelta}`
														: simulation.diff.firstChoiceDelta} 1st choice
												</span> ·
												<span class="font-semibold text-[#515f74]">
													{simulation.diff.shiftedCount} shifted · {simulation.diff.gainedCount} gained · {simulation.diff.lostCount} lost
												</span>
											</span>
										</div>
										<button
											type="button"
											onclick={openRunsFromMatching}
											class="text-xs sm:text-sm font-semibold text-[#003ea8] hover:underline whitespace-nowrap cursor-pointer"
										>
											View Saved Runs &rarr;
										</button>
									</div>
								{/if}
							{/if}

							<!-- Overhauled Matching Results Card -->
							<div class="bg-white rounded-2xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
								<!-- Results Tabs + Layout Switcher -->
								<div class="px-4 py-3.5 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex flex-col sm:flex-row sm:items-center justify-between gap-3">
									<div class="flex items-center gap-1.5 bg-[#d3e4fe]/40 p-1 rounded-xl max-w-full overflow-x-auto no-scrollbar">
										{#if simulation.mode === 'shortlists'}
											<button
												type="button"
												onclick={() => {
													matchTab = 'shortlists';
													matchPage = 1;
												}}
												class="h-9 sm:h-10 px-3.5 sm:px-4 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer bg-white text-[#0b1c30] font-semibold shadow-sm"
											>
												Ranked Shortlists ({filteredShortlists.length} / {simulation.shortlists.length})
											</button>
										{:else}
											<button
												type="button"
												onclick={() => {
													matchTab = 'matched';
													matchPage = 1;
												}}
												class="h-9 sm:h-10 px-3.5 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
												'matched'
													? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
													: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
											>
												Matched ({filteredPlacements.length === simulation.placed
													? simulation.placed
													: `${filteredPlacements.length}/${simulation.placed}`})
											</button>
											<button
												type="button"
												onclick={() => {
													matchTab = 'unplaced';
													matchPage = 1;
												}}
												class="h-9 sm:h-10 px-3.5 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
												'unplaced'
													? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
													: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
											>
												Unplaced ({filteredUnplaced.length === simulation.unplacedCount
													? simulation.unplacedCount
													: `${filteredUnplaced.length}/${simulation.unplacedCount}`})
											</button>
											<button
												type="button"
												onclick={() => {
													matchTab = 'unis';
													matchPage = 1;
												}}
												class="h-9 sm:h-10 px-3.5 text-xs sm:text-sm rounded-lg whitespace-nowrap shrink-0 cursor-pointer {matchTab ===
												'unis'
													? 'bg-white text-[#0b1c30] font-semibold shadow-sm'
													: 'text-[#515f74] hover:text-[#0b1c30] font-medium'}"
											>
												Fill Rates ({filteredUniversityFills.length})
											</button>
										{/if}
									</div>

									<div class="flex items-center justify-between sm:justify-end gap-2.5 flex-wrap">
										{#if simulation.mode !== 'shortlists' && matchTab === 'matched'}
											<div class="inline-flex p-0.5 rounded-xl bg-[#d3e4fe]/40 border border-[#c6c6cd]/30 text-xs font-semibold">
												<button
													type="button"
													onclick={() => {
														matchedLayout = 'grouped';
														matchPage = 1;
													}}
													class="px-3 py-1.5 rounded-lg cursor-pointer transition {matchedLayout === 'grouped'
														? 'bg-white text-[#0b1c30] shadow-sm'
														: 'text-[#515f74] hover:text-[#0b1c30]'}"
												>
													By University ({groupedPlacements.length})
												</button>
												<button
													type="button"
													onclick={() => {
														matchedLayout = 'roster';
														matchPage = 1;
													}}
													class="px-3 py-1.5 rounded-lg cursor-pointer transition {matchedLayout === 'roster'
														? 'bg-white text-[#0b1c30] shadow-sm'
														: 'text-[#515f74] hover:text-[#0b1c30]'}"
												>
													Student Roster
												</button>
											</div>
										{/if}
										<button
											type="button"
											onclick={exportActiveSimulationCsv}
											class="h-8 px-3 rounded-lg bg-white hover:bg-[#dce9ff] border border-[#c6c6cd]/50 text-xs font-semibold text-[#0b1c30] whitespace-nowrap shrink-0 cursor-pointer"
										>
											Export CSV
										</button>
										<span class="text-xs font-mono-num text-[#515f74] whitespace-nowrap">
											{simulation.solveTimeMs > 0
												? `Solved in ${simulation.solveTimeMs.toFixed(1)} ms`
												: 'Saved snapshot'}
										</span>
									</div>
								</div>

								<!-- Dedicated Live Filtration Bar per Results Tab -->
								<div class="px-4 py-3 bg-white border-b border-[#c6c6cd]/30 space-y-2.5">
									{#if simulation.mode === 'shortlists' || matchTab === 'shortlists'}
										<div class="flex flex-col sm:flex-row sm:items-center gap-2.5">
											<div class="relative flex-1 min-w-[200px]">
												<input
													type="text"
													value={shortlistSearch}
													oninput={(e) => {
														shortlistSearch = (e.currentTarget as HTMLInputElement).value;
														matchPage = 1;
													}}
													placeholder="Search university, country, student, AUC ID, or major..."
													class="w-full h-9 pl-9 pr-3 bg-white text-[#0b1c30] placeholder:text-[#515f74] rounded-xl border border-[#c6c6cd]/50 text-xs sm:text-sm focus:outline-none focus:border-[#003ea8]"
												/>
												<svg class="w-4 h-4 text-[#515f74] absolute left-3 top-2.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
													<circle cx="11" cy="11" r="8" />
													<line x1="21" y1="21" x2="16.65" y2="16.65" />
												</svg>
											</div>

											<div class="flex items-center gap-2 flex-wrap">
												<select
													value={shortlistPartnerSlug}
													onchange={(e) => {
														shortlistPartnerSlug = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50 max-w-[220px] truncate"
												>
													<option value="all">All Partners ({simulation.shortlists.length})</option>
													{#each simulation.shortlists as sl (sl.slug)}
														<option value={sl.slug}>{sl.name}</option>
													{/each}
												</select>

												<select
													value={shortlistMajor}
													onchange={(e) => {
														shortlistMajor = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50 max-w-[190px] truncate"
												>
													<option value="all">All Majors</option>
													{#each shortlistMajorOptions as maj (maj)}
														<option value={maj}>{maj}</option>
													{/each}
												</select>

												<select
													value={String(shortlistMinGpa)}
													onchange={(e) => {
														shortlistMinGpa = Number((e.currentTarget as HTMLSelectElement).value);
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50"
												>
													<option value="0">Any GPA</option>
													<option value="3.7">GPA ≥ 3.70</option>
													<option value="3.5">GPA ≥ 3.50</option>
													<option value="3.2">GPA ≥ 3.20</option>
													<option value="3">GPA ≥ 3.00</option>
												</select>
											</div>
										</div>

										<div class="flex items-center justify-between flex-wrap gap-2">
											<div class="flex items-center gap-2 flex-wrap">
												<div class="flex items-center gap-1.5 flex-wrap">
													{#each [['all', 'All Ranked'], ['in_quota', 'In Quota Only'], ['waitlist', 'Waitlist Only']] as [key, label] (key)}
														<button
															type="button"
															onclick={() => {
																shortlistQuotaFilter = key as ShortlistQuotaFilter;
																matchPage = 1;
															}}
															class="px-3 py-1 rounded-full text-xs font-semibold cursor-pointer {shortlistQuotaFilter ===
															key
																? 'bg-[#131b2e] text-white'
																: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
														>
															{label}
														</button>
													{/each}
												</div>
												<label class="inline-flex items-center gap-1.5 text-xs text-[#515f74] cursor-pointer ml-1">
													<input
														type="checkbox"
														checked={shortlistHideEmpty}
														onchange={(e) => {
															shortlistHideEmpty = (e.currentTarget as HTMLInputElement).checked;
															matchPage = 1;
														}}
														class="rounded w-3.5 h-3.5"
													/>
													<span>Hide empty partners</span>
												</label>
											</div>
											<div class="flex items-center gap-2.5 text-xs text-[#515f74] font-mono-num">
												<span>
													Showing <strong class="text-[#0b1c30]">{filteredShortlists.length}</strong> partners ·
													<strong class="text-[#0b1c30]">{shortlistFilteredCandidatesCount}</strong> candidates
												</span>
												{#if shortlistSearch || shortlistPartnerSlug !== 'all' || shortlistMajor !== 'all' || shortlistQuotaFilter !== 'all' || shortlistMinGpa > 0 || !shortlistHideEmpty}
													<button
														type="button"
														onclick={() => resetMatchResultsFilters('shortlists')}
														class="font-sans font-semibold text-[#003ea8] hover:underline cursor-pointer"
													>
														Reset
													</button>
												{/if}
											</div>
										</div>
									{:else if matchTab === 'matched'}
										<div class="flex items-center gap-2">
											<div class="relative flex-1 min-w-0">
												<input
													type="text"
													value={matchSearch}
													oninput={(e) => {
														matchSearch = (e.currentTarget as HTMLInputElement).value;
														matchPage = 1;
													}}
													placeholder="Search student, AUC ID, major, seat slot, or university..."
													class="w-full h-9 pl-9 pr-3 bg-white text-[#0b1c30] placeholder:text-[#515f74] rounded-xl border border-[#c6c6cd]/50 text-xs sm:text-sm focus:outline-none focus:border-[#003ea8]"
												/>
												<svg class="w-4 h-4 text-[#515f74] absolute left-3 top-2.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
													<circle cx="11" cy="11" r="8" />
													<line x1="21" y1="21" x2="16.65" y2="16.65" />
												</svg>
											</div>

											<button
												type="button"
												onclick={() => (mobileMatchFiltersOpen = !mobileMatchFiltersOpen)}
												class="md:hidden h-9 px-3 rounded-xl border border-[#c6c6cd]/50 bg-white text-xs font-semibold text-[#0b1c30] shrink-0 cursor-pointer"
											>
												Filters{matchedActiveFilterCount > 0 ? ` (${matchedActiveFilterCount})` : ''}
											</button>
										</div>

										<div class="{mobileMatchFiltersOpen ? 'flex' : 'hidden md:flex'} flex-wrap items-center gap-2">
											<div class="flex items-center gap-1.5 overflow-x-auto no-scrollbar">
												{#each [['all', 'All Ranks'], ['1', '1st Choice'], ['2', '2nd'], ['3', '3rd'], ['0', 'Fallback']] as [key, label] (key)}
													<button
														type="button"
														onclick={() => {
															matchRankFilter = key as 'all' | '1' | '2' | '3' | '0';
															matchPage = 1;
														}}
														class="px-3 py-1 rounded-full text-xs font-semibold whitespace-nowrap cursor-pointer {matchRankFilter ===
														key
															? 'bg-[#131b2e] text-white'
															: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
													>
														{label}
													</button>
												{/each}
											</div>

											<div class="flex items-center gap-2 flex-wrap w-full md:w-auto md:ml-auto">
												<select
													value={matchPartnerSlug}
													onchange={(e) => {
														matchPartnerSlug = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="flex-1 md:flex-initial h-8 pl-2.5 pr-8 rounded-lg border border-[#c6c6cd]/50 bg-white text-[#0b1c30] text-xs font-medium max-w-[210px] truncate"
												>
													<option value="all">All Partners ({matchedPartnerOptions.length})</option>
													{#each matchedPartnerOptions as p (p.slug)}
														<option value={p.slug}>{p.name}</option>
													{/each}
												</select>

												<select
													value={matchMajorFilter}
													onchange={(e) => {
														matchMajorFilter = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="flex-1 md:flex-initial h-8 pl-2.5 pr-8 rounded-lg border border-[#c6c6cd]/50 bg-white text-[#0b1c30] text-xs font-medium max-w-[180px] truncate"
												>
													<option value="all">All Majors</option>
													{#each matchedMajorOptions as maj (maj)}
														<option value={maj}>{maj}</option>
													{/each}
												</select>

												<select
													value={String(matchMinGpa)}
													onchange={(e) => {
														matchMinGpa = Number((e.currentTarget as HTMLSelectElement).value);
														matchPage = 1;
													}}
													class="h-8 pl-2.5 pr-8 rounded-lg border border-[#c6c6cd]/50 bg-white text-[#0b1c30] text-xs font-medium"
												>
													<option value="0">Any GPA</option>
													<option value="3.7">GPA ≥ 3.70</option>
													<option value="3.5">GPA ≥ 3.50</option>
													<option value="3.2">GPA ≥ 3.20</option>
													<option value="3">GPA ≥ 3.00</option>
												</select>

												<select
													value={matchSortBy}
													onchange={(e) => {
														matchSortBy = (e.currentTarget as HTMLSelectElement).value as PlacementSortKey;
														matchPage = 1;
													}}
													class="h-8 pl-2.5 pr-8 rounded-lg border border-[#c6c6cd]/50 bg-white text-[#0b1c30] text-xs font-medium"
												>
													<option value="score_desc">Sort: Fit Score ↓</option>
													<option value="gpa_desc">Sort: GPA ↓</option>
													<option value="rank_asc">Sort: Pref Rank ↑</option>
													<option value="uni_asc">Sort: University A–Z</option>
													<option value="name_asc">Sort: Student A–Z</option>
												</select>

												{#if matchedActiveFilterCount > 0}
													<button
														type="button"
														onclick={() => resetMatchResultsFilters('matched')}
														class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer px-1"
													>
														Reset
													</button>
												{/if}
											</div>
										</div>
									{:else if matchTab === 'unplaced'}
										<div class="flex flex-col sm:flex-row sm:items-center gap-2.5">
											<div class="relative flex-1 min-w-[200px]">
												<input
													type="text"
													value={unplacedSearch}
													oninput={(e) => {
														unplacedSearch = (e.currentTarget as HTMLInputElement).value;
														matchPage = 1;
													}}
													placeholder="Search unplaced student, AUC ID, preferences, or scorer reason..."
													class="w-full h-9 pl-9 pr-3 bg-white text-[#0b1c30] placeholder:text-[#515f74] rounded-xl border border-[#c6c6cd]/50 text-xs sm:text-sm focus:outline-none focus:border-[#003ea8]"
												/>
												<svg class="w-4 h-4 text-[#515f74] absolute left-3 top-2.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
													<circle cx="11" cy="11" r="8" />
													<line x1="21" y1="21" x2="16.65" y2="16.65" />
												</svg>
											</div>

											<div class="flex items-center gap-2 flex-wrap">
												<select
													value={unplacedMajor}
													onchange={(e) => {
														unplacedMajor = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50 max-w-[190px] truncate"
												>
													<option value="all">All Majors</option>
													{#each unplacedMajorOptions as maj (maj)}
														<option value={maj}>{maj}</option>
													{/each}
												</select>

												<select
													value={unplacedSortBy}
													onchange={(e) => {
														unplacedSortBy = (e.currentTarget as HTMLSelectElement).value as 'gpa_desc' | 'name_asc';
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50"
												>
													<option value="gpa_desc">Sort: GPA ↓</option>
													<option value="name_asc">Sort: Name A–Z</option>
												</select>
											</div>
										</div>

										<div class="flex items-center justify-between flex-wrap gap-2">
											<div class="flex items-center gap-1.5 flex-wrap">
												<button
													type="button"
													onclick={() => {
														unplacedBottleneck = 'all';
														matchPage = 1;
													}}
													class="px-3 py-1 rounded-full text-xs font-semibold cursor-pointer {unplacedBottleneck === 'all'
														? 'bg-[#131b2e] text-white'
														: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
												>
													All Bottlenecks
												</button>
												{#each unplacedBottleneckOptions as bn (bn)}
													<button
														type="button"
														onclick={() => {
															unplacedBottleneck = bn;
															matchPage = 1;
														}}
														class="px-3 py-1 rounded-full text-xs font-semibold cursor-pointer {unplacedBottleneck === bn
															? 'bg-[#131b2e] text-white'
															: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
													>
														{bn}
													</button>
												{/each}
											</div>
											{#if unplacedSearch || unplacedBottleneck !== 'all' || unplacedMajor !== 'all'}
												<button
													type="button"
													onclick={() => resetMatchResultsFilters('unplaced')}
													class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer"
												>
													Reset
												</button>
											{/if}
										</div>
									{:else}
										<div class="flex flex-col sm:flex-row sm:items-center gap-2.5">
											<div class="relative flex-1 min-w-[200px]">
												<input
													type="text"
													value={fillSearch}
													oninput={(e) => {
														fillSearch = (e.currentTarget as HTMLInputElement).value;
														matchPage = 1;
													}}
													placeholder="Search partner university, country, or region..."
													class="w-full h-9 pl-9 pr-3 bg-white text-[#0b1c30] placeholder:text-[#515f74] rounded-xl border border-[#c6c6cd]/50 text-xs sm:text-sm focus:outline-none focus:border-[#003ea8]"
												/>
												<svg class="w-4 h-4 text-[#515f74] absolute left-3 top-2.5 pointer-events-none" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
													<circle cx="11" cy="11" r="8" />
													<line x1="21" y1="21" x2="16.65" y2="16.65" />
												</svg>
											</div>

											<div class="flex items-center gap-2 flex-wrap">
												<select
													value={fillRegion}
													onchange={(e) => {
														fillRegion = (e.currentTarget as HTMLSelectElement).value;
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50"
												>
													<option value="all">All Regions</option>
													{#each fillRegionOptions as reg (reg)}
														<option value={reg}>{reg}</option>
													{/each}
												</select>

												<select
													value={fillSortBy}
													onchange={(e) => {
														fillSortBy = (e.currentTarget as HTMLSelectElement).value as FillSortKey;
														matchPage = 1;
													}}
													class="h-9 pl-3 pr-8 bg-white text-[#0b1c30] text-xs font-medium rounded-xl border border-[#c6c6cd]/50"
												>
													<option value="demand_desc">Sort: Demand Ratio ↓</option>
													<option value="fill_desc">Sort: Fill % ↓</option>
													<option value="open_desc">Sort: Open Seats ↓</option>
													<option value="cutoff_desc">Sort: Cutoff Score ↓</option>
													<option value="name_asc">Sort: Name A–Z</option>
												</select>
											</div>
										</div>

										<div class="flex items-center justify-between flex-wrap gap-2">
											<div class="flex items-center gap-1.5 flex-wrap">
												{#each [['all', 'All Partners'], ['full', 'Full (100%)'], ['open', 'Open Seats'], ['high', 'High Demand (≥3x)'], ['empty', '0 Placed']] as [key, label] (key)}
													<button
														type="button"
														onclick={() => {
															fillStatusFilter = key as FillStatusFilter;
															matchPage = 1;
														}}
														class="px-3 py-1 rounded-full text-xs font-semibold cursor-pointer {fillStatusFilter ===
														key
															? 'bg-[#131b2e] text-white'
															: 'bg-[#eff4ff] text-[#515f74] hover:bg-[#dce9ff]'}"
													>
														{label}
													</button>
												{/each}
											</div>
											{#if fillSearch || fillRegion !== 'all' || fillStatusFilter !== 'all'}
												<button
													type="button"
													onclick={() => resetMatchResultsFilters('unis')}
													class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer"
												>
													Reset
												</button>
											{/if}
										</div>
									{/if}
								</div>

								<!-- Results Views -->
								{#if simulation.mode === 'shortlists' || matchTab === 'shortlists'}
									<!-- Option 3 Heap Shortlists Grid -->
									<div class="p-3.5 sm:p-5 bg-[#f8f9ff]">
										{#if pagedShortlists.length === 0}
											<div class="bg-white rounded-xl border border-[#c6c6cd]/40 p-8 text-center text-sm text-[#515f74]">
												No partner shortlists match your current filters.
											</div>
										{:else}
											<div class="grid grid-cols-1 lg:grid-cols-2 gap-4">
												{#each pagedShortlists as sl (sl.slug)}
													<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm overflow-hidden flex flex-col">
														<div class="px-4 py-3 bg-[#eff4ff] border-b border-[#c6c6cd]/30 flex items-center justify-between gap-2">
															<div class="min-w-0">
																<button
																	type="button"
																	onclick={() => inspectPartnerDossier(sl.slug)}
																	class="font-bold text-sm sm:text-[15px] text-[#0b1c30] hover:underline text-left truncate block max-w-full cursor-pointer"
																>
																	{sl.name} ↗
																</button>
																<div class="text-xs text-[#515f74]">
																	{sl.country} · {sl.eligibleCount} eligible candidates
																</div>
															</div>
															<span class="px-2.5 py-1 rounded-full bg-white border border-[#c6c6cd]/40 text-xs font-mono-num font-semibold text-[#0b1c30] shrink-0">
																{sl.totalSeats} seats
															</span>
														</div>
														<div class="divide-y divide-[#c6c6cd]/20 text-xs sm:text-sm">
															{#each sl.candidates as c, idx (c.rank)}
																{@const prevInQuota = idx > 0 && sl.candidates[idx - 1].rank <= sl.totalSeats}
																{@const isWaitlistCutoff = c.rank > sl.totalSeats && prevInQuota}
																{#if isWaitlistCutoff}
																	<div class="px-4 py-1.5 bg-[#eff4ff] text-xs font-bold uppercase tracking-wider text-[#515f74]">
																		── Waitlist Cutoff (Below Seat Quota {sl.totalSeats}) ──
																	</div>
																{/if}
																<button
																	type="button"
																	onclick={() => inspectStudent(c.studentEmail)}
																	class="w-full text-left px-4 py-2.5 hover:bg-[#eff4ff]/50 flex items-center justify-between gap-2 cursor-pointer transition {c.rank <=
																	sl.totalSeats
																		? 'bg-[#f0fdf4]/40'
																		: ''}"
																>
																	<div class="min-w-0 flex items-center gap-2.5">
																		<span
																			class="w-6 h-6 rounded-full flex items-center justify-center font-mono-num text-xs font-bold shrink-0 {c.rank <=
																			sl.totalSeats
																				? 'bg-[#f0fdf4] text-[#15803d]'
																				: 'bg-[#eff4ff] text-[#515f74]'}"
																		>
																			{c.rank}
																		</span>
																		<div class="min-w-0">
																			<div class="font-semibold text-[#0b1c30] truncate flex items-center gap-1.5 flex-wrap">
																				<span>{c.studentName}</span>
																				{#if c.statementMissing}
																					<span class="px-2 py-0.5 rounded-full bg-[#fffbeb] text-[#b45309] text-xs font-medium">
																						Unrated Stmt
																					</span>
																				{/if}
																				{#if c.minGpaUnknown}
																					<span class="px-2 py-0.5 rounded-full bg-[#eff4ff] text-[#515f74] text-xs font-medium">
																						Min GPA?
																					</span>
																				{/if}
																			</div>
																			<div class="text-xs text-[#515f74] truncate">
																				{c.studentMajor} · GPA <strong class="font-mono-num text-[#0b1c30]">{c.studentGpa.toFixed(2)}</strong>
																			</div>
																		</div>
																	</div>
																	<div class="text-right shrink-0">
																		<div class="font-mono-num font-bold text-[#0b1c30]">{c.score.toFixed(3)}</div>
																		<div class="text-xs font-semibold {c.rank <= sl.totalSeats ? 'text-[#15803d]' : 'text-[#515f74]'}">
																			{c.rank <= sl.totalSeats ? 'In quota' : 'Waitlist'}
																		</div>
																	</div>
																</button>
															{:else}
																<div class="px-4 py-4 text-xs text-[#515f74]">
																	No eligible candidates for this partner under current filters.
																</div>
															{/each}
														</div>
													</div>
												{/each}
											</div>
										{/if}
									</div>
								{:else if matchTab === 'matched'}
									{#if matchedLayout === 'grouped'}
										<!-- Grouped by Partner University Cards (Desktop 2-col, Mobile 1-col) -->
										<div class="p-3.5 sm:p-5 bg-[#f8f9ff]">
											{#if pagedGroupedPartners.length === 0}
												<div class="bg-white rounded-xl border border-[#c6c6cd]/40 p-8 text-center text-sm text-[#515f74]">
													No matched placements match your current filters.
												</div>
											{:else}
												<div class="grid grid-cols-1 lg:grid-cols-2 gap-4">
													{#each pagedGroupedPartners as g (g.slug)}
														{@const fillPct = g.totalSeats > 0 ? Math.min(100, Math.round((g.totalPlacedAtPartner / g.totalSeats) * 100)) : 0}
														<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 shadow-sm overflow-hidden flex flex-col">
															<div class="px-4 py-3 bg-[#eff4ff] border-b border-[#c6c6cd]/30 space-y-2">
																<div class="flex items-start justify-between gap-2">
																	<div class="min-w-0">
																		<button
																			type="button"
																			onclick={() => inspectPartnerDossier(g.slug)}
																			class="text-left font-bold text-sm sm:text-base text-[#0b1c30] hover:underline truncate block max-w-full cursor-pointer"
																		>
																			{g.name} ↗
																		</button>
																		<div class="text-xs text-[#515f74] flex items-center gap-1.5 flex-wrap mt-0.5">
																			{#if g.country}
																				<span>{g.country}{g.region ? ` · ${g.region}` : ''}</span>
																				<span>·</span>
																			{/if}
																			<span class="font-mono-num">
																				Cutoff {g.cutoffScore !== null ? g.cutoffScore.toFixed(3) : '—'}
																			</span>
																		</div>
																	</div>
																	<span
																		class="px-2.5 py-1 rounded-full text-xs font-mono-num font-bold shrink-0 {g.openSeats === 0
																			? 'bg-[#f0fdf4] text-[#15803d]'
																			: 'bg-[#fffbeb] text-[#b45309]'}"
																	>
																		{g.totalPlacedAtPartner}/{g.totalSeats} seats
																	</span>
																</div>

																<div class="flex items-center gap-2">
																	<div class="flex-1 h-1.5 rounded-full bg-[#e5eeff] overflow-hidden">
																		<div
																			class="h-full rounded-full {g.openSeats === 0 ? 'bg-[#15803d]' : 'bg-[#d97706]'}"
																			style="width: {fillPct}%"
																		></div>
																	</div>
																	<span class="text-xs font-mono-num text-[#515f74]">
																		{g.openSeats === 0 ? 'Full' : `${g.openSeats} open`}
																	</span>
																</div>

																{#if g.placements.length < g.totalPlacedAtPartner}
																	<div class="text-xs font-medium text-[#003ea8]">
																		Showing {g.placements.length} matching filter (of {g.totalPlacedAtPartner} placed)
																	</div>
																{/if}
															</div>

															<div class="divide-y divide-[#c6c6cd]/20">
																{#each g.placements as r (r.studentEmail)}
																	<button
																		type="button"
																		onclick={() => inspectStudent(r.studentEmail)}
																		class="w-full text-left px-4 py-3 hover:bg-[#eff4ff]/50 transition flex items-center justify-between gap-2.5 cursor-pointer"
																	>
																		<div class="flex items-center gap-2.5 min-w-0">
																			<span
																				title={r.seatLabel}
																				class="min-w-8 h-8 px-2 rounded-lg bg-[#e5eeff] text-[#0b1c30] font-mono-num text-xs font-bold inline-flex items-center justify-center shrink-0 whitespace-nowrap"
																			>
																				{formatShortSeatLabel(r.seatLabel)}
																			</span>
																			<div class="min-w-0">
																				<div class="flex items-center gap-1.5 flex-wrap">
																					<span class="font-semibold text-sm text-[#0b1c30] truncate">{r.studentName}</span>
																					<span
																						class="px-2 py-0.5 rounded-full text-xs font-semibold {r.prefRank === 1
																							? 'bg-[#f0fdf4] text-[#15803d]'
																							: r.prefRank === 0
																								? 'bg-[#fffbeb] text-[#b45309]'
																								: 'bg-[#e5eeff] text-[#0b1c30]'}"
																					>
																						{r.rankLabel}
																					</span>
																				</div>
																				<div class="text-xs text-[#515f74] truncate mt-0.5">
																					{r.studentMajor} ·
																					<strong class="font-mono-num text-[#0b1c30]">GPA {r.studentGpa.toFixed(2)}</strong>
																				</div>
																			</div>
																		</div>

																		<div class="text-right shrink-0 flex items-center gap-2">
																			<div>
																				<div class="font-mono-num text-sm font-bold text-[#0b1c30]">
																					{r.weight.toFixed(3)}
																				</div>
																				<div class="w-12 h-1 rounded-full bg-[#e5eeff] overflow-hidden mt-1 ml-auto">
																					<div
																						class="h-full rounded-full bg-[#003ea8]"
																						style="width: {Math.max(5, Math.min(100, Math.round(r.weight * 100)))}%"
																					></div>
																				</div>
																			</div>
																			<span class="text-[#515f74] text-sm">&rsaquo;</span>
																		</div>
																	</button>
																{/each}
															</div>
														</div>
													{/each}
												</div>
											{/if}
										</div>
									{:else}
										<!-- Student Roster View: Desktop Table + Mobile Structured Cards -->
										{#if pagedMatchedRows.length === 0}
											<div class="p-8 text-center text-sm text-[#515f74] bg-[#f8f9ff]">
												No matched students match your current filters.
											</div>
										{:else}
											<div class="hidden md:block overflow-x-auto">
												<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
													<thead class="bg-[#eff4ff] text-[#515f74] text-xs font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
														<tr>
															<th class="py-3.5 px-5">Student</th>
															<th class="py-3.5 px-4 text-right">GPA</th>
															<th class="py-3.5 px-4">Assigned University (Slot)</th>
															<th class="py-3.5 px-4">Preference Rank</th>
															<th class="py-3.5 px-5 text-right">Fit Score</th>
														</tr>
													</thead>
													<tbody class="divide-y divide-[#c6c6cd]/20">
														{#each pagedMatchedRows as r (r.studentEmail)}
															<tr class="hover:bg-[#eff4ff]/50 transition">
																<td class="py-3.5 px-5">
																	<button
																		type="button"
																		onclick={() => inspectStudent(r.studentEmail)}
																		class="text-left group cursor-pointer"
																	>
																		<div class="font-semibold text-[#0b1c30] group-hover:underline">
																			{r.studentName}
																		</div>
																		<div class="text-xs font-mono-num text-[#515f74]">
																			{r.studentAucId ? `#${r.studentAucId} · ` : ''}{r.studentMajor}
																		</div>
																	</button>
																</td>
																<td class="py-3.5 px-4 text-right font-mono-num font-semibold text-[#0b1c30]">
																	{r.studentGpa.toFixed(2)}
																</td>
																<td class="py-3.5 px-4 font-medium">
																	<button
																		type="button"
																		onclick={() => inspectPartnerDossier(r.universitySlug)}
																		class="hover:underline text-left text-[#0b1c30] cursor-pointer"
																	>
																		{r.universityName}
																	</button>
																	<span
																		title={r.seatLabel}
																		class="ml-1.5 px-2 py-0.5 rounded bg-[#eff4ff] text-xs font-mono-num font-semibold text-[#515f74] whitespace-nowrap inline-block"
																	>
																		{formatShortSeatLabel(r.seatLabel)}
																	</span>
																</td>
																<td class="py-3.5 px-4">
																	<span
																		class="px-2.5 py-1 rounded-full text-xs font-semibold {r.prefRank === 1
																			? 'bg-[#f0fdf4] text-[#15803d]'
																			: r.prefRank === 0
																				? 'bg-[#fffbeb] text-[#b45309]'
																				: 'bg-[#e5eeff] text-[#0b1c30]'}"
																	>
																		{r.rankLabel}
																	</span>
																</td>
																<td class="py-3.5 px-5 text-right">
																	<div class="inline-flex items-center gap-2.5">
																		<div class="w-16 h-1.5 rounded-full bg-[#e5eeff] overflow-hidden">
																			<div
																				class="h-full rounded-full bg-[#003ea8]"
																				style="width: {Math.max(5, Math.min(100, Math.round(r.weight * 100)))}%"
																			></div>
																		</div>
																		<span class="font-mono-num font-bold text-[#0b1c30] w-12">
																			{r.weight.toFixed(3)}
																		</span>
																	</div>
																</td>
															</tr>
														{/each}
													</tbody>
												</table>
											</div>

											<div class="md:hidden p-3 space-y-2.5 bg-[#f8f9ff]">
												{#each pagedMatchedRows as r (r.studentEmail)}
													<div class="bg-white p-3.5 rounded-2xl shadow-sm border border-[#c6c6cd]/40 space-y-2.5">
														<div class="flex justify-between items-start gap-2">
															<button
																type="button"
																onclick={() => inspectStudent(r.studentEmail)}
																class="text-left min-w-0 cursor-pointer"
															>
																<div class="font-bold text-sm text-[#0b1c30] truncate">{r.studentName}</div>
																<div class="text-xs text-[#515f74] truncate">
																	{r.studentMajor}{r.studentAucId ? ` · #${r.studentAucId}` : ''}
																</div>
															</button>
															<span
																class="px-2.5 py-0.5 rounded-full font-semibold text-xs shrink-0 {r.prefRank === 1
																	? 'bg-[#f0fdf4] text-[#15803d]'
																	: r.prefRank === 0
																		? 'bg-[#fffbeb] text-[#b45309]'
																		: 'bg-[#e5eeff] text-[#0b1c30]'}"
															>
																{r.rankLabel}
															</span>
														</div>

														<button
															type="button"
															onclick={() => inspectPartnerDossier(r.universitySlug)}
															class="w-full text-left px-3 py-2 rounded-xl bg-[#eff4ff] border border-[#c6c6cd]/30 flex items-center justify-between gap-2 cursor-pointer"
														>
															<span class="text-xs font-semibold text-[#0b1c30] truncate">
																&rarr; {r.universityName}
															</span>
															<span
																title={r.seatLabel}
																class="font-mono-num text-xs font-bold px-2 py-0.5 rounded bg-[#e5eeff] text-[#003ea8] shrink-0 whitespace-nowrap"
															>
																{formatShortSeatLabel(r.seatLabel)}
															</span>
														</button>

														<div class="flex items-center justify-between gap-2 pt-0.5 text-xs">
															<div class="flex items-center gap-3 font-mono-num text-[#515f74]">
																<span>GPA <strong class="text-[#0b1c30]">{r.studentGpa.toFixed(2)}</strong></span>
																<span>Fit <strong class="text-[#0b1c30]">{r.weight.toFixed(3)}</strong></span>
															</div>
															<button
																type="button"
																onclick={() => inspectStudent(r.studentEmail)}
																class="font-semibold text-[#003ea8] hover:underline cursor-pointer"
															>
																Inspect &rsaquo;
															</button>
														</div>
													</div>
												{/each}
											</div>
										{/if}
									{/if}
								{:else if matchTab === 'unplaced'}
									<!-- Unplaced Analysis Responsive Card Grid -->
									<div class="p-3.5 sm:p-5 bg-[#f8f9ff]">
										{#if pagedUnplacedRows.length === 0}
											<div class="bg-white rounded-xl border border-[#c6c6cd]/40 p-8 text-center text-sm text-[#515f74]">
												No unplaced students match your current filters.
											</div>
										{:else}
											<div class="grid grid-cols-1 lg:grid-cols-2 gap-3.5">
												{#each pagedUnplacedRows as u (u.studentEmail)}
													<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 p-4 shadow-sm space-y-2.5">
														<div class="flex items-start justify-between gap-2">
															<button
																type="button"
																onclick={() => inspectStudent(u.studentEmail)}
																class="text-left min-w-0 cursor-pointer"
															>
																<div class="font-bold text-sm sm:text-base text-[#0b1c30] hover:underline truncate">
																	{u.studentName}
																</div>
																<div class="text-xs text-[#515f74]">
																	{u.studentMajor} ·
																	<strong class="font-mono-num {u.gpa < 2.8 ? 'text-[#ba1a1a]' : 'text-[#0b1c30]'}">
																		GPA {u.gpa.toFixed(2)}
																	</strong>
																</div>
															</button>
															<span
																class="px-2.5 py-0.5 rounded-full text-xs font-semibold shrink-0 {u.bottleneck ===
																'Capacity Contention'
																	? 'bg-[#fffbeb] text-[#b45309]'
																	: 'bg-[#fef2f2] text-[#ba1a1a]'}"
															>
																{u.bottleneck}
															</span>
														</div>
														{#if u.prefsLabel}
															<div class="px-3 py-1.5 rounded-xl bg-[#eff4ff] text-xs font-medium text-[#0b1c30]">
																Requested: {u.prefsLabel}
															</div>
														{/if}
														<p class="text-xs sm:text-sm text-[#515f74] leading-relaxed">{u.reason}</p>
													</div>
												{/each}
											</div>
										{/if}
									</div>
								{:else}
									<!-- Partner University Fill Rates Responsive Card Grid -->
									<div class="p-3.5 sm:p-5 bg-[#f8f9ff]">
										{#if pagedFillRows.length === 0}
											<div class="bg-white rounded-xl border border-[#c6c6cd]/40 p-8 text-center text-sm text-[#515f74]">
												No partner fill rates match your current filters.
											</div>
										{:else}
											<div class="grid grid-cols-1 md:grid-cols-2 gap-3.5">
												{#each pagedFillRows as u (u.slug)}
													{@const pct = u.totalSeats > 0 ? Math.min(100, Math.round((u.filledSeats / u.totalSeats) * 100)) : 0}
													<div class="bg-white rounded-2xl border border-[#c6c6cd]/40 p-4 shadow-sm space-y-3">
														<div class="flex items-start justify-between gap-2">
															<div class="min-w-0">
																<button
																	type="button"
																	onclick={() => inspectPartnerDossier(u.slug)}
																	class="font-bold text-sm sm:text-base text-[#0b1c30] hover:underline text-left truncate block max-w-full cursor-pointer"
																>
																	{u.name} ↗
																</button>
																<div class="text-xs text-[#515f74]">{u.country} · {u.region}</div>
															</div>
															<span
																class="px-2.5 py-0.5 rounded-full text-xs font-mono-num font-bold shrink-0 {u.demandRatio >= 3
																	? 'bg-[#fef2f2] text-[#ba1a1a]'
																	: 'bg-[#e5eeff] text-[#003ea8]'}"
															>
																{u.demandRatio.toFixed(1)}x demand
															</span>
														</div>

														<div class="space-y-1.5">
															<div class="flex justify-between text-xs font-mono-num text-[#515f74]">
																<span>
																	Seats Filled: <strong class="text-[#0b1c30]">{u.filledSeats} / {u.totalSeats}</strong> ({pct}%)
																</span>
																<span>
																	Cutoff: <strong class="text-[#0b1c30]">{u.cutoffScore !== null ? u.cutoffScore.toFixed(3) : '—'}</strong>
																</span>
															</div>
															<div class="h-2 rounded-full bg-[#e5eeff] overflow-hidden">
																<div
																	class="h-full rounded-full {u.leftSeats === 0 && u.totalSeats > 0
																		? 'bg-[#15803d]'
																		: 'bg-[#003ea8]'}"
																	style="width: {pct}%"
																></div>
															</div>
														</div>

														<div class="flex items-center justify-between pt-2 border-t border-[#c6c6cd]/25 text-xs">
															<span class="text-[#515f74] font-mono-num">{u.demandCount} student preferences</span>
															{#if u.filledSeats > 0}
																<button
																	type="button"
																	onclick={() => jumpToPartnerMatched(u.slug, true)}
																	class="font-semibold text-[#003ea8] hover:underline whitespace-nowrap cursor-pointer"
																>
																	View {u.filledSeats} Placed &rsaquo;
																</button>
															{:else}
																<span class="text-[#515f74]">0 allocated</span>
															{/if}
														</div>
													</div>
												{/each}
											</div>
										{/if}
									</div>
								{/if}

								<!-- Match Results Pagination -->
								<div class="px-4 py-3.5 bg-[#eff4ff] border-t border-[#c6c6cd]/30 flex flex-wrap items-center justify-between gap-3">
									<span class="text-xs text-[#515f74] font-mono-num">
										{#if matchActiveTotal === 0}
											0 records in view
										{:else}
											Showing <strong class="text-[#0b1c30]">
												{(clampedMatchPage - 1) * matchCurrentPageSize + 1}–{Math.min(
													matchActiveTotal,
													clampedMatchPage * matchCurrentPageSize
												)}
											</strong> of <strong class="text-[#0b1c30]">{matchActiveTotal}</strong>
										{/if}
									</span>
									<div class="flex items-center gap-2">
										<button
											type="button"
											disabled={clampedMatchPage <= 1}
											onclick={() => (matchPage = Math.max(1, clampedMatchPage - 1))}
											class="h-8 px-3 bg-white text-[#0b1c30] rounded-lg border border-[#c6c6cd]/40 text-xs font-semibold cursor-pointer disabled:opacity-50"
										>
											Previous
										</button>
										<span class="px-2.5 h-8 rounded-lg bg-[#131b2e] text-white font-semibold text-xs flex items-center font-mono-num">
											{clampedMatchPage} / {matchTotalPages}
										</span>
										<button
											type="button"
											disabled={clampedMatchPage >= matchTotalPages}
											onclick={() => (matchPage = Math.min(matchTotalPages, clampedMatchPage + 1))}
											class="h-8 px-3 bg-white text-[#0b1c30] rounded-lg border border-[#c6c6cd]/40 text-xs font-semibold cursor-pointer disabled:opacity-50"
										>
											Next
										</button>
									</div>
								</div>
							</div>
						</div>
					{/if}
				</section>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 3: PARTNER UNIVERSITIES & SEAT QUOTAS (155 PARTNERS) -->
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
								<span class="hidden sm:inline text-[#0b1c30] font-semibold">
									Partner Universities &amp; Seat Quotas
								</span>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Partner Universities &amp; Seat Quotas
							</h1>
						</div>
						<button
							type="button"
							onclick={exportPartnersCsv}
							class="h-10 sm:h-11 px-4 sm:px-5 bg-white hover:bg-[#eff4ff] text-[#0b1c30] border border-[#c6c6cd]/50 text-xs sm:text-sm font-semibold rounded-xl shadow-sm inline-flex items-center gap-2 cursor-pointer shrink-0 self-start sm:self-auto"
						>
							<svg class="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75">
								<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
								<polyline points="7 10 12 15 17 10" />
								<line x1="12" y1="15" x2="12" y2="3" />
							</svg>
							<span>Export Partners CSV</span>
						</button>
					</div>

					<!-- Partner KPI Strip -->
					<div class="grid grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
							<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Partner Catalog</div>
							<div class="mt-2 flex items-baseline gap-2">
								<span class="text-2xl sm:text-3xl font-bold text-[#0b1c30] tnum">{partnerStats.totalPartners}</span>
								<span class="text-xs text-[#15803d] font-medium">{partnerStats.activePartners} active</span>
							</div>
							<div class="mt-1.5 text-xs text-[#515f74]">
								{partnerStats.checkSlotsPartners} check slots · {partnerStats.loiPartners} LOI
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
							<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Total Seats</div>
							<div class="mt-2 flex items-baseline gap-2">
								<span class="text-2xl sm:text-3xl font-bold text-[#0b1c30] tnum">
									{partnerStats.totalConfiguredSeats}
								</span>
								<span class="text-xs text-[#515f74]">{partnerStats.customQuotaCount} confirmed</span>
							</div>
							<div class="mt-1.5 text-xs text-[#515f74]">
								{partnerStats.totalPartners - partnerStats.customQuotaCount} using default (2)
							</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
							<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Contested Partners</div>
							<div class="mt-2 flex items-baseline gap-2">
								<span class="text-2xl sm:text-3xl font-bold text-[#0b1c30] tnum">{partnerStats.highDemandCount}</span>
								<span class="text-xs text-[#b45309] font-medium">&ge; 2x demand</span>
							</div>
							<div class="mt-1.5 text-xs text-[#515f74]">Across current applicant choices</div>
						</div>

						<div class="bg-white p-4 sm:p-5 rounded-xl shadow-sm border border-[#c6c6cd]/40 min-w-0">
							<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Verified Fact Sheets</div>
							<div class="mt-2 flex items-baseline gap-2">
								<span class="text-2xl sm:text-3xl font-bold text-[#0b1c30] tnum">
									{partnerStats.factSheetMatchedCount}
								</span>
								<span class="text-xs text-[#515f74]">/ {partnerStats.totalPartners}</span>
							</div>
							<div class="mt-1.5 text-xs text-[#515f74]">
								{partnerStats.knownMinGpaCount} specify host min GPA
							</div>
						</div>
					</div>

					<!-- Filters + Partner Table -->
					<div class="bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
						<div class="p-4 sm:p-5 bg-[#eff4ff] border-b border-[#c6c6cd]/30 space-y-3.5">
							<div class="flex flex-wrap items-center justify-between gap-2">
								<span class="text-xs sm:text-sm font-medium text-[#515f74]">
									Edit seat quotas directly below — changes persist to SQLite and immediately update Hopcroft–Karp capacity.
								</span>
								<button
									type="button"
									onclick={resetPartnerFilters}
									class="text-xs sm:text-sm font-medium text-[#515f74] hover:text-[#0b1c30] underline cursor-pointer"
								>
									Reset Filters
								</button>
							</div>

							<div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-6 gap-3">
								<div class="lg:col-span-2 relative">
									<input
										type="search"
										bind:value={partnerSearch}
										oninput={() => (partnerPage = 1)}
										placeholder="Search university, short code, city, country..."
										class="w-full h-10 pl-3.5 pr-3 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm focus:outline-none focus:border-black"
									/>
								</div>

								<select
									bind:value={partnerRegion}
									onchange={() => (partnerPage = 1)}
									aria-label="Filter by region"
									class="stitch-select h-10 px-3 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm"
								>
									<option value="all">All Regions</option>
									{#each partnerRegions as reg (reg)}
										<option value={reg}>{reg}</option>
									{/each}
								</select>

								<select
									bind:value={partnerStatus}
									onchange={() => (partnerPage = 1)}
									aria-label="Filter by agreement status"
									class="stitch-select h-10 px-3 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm"
								>
									<option value="all">All Agreement Statuses</option>
									<option value="active">Active Partner</option>
									<option value="check_slots">Check Slots</option>
									<option value="letter_of_intent">Letter of Intent</option>
								</select>

								<select
									bind:value={partnerTerm}
									onchange={() => (partnerPage = 1)}
									aria-label="Filter by term"
									class="stitch-select h-10 px-3 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm"
								>
									<option value="all">All Terms</option>
									<option value="Fall">Fall Term (incl. AY)</option>
									<option value="Spring">Spring Term (incl. AY)</option>
									<option value="Summer">Summer</option>
								</select>

								<select
									bind:value={partnerQuotaState}
									onchange={() => (partnerPage = 1)}
									aria-label="Filter by quota status"
									class="stitch-select h-10 px-3 bg-white text-[#0b1c30] text-sm rounded-xl border border-[#c6c6cd]/40 shadow-sm"
								>
									<option value="all">All Quotas &amp; Demand</option>
									<option value="custom">Confirmed Quota Only</option>
									<option value="default">Default Assumed Only</option>
									<option value="demanded">With Applicant Demand</option>
								</select>
							</div>
						</div>

						<!-- Desktop Partners Table -->
						<div class="hidden md:block overflow-x-auto">
							<table class="w-full text-left border-collapse">
								<thead class="bg-[#eff4ff] text-[#515f74] text-xs font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
									<tr>
										<th class="py-3.5 px-5">Partner &amp; Location</th>
										<th class="py-3.5 px-4">Agreement</th>
										<th class="py-3.5 px-4">Eligibility &amp; Terms</th>
										<th class="py-3.5 px-4 text-right">Est. Living</th>
										<th class="py-3.5 px-4 text-center">Seat Quota</th>
										<th class="py-3.5 px-4 text-right">Applicant Demand</th>
										<th class="py-3.5 px-5 text-right">Dossier</th>
									</tr>
								</thead>
								<tbody class="divide-y divide-[#c6c6cd]/20 text-sm">
									{#each pagedPartners as u (u.slug)}
										{@const badge = partnerStatusBadge(u.agreementStatus)}
										<tr class="hover:bg-[#eff4ff]/50 transition-colors">
											<td class="py-3.5 px-5 max-w-[270px]">
												<div class="flex items-center gap-2">
													<span class="font-semibold text-[#0b1c30] truncate" title={u.officialName}>
														{u.officialName}
													</span>
													{#if u.shortName}
														<span class="px-1.5 py-0.5 rounded bg-[#e5eeff] text-[#0b1c30] text-[10px] font-bold shrink-0">
															{u.shortName}
														</span>
													{/if}
												</div>
												<div class="text-xs text-[#515f74] mt-0.5 truncate">
													{u.city}, {u.country} · {u.region}
												</div>
											</td>

											<td class="py-3.5 px-4">
												<div class="flex flex-col items-start gap-1">
													<span class="px-2.5 py-0.5 rounded-full text-[11px] font-semibold {badge.cls}">
														{badge.label}
													</span>
													<span class="text-xs text-[#515f74] font-mono-num">
														{u.studentLevels.length ? u.studentLevels.join(' · ') : 'UG'}
													</span>
												</div>
											</td>

											<td class="py-3.5 px-4 max-w-[260px]">
												<div class="flex items-center gap-1.5 flex-wrap">
													<span
														class="px-2 py-0.5 rounded-full text-[11px] font-mono-num font-semibold {u.minGpa !==
														null
															? 'bg-[#e5eeff] text-[#0b1c30]'
															: 'bg-[#eff4ff] text-[#515f74]'}"
													>
														GPA {u.minGpa !== null ? u.minGpa.toFixed(2) : '2.80*'}
													</span>
													{#each u.availableTerms.slice(0, 2) as term (term)}
														<span class="px-2 py-0.5 rounded-full bg-[#eff4ff] text-[#515f74] text-[11px]">
															{term}
														</span>
													{/each}
												</div>
												<div class="text-xs text-[#515f74] truncate mt-1">
													Majors: {u.eligibleMajors.length ? u.eligibleMajors.join(', ') : 'All / Open'}
												</div>
											</td>

											<td class="py-3.5 px-4 text-right">
												<div class="font-mono-num font-semibold text-[#0b1c30]">
													{u.livingCostUsd !== null
														? `$${Math.round(u.livingCostUsd).toLocaleString()}`
														: '—'}
												</div>
												<div class="text-[11px] text-[#515f74] capitalize">
													{u.housingAvailability || 'Housing TBD'}
												</div>
											</td>

											<td class="py-3.5 px-4 text-center">
												<div class="inline-flex flex-col items-center gap-1">
													<div class="inline-flex items-center border border-[#c6c6cd]/40 rounded-xl bg-white shadow-sm">
														<button
															type="button"
															disabled={savingSeatSlug === u.slug || u.seats <= 0}
															onclick={() => updatePartnerSeatQuota(u.slug, u.seats - 1)}
															class="w-8 h-8 text-[#515f74] hover:text-[#0b1c30] font-semibold cursor-pointer disabled:opacity-40"
															aria-label={`Decrease seats for ${u.officialName}`}
														>
															−
														</button>
														<span class="w-9 text-center font-mono-num font-bold text-sm">
															{u.seats}
														</span>
														<button
															type="button"
															disabled={savingSeatSlug === u.slug}
															onclick={() => updatePartnerSeatQuota(u.slug, u.seats + 1)}
															class="w-8 h-8 text-[#515f74] hover:text-[#0b1c30] font-semibold cursor-pointer disabled:opacity-40"
															aria-label={`Increase seats for ${u.officialName}`}
														>
															+
														</button>
													</div>
													<span class="text-[10px] font-medium {u.seatsAssumed ? 'text-[#515f74]' : 'text-[#15803d]'}">
														{u.seatsAssumed ? 'Default' : 'Confirmed'}
													</span>
												</div>
											</td>

											<td class="py-3.5 px-4 text-right">
												<div class="font-mono-num font-semibold text-[#0b1c30]">
													{u.firstChoiceDemand} 1st choice
												</div>
												<div class="text-xs text-[#515f74] font-mono-num">
													{u.totalDemand} total prefs
												</div>
											</td>

											<td class="py-3.5 px-5 text-right">
												<button
													type="button"
													onclick={() => inspectPartnerDossier(u.slug)}
													class="h-8 px-3 rounded-lg bg-[#eff4ff] hover:bg-[#dce9ff] text-xs font-semibold text-[#003ea8] cursor-pointer"
												>
													Dossier
												</button>
											</td>
										</tr>
									{:else}
										<tr>
											<td colspan="7" class="py-10 text-center text-sm text-[#515f74]">
												No partner universities match the current filters.
											</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>

						<!-- Mobile Partner Cards -->
						<div class="md:hidden p-3 space-y-3 bg-[#f8f9ff]">
							{#each pagedPartners as u (u.slug)}
								<div class="bg-white p-4 rounded-xl shadow-sm border border-[#c6c6cd]/40 space-y-3 text-xs sm:text-sm">
									<div class="flex items-start justify-between gap-2">
										<div class="min-w-0">
											<div class="font-semibold text-[15px] text-[#0b1c30]">{u.officialName}</div>
											<div class="text-[#515f74] mt-0.5">
												{u.city}, {u.country} · Min GPA {u.minGpa !== null ? u.minGpa.toFixed(2) : '2.80*'}
											</div>
										</div>
										<button
											type="button"
											onclick={() => inspectPartnerDossier(u.slug)}
											class="h-8 px-3 rounded-lg bg-[#eff4ff] text-xs font-semibold text-[#003ea8] shrink-0 cursor-pointer"
										>
											Dossier
										</button>
									</div>

									<div class="flex items-center justify-between pt-2 border-t border-[#c6c6cd]/20">
										<span class="text-[#515f74]">
											<strong class="text-[#0b1c30]">{u.firstChoiceDemand}</strong> 1st choice ·
											{u.totalDemand} prefs
										</span>
										<div class="inline-flex items-center border border-[#c6c6cd]/40 rounded-xl bg-[#f8f9ff] shrink-0">
											<button
												type="button"
												disabled={savingSeatSlug === u.slug || u.seats <= 0}
												onclick={() => updatePartnerSeatQuota(u.slug, u.seats - 1)}
												class="w-9 h-9 cursor-pointer font-semibold text-base disabled:opacity-40"
												aria-label={`Decrease seats for ${u.officialName}`}
											>
												−
											</button>
											<span class="w-8 text-center font-mono-num font-bold text-sm">{u.seats}</span>
											<button
												type="button"
												disabled={savingSeatSlug === u.slug}
												onclick={() => updatePartnerSeatQuota(u.slug, u.seats + 1)}
												class="w-9 h-9 cursor-pointer font-semibold text-base disabled:opacity-40"
												aria-label={`Increase seats for ${u.officialName}`}
											>
												+
											</button>
										</div>
									</div>
								</div>
							{/each}
						</div>

						<!-- Partners Pagination Footer -->
						<div class="p-4 bg-[#eff4ff] flex flex-col sm:flex-row items-center justify-between gap-3 border-t border-[#c6c6cd]/30">
							<div class="flex items-center gap-3 flex-wrap">
								<span class="text-xs sm:text-sm text-[#515f74]">
									Showing
									<strong class="text-[#0b1c30]">
										{filteredPartners.length > 0
											? `${(clampedPartnerPage - 1) * partnerRowsPerPage + 1}–${Math.min(
													clampedPartnerPage * partnerRowsPerPage,
													filteredPartners.length
												)}`
											: '0'}
									</strong>
									of <strong class="text-[#0b1c30]">{filteredPartners.length}</strong> partners
								</span>
								<select
									bind:value={partnerRowsPerPage}
									onchange={() => (partnerPage = 1)}
									aria-label="Partners per page"
									class="stitch-select h-9 pl-2.5 pr-6 bg-white text-[#0b1c30] text-xs font-medium rounded-lg border border-[#c6c6cd]/40"
								>
									<option value={25}>25</option>
									<option value={50}>50</option>
									<option value={100}>100</option>
								</select>
							</div>

							<div class="flex items-center gap-2">
								<button
									type="button"
									disabled={clampedPartnerPage <= 1}
									onclick={() => (partnerPage = Math.max(1, clampedPartnerPage - 1))}
									class="h-9 px-3.5 bg-white text-[#0b1c30] rounded-lg border border-[#c6c6cd]/40 text-xs font-semibold cursor-pointer disabled:opacity-50"
								>
									Previous
								</button>
								<span class="px-2.5 h-9 rounded-lg bg-[#131b2e] text-white font-semibold text-xs flex items-center font-mono-num">
									{clampedPartnerPage} / {partnerTotalPages}
								</span>
								<button
									type="button"
									disabled={clampedPartnerPage >= partnerTotalPages}
									onclick={() => (partnerPage = Math.min(partnerTotalPages, clampedPartnerPage + 1))}
									class="h-9 px-3.5 bg-white text-[#0b1c30] rounded-lg border border-[#c6c6cd]/40 text-xs font-semibold cursor-pointer disabled:opacity-50"
								>
									Next
								</button>
							</div>
						</div>
					</div>
				</section>
			{/if}

			<!-- ========================================================= -->
			<!-- VIEW 4: SAVED RUNS & AUDIT HISTORY                        -->
			<!-- ========================================================= -->
			{#if activePage === 'runs'}
				<section class="space-y-5 md:space-y-6">
					<div class="flex flex-col sm:flex-row sm:items-end justify-between gap-3.5">
						<div class="flex flex-col gap-1.5">
							<div class="flex items-center gap-1.5 text-xs font-medium text-[#515f74]">
								<span>AUC IPSO</span>
								<span class="text-[#c6c6cd]">&rsaquo;</span>
								<span>Governance</span>
								<span class="text-[#c6c6cd] hidden sm:inline">&rsaquo;</span>
								<span class="hidden sm:inline text-[#0b1c30] font-semibold">
									Saved Runs &amp; Audit History
								</span>
							</div>
							<h1 class="text-2xl sm:text-3xl md:text-[32px] font-bold tracking-tight text-[#0b1c30] leading-tight">
								Saved Matching Runs
							</h1>
						</div>

						{#if savedRuns.length >= 2}
							<button
								type="button"
								onclick={() => openDiffComparison(savedRuns[0].id, savedRuns[1].id)}
								class="h-10 sm:h-11 px-4 sm:px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs sm:text-sm font-semibold rounded-xl shadow-sm cursor-pointer shrink-0 self-start sm:self-auto"
							>
								Compare Runs Side-by-Side
							</button>
						{/if}
					</div>

					{#if savedRuns.length === 0}
						<div class="bg-white p-8 rounded-xl border border-[#c6c6cd]/40 shadow-sm text-center space-y-2.5">
							<h2 class="text-lg font-semibold text-[#0b1c30]">No Saved Runs Yet</h2>
							<p class="text-xs sm:text-sm text-[#515f74]">
								Run a simulation in the Matching Console and click <strong>Save Draft Run</strong> to snapshot allocations and compare weight strategies in SQLite.
							</p>
							<button
								type="button"
								onclick={() => navigate('matching')}
								class="h-10 px-4 bg-[#131b2e] text-white text-xs sm:text-sm font-semibold rounded-xl cursor-pointer mt-2"
							>
								Go to Matching Console
							</button>
						</div>
					{:else}
						<!-- Desktop Table -->
						<div class="hidden md:block bg-white rounded-xl shadow-sm border border-[#c6c6cd]/40 overflow-hidden">
							<div class="overflow-x-auto">
								<table class="w-full text-left border-collapse text-sm sm:text-[15px]">
									<thead class="bg-[#eff4ff] text-[#515f74] text-xs sm:text-[13px] font-semibold uppercase tracking-wider border-b border-[#c6c6cd]/30">
										<tr>
											<th class="py-4 px-5 whitespace-nowrap">Run ID</th>
											<th class="py-4 px-4">Configuration Notes</th>
											<th class="py-4 px-4">Weight Profile</th>
											<th class="py-4 px-4 text-right whitespace-nowrap">Placed / HK Max</th>
											<th class="py-4 px-4 text-right whitespace-nowrap">1st Choice %</th>
											<th class="py-4 px-4 whitespace-nowrap">Status</th>
											<th class="py-4 px-5 text-right whitespace-nowrap">Actions</th>
										</tr>
									</thead>
									<tbody class="divide-y divide-[#c6c6cd]/20">
										{#each savedRuns as run (run.id)}
											{@const isLoadedRun = loadedRunSummary?.id === run.id}
											<tr class="hover:bg-[#eff4ff]/50 transition-colors {isLoadedRun ? 'bg-[#eff4ff]/50 border-l-2 border-l-[#003ea8]' : ''}">
												<td class="py-4 px-5 font-mono-num font-semibold whitespace-nowrap align-top">
													<div>{run.runCode}</div>
													{#if isLoadedRun}
														<span class="inline-block mt-1 px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#003ea8] text-xs font-sans font-semibold whitespace-nowrap">
															Loaded
														</span>
													{/if}
												</td>
												<td class="py-4 px-4">
													<span class="font-semibold text-[#0b1c30]">{run.title}</span>
													<span class="block text-xs text-[#515f74] mt-0.5">
														{run.cycle} · {run.createdBy} · {run.createdAt}
													</span>
												</td>
												<td class="py-4 px-4 font-mono-num text-xs sm:text-sm text-[#515f74] whitespace-nowrap">
													Pref={run.weights.pref.toFixed(2)} · GPA={run.weights.gpa.toFixed(2)}<br />
													Stmt={run.weights.llm.toFixed(2)} · Eq={run.weights.equity.toFixed(2)}
												</td>
												<td class="py-4 px-4 text-right font-mono-num font-semibold whitespace-nowrap">
													{run.placed} / {run.hkMax}
												</td>
												<td class="py-4 px-4 text-right font-mono-num font-semibold whitespace-nowrap {run.firstChoicePct >= 50 ? 'text-[#15803d]' : ''}">
													{run.firstChoicePct.toFixed(1)}%
												</td>
												<td class="py-4 px-4 whitespace-nowrap">
													<span
														class="px-2.5 py-1 rounded-full text-xs font-semibold whitespace-nowrap inline-block {run.status ===
														'Published'
															? 'bg-[#f0fdf4] text-[#15803d]'
															: run.status === 'Draft'
																? 'bg-[#e5eeff] text-[#0b1c30]'
																: 'bg-[#eff4ff] text-[#515f74]'}"
													>
														{run.status}
													</span>
												</td>
												<td class="py-4 px-5 text-right whitespace-nowrap">
													{#if confirmDeleteRunId === run.id}
														<div class="inline-flex items-center justify-end gap-2 whitespace-nowrap">
															<span class="text-xs font-medium text-[#ba1a1a]">Delete {run.runCode}?</span>
															<button
																type="button"
																disabled={deletingRunId === run.id}
																onclick={() => handleDeleteRun(run)}
																class="h-8 px-3 rounded-lg bg-[#ba1a1a] text-white text-xs font-semibold inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
															>
																{deletingRunId === run.id ? 'Deleting…' : 'Confirm'}
															</button>
															<button
																type="button"
																disabled={deletingRunId === run.id}
																onclick={() => (confirmDeleteRunId = null)}
																class="h-8 px-3 rounded-lg bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#0b1c30] inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer"
															>
																Cancel
															</button>
														</div>
													{:else}
														<div class="inline-flex items-center justify-end gap-1.5 whitespace-nowrap">
															<button
																type="button"
																onclick={() =>
																	loadSavedRun(run, {
																		label: 'Saved Runs',
																		kind: 'page',
																		page: 'runs'
																	})}
																class="h-8 px-3 rounded-lg bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs font-semibold inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer shadow-sm"
															>
																{isLoadedRun ? 'Open Loaded' : 'Load Run'}
															</button>
															<button
																type="button"
																disabled={exportingRunId === run.id}
																onclick={() => exportSavedRunCsv(run)}
																class="h-8 px-2.5 rounded-lg bg-[#eff4ff] hover:bg-[#dce9ff] border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8] inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
															>
																{exportingRunId === run.id ? '…' : 'CSV'}
															</button>
															{#if savedRuns.length >= 2}
																<button
																	type="button"
																	onclick={() => openDiffComparison(run.id)}
																	class="h-8 px-2.5 rounded-lg bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#515f74] hover:text-[#0b1c30] inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer"
																>
																	Compare
																</button>
															{/if}
															{#if run.status !== 'Published'}
																<button
																	type="button"
																	onclick={() => publishCurrentOrSelectedRun(run.id)}
																	class="h-8 px-2.5 rounded-lg bg-[#f0fdf4] text-xs font-semibold text-[#15803d] inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer"
																>
																	Publish
																</button>
															{/if}
															<button
																type="button"
																onclick={() => (confirmDeleteRunId = run.id)}
																class="h-8 px-2.5 rounded-lg bg-[#fef2f2] hover:bg-[#fee2e2] text-xs font-semibold text-[#ba1a1a] inline-flex items-center justify-center whitespace-nowrap shrink-0 cursor-pointer"
															>
																Delete
															</button>
														</div>
													{/if}
												</td>
											</tr>
										{/each}
									</tbody>
								</table>
							</div>
						</div>

						<!-- Mobile Run Cards -->
						<div class="md:hidden space-y-3 text-xs sm:text-sm">
							{#each savedRuns as run (run.id)}
								{@const isLoadedRun = loadedRunSummary?.id === run.id}
								<div class="bg-white p-4 rounded-2xl shadow-sm border border-[#c6c6cd]/40 space-y-2.5 {isLoadedRun ? 'ring-2 ring-[#003ea8]/40' : ''}">
									<div class="flex justify-between items-center gap-2 flex-wrap">
										<div class="flex items-center gap-2">
											<span class="font-mono-num font-bold text-[15px] whitespace-nowrap">{run.runCode}</span>
											{#if isLoadedRun}
												<span class="px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#003ea8] text-xs font-semibold whitespace-nowrap">
													Loaded in Console
												</span>
											{/if}
										</div>
										<span
											class="px-2.5 py-0.5 rounded-full font-semibold text-xs whitespace-nowrap {run.status === 'Published'
												? 'bg-[#f0fdf4] text-[#15803d]'
												: run.status === 'Draft'
													? 'bg-[#e5eeff] text-[#0b1c30]'
													: 'bg-[#eff4ff] text-[#515f74]'}"
										>
											{run.status}
										</span>
									</div>
									<div class="font-semibold text-[15px] text-[#0b1c30]">{run.title}</div>
									<div class="font-mono-num text-xs text-[#515f74]">
										Pref={run.weights.pref.toFixed(2)} · GPA={run.weights.gpa.toFixed(2)} · Stmt={run.weights.llm.toFixed(2)} · Eq={run.weights.equity.toFixed(2)}
									</div>
									<div class="flex flex-col gap-2.5 text-[#515f74] pt-2.5 border-t border-[#c6c6cd]/20">
										<span>
											Placed: <strong class="text-[#0b1c30] font-mono-num">{run.placed}/{run.hkMax}</strong> · 1st:
											<strong class="text-[#15803d] font-mono-num">{run.firstChoicePct.toFixed(1)}%</strong>
										</span>
										{#if confirmDeleteRunId === run.id}
											<div class="flex flex-wrap items-center gap-2 pt-1">
												<span class="text-xs font-semibold text-[#ba1a1a]">Delete {run.runCode}?</span>
												<button
													type="button"
													disabled={deletingRunId === run.id}
													onclick={() => handleDeleteRun(run)}
													class="h-9 px-3 rounded-xl bg-[#ba1a1a] text-white text-xs font-semibold whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
												>
													{deletingRunId === run.id ? 'Deleting…' : 'Confirm Delete'}
												</button>
												<button
													type="button"
													disabled={deletingRunId === run.id}
													onclick={() => (confirmDeleteRunId = null)}
													class="h-9 px-3 rounded-xl bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#0b1c30] whitespace-nowrap shrink-0 cursor-pointer"
												>
													Cancel
												</button>
											</div>
										{:else}
											<div class="flex flex-wrap gap-2">
												<button
													type="button"
													onclick={() =>
														loadSavedRun(run, {
															label: 'Saved Runs',
															kind: 'page',
															page: 'runs'
														})}
													class="h-9 px-3.5 rounded-xl bg-[#131b2e] text-white text-xs font-semibold whitespace-nowrap shrink-0 cursor-pointer shadow-sm"
												>
													{isLoadedRun ? 'Open Loaded Run' : 'Load Run'}
												</button>
												<button
													type="button"
													disabled={exportingRunId === run.id}
													onclick={() => exportSavedRunCsv(run)}
													class="h-9 px-3 rounded-xl bg-[#eff4ff] border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8] whitespace-nowrap shrink-0 cursor-pointer disabled:opacity-50"
												>
													{exportingRunId === run.id ? 'Exporting…' : 'Export CSV'}
												</button>
												{#if savedRuns.length >= 2}
													<button
														type="button"
														onclick={() => openDiffComparison(run.id)}
														class="h-9 px-3 rounded-xl bg-white border border-[#c6c6cd]/40 text-xs font-semibold text-[#0b1c30] whitespace-nowrap shrink-0 cursor-pointer"
													>
														Compare
													</button>
												{/if}
												{#if run.status !== 'Published'}
													<button
														type="button"
														onclick={() => publishCurrentOrSelectedRun(run.id)}
														class="h-9 px-3 rounded-xl bg-[#f0fdf4] text-xs font-semibold text-[#15803d] whitespace-nowrap shrink-0 cursor-pointer"
													>
														Publish
													</button>
												{/if}
												<button
													type="button"
													onclick={() => (confirmDeleteRunId = run.id)}
													class="h-9 px-3 rounded-xl bg-[#fef2f2] text-xs font-semibold text-[#ba1a1a] whitespace-nowrap shrink-0 cursor-pointer"
												>
													Delete
												</button>
											</div>
										{/if}
									</div>
								</div>
							{/each}
						</div>
					{/if}
				</section>
			{/if}
		</main>
	</div>

	<!-- MOBILE STICKY BOTTOM NAV -->
	<nav
		class="md:hidden fixed bottom-0 left-0 right-0 h-15 bg-white border-t border-[#c6c6cd]/70 flex items-center justify-around px-2 z-30 shadow-lg"
	>
		{#each MOBILE_TABS as tab (tab.id)}
			{@const active = activePage === tab.id}
			<button
				type="button"
				onclick={() => navigate(tab.id, true)}
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

	<!-- CENTERED STUDENT AUDIT REVIEW MODAL -->
	<StudentAuditDrawer
		student={inspectedStudent}
		loading={loadingStudent}
		saving={savingReview}
		placement={inspectedStudentPlacement}
		backLabel={modalReturnPoint?.label ?? null}
		onBack={goBackOneStep}
		onClose={() => {
			inspectedStudent = null;
			loadingStudent = false;
			clearModalReturns();
		}}
		onInspectPartner={(slug) => {
			if (inspectedStudent) {
				pushReturnPoint({
					label: inspectedStudent.name,
					kind: 'modal',
					page: activePage,
					matchTab,
					matchedLayout,
					matchPartnerSlug,
					matchPage,
					studentEmail: inspectedStudent.email
				});
			}
			inspectedStudent = null;
			loadingStudent = false;
			inspectPartnerDossier(slug);
		}}
		onSaveReview={handleSaveReview}
	/>

	<!-- CENTERED PARTNER UNIVERSITY DOSSIER MODAL -->
	<PartnerDossierDrawer
		partner={inspectedPartner}
		loading={loadingPartner}
		savingSeats={savingSeatSlug !== null}
		placedCount={inspectedPartnerPlacedCount}
		backLabel={modalReturnPoint?.label ?? null}
		onBack={goBackOneStep}
		onClose={() => {
			inspectedPartner = null;
			loadingPartner = false;
			clearModalReturns();
		}}
		onAdjustSeats={updatePartnerSeatQuota}
		onViewPlacedStudents={simulation && simulation.mode !== 'shortlists'
			? (slug) => {
					if (inspectedPartner) {
						pushReturnPoint({
							label: `${inspectedPartner.shortName || inspectedPartner.officialName} Dossier`,
							kind: 'page',
							page: activePage,
							matchTab,
							matchedLayout,
							matchPartnerSlug,
							matchPage,
							partnerSlug: inspectedPartner.slug
						});
					}
					inspectedPartner = null;
					loadingPartner = false;
					jumpToPartnerMatched(slug, false);
				}
			: undefined}
	/>

	<!-- SAVED RUN DIFF COMPARISON MODAL -->
	<RunDiffModal
		open={diffModalOpen}
		runs={savedRuns}
		selectedRunA={diffRunA}
		selectedRunB={diffRunB}
		diff={diffData}
		loading={loadingDiff}
		onSelectRuns={fetchDiff}
		onLoadRun={(run) =>
			loadSavedRun(run, {
				label: 'Run Comparison',
				kind: 'page',
				page: activePage,
				diffPair: { a: diffRunA, b: diffRunB }
			})}
		onClose={() => (diffModalOpen = false)}
	/>

	<!-- BIPARTITE GRAPH MATCHING VISUALIZATION -->
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
