<script lang="ts">
	// Course Equivalencies & Draft Course Planner.
	// Searches AUC Banner's live Transfer Equivalency tool via /api/transfer-equivalencies,
	// persists per-university Draft Course Lists in C++ + SQLite (/api/course-plans),
	// and exports/attaches the official AUC Draft List of Courses PDF directly to /apply Step 6.
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import PageHeader from '#lib/components/student/PageHeader.svelte';
	import Icon from '#lib/components/student/Icon.svelte';
	import PartnerSelect from '#lib/components/student/PartnerSelect.svelte';
	import { catalog } from '#lib/explore/catalog.svelte.ts';
	import type { UniversityRecord } from '#lib/explore/universities.ts';
	import { callServer, errorText } from '#lib/student/api.ts';
	import {
		buildCoursePlanFilename,
		buildCoursePlanPdf,
		type CoursePlanPdfSection
	} from '#lib/student/coursePlanPdf.ts';
	import {
		coursePlans,
		type PlannedCourse
	} from '#lib/student/coursePlans.svelte.ts';
	import { discovery } from '#lib/student/discovery.svelte.ts';
	import { store } from '#lib/student/store.svelte.ts';

	interface BannerInstitution {
		code: string;
		name: string;
	}

	interface TransferEquivalencyItem {
		id: string;
		instCode: string;
		effectiveTermCode: string;
		effectiveTermLabel: string;
		hostCourseCode: string;
		hostCourseTitle: string;
		rawCourseGroup: string;
		aucEquivalent: string;
		aucDepartments: string[];
		hasEquivalent: boolean;
	}

	interface InstitutionsResponse {
		source: string;
		bannerUrl: string;
		query: string;
		count: number;
		institutions: BannerInstitution[];
	}

	type SearchByMode = 'auc' | 'host' | 'all';

	interface CoursesResponse {
		source: string;
		cached: boolean;
		bannerUrl: string;
		instCode: string;
		searchBy: SearchByMode;
		totalCatalogCount: number;
		totalMatchedCount: number;
		count: number;
		hasMore: boolean;
		departments: string[];
		courses: TransferEquivalencyItem[];
	}

	const BANNER_URL =
		'https://generalssb-prod.ec.aucegypt.edu/BannerExtensibility/customPage/page/TransferEquivalence';
	const MAX_COURSES = 15;
	const PAGE_SIZE = 100;

	// ---- Host university selection ----
	let selectedSlug = $state('');
	let studyTerm = $state('Fall 2027');

	// ---- Banner institution matching + live equivalencies ----
	let bannerInstQuery = $state('');
	let bannerInstitutions = $state<BannerInstitution[]>([]);
	let bannerInstLoading = $state(false);
	let bannerInstError = $state('');
	let selectedBannerCode = $state('');
	let selectedBannerName = $state('');
	let showInstPicker = $state(false);

	let courseQuery = $state('');
	let searchBy = $state<SearchByMode>('auc');
	let deptFilter = $state('ALL');
	let deptOpen = $state(false);
	let deptSearch = $state('');
	let deptRootEl = $state<HTMLDivElement | null>(null);
	let deptSearchEl = $state<HTMLInputElement | null>(null);
	let allCatalogDepartments = $state<string[]>([]);
	let totalCatalogCount = $state(0);
	let totalMatchedCount = $state(0);
	let hideNoEquivalent = $state(true);
	let equivalencies = $state<TransferEquivalencyItem[]>([]);
	let coursesLoading = $state(false);
	let coursesLoadingMore = $state(false);
	let coursesError = $state('');
	let hasMoreCourses = $state(false);
	let currentOffset = $state(0);

	// ---- Editable Draft List of Courses (right panel) ----
	let draftCourses = $state<PlannedCourse[]>([]);
	let dirty = $state(false);
	let saveStatus = $state('');
	let exportScope = $state<'current' | 'all'>('current');
	let attachingPdf = $state(false);

	let instSeq = 0;
	let courseSeq = 0;

	const prefSlugs = $derived(store.application.pref.filter((s) => s.trim() !== ''));

	/** Quick-pick universities: application preferences first, then saved universities, then any already-planned universities. */
	const quickUniversities = $derived.by((): { record: UniversityRecord; tag: string }[] => {
		const seen = new Set<string>();
		const out: { record: UniversityRecord; tag: string }[] = [];

		prefSlugs.forEach((slug, idx) => {
			if (seen.has(slug)) return;
			const rec = catalog.bySlug(slug);
			if (rec) {
				seen.add(slug);
				out.push({ record: rec, tag: `Pref #${idx + 1}` });
			}
		});

		for (const slug of discovery.savedSlugs) {
			if (seen.has(slug)) continue;
			const rec = catalog.bySlug(slug);
			if (rec) {
				seen.add(slug);
				out.push({ record: rec, tag: 'Saved' });
			}
		}

		for (const plan of coursePlans.plans) {
			if (seen.has(plan.universitySlug)) continue;
			const rec = catalog.bySlug(plan.universitySlug);
			if (rec) {
				seen.add(plan.universitySlug);
				out.push({ record: rec, tag: 'Planned' });
			}
		}

		return out;
	});

	const allPartnersSorted = $derived(
		[...catalog.records].sort((a, b) =>
			(a.official_name.value ?? a.slug).localeCompare(b.official_name.value ?? b.slug)
		)
	);

	const selectedUniversity = $derived(selectedSlug ? catalog.bySlug(selectedSlug) ?? null : null);
	const selectedUniversityName = $derived(
		selectedUniversity?.official_name.value ||
			coursePlans.bySlug(selectedSlug)?.universityName ||
			selectedSlug
	);

	const availableDepartments = $derived.by(() => {
		if (allCatalogDepartments.length > 0) return allCatalogDepartments;
		const set = new Set<string>();
		for (const item of equivalencies) {
			for (const d of item.aucDepartments) set.add(d);
		}
		return [...set].sort();
	});

	const filteredDepartments = $derived.by(() => {
		const needle = deptSearch.trim().toLowerCase();
		if (!needle) return availableDepartments;
		return availableDepartments.filter((d) => d.toLowerCase().includes(needle));
	});

	const searchPlaceholder = $derived(
		searchBy === 'auc'
			? 'Search AUC course code or title (e.g. CSCE 4701, Calculus, MGMT 5202)…'
			: searchBy === 'host'
				? 'Search host university course code or title (e.g. Algorithms, Finance)…'
				: 'Search by AUC or host course code/title (e.g. CSCE, ECON 2011, Finance)…'
	);

	function toggleDeptMenu() {
		deptOpen = !deptOpen;
		if (deptOpen) {
			deptSearch = '';
			queueMicrotask(() => deptSearchEl?.focus());
		}
	}

	function selectDept(dept: string) {
		deptFilter = dept;
		deptOpen = false;
		deptSearch = '';
	}

	function handleWindowClick(event: MouseEvent) {
		if (deptOpen && deptRootEl && !deptRootEl.contains(event.target as Node)) {
			deptOpen = false;
		}
	}

	function handleWindowKeydown(event: KeyboardEvent) {
		if (event.key === 'Escape' && deptOpen) {
			deptOpen = false;
		}
	}

	const filteredEquivalencies = $derived.by(() => {
		return equivalencies.filter((item) => {
			if (hideNoEquivalent && !item.hasEquivalent) return false;
			if (deptFilter !== 'ALL' && !item.aucDepartments.includes(deptFilter)) return false;
			return true;
		});
	});

	const filledDraftCourses = $derived(
		draftCourses.filter(
			(c) =>
				c.hostCourseCode.trim() !== '' ||
				c.hostCourseTitle.trim() !== '' ||
				c.aucEquivalent.trim() !== ''
		)
	);

	const plannedCountBySlug = $derived.by(() => {
		const map = new Map<string, number>();
		for (const p of coursePlans.plans) {
			map.set(p.universitySlug, p.courses.length);
		}
		return map;
	});

	const studentDisplayName = $derived(
		[store.application.firstName, store.application.surname].filter(Boolean).join(' ').trim() ||
			store.user?.name?.trim() ||
			''
	);

	const studentAucId = $derived(
		store.application.aucId.trim() || store.user?.aucId?.trim() || ''
	);

	function emptySlot(): PlannedCourse {
		return {
			hostCourseCode: '',
			hostCourseTitle: '',
			aucEquivalent: '',
			effectiveTerm: '',
			source: 'manual'
		};
	}

	function padToFive(items: PlannedCourse[]): PlannedCourse[] {
		const copy = items.map((c) => ({ ...c }));
		while (copy.length < 5) copy.push(emptySlot());
		return copy;
	}

	function isInDraft(item: TransferEquivalencyItem): boolean {
		const code = item.hostCourseCode.trim().toLowerCase();
		const title = item.hostCourseTitle.trim().toLowerCase();
		return draftCourses.some((c) => {
			const cCode = c.hostCourseCode.trim().toLowerCase();
			const cTitle = c.hostCourseTitle.trim().toLowerCase();
			if (code && cCode && code === cCode && title === cTitle) return true;
			return title !== '' && cTitle === title;
		});
	}

	async function loadBannerInstitutions(query: string, fallback: boolean, autoSelectFirst: boolean) {
		const seq = ++instSeq;
		bannerInstLoading = true;
		bannerInstError = '';
		try {
			const params = new URLSearchParams({
				mode: 'institutions',
				q: query.trim(),
				max: '30'
			});
			if (fallback) params.set('fallback', '1');

			const res = await callServer<InstitutionsResponse>(
				'GET',
				`/api/transfer-equivalencies?${params.toString()}`
			);
			if (seq !== instSeq) return;
			bannerInstitutions = res.institutions ?? [];

			if (autoSelectFirst) {
				if (bannerInstitutions.length > 0) {
					// Prefer closest name match to the selected partner university.
					const target = selectedUniversityName.toLowerCase();
					const exact =
						bannerInstitutions.find((i) => i.name.toLowerCase() === target) ??
						bannerInstitutions.find(
							(i) =>
								target.includes(i.name.toLowerCase()) ||
								i.name.toLowerCase().includes(target)
						) ??
						bannerInstitutions[0];
					selectedBannerCode = exact.code;
					selectedBannerName = exact.name;
					showInstPicker = bannerInstitutions.length > 1;
					void loadBannerCourses(exact.code, '', 0, false);
				} else {
					selectedBannerCode = '';
					selectedBannerName = '';
					equivalencies = [];
					hasMoreCourses = false;
					showInstPicker = true;
				}
			}
		} catch (e) {
			if (seq !== instSeq) return;
			bannerInstError = errorText(e);
			bannerInstitutions = [];
			showInstPicker = true;
		} finally {
			if (seq === instSeq) bannerInstLoading = false;
		}
	}

	async function loadBannerCourses(
		instCode: string,
		query: string,
		offset: number,
		append: boolean
	) {
		if (!instCode) {
			equivalencies = [];
			hasMoreCourses = false;
			return;
		}
		const seq = ++courseSeq;
		if (append) coursesLoadingMore = true;
		else coursesLoading = true;
		coursesError = '';

		try {
			const params = new URLSearchParams({
				mode: 'courses',
				instCode,
				searchBy,
				offset: String(offset),
				max: String(PAGE_SIZE)
			});
			if (query.trim()) params.set('q', query.trim());

			const res = await callServer<CoursesResponse>(
				'GET',
				`/api/transfer-equivalencies?${params.toString()}`
			);
			if (seq !== courseSeq) return;

			const incoming = res.courses ?? [];
			equivalencies = append ? [...equivalencies, ...incoming] : incoming;
			hasMoreCourses = Boolean(res.hasMore);
			currentOffset = offset + incoming.length;
			totalCatalogCount = res.totalCatalogCount ?? equivalencies.length;
			totalMatchedCount = res.totalMatchedCount ?? equivalencies.length;
			if (Array.isArray(res.departments)) {
				allCatalogDepartments = res.departments;
			}
			if (!append && deptFilter !== 'ALL' && !allCatalogDepartments.includes(deptFilter)) {
				deptFilter = 'ALL';
			}
		} catch (e) {
			if (seq !== courseSeq) return;
			coursesError = errorText(e);
			if (!append) equivalencies = [];
		} finally {
			if (seq === courseSeq) {
				coursesLoading = false;
				coursesLoadingMore = false;
			}
		}
	}

	function selectUniversity(slug: string) {
		if (!slug) return;
		selectedSlug = slug;
		dirty = false;
		saveStatus = '';
		courseQuery = '';
		deptFilter = 'ALL';
		deptOpen = false;
		allCatalogDepartments = [];
		totalCatalogCount = 0;
		totalMatchedCount = 0;
		coursesError = '';
		bannerInstError = '';

		const existing = coursePlans.bySlug(slug);
		const rec = catalog.bySlug(slug);
		const displayName = rec?.official_name.value || existing?.universityName || slug;

		if (existing) {
			studyTerm = existing.studyTerm || 'Fall 2027';
			draftCourses = padToFive(existing.courses);
		} else {
			studyTerm = 'Fall 2027';
			draftCourses = padToFive([]);
		}

		bannerInstQuery = displayName;

		if (existing && existing.bannerInstCode) {
			selectedBannerCode = existing.bannerInstCode;
			selectedBannerName = existing.bannerInstName || displayName;
			showInstPicker = false;
			void loadBannerCourses(existing.bannerInstCode, '', 0, false);
		} else {
			selectedBannerCode = '';
			selectedBannerName = '';
			equivalencies = [];
			void loadBannerInstitutions(displayName, true, true);
		}
	}

	function pickBannerInstitution(inst: BannerInstitution) {
		selectedBannerCode = inst.code;
		selectedBannerName = inst.name;
		showInstPicker = false;
		courseQuery = '';
		dirty = true;
		void loadBannerCourses(inst.code, '', 0, false);
	}

	function runBannerCourseSearch(e?: Event) {
		e?.preventDefault();
		if (!selectedBannerCode) return;
		void loadBannerCourses(selectedBannerCode, courseQuery, 0, false);
	}

	function changeSearchBy(next: SearchByMode) {
		if (searchBy === next) return;
		searchBy = next;
		if (selectedBannerCode && courseQuery.trim()) {
			void loadBannerCourses(selectedBannerCode, courseQuery, 0, false);
		}
	}

	function runBannerInstitutionSearch(e?: Event) {
		e?.preventDefault();
		void loadBannerInstitutions(bannerInstQuery, false, false);
	}

	function addFromBanner(item: TransferEquivalencyItem) {
		if (isInDraft(item)) {
			store.showToast('This course is already in your draft list.');
			return;
		}
		const entry: PlannedCourse = {
			hostCourseCode: item.hostCourseCode || item.rawCourseGroup,
			hostCourseTitle: item.hostCourseTitle,
			aucEquivalent: item.hasEquivalent ? item.aucEquivalent : 'Pending registrar evaluation',
			effectiveTerm: item.effectiveTermLabel,
			source: 'banner'
		};

		// Place into the first completely blank slot, or append up to MAX_COURSES.
		const emptyIdx = draftCourses.findIndex(
			(c) =>
				c.hostCourseCode.trim() === '' &&
				c.hostCourseTitle.trim() === '' &&
				c.aucEquivalent.trim() === ''
		);
		if (emptyIdx >= 0) {
			draftCourses = draftCourses.map((c, idx) => (idx === emptyIdx ? entry : c));
		} else if (draftCourses.length < MAX_COURSES) {
			draftCourses = [...draftCourses, entry];
		} else {
			store.showToast(`Maximum of ${MAX_COURSES} courses per university reached.`);
			return;
		}
		dirty = true;
		saveStatus = '';
		store.showToast('Added to your draft course list.');
	}

	function addManualSlot() {
		if (draftCourses.length >= MAX_COURSES) {
			store.showToast(`Maximum of ${MAX_COURSES} courses per university reached.`);
			return;
		}
		draftCourses = [...draftCourses, emptySlot()];
		dirty = true;
	}

	function removeSlot(index: number) {
		const next = draftCourses.filter((_, i) => i !== index);
		draftCourses = padToFive(next);
		dirty = true;
		saveStatus = '';
	}

	function markDirty() {
		dirty = true;
		saveStatus = '';
	}

	async function saveCurrentPlan(silent = false): Promise<boolean> {
		if (!selectedSlug) return false;
		const res = await coursePlans.savePlan({
			universitySlug: selectedSlug,
			universityName: selectedUniversityName,
			bannerInstCode: selectedBannerCode,
			bannerInstName: selectedBannerName,
			studyTerm: studyTerm.trim() || 'Fall 2027',
			courses: filledDraftCourses
		});
		if (res.ok) {
			dirty = false;
			saveStatus = 'Saved to your account.';
			if (!silent) store.showToast('Draft course list saved.');
			return true;
		}
		saveStatus = res.error ?? 'Could not save your course list.';
		store.showToast(saveStatus);
		return false;
	}

	function buildExportSections(): CoursePlanPdfSection[] {
		if (exportScope === 'all') {
			const sections: CoursePlanPdfSection[] = [];
			const seen = new Set<string>();

			// Current edits first (even if unsaved yet)
			if (selectedSlug && filledDraftCourses.length > 0) {
				seen.add(selectedSlug);
				sections.push({
					universityName: selectedUniversityName,
					studyTerm: studyTerm.trim() || 'Fall 2027',
					courses: filledDraftCourses
				});
			}

			for (const plan of coursePlans.plans) {
				if (seen.has(plan.universitySlug) || plan.courses.length === 0) continue;
				seen.add(plan.universitySlug);
				sections.push({
					universityName: plan.universityName || plan.universitySlug,
					studyTerm: plan.studyTerm || 'Fall 2027',
					courses: plan.courses
				});
			}

			if (sections.length > 0) return sections;
		}

		return [
			{
				universityName: selectedUniversityName || 'Host Institution',
				studyTerm: studyTerm.trim() || 'Fall 2027',
				courses: filledDraftCourses
			}
		];
	}

	function generatePdfBytes(): { bytes: Uint8Array; filename: string } {
		const sections = buildExportSections();
		const bytes = buildCoursePlanPdf({
			studentName: studentDisplayName,
			aucId: studentAucId,
			studyTerm: studyTerm.trim() || 'Fall 2027',
			sections
		});
		const filename = buildCoursePlanFilename(studentDisplayName, studentAucId);
		return { bytes, filename };
	}

	async function downloadPdf() {
		if (dirty && selectedSlug) {
			await saveCurrentPlan(true);
		}
		const { bytes, filename } = generatePdfBytes();
		const blob = new Blob([new Uint8Array(bytes)], { type: 'application/pdf' });
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = filename;
		document.body.appendChild(a);
		a.click();
		a.remove();
		setTimeout(() => URL.revokeObjectURL(url), 1000);
		store.showToast('Downloaded Draft List of Courses PDF.');
	}

	async function attachPdfToApplication() {
		if (store.locked) {
			store.showToast('Your application is already submitted and locked.');
			return;
		}
		if (filledDraftCourses.length === 0 && coursePlans.plans.every((p) => p.courses.length === 0)) {
			store.showToast('Add at least one course to your draft list before attaching.');
			return;
		}

		attachingPdf = true;
		try {
			if (dirty && selectedSlug) {
				await saveCurrentPlan(true);
			}
			const { bytes, filename } = generatePdfBytes();
			const file = new File([new Uint8Array(bytes)], filename, { type: 'application/pdf' });
			await store.uploadDocumentAsync('courses', file);
		} finally {
			attachingPdf = false;
		}
	}

	onMount(async () => {
		await Promise.all([catalog.load(), coursePlans.loadAll()]);

		const fromQuery = page.url.searchParams.get('university')?.trim() ?? '';
		const initialSlug =
			(fromQuery && catalog.bySlug(fromQuery) ? fromQuery : '') ||
			prefSlugs[0] ||
			discovery.savedSlugs[0] ||
			coursePlans.plans[0]?.universitySlug ||
			allPartnersSorted[0]?.slug ||
			'';

		if (initialSlug) {
			selectUniversity(initialSlug);
		}
	});
