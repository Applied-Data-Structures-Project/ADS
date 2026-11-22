<script lang="ts">
  import { onMount } from 'svelte';
  import { page } from '$app/state';
  import PageHeader from '#lib/components/student/PageHeader.svelte';
  import Icon from '#lib/components/student/Icon.svelte';
  import { catalog } from '#lib/explore/catalog.svelte.ts';
  import { daysLeft, demoMetrics, formatDeadline, formatUsd, SAMPLE_METRICS_NOTE } from '#lib/explore/metrics.ts';
  import { rememberUniversity, shown, textList, statusLabel, loadQsRankings, qsDisplay, type QsMatches, type UniversityRecord } from '#lib/explore/universities.ts';
  import { discovery } from '#lib/student/discovery.svelte.ts';
  import { store } from '#lib/student/store.svelte.ts';

  let uni = $state<UniversityRecord | null>(null);
  let rankings = $state<QsMatches>({});
  let loading = $state(true);
  let error = $state('');
  let tab = $state<'overview' | 'academics' | 'living' | 'resources'>('overview');
  const tabs = [
    { id: 'overview', label: 'Overview' },
    { id: 'academics', label: 'Academics & requirements' },
    { id: 'living', label: 'Living abroad' },
    { id: 'resources', label: 'Links & resources' }
  ] as const;

  const metrics = $derived(
    uni ? demoMetrics(uni, store.eligibilityProfile, store.application.budget) : null
  );

  // Present source notes as individual scan-friendly rows without editing the dataset.
  function detailLines(value: string | null | undefined): { label: string | null; text: string }[] {
    const raw = shown(value);
    return raw.split(/\s*;\s*|\n+/).map((part) => part.trim()).filter(Boolean).map((part) => {
      const labeled = part.match(/^([^:]{2,65}):\s+(.+)$/);
      return labeled ? { label: labeled[1], text: labeled[2] } : { label: null, text: part };
    });
  }

  function toggleSave() {
    if (!uni) return;
    const saved = discovery.toggleSaved(uni.slug);
    store.showToast(saved ? 'Saved to your list.' : 'Removed from your list.');
  }

  function toggleCompare() {
    if (!uni) return;
    if (discovery.isCompared(uni.slug)) {
      discovery.toggleCompare(uni.slug);
      return;
    }
    if (!discovery.toggleCompare(uni.slug)) {
      store.showToast('You can compare up to three universities at a time.');
    }
  }

  onMount(async () => {
    try {
      const [, qs] = await Promise.all([catalog.load(), loadQsRankings()]);
      rankings = qs;
      uni = catalog.bySlug(page.params.slug ?? '') ?? null;
      if (uni) rememberUniversity(uni.slug);
    } catch (err) { error = err instanceof Error ? err.message : 'Unable to load university'; }
    finally { loading = false; }
  });
</script>

<svelte:head><title>{uni?.official_name.value ?? 'University Details'} · AUC Exchange</title></svelte:head>

