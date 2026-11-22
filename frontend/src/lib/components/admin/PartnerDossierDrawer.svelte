<script lang="ts">
	import type { AdminPartnerDetail } from '#lib/admin/types.ts';

	interface Props {
		partner: AdminPartnerDetail | null;
		loading?: boolean;
		savingSeats?: boolean;
		placedCount?: number;
		backLabel?: string | null;
		onClose: () => void;
		onBack?: () => void;
		onAdjustSeats: (slug: string, nextSeats: number) => void;
		onViewPlacedStudents?: (slug: string) => void;
	}

	let {
		partner,
		loading = false,
		savingSeats = false,
		placedCount = 0,
		backLabel = null,
		onClose,
		onBack,
		onAdjustSeats,
		onViewPlacedStudents
	}: Props = $props();

	let customSeatDraft = $state(2);
	let activeTab = $state<'overview' | 'calendar' | 'logistics'>('overview');
	let lastLoadedSlug = '';

	$effect(() => {
		if (partner) {
			customSeatDraft = partner.seats;
			if (partner.slug !== lastLoadedSlug) {
				lastLoadedSlug = partner.slug;
				activeTab = 'overview';
			}
		} else {
			lastLoadedSlug = '';
		}
	});

	function initialsOf(name: string): string {
		return (
			name
				.split(' ')
				.filter(Boolean)
				.slice(0, 2)
				.map((w) => w[0])
				.join('')
				.toUpperCase() || 'PU'
		);
	}

	function statusPillClass(status: string): string {
		if (status === 'active') return 'bg-[#f0fdf4] text-[#15803d]';
		if (status === 'check_slots') return 'bg-[#fffbeb] text-[#b45309]';
		return 'bg-[#fef2f2] text-[#ba1a1a]';
	}

	function statusLabel(status: string): string {
		if (status === 'active') return 'Active Agreement';
		if (status === 'check_slots') return 'Check Slots';
		if (status === 'letter_of_intent') return 'Letter of Intent';
		return status || 'Unknown';
	}

	function showText(val: string | null | undefined, fallback = 'Not published in fact sheet'): string {
		return val && val.trim() ? val : fallback;
	}
</script>

<svelte:window
	onkeydown={(e) => {
		if (e.key === 'Escape' && (partner || loading)) onClose();
	}}
/>