</script>

<svelte:window onclick={handleWindowClick} onkeydown={handleWindowKeydown} />
<svelte:head><title>Course Equivalencies &amp; Planner · AUC Exchange</title></svelte:head>

<section class="space-y-6 pb-16">
	<PageHeader
		crumbs={['Discover', 'Course Equivalencies']}
		title="Transferable Courses &amp; Draft Planner"
		subtitle="Search live AUC Banner course equivalencies for your host universities, build your Draft List of Courses, and attach the PDF straight to your application."
	>
		{#snippet actions()}
			<a
				href={BANNER_URL}
				target="_blank"
				rel="noopener noreferrer"
				class="h-10 px-4 rounded-xl border border-[#c6c6cd]/60 bg-white hover:bg-[#eff4ff] text-[#0b1c30] text-xs font-semibold inline-flex items-center gap-1.5 shadow-sm"
			>
				Official AUC Banner Tool ↗
			</a>
		{/snippet}
	</PageHeader>

	<!-- OFFICIAL AUC DISCLAIMER -->
	<div class="rounded-2xl border border-[#b8cbe9] bg-[#eff4ff] px-4 py-3.5 text-xs text-[#233853] leading-relaxed">
		<strong class="font-bold text-[#0b1c30]">AUC Registrar Disclaimer:</strong>
		Transfer equivalencies shown here are fetched live from AUC Banner as a planning guide and are
		<strong>not</strong> a final determination of credit transfer. Before traveling, final course approval
		must be secured through your academic advisor and the Registrar&apos;s Transfer Credit Unit.
	</div>

	<!-- UNIVERSITY SELECTOR -->
	<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-4">
		<div class="flex flex-wrap items-end justify-between gap-3">
			<div>
				<h2 class="text-base font-bold text-[#0b1c30]">1. Choose a host university</h2>
				<p class="text-xs text-[#515f74] mt-0.5">
					Pick from your application preferences, saved list, or any of AUC&apos;s 155 partner universities.
				</p>
			</div>
			{#if selectedUniversity?.course_catalog_url.value}
				<a
					href={selectedUniversity.course_catalog_url.value}
					target="_blank"
					rel="noopener noreferrer"
					class="text-xs font-semibold text-[#003ea8] hover:underline inline-flex items-center gap-1"
				>
					Open {selectedUniversity.short_name.value ?? 'host'} course catalog ↗
				</a>
			{/if}
		</div>

		{#if quickUniversities.length > 0}
			<div class="flex flex-wrap gap-2">
				{#each quickUniversities as item (item.record.slug)}
					{@const active = selectedSlug === item.record.slug}
					{@const count = plannedCountBySlug.get(item.record.slug) ?? 0}
					<button
						type="button"
						onclick={() => selectUniversity(item.record.slug)}
						class="rounded-xl border px-3 py-2 text-xs font-semibold transition-colors cursor-pointer inline-flex items-center gap-2 {active
							? 'border-[#131b2e] bg-[#131b2e] text-white'
							: 'border-[#c6c6cd]/50 bg-[#f8faff] text-[#0b1c30] hover:bg-[#eff4ff]'}"
					>
						<span
							class="rounded px-1.5 py-0.5 text-[10px] font-bold {active
								? 'bg-white/20 text-white'
								: 'bg-white text-[#515f74]'}"
						>
							{item.tag}
						</span>
						<span class="truncate max-w-[220px]">{item.record.official_name.value ?? item.record.slug}</span>
						{#if count > 0}
							<span
								class="rounded-full px-1.5 py-0.5 text-[10px] font-bold font-mono-num {active
									? 'bg-[#22c55e] text-[#052e16]'
									: 'bg-[#dcfce7] text-[#15803d]'}"
							>
								{count}
							</span>
						{/if}
					</button>
				{/each}
			</div>
		{/if}

		<div class="grid grid-cols-1 sm:grid-cols-[minmax(0,1fr)_auto] gap-3 items-end">
			<div>
				<label for="partner-select" class="block text-xs font-semibold uppercase tracking-wider text-[#515f74] mb-1.5">
					All partner universities ({allPartnersSorted.length})
				</label>
				<PartnerSelect
					id="partner-select"
					universities={allPartnersSorted}
					value={selectedSlug}
					plannedCounts={plannedCountBySlug}
					onchange={selectUniversity}
				/>
			</div>
			{#if selectedUniversity}
				<a
					href={`/explore/university/${selectedUniversity.slug}`}
					class="h-11 px-4 rounded-xl border border-[#c6c6cd]/50 bg-white hover:bg-[#eff4ff] text-xs font-semibold text-[#0b1c30] inline-flex items-center justify-center gap-1.5"
				>
					University details <Icon name="arrow-right" class="w-3.5 h-3.5" />
				</a>
			{/if}
		</div>
	</section>

	<!-- MAIN WORKSPACE -->
	<div class="grid grid-cols-1 xl:grid-cols-[minmax(0,1.15fr)_minmax(0,1fr)] gap-5 items-start">
		<!-- LEFT: LIVE AUC BANNER EQUIVALENCIES -->
		<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-4">
			<div class="flex flex-wrap items-start justify-between gap-2">
				<div>
					<div class="flex items-center gap-2 flex-wrap">
						<h2 class="text-base font-bold text-[#0b1c30]">2. Live AUC Banner Equivalencies</h2>
						<span class="rounded-lg bg-[#eff4ff] px-2 py-0.5 text-[11px] font-semibold text-[#314c76]">
							Live · AUC Banner
						</span>
					</div>
					{#if selectedBannerName}
						<p class="text-xs text-[#515f74] mt-1">
							Matched Banner institution:
							<strong class="text-[#0b1c30]">{selectedBannerName}</strong>
							<span class="font-mono-num text-[11px] text-[#657087]">({selectedBannerCode})</span>
						</p>
					{:else if !bannerInstLoading}
						<p class="text-xs text-[#b45309] mt-1">
							No automatic Banner match found for this name — search Banner manually below, or add courses manually from the host catalog.
						</p>
					{/if}
				</div>
				<button
					type="button"
					onclick={() => (showInstPicker = !showInstPicker)}
					class="text-xs font-semibold text-[#003ea8] hover:underline cursor-pointer"
				>
					{showInstPicker ? 'Hide institution search' : 'Change Banner match'}
				</button>
			</div>

			<!-- MANUAL BANNER INSTITUTION MATCHER -->
			{#if showInstPicker || !selectedBannerCode}
				<div class="rounded-xl border border-[#dce4f0] bg-[#f8faff] p-3.5 space-y-3">
					<form onsubmit={runBannerInstitutionSearch} class="flex gap-2">
						<input
							type="text"
							bind:value={bannerInstQuery}
							placeholder="Search institution name in AUC Banner (e.g. Bocconi, Aarhus, Paris)…"
							class="flex-1 h-9 px-3 rounded-lg border border-[#c6c6cd]/60 bg-white text-xs text-[#0b1c30] focus:outline-none focus:border-black"
						/>
						<button
							type="submit"
							disabled={bannerInstLoading}
							class="h-9 px-3.5 rounded-lg bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs font-semibold cursor-pointer disabled:opacity-50"
						>
							{bannerInstLoading ? 'Searching…' : 'Find in Banner'}
						</button>
					</form>

					{#if bannerInstError}
						<div role="alert" class="rounded-lg bg-red-50 border border-red-200 px-3 py-2 text-xs text-red-800 flex flex-wrap items-center justify-between gap-2">
							<span>{bannerInstError}</span>
							<a href={BANNER_URL} target="_blank" rel="noopener noreferrer" class="font-semibold underline">
								Open Banner directly ↗
							</a>
						</div>
					{:else if bannerInstitutions.length > 0}
						<div class="max-h-44 overflow-y-auto divide-y divide-[#dce4f0] rounded-lg border border-[#dce4f0] bg-white">
							{#each bannerInstitutions as inst (inst.code)}
								<button
									type="button"
									onclick={() => pickBannerInstitution(inst)}
									class="w-full px-3 py-2 text-left text-xs flex items-center justify-between gap-2 hover:bg-[#eff4ff] cursor-pointer {inst.code ===
									selectedBannerCode
										? 'bg-[#eff4ff] font-bold text-[#0b1c30]'
										: 'text-[#0b1c30]'}"
								>
									<span class="truncate">{inst.name}</span>
									<span class="font-mono-num text-[11px] text-[#657087] shrink-0">{inst.code}</span>
								</button>
							{/each}
						</div>
					{:else if !bannerInstLoading}
						<p class="text-xs text-[#515f74]">
							No Banner institutions matched &ldquo;{bannerInstQuery}&rdquo;. Try a shorter keyword (e.g. city or last word), or use the host catalog link above and add courses manually to your draft list.
						</p>
					{/if}
				</div>
			{/if}

			<!-- COURSE SEARCH + FILTERS -->
			{#if selectedBannerCode}
				<form onsubmit={runBannerCourseSearch} class="space-y-3">
					<div class="flex flex-wrap items-center justify-between gap-2">
						<span class="text-xs font-semibold text-[#515f74]">Search by:</span>
						<div class="inline-flex items-center gap-1 rounded-xl border border-[#c6c6cd]/50 bg-[#f8faff] p-1">
							{#each [
								{ id: 'auc', label: 'AUC Course' },
								{ id: 'host', label: 'Host Course' },
								{ id: 'all', label: 'Both' }
							] as mode (mode.id)}
								<button
									type="button"
									onclick={() => changeSearchBy(mode.id as SearchByMode)}
									aria-pressed={searchBy === mode.id}
									class="rounded-lg px-2.5 py-1 text-[11px] font-semibold transition-colors cursor-pointer {searchBy ===
									mode.id
										? 'bg-[#0b1c30] text-white'
										: 'text-[#515f74] hover:bg-[#eff4ff]'}"
								>
									{mode.label}
								</button>
							{/each}
						</div>
					</div>

					<div class="flex gap-2">
						<div class="relative flex-1">
							<input
								type="text"
								bind:value={courseQuery}
								placeholder={searchPlaceholder}
								class="w-full h-10 pl-3.5 pr-3 rounded-xl border border-[#c6c6cd]/50 bg-white text-sm text-[#0b1c30] focus:outline-none focus:border-black"
							/>
						</div>
						<button
							type="submit"
							disabled={coursesLoading}
							class="h-10 px-4 rounded-xl bg-[#0b1c30] hover:bg-[#233853] text-white text-xs font-semibold inline-flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
						>
							<Icon name="search" class="w-3.5 h-3.5" />
							{coursesLoading ? 'Loading…' : 'Search'}
						</button>
					</div>

					<div class="flex flex-wrap items-center justify-between gap-2 text-xs">
						<div class="flex items-center gap-2.5 flex-wrap">
							<span class="text-[#515f74] font-medium">AUC Dept:</span>
							<div bind:this={deptRootEl} class="relative min-w-[175px]">
								<button
									id="dept-filter"
									type="button"
									onclick={toggleDeptMenu}
									aria-haspopup="listbox"
									aria-expanded={deptOpen}
									class="w-full h-9 px-3 rounded-lg border {deptOpen
										? 'border-black'
										: 'border-[#c6c6cd]/60'} bg-white text-xs text-[#0b1c30] font-medium flex items-center justify-between gap-3 cursor-pointer"
								>
									<span class="truncate">
										{deptFilter === 'ALL'
											? `All departments (${availableDepartments.length})`
											: deptFilter}
									</span>
									<svg
										class="w-3.5 h-3.5 text-[#515f74] shrink-0 transition-transform duration-150 {deptOpen
											? 'rotate-180'
											: ''}"
										viewBox="0 0 24 24"
										fill="none"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
										aria-hidden="true"
									>
										<polyline points="6 9 12 15 18 9" />
									</svg>
								</button>

								{#if deptOpen}
									<div
										role="listbox"
										aria-label="AUC Departments"
										class="absolute left-0 top-full mt-1 z-40 w-56 rounded-xl border border-[#c6c6cd]/70 bg-white shadow-lg overflow-hidden"
									>
										{#if availableDepartments.length > 8}
											<div class="p-1.5 border-b border-[#c6c6cd]/40 bg-[#f8f9ff]">
												<input
													bind:this={deptSearchEl}
													bind:value={deptSearch}
													type="text"
													placeholder="Filter code (e.g. CSCE)…"
													class="w-full h-8 px-2.5 rounded-lg border border-[#c6c6cd]/60 bg-white text-xs text-[#0b1c30] focus:outline-none focus:border-black focus:ring-0"
												/>
											</div>
										{/if}
										<div class="max-h-56 overflow-y-auto py-1">
											<button
												type="button"
												role="option"
												aria-selected={deptFilter === 'ALL'}
												onclick={() => selectDept('ALL')}
												class="w-full px-3 py-1.5 text-left text-xs flex items-center justify-between gap-2 cursor-pointer {deptFilter ===
												'ALL'
													? 'bg-[#dce9ff]/80 font-semibold text-[#0b1c30]'
													: 'text-[#0b1c30] hover:bg-[#eff4ff]'}"
											>
												<span>All departments ({availableDepartments.length})</span>
												{#if deptFilter === 'ALL'}
													<Icon name="check" class="w-3.5 h-3.5 text-[#131b2e]" strokeWidth={2.5} />
												{/if}
											</button>
											{#each filteredDepartments as dept (dept)}
												<button
													type="button"
													role="option"
													aria-selected={deptFilter === dept}
													onclick={() => selectDept(dept)}
													class="w-full px-3 py-1.5 text-left text-xs font-mono-num flex items-center justify-between gap-2 cursor-pointer {deptFilter ===
													dept
														? 'bg-[#dce9ff]/80 font-semibold text-[#0b1c30]'
														: 'text-[#0b1c30] hover:bg-[#eff4ff]'}"
												>
													<span>{dept}</span>
													{#if deptFilter === dept}
														<Icon name="check" class="w-3.5 h-3.5 text-[#131b2e]" strokeWidth={2.5} />
													{/if}
												</button>
											{/each}
										</div>
									</div>
								{/if}
							</div>

							<label class="inline-flex items-center gap-1.5 text-[#515f74] cursor-pointer select-none">
								<input type="checkbox" bind:checked={hideNoEquivalent} class="accent-[#131b2e]" />
								Hide &ldquo;No equivalent&rdquo;
							</label>
						</div>
						<span class="text-[#657087] font-mono-num">
							Showing {filteredEquivalencies.length} of {totalMatchedCount || equivalencies.length}
							{#if totalCatalogCount > 0 && totalMatchedCount !== totalCatalogCount}
								({totalCatalogCount} total)
							{/if}
						</span>
					</div>
				</form>

				<!-- COURSE RESULTS -->
				{#if coursesLoading}
					<div class="space-y-2.5">
						{#each [1, 2, 3, 4] as i (i)}
							<div class="h-20 rounded-xl border border-[#c6c6cd]/40 bg-[#f8faff] animate-pulse"></div>
						{/each}
					</div>
				{:else if coursesError}
					<div role="alert" class="rounded-xl border border-red-200 bg-red-50 p-4 space-y-2 text-xs text-red-800">
						<p class="font-semibold">{coursesError}</p>
						<div class="flex flex-wrap gap-2">
							<button
								type="button"
								onclick={() => loadBannerCourses(selectedBannerCode, courseQuery, 0, false)}
								class="px-3 py-1.5 rounded-lg bg-red-700 text-white font-semibold cursor-pointer hover:bg-red-800"
							>
								Retry Banner lookup
							</button>
							<a
								href={BANNER_URL}
								target="_blank"
								rel="noopener noreferrer"
								class="px-3 py-1.5 rounded-lg border border-red-300 bg-white text-red-800 font-semibold hover:bg-red-100"
							>
								Open Official Banner Tool ↗
							</a>
						</div>
					</div>
				{:else if filteredEquivalencies.length === 0}
					<div class="rounded-xl border border-[#dce4f0] bg-[#f8faff] p-6 text-center space-y-2">
						<p class="text-sm font-semibold text-[#0b1c30]">No matching Banner equivalencies found</p>
						<p class="text-xs text-[#515f74]">
							{#if equivalencies.length > 0}
								Try clearing your department filter or unchecking &ldquo;Hide No equivalent&rdquo;.
							{:else}
								Courses not yet listed in AUC Banner can still be taken — check the host course catalog and add them manually on the right for advisor evaluation.
							{/if}
						</p>
					</div>
				{:else}
					<div class="space-y-2.5 max-h-[640px] overflow-y-auto pr-1">
						{#each filteredEquivalencies as item (item.id)}
							{@const added = isInDraft(item)}
							<article class="rounded-xl border border-[#dce4f0] bg-[#f8faff]/60 hover:border-[#a4b3d0] p-3.5 transition-colors">
								<div class="flex items-start justify-between gap-3">
									<div class="min-w-0 space-y-1">
										<div class="flex items-center gap-1.5 flex-wrap">
											{#if item.hostCourseCode}
												<span class="rounded bg-[#0b1c30] text-white px-2 py-0.5 text-[11px] font-bold font-mono-num">
													{item.hostCourseCode}
												</span>
											{/if}
											<span class="rounded bg-white border border-[#dce4f0] px-2 py-0.5 text-[10px] font-medium text-[#515f74]">
												Eff. {item.effectiveTermLabel}
											</span>
										</div>
										<h3 class="text-sm font-bold text-[#0b1c30] leading-snug">
											{item.hostCourseTitle}
										</h3>
										<p class="text-xs leading-relaxed {item.hasEquivalent ? 'text-[#15803d] font-medium' : 'text-[#657087] italic'}">
											<span class="text-[#515f74] not-italic font-semibold">AUC Equivalent:</span>
											{item.aucEquivalent}
										</p>
									</div>
									<button
										type="button"
										onclick={() => addFromBanner(item)}
										disabled={added}
										class="shrink-0 h-8 px-3 rounded-lg text-xs font-semibold inline-flex items-center gap-1 cursor-pointer transition-colors {added
											? 'bg-[#dcfce7] text-[#15803d] cursor-default'
											: 'bg-[#131b2e] hover:bg-[#1e293b] text-white'}"
									>
										<Icon name={added ? 'check' : 'plus'} class="w-3.5 h-3.5" />
										{added ? 'Added' : 'Add'}
									</button>
								</div>
							</article>
						{/each}
					</div>

					{#if hasMoreCourses}
						<div class="pt-1 text-center">
							<button
								type="button"
								onclick={() => loadBannerCourses(selectedBannerCode, courseQuery, currentOffset, true)}
								disabled={coursesLoadingMore}
								class="h-9 px-4 rounded-xl border border-[#c6c6cd]/60 bg-white hover:bg-[#eff4ff] text-xs font-semibold text-[#0b1c30] cursor-pointer disabled:opacity-50"
							>
								{coursesLoadingMore ? 'Loading more…' : 'Load more courses from Banner'}
							</button>
						</div>
					{/if}
				{/if}
			{/if}
		</section>

		<!-- RIGHT: OFFICIAL DRAFT LIST OF COURSES BUILDER -->
		<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm space-y-4">
			<div class="flex flex-wrap items-start justify-between gap-2">
				<div>
					<h2 class="text-base font-bold text-[#0b1c30]">3. Draft List of Courses for Study Abroad</h2>
					<p class="text-xs text-[#515f74] mt-0.5">
						Matches the official AUC form. Saved per university in C++ + SQLite.
					</p>
				</div>
				<span class="rounded-lg bg-[#f2f3f7] px-2.5 py-1 text-[11px] font-bold font-mono-num text-[#0b1c30]">
					{filledDraftCourses.length} course{filledDraftCourses.length === 1 ? '' : 's'}
				</span>
			</div>

			<!-- TEMPLATE HEADER FIELDS -->
			<div class="rounded-xl bg-[#f8faff] border border-[#dce4f0] p-3.5 grid grid-cols-1 sm:grid-cols-2 gap-3 text-xs">
				<div>
					<div class="text-[#515f74]">Student Name</div>
					<div class="font-semibold text-[#0b1c30] mt-0.5">{studentDisplayName || 'Not set yet'}</div>
				</div>
				<div>
					<div class="text-[#515f74]">AUC ID Number</div>
					<div class="font-semibold text-[#0b1c30] font-mono-num mt-0.5">{studentAucId || 'Not set yet'}</div>
				</div>
				<div class="sm:col-span-2">
					<div class="text-[#515f74]">Host Institution</div>
					<div class="font-semibold text-[#0b1c30] mt-0.5">{selectedUniversityName || 'Select a university'}</div>
				</div>
				<div class="sm:col-span-2">
					<label for="study-term" class="block text-[#515f74] mb-1">Study Abroad Term</label>
					<input
						id="study-term"
						type="text"
						bind:value={studyTerm}
						oninput={markDirty}
						placeholder="e.g. Fall 2027"
						class="w-full h-9 px-3 rounded-lg border border-[#c6c6cd]/60 bg-white text-xs text-[#0b1c30] focus:outline-none focus:border-black"
					/>
				</div>
			</div>

			<!-- NUMBERED COURSE SLOTS -->
			<div class="space-y-3 max-h-[500px] overflow-y-auto pr-1">
				{#each draftCourses as slot, i (i)}
					<div class="rounded-xl border border-[#c6c6cd]/50 bg-white p-3.5 space-y-2.5">
						<div class="flex items-center justify-between gap-2">
							<div class="flex items-center gap-2">
								<span class="w-6 h-6 rounded-lg bg-[#eff4ff] text-[#0b1c30] text-xs font-bold font-mono-num inline-flex items-center justify-center">
									{i + 1}
								</span>
								<span class="text-[11px] font-semibold uppercase tracking-wider text-[#515f74]">
									{slot.source === 'banner' ? 'From AUC Banner' : 'Manual entry'}
								</span>
							</div>
							{#if slot.hostCourseCode || slot.hostCourseTitle || slot.aucEquivalent || draftCourses.length > 5}
								<button
									type="button"
									onclick={() => removeSlot(i)}
									aria-label={`Remove course ${i + 1}`}
									class="text-xs text-[#515f74] hover:text-[#ba1a1a] cursor-pointer inline-flex items-center gap-1"
								>
									<Icon name="x" class="w-3.5 h-3.5" /> Clear
								</button>
							{/if}
						</div>

						<div class="grid grid-cols-1 sm:grid-cols-[minmax(0,2fr)_minmax(0,1fr)] gap-2">
							<div>
								<label for={`course-title-${i}`} class="block text-[11px] font-medium text-[#515f74] mb-1">
									Host University Course Title
								</label>
								<input
									id={`course-title-${i}`}
									type="text"
									bind:value={slot.hostCourseTitle}
									oninput={markDirty}
									placeholder="e.g. Algorithms in Bioinformatics"
									class="w-full h-9 px-2.5 rounded-lg border border-[#c6c6cd]/50 bg-white text-xs text-[#0b1c30] focus:outline-none focus:border-black"
								/>
							</div>
							<div>
								<label for={`course-code-${i}`} class="block text-[11px] font-medium text-[#515f74] mb-1">
									Host Course Code
								</label>
								<input
									id={`course-code-${i}`}
									type="text"
									bind:value={slot.hostCourseCode}
									oninput={markDirty}
									placeholder="e.g. CS 301"
									class="w-full h-9 px-2.5 rounded-lg border border-[#c6c6cd]/50 bg-white text-xs font-mono-num text-[#0b1c30] focus:outline-none focus:border-black"
								/>
							</div>
						</div>

						<div>
							<label for={`auc-eq-${i}`} class="block text-[11px] font-medium text-[#515f74] mb-1">
								AUC Equivalent Course (if known)
							</label>
							<input
								id={`auc-eq-${i}`}
								type="text"
								bind:value={slot.aucEquivalent}
								oninput={markDirty}
								placeholder="e.g. CSCE 3201 Algorithms, cr.hrs.: 3"
								class="w-full h-9 px-2.5 rounded-lg border border-[#c6c6cd]/50 bg-white text-xs text-[#0b1c30] focus:outline-none focus:border-black"
							/>
						</div>
					</div>
				{/each}
			</div>

			<div class="flex flex-wrap items-center justify-between gap-2 pt-1">
				<button
					type="button"
					onclick={addManualSlot}
					disabled={draftCourses.length >= MAX_COURSES}
					class="h-9 px-3 rounded-xl border border-[#c6c6cd]/60 bg-white hover:bg-[#eff4ff] text-xs font-semibold text-[#0b1c30] inline-flex items-center gap-1.5 cursor-pointer disabled:opacity-50"
				>
					<Icon name="plus" class="w-3.5 h-3.5" /> Add course slot
				</button>

				<button
					type="button"
					onclick={() => saveCurrentPlan(false)}
					disabled={!selectedSlug || coursePlans.saving}
					class="h-9 px-4 rounded-xl bg-[#0b1c30] hover:bg-[#233853] text-white text-xs font-semibold cursor-pointer disabled:opacity-50"
				>
					{coursePlans.saving ? 'Saving…' : dirty ? 'Save draft list *' : 'Save draft list'}
				</button>
			</div>

			{#if saveStatus}
				<p class="text-xs text-[#15803d] font-medium">{saveStatus}</p>
			{/if}

			<!-- EXPORT & ATTACH TO APPLICATION -->
			<div class="border-t border-[#dce4f0] pt-4 space-y-3">
				{#if coursePlans.plans.filter((p) => p.courses.length > 0).length > 1}
					<div class="flex items-center gap-3 text-xs text-[#515f74]">
						<span class="font-semibold text-[#0b1c30]">PDF includes:</span>
						<label class="inline-flex items-center gap-1.5 cursor-pointer">
							<input type="radio" name="exportScope" value="current" bind:group={exportScope} class="accent-[#131b2e]" />
							This university only
						</label>
						<label class="inline-flex items-center gap-1.5 cursor-pointer">
							<input type="radio" name="exportScope" value="all" bind:group={exportScope} class="accent-[#131b2e]" />
							All planned universities ({coursePlans.plans.filter((p) => p.courses.length > 0).length})
						</label>
					</div>
				{/if}

				<div class="grid grid-cols-1 sm:grid-cols-2 gap-2.5">
					<button
						type="button"
						onclick={downloadPdf}
						disabled={!selectedSlug}
						class="h-10 px-4 rounded-xl border border-[#131b2e] bg-white hover:bg-[#eff4ff] text-[#0b1c30] text-xs font-semibold inline-flex items-center justify-center gap-1.5 cursor-pointer disabled:opacity-50"
					>
						<Icon name="download" class="w-4 h-4" /> Download Official PDF
					</button>
					<button
						type="button"
						onclick={attachPdfToApplication}
						disabled={!selectedSlug || store.locked || attachingPdf}
						class="h-10 px-4 rounded-xl bg-[#131b2e] hover:bg-[#1e293b] text-white text-xs font-semibold inline-flex items-center justify-center gap-1.5 cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
					>
						<Icon name="upload" class="w-4 h-4" />
						{attachingPdf
							? 'Attaching…'
							: store.locked
								? 'Application Submitted'
								: 'Attach to Application'}
					</button>
				</div>

				{#if store.docs.courses}
					<div class="rounded-xl bg-[#f0fdf4] border border-[#bbf7d0] px-3.5 py-2.5 text-xs text-[#15803d] flex items-center justify-between gap-2">
						<span class="inline-flex items-center gap-1.5 font-medium truncate">
							<Icon name="check" class="w-3.5 h-3.5 shrink-0" strokeWidth={2.4} />
							Attached in Step 6: {store.docs.courses.fileName}
						</span>
						<a href="/apply?step=6" class="font-semibold underline shrink-0">View in /apply</a>
					</div>
				{/if}

				<p class="text-[11px] text-[#657087] leading-relaxed">
					Note: The final course selection will depend on availability at the host institution and your ability to register for the courses.
				</p>
			</div>
		</section>
	</div>
</section>