{#snippet readableDetails(value: string | null | undefined)}
  <div class="mt-2 space-y-2">
    {#each detailLines(value) as line}
      <div class="rounded-lg border border-[#dce4f0] bg-[#f8faff] px-3.5 py-3 text-sm leading-6">
        {#if line.label}
          <div class="mb-1 font-semibold text-[#0b1c30]">{line.label}</div>
        {/if}
        <div class="text-[#515f74] break-words">{line.text}</div>
      </div>
    {/each}
  </div>
{/snippet}

<section class="space-y-5 pb-10">
  <a href="/explore" class="inline-flex items-center gap-2 text-sm font-semibold text-[#003ea8] hover:underline"><Icon name="arrow-left" class="w-4 h-4" /> Back to Explore Universities</a>
  {#if loading}
    <div class="h-60 rounded-2xl bg-white border border-[#c6c6cd]/50 animate-pulse"></div>
  {:else if error}
    <div role="alert" class="rounded-xl border border-red-200 bg-red-50 p-4 text-red-800">{error}</div>
  {:else if !uni}
    <div class="rounded-xl bg-white border border-[#c6c6cd]/50 p-8"><h1 class="text-xl font-bold">University not found</h1><p class="mt-2 text-[#515f74]">This university is not in the currently loaded partner dataset.</p></div>
  {:else}
    <PageHeader crumbs={['Discover', 'Explore Universities', 'University Details']} title={uni.official_name.value ?? 'University'} subtitle={`${shown(uni.city.value)}, ${shown(uni.country.value)} · ${shown(uni.region.value)}`} />
    <div class="rounded-2xl bg-[#0b1c30] text-white px-5 py-6 sm:px-7 relative overflow-hidden">
      <div class="absolute w-64 h-64 rounded-full bg-[#405d87]/30 -right-16 -top-24"></div>
      <div class="relative flex flex-col sm:flex-row sm:justify-between gap-5 sm:items-center"><div class="space-y-3"><span class="inline-flex items-center gap-2 bg-white/10 rounded-lg px-3 py-2 text-xs font-semibold"><Icon name="globe" class="w-4 h-4"/> {shown(uni.region.value)} · {statusLabel(uni.agreement_status.value)}</span><p class="text-xl sm:text-2xl font-bold max-w-2xl">Study abroad at {uni.official_name.value}</p><p class="text-sm text-[#d0daea]">{shown(uni.agreement_scope_raw.value)} · {textList(uni.agreement_student_levels)}</p></div>
      <div class="shrink-0 flex flex-col gap-2 sm:items-end">
        <div class="flex gap-2">
          <button type="button" onclick={toggleSave} aria-pressed={discovery.isSaved(uni.slug)} class="rounded-xl px-4 py-2.5 text-xs font-semibold inline-flex items-center gap-1.5 cursor-pointer {discovery.isSaved(uni.slug) ? 'bg-white text-[#0b1c30]' : 'bg-white/15 text-white hover:bg-white/25'}">
            <Icon name="saved" class="w-3.5 h-3.5" /> {discovery.isSaved(uni.slug) ? 'Saved' : 'Save'}
          </button>
          <button type="button" onclick={toggleCompare} aria-pressed={discovery.isCompared(uni.slug)} class="rounded-xl px-4 py-2.5 text-xs font-semibold inline-flex items-center gap-1.5 cursor-pointer {discovery.isCompared(uni.slug) ? 'bg-white text-[#0b1c30]' : 'bg-white/15 text-white hover:bg-white/25'}">
            <Icon name="compare" class="w-3.5 h-3.5" /> {discovery.isCompared(uni.slug) ? 'In compare' : 'Compare'}
          </button>
        </div>
        {#if uni.official_website_url.value}<a target="_blank" rel="noopener noreferrer" href={uni.official_website_url.value} class="shrink-0 rounded-xl bg-white px-4 py-3 text-[#0b1c30] font-semibold text-sm hover:bg-[#eff4ff]">Official website ↗</a>{/if}
      </div></div>
    </div>
    <section class="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-[#b8cbe9] bg-[#eff4ff] p-4" aria-label="QS worldwide ranking">
      <div><p class="text-xs font-bold uppercase tracking-wide text-[#314c76]">Worldwide ranking</p><h2 class="mt-1 text-lg sm:text-xl font-bold text-[#0b1c30]">QS World University Rankings 2027</h2></div>
      <p class="text-lg sm:text-2xl font-bold text-[#0b1c30]">{qsDisplay(rankings[uni.slug])}</p>
      {#if !rankings[uni.slug]}<p class="w-full text-xs text-[#515f74]">No confirmed entry among the top 1,500 in the supplied QS 2027 list. Some institutions may appear under another name or a parent university.</p>{/if}
    </section>
    <div class="flex gap-1 overflow-x-auto rounded-xl border border-[#c6c6cd]/50 bg-white p-1.5" aria-label="University information tabs">
      {#each tabs as t}<button type="button" onclick={() => tab = t.id} class="shrink-0 rounded-lg px-3 sm:px-4 py-2.5 text-xs sm:text-sm font-semibold transition-colors {tab === t.id ? 'bg-[#0b1c30] text-white' : 'text-[#515f74] hover:bg-[#eff4ff]'}" aria-current={tab === t.id ? 'page' : undefined}>{t.label}</button>{/each}
    </div>
    <div class="grid grid-cols-1 xl:grid-cols-[minmax(0,2fr)_minmax(260px,1fr)] gap-4">
      <div class="space-y-4">
        {#if tab === 'overview'}
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 shadow-sm space-y-4"><h2 class="font-bold text-lg">At a glance</h2><dl class="grid sm:grid-cols-2 gap-5 text-sm"><div><dt class="text-[#515f74] text-xs font-semibold">Country & city</dt><dd class="mt-1 font-medium">{shown(uni.city.value)}, {shown(uni.country.value)}</dd></div><div><dt class="text-[#515f74] text-xs font-semibold">Student levels listed</dt><dd class="mt-1 font-medium">{textList(uni.agreement_student_levels)}</dd></div><div><dt class="text-[#515f74] text-xs font-semibold">Available semesters</dt><dd class="mt-1 font-medium">{textList(uni.available_terms)}</dd></div><div><dt class="text-[#515f74] text-xs font-semibold">Agreement type</dt><dd class="mt-1 font-medium">{shown(uni.agreement_scope_raw.value)}</dd></div><div><dt class="text-[#515f74] text-xs font-semibold">Teaching languages</dt><dd class="mt-1 font-medium">{textList(uni.instruction_languages)}</dd></div><div><dt class="text-[#515f74] text-xs font-semibold">Minimum host GPA</dt><dd class="mt-1 font-medium">{uni.min_gpa_host.value ?? 'Not verified in dataset'}</dd></div></dl></section>
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 space-y-4"><h2 class="font-bold text-lg">Study experience</h2><div class="space-y-3 text-sm"><div><h3 class="font-semibold">Academic style</h3>{@render readableDetails(uni.academic_style.value)}</div><div><h3 class="font-semibold">Campus setting</h3><p class="mt-1 text-[#515f74]">{uni.campus_type.value === 'multi_campus' ? 'Multiple campuses' : shown(uni.campus_type.value)?.replaceAll('_', ' ')}</p></div><div><h3 class="font-semibold">International student support</h3>{@render readableDetails(uni.international_support_services.value)}</div></div></section>
        {:else if tab === 'academics'}
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 space-y-5"><h2 class="font-bold text-lg">Academic requirements</h2><div class="grid sm:grid-cols-2 gap-4 text-sm"><div><h3 class="text-xs font-semibold text-[#515f74]">AUC schools listed</h3><p class="mt-1">{textList(uni.eligible_auc_schools)}</p></div><div><h3 class="text-xs font-semibold text-[#515f74]">Majors listed in agreement</h3><p class="mt-1">{textList(uni.eligible_majors)}</p></div><div><h3 class="text-xs font-semibold text-[#515f74]">Excluded majors</h3><p class="mt-1">{textList(uni.excluded_majors)}</p></div><div><h3 class="text-xs font-semibold text-[#515f74]">Host minimum GPA</h3><p class="mt-1">{uni.min_gpa_host.value ?? 'Not verified in dataset'}</p></div></div><div><h3 class="font-semibold text-sm">Academic standing</h3>{@render readableDetails(uni.min_standing_note.value)}</div><div><h3 class="font-semibold text-sm">Course restrictions</h3>{@render readableDetails(uni.course_restrictions_note.value)}</div><div><h3 class="font-semibold text-sm">Credit system & course load</h3>{@render readableDetails(uni.credit_system_and_load.value)}</div></section>
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 space-y-4"><h2 class="font-bold text-lg">Language & documents</h2><div class="text-sm space-y-4"><div><h3 class="font-semibold">Languages of instruction</h3><p class="mt-1 text-[#515f74]">{textList(uni.instruction_languages)}</p></div><div><h3 class="font-semibold">English proficiency</h3>{@render readableDetails(uni.english_proficiency_and_rhet1020.value)}</div><div><h3 class="font-semibold">Host language requirement</h3>{@render readableDetails(uni.host_language_requirement.value)}</div><div><h3 class="font-semibold">Required application documents</h3>{@render readableDetails(uni.required_application_docs.value?.join("; ") ?? null)}</div></div></section>
        {:else if tab === 'living'}
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 space-y-5"><h2 class="font-bold text-lg">Costs, housing & travel</h2><div class="grid sm:grid-cols-2 gap-4 text-sm"><div class="rounded-xl bg-[#eff4ff] p-4"><h3 class="text-xs font-semibold text-[#515f74]">Estimated monthly living costs</h3><p class="text-lg font-bold mt-2">{uni.est_monthly_living_local.value ? `${uni.est_monthly_living_local.value.min.toLocaleString()}–${uni.est_monthly_living_local.value.max.toLocaleString()} ${uni.est_monthly_living_local.value.currency}` : 'Not available'}</p></div><div class="rounded-xl bg-[#eff4ff] p-4"><h3 class="text-xs font-semibold text-[#515f74]">Tuition model</h3><p class="mt-2 font-semibold">{shown(uni.tuition_model.value).replaceAll('_', ' ')}</p></div></div><div class="space-y-4 text-sm"><div><h3 class="font-semibold">Housing availability</h3>{@render readableDetails(uni.housing_availability.value)}</div><div><h3 class="font-semibold">Housing details</h3>{@render readableDetails(uni.housing_notes.value)}</div><div><h3 class="font-semibold">Host fees and insurance</h3>{@render readableDetails(uni.mandatory_host_fees_and_insurance.value)}</div><div><h3 class="font-semibold">Scholarships, grants & stipends</h3>{@render readableDetails(uni.host_grants_or_stipends.value)}</div><div><h3 class="font-semibold">Visa & financial proof</h3>{@render readableDetails(uni.visa_lead_time_and_financial_proof.value)}</div></div></section>
        {:else}
          <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 sm:p-6 space-y-5"><h2 class="font-bold text-lg">Official university resources</h2><p class="text-sm text-[#515f74]">Explore host university pages directly for the latest academic information.</p><div class="space-y-3">{#if uni.official_website_url.value}<a class="block rounded-xl border border-[#c6c6cd]/50 p-4 text-sm font-semibold text-[#003ea8] hover:bg-[#eff4ff]" href={uni.official_website_url.value} target="_blank" rel="noopener noreferrer">Official university website ↗</a>{/if}{#if uni.exchange_page_url.value}<a class="block rounded-xl border border-[#c6c6cd]/50 p-4 text-sm font-semibold text-[#003ea8] hover:bg-[#eff4ff]" href={uni.exchange_page_url.value} target="_blank" rel="noopener noreferrer">Incoming exchange page ↗</a>{/if}{#if uni.course_catalog_url.value}<a class="block rounded-xl border border-[#c6c6cd]/50 p-4 text-sm font-semibold text-[#003ea8] hover:bg-[#eff4ff]" href={uni.course_catalog_url.value} target="_blank" rel="noopener noreferrer">Course catalog ↗</a>{/if}{#if !uni.official_website_url.value && !uni.exchange_page_url.value && !uni.course_catalog_url.value}<p class="text-sm text-[#515f74]">No official links available.</p>{/if}</div><section class="space-y-3 border-t border-[#dce4f0] pt-5">
            <h3 class="font-bold text-base">Nomination deadlines</h3>
            {@render readableDetails(uni.nomination_deadlines.value)}
          </section>
          <section class="space-y-3 border-t border-[#dce4f0] pt-5">
            <h3 class="font-bold text-base">Semester dates</h3>
            <div class="grid gap-3 lg:grid-cols-2">
              <div class="rounded-xl border border-[#dce4f0] p-4">
                <h4 class="font-bold text-sm">Fall semester record</h4>
                {@render readableDetails(uni.fall_term_dates.value)}
              </div>
              <div class="rounded-xl border border-[#dce4f0] p-4">
                <h4 class="font-bold text-sm">Spring semester record</h4>
                {@render readableDetails(uni.spring_term_dates.value)}
              </div>
            </div>
            <p class="text-xs text-[#67748a]">Host universities may use different names for their academic terms. The descriptions above are preserved from the source dataset.</p>
          </section></section>
        {/if}
      </div>
      <aside class="space-y-4">{#if metrics}<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 space-y-3"><div class="flex items-center justify-between gap-2"><h2 class="font-bold text-base">Sample comparison data</h2><span class="rounded-lg px-2.5 py-1 text-[11px] font-bold {metrics.qualification ? 'bg-[#edf8f1] text-[#247047]' : 'bg-[#fff6e3] text-[#986b1a]'}">{metrics.qualification ? 'You qualify' : 'Check rules'}</span></div><dl class="grid grid-cols-2 gap-2 text-xs"><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">Fit score</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{metrics.fit}%</dd></div><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">Semester cost</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{formatUsd(metrics.semesterCostUsd)}</dd></div><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">Deadline</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{formatDeadline(metrics.deadline)}</dd><dd class="text-[11px] text-[#515f74]">{daysLeft(metrics.deadline) < 0 ? 'Closed' : `${daysLeft(metrics.deadline)} days left`}</dd></div><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">Seats</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{metrics.seats}</dd></div><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">GPA needed</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{metrics.gpaRequired.toFixed(2)}</dd></div><div class="rounded-lg bg-[#f8faff] border border-[#dce4f0] px-3 py-2"><dt class="text-[#515f74]">IELTS</dt><dd class="mt-0.5 font-bold text-[#0b1c30] font-mono-num">{metrics.ielts.toFixed(1)}</dd></div></dl><p class="text-[11px] text-[#657087]">{SAMPLE_METRICS_NOTE}</p></section>{/if}<section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 space-y-4"><h2 class="font-bold text-base">Planning your exchange</h2><p class="text-sm text-[#515f74]">Review the university information, then select your three preferences in the application form.</p><a href="/apply?step=3" class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-[#0b1c30] text-white px-4 py-3 text-sm font-semibold hover:bg-[#233853]">Go to preferences <Icon name="arrow-right" class="w-4 h-4" /></a></section><section class="rounded-2xl border border-[#c6c6cd]/50 bg-[#eff4ff] p-5 space-y-2"><div class="font-bold text-sm flex items-center gap-2"><Icon name="alert" class="w-4 h-4"/> Confirm details with IPSO</div><p class="text-sm text-[#515f74]">Academic information and course restrictions are provided for reference. IPSO and the host university must confirm the latest requirements and seat availability.</p></section><section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 space-y-2"><h2 class="font-bold text-sm">QS World University Ranking (2027)</h2><p class="text-lg font-bold text-[#0b1c30]">{qsDisplay(rankings[uni.slug])}</p><p class="text-xs text-[#515f74]">{rankings[uni.slug] ? `QS 2027 source name: ${rankings[uni.slug].name}. Ranking taken from the supplied QS spreadsheet.` : "No confirmed name match in the supplied QS 2027 top-1,500 list. This is not proof the institution is unranked; alternative or parent institution names may need review."}</p><a class="inline-block text-sm font-semibold text-[#003ea8] hover:underline" href="https://www.topuniversities.com/world-university-rankings" target="_blank" rel="noopener noreferrer">View official QS rankings ↗</a></section></aside>
    </div>
  {/if}
</section>