{#if partner || loading}
	<div
		class="fixed inset-0 z-50 flex items-center justify-center p-2.5 sm:p-5"
		role="dialog"
		aria-modal="true"
		aria-label="Partner University Dossier"
	>
		<button
			type="button"
			aria-label="Close partner dossier modal"
			onclick={onClose}
			class="fixed inset-0 bg-[#0b1c30]/45 backdrop-blur-[3px]"
		></button>

		<div class="modal-pop relative z-10 w-full max-w-4xl max-h-[92vh] bg-white border border-[#c6c6cd]/60 rounded-2xl shadow-2xl flex flex-col overflow-hidden">
			{#if backLabel && onBack}
				<div class="px-5 py-2 bg-[#e5eeff] border-b border-[#c6c6cd]/40 flex items-center justify-between gap-2">
					<button
						type="button"
						onclick={onBack}
						class="inline-flex items-center gap-1.5 text-xs font-semibold text-[#003ea8] hover:underline whitespace-nowrap shrink-0 cursor-pointer"
					>
						<span>&larr;</span>
						<span>Back to {backLabel}</span>
					</button>
					<span class="text-xs text-[#515f74] hidden sm:inline font-mono-num">Alt + &larr;</span>
				</div>
			{/if}

			<!-- Hero Header -->
			<div class="px-5 py-4 bg-[#eff4ff] border-b border-[#c6c6cd]/40 flex items-start justify-between gap-3">
				<div class="flex items-start gap-3.5 min-w-0">
					{#if loading && !partner}
						<div class="w-11 h-11 rounded-2xl bg-[#131b2e] text-white font-bold text-sm flex items-center justify-center shrink-0">
							…
						</div>
						<div class="text-sm font-semibold text-[#515f74] mt-2">Loading partner dossier…</div>
					{:else if partner}
						<div class="w-11 h-11 rounded-2xl bg-[#131b2e] text-white font-bold text-sm flex items-center justify-center shrink-0 shadow-sm">
							{initialsOf(partner.shortName || partner.officialName)}
						</div>
						<div class="min-w-0">
							<div class="flex items-center gap-2 flex-wrap">
								<span class="px-2.5 py-0.5 rounded-full text-xs font-semibold {statusPillClass(partner.agreementStatus)}">
									{statusLabel(partner.agreementStatus)}
								</span>
								{#if partner.region}
									<span class="px-2 py-0.5 rounded-full bg-[#e5eeff] text-[#0b1c30] text-xs font-semibold">
										{partner.region}
									</span>
								{/if}
								<span class="font-mono-num text-xs text-[#515f74] break-all">{partner.slug}</span>
							</div>
							<h2 class="text-lg sm:text-xl font-bold text-[#0b1c30] mt-1 leading-snug">
								{partner.officialName}
							</h2>
							<p class="text-xs sm:text-sm text-[#515f74] mt-0.5">
								{partner.city}, {partner.stateOrProvince ? `${partner.stateOrProvince}, ` : ''}{partner.country}
								{partner.agreementScope ? ` · ${partner.agreementScope}` : ''}
							</p>
						</div>
					{/if}
				</div>

				<button
					type="button"
					onclick={onClose}
					aria-label="Close"
					class="h-9 w-9 rounded-xl border border-[#c6c6cd]/50 bg-white text-[#515f74] hover:text-[#0b1c30] shadow-sm flex items-center justify-center cursor-pointer shrink-0 text-lg"
				>
					&times;
				</button>
			</div>

			{#if partner}
				<!-- Pinned Seat Quota & Demand Bar -->
				<div class="px-5 py-3.5 bg-white border-b border-[#c6c6cd]/40 flex flex-col sm:flex-row sm:items-center justify-between gap-3">
					<div class="space-y-0.5">
						<div class="flex items-center gap-2 flex-wrap">
							<span class="text-xs font-bold uppercase tracking-wider text-[#0b1c30]">
								Exchange Seat Quota
							</span>
							<span
								class="px-2 py-0.5 rounded-full text-xs font-semibold {partner.seatsAssumed
									? 'bg-[#fffbeb] text-[#b45309]'
									: 'bg-[#f0fdf4] text-[#15803d]'}"
							>
								{partner.seatsAssumed ? 'Default (Assumed 2)' : 'IPSO Confirmed'}
							</span>
						</div>
						<div class="text-xs text-[#515f74]">
							Live demand: <strong class="text-[#0b1c30] font-mono-num">{partner.firstChoiceDemand}</strong> 1st-choice ·
							<strong class="text-[#0b1c30] font-mono-num">{partner.totalDemand}</strong> total
							({partner.seats > 0 ? `${(partner.totalDemand / partner.seats).toFixed(1)}x` : '∞'})
							{#if placedCount > 0}
								· <strong class="text-[#15803d] font-mono-num">{placedCount} placed in active run</strong>
							{/if}
						</div>
					</div>

					<div class="inline-flex items-center border border-[#c6c6cd]/50 rounded-xl bg-[#eff4ff] shadow-sm self-start sm:self-auto">
						<button
							type="button"
							disabled={savingSeats || partner.seats <= 0}
							onclick={() => onAdjustSeats(partner.slug, Math.max(0, partner.seats - 1))}
							class="w-9 h-9 text-[#0b1c30] hover:bg-[#dce9ff] rounded-l-xl font-bold cursor-pointer disabled:opacity-40"
							aria-label="Decrease seats"
						>
							−
						</button>
						<input
							type="number"
							min="0"
							max="100"
							bind:value={customSeatDraft}
							onchange={() =>
								onAdjustSeats(
									partner.slug,
									Math.max(0, Math.min(100, Math.round(customSeatDraft || 0)))
								)}
							aria-label="Seat quota"
							class="w-12 h-9 text-center font-mono-num font-bold text-sm text-[#0b1c30] bg-transparent border-0 focus:ring-0"
						/>
						<button
							type="button"
							disabled={savingSeats}
							onclick={() => onAdjustSeats(partner.slug, partner.seats + 1)}
							class="w-9 h-9 text-[#0b1c30] hover:bg-[#dce9ff] rounded-r-xl font-bold cursor-pointer disabled:opacity-40"
							aria-label="Increase seats"
						>
							+
						</button>
					</div>
				</div>

				<!-- Clean 3-Tab Switcher -->
				<div class="px-5 py-2.5 bg-[#f8f9ff] border-b border-[#c6c6cd]/30 flex items-center gap-2 overflow-x-auto no-scrollbar">
					{#each [['overview', 'Overview & Eligibility'], ['calendar', 'Deadlines & Terms'], ['logistics', 'Language, Housing & Visa']] as [tabKey, tabLabel] (tabKey)}
						<button
							type="button"
							onclick={() => (activeTab = tabKey as 'overview' | 'calendar' | 'logistics')}
							class="px-3.5 py-1.5 rounded-xl text-xs font-semibold whitespace-nowrap cursor-pointer transition {activeTab ===
							tabKey
								? 'bg-[#131b2e] text-white shadow-sm'
								: 'bg-white border border-[#c6c6cd]/40 text-[#515f74] hover:text-[#0b1c30]'}"
						>
							{tabLabel}
						</button>
					{/each}
				</div>

				<!-- Modal Scrollable Content -->
				<div class="p-4 sm:p-5 overflow-y-auto space-y-4 text-xs sm:text-sm flex-1 bg-[#f8f9ff]">
					{#if activeTab === 'overview'}
						<div class="grid grid-cols-2 sm:grid-cols-4 gap-3">
							<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Host Min GPA</div>
								<div class="text-lg font-bold font-mono-num text-[#0b1c30] mt-1">
									{partner.minGpa !== null ? partner.minGpa.toFixed(2) : '2.80*'}
								</div>
								<div class="text-xs text-[#515f74]">{partner.minGpa !== null ? 'Host explicit' : 'AUC fallback'}</div>
							</div>

							<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Student Levels</div>
								<div class="text-sm font-bold text-[#0b1c30] mt-1">
									{partner.studentLevels.length ? partner.studentLevels.join(' · ') : 'UG / GR'}
								</div>
								<div class="text-xs text-[#515f74] truncate">
									{partner.availableTerms.length ? partner.availableTerms.join(', ') : 'Fall / Spring'}
								</div>
							</div>

							<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Sem. Cost Est.</div>
								<div class="text-lg font-bold font-mono-num text-[#0b1c30] mt-1">
									{partner.livingCostUsd !== null ? `$${Math.round(partner.livingCostUsd).toLocaleString()}` : 'N/A'}
								</div>
								<div class="text-xs text-[#515f74] truncate">{partner.estMonthlyLocal || 'See notes'}</div>
							</div>

							<div class="p-3.5 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm">
								<div class="text-xs font-semibold uppercase tracking-wider text-[#515f74]">Fact Sheets</div>
								<div class="text-sm font-bold text-[#0b1c30] mt-1 capitalize">
									{partner.factSheetStatus} ({partner.factSheetFilesCount})
								</div>
								<div class="text-xs text-[#515f74] capitalize truncate">{partner.housingAvailability || 'Housing TBD'}</div>
							</div>
						</div>

						<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-3.5">
							<div>
								<div class="text-xs font-bold uppercase tracking-wider text-[#515f74] mb-1.5">
									Eligible Majors
								</div>
								<div class="flex flex-wrap gap-1.5">
									{#each partner.eligibleMajors as maj (maj)}
										<span class="px-2.5 py-0.5 rounded-full bg-[#e5eeff] text-[#003ea8] text-xs font-medium">{maj}</span>
									{:else}
										<span class="text-[#515f74]">All / Unspecified</span>
									{/each}
								</div>
							</div>

							{#if partner.excludedMajors.length > 0}
								<div>
									<div class="text-xs font-bold uppercase tracking-wider text-[#ba1a1a] mb-1.5">
										Excluded / Restricted Programs
									</div>
									<div class="flex flex-wrap gap-1.5">
										{#each partner.excludedMajors as ex (ex)}
											<span class="px-2.5 py-0.5 rounded-full bg-[#fef2f2] text-[#ba1a1a] text-xs font-medium">{ex}</span>
										{/each}
									</div>
								</div>
							{/if}

							<div class="grid grid-cols-1 sm:grid-cols-2 gap-3 pt-2 border-t border-[#c6c6cd]/30">
								<div>
									<span class="font-semibold text-[#0b1c30] block text-xs">Standing Requirement</span>
									<span class="text-[#515f74] text-xs">{showText(partner.minStandingNote, 'Standard AUC standing applies')}</span>
								</div>
								<div>
									<span class="font-semibold text-[#0b1c30] block text-xs">Credit Load &amp; Course Restrictions</span>
									<span class="text-[#515f74] text-xs">{showText(partner.creditSystemAndLoad, '')} {partner.courseRestrictionsNote}</span>
								</div>
							</div>
						</div>
					{:else if activeTab === 'calendar'}
						<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-3.5">
							<div>
								<span class="font-semibold text-[#0b1c30] block text-xs">Available Terms</span>
								<span class="text-[#515f74] text-xs sm:text-sm">
									{partner.availableTerms.length ? partner.availableTerms.join(', ') : 'Not specified'}
									{#if partner.finalSemesterAllowed !== null}
										· Final-semester applicants: {partner.finalSemesterAllowed ? 'Allowed' : 'Not permitted'}
									{/if}
								</span>
							</div>

							<div>
								<span class="font-semibold text-[#0b1c30] block text-xs">Nomination &amp; Application Deadlines</span>
								<span class="text-[#515f74] text-xs sm:text-sm">{showText(partner.nominationDeadlines)}</span>
							</div>

							<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
								<div class="p-3 rounded-xl bg-[#eff4ff]">
									<span class="font-semibold text-[#0b1c30] block text-xs">Fall Term Dates</span>
									<span class="text-[#515f74] text-xs sm:text-sm">{showText(partner.fallTermDates, 'Not stated')}</span>
								</div>
								<div class="p-3 rounded-xl bg-[#eff4ff]">
									<span class="font-semibold text-[#0b1c30] block text-xs">Spring Term Dates</span>
									<span class="text-[#515f74] text-xs sm:text-sm">{showText(partner.springTermDates, 'Not stated')}</span>
								</div>
							</div>

							{#if partner.requiredDocs.length > 0}
								<div class="pt-2 border-t border-[#c6c6cd]/30">
									<span class="font-semibold text-[#0b1c30] block text-xs mb-2">
										Host Nomination Documents ({partner.requiredDocs.length})
									</span>
									<div class="flex flex-wrap gap-1.5">
										{#each partner.requiredDocs as doc (doc)}
											<span class="px-2.5 py-1 rounded-lg bg-[#eff4ff] text-[#0b1c30] text-xs font-medium">{doc}</span>
										{/each}
									</div>
								</div>
							{/if}
						</div>
					{:else}
						<div class="p-4 rounded-xl bg-white border border-[#c6c6cd]/40 shadow-sm space-y-3.5">
							<div>
								<span class="font-semibold text-[#0b1c30] block text-xs">Instruction Languages</span>
								<span class="text-[#515f74] text-xs sm:text-sm">
									{partner.instructionLanguages.length ? partner.instructionLanguages.join(', ') : 'Not listed'}
									{partner.englishCourseAvailability ? ` (${partner.englishCourseAvailability} English availability)` : ''}
								</span>
							</div>

							<div>
								<span class="font-semibold text-[#0b1c30] block text-xs">English &amp; Host Language Proficiency</span>
								<span class="text-[#515f74] text-xs sm:text-sm">
									{showText(partner.englishProficiency, 'Standard AUC proof')}
									{partner.hostLanguageRequirement ? ` · Host lang: ${partner.hostLanguageRequirement}` : ''}
								</span>
							</div>

							<div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
								<div class="p-3 rounded-xl bg-[#eff4ff] space-y-1">
									<span class="font-semibold text-[#0b1c30] block text-xs">Housing &amp; Living</span>
									<p class="text-[#515f74] text-xs leading-relaxed">{showText(partner.housingNotes, 'See host portal')}</p>
								</div>
								<div class="p-3 rounded-xl bg-[#eff4ff] space-y-1">
									<span class="font-semibold text-[#0b1c30] block text-xs">Fees, Grants &amp; Visa</span>
									<p class="text-[#515f74] text-xs leading-relaxed">
										{showText(partner.mandatoryHostFees, '')} {partner.hostGrants} {partner.visaNotes}
									</p>
								</div>
							</div>

							{#if partner.citizenshipRestrictions || partner.academicStyle || partner.internationalSupport}
								<div class="space-y-1.5 pt-2 border-t border-[#c6c6cd]/30 text-xs text-[#515f74]">
									{#if partner.citizenshipRestrictions}
										<div><strong class="text-[#0b1c30]">Citizenship Rules:</strong> {partner.citizenshipRestrictions}</div>
									{/if}
									{#if partner.academicStyle}
										<div><strong class="text-[#0b1c30]">Academic Style:</strong> {partner.academicStyle}</div>
									{/if}
									{#if partner.internationalSupport}
										<div><strong class="text-[#0b1c30]">International Support:</strong> {partner.internationalSupport}</div>
									{/if}
								</div>
							{/if}
						</div>
					{/if}
				</div>

				<!-- Footer -->
				<div class="px-5 py-3.5 border-t border-[#c6c6cd]/40 bg-[#eff4ff] flex flex-wrap items-center justify-between gap-2">
					<div class="flex flex-wrap items-center gap-2">
						{#if onViewPlacedStudents}
							<button
								type="button"
								onclick={() => onViewPlacedStudents(partner.slug)}
								class="h-9 px-3.5 rounded-xl bg-white hover:bg-[#dce9ff] border border-[#c6c6cd]/50 text-xs font-semibold text-[#0b1c30] whitespace-nowrap cursor-pointer"
							>
								View Placed Students
							</button>
						{/if}
						{#if partner.officialWebsiteUrl}
							<a
								href={partner.officialWebsiteUrl}
								target="_blank"
								rel="noreferrer"
								class="px-3 py-2 rounded-xl bg-white hover:bg-[#dce9ff] border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8]"
							>
								Website ↗
							</a>
						{/if}
						{#if partner.exchangePageUrl}
							<a
								href={partner.exchangePageUrl}
								target="_blank"
								rel="noreferrer"
								class="px-3 py-2 rounded-xl bg-white hover:bg-[#dce9ff] border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8]"
							>
								Exchange Portal ↗
							</a>
						{/if}
						{#if partner.courseCatalogUrl}
							<a
								href={partner.courseCatalogUrl}
								target="_blank"
								rel="noreferrer"
								class="px-3 py-2 rounded-xl bg-white hover:bg-[#dce9ff] border border-[#c6c6cd]/40 text-xs font-semibold text-[#003ea8]"
							>
								Courses ↗
							</a>
						{/if}
					</div>

					<button
						type="button"
						onclick={onClose}
						class="h-10 px-5 bg-[#131b2e] hover:bg-[#1e293b] text-white rounded-xl text-xs sm:text-sm font-semibold shadow-sm cursor-pointer"
					>
						Close Dossier
					</button>
				</div>
			{/if}
		</div>
	</div>
{/if}
