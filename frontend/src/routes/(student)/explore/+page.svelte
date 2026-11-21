<script lang="ts">
  import { onMount } from 'svelte';
  import PageHeader from '#lib/components/student/PageHeader.svelte';
  import Icon from '#lib/components/student/Icon.svelte';
  import { loadUniversities, shown, textList, recentSlugs, statusLabel, loadQsRankings, qsSortKey, qsDisplay, type QsMatches, type UniversityRecord } from '#lib/explore/universities.ts';

  let universities = $state<UniversityRecord[]>([]);
  let rankings = $state<QsMatches>({});
  let recent = $state<string[]>([]);
  let loading = $state(true);
  let error = $state('');
  let query = $state('');
  let country = $state('All countries');
  let region = $state('All regions');
  let level = $state('All levels');
  let term = $state('Any term');
  let sortBy = $state('name');
  let showCount = $state(18);

  const countries = $derived(['All countries', ...Array.from(new Set(universities.map((u) => u.country.value).filter((v): v is string => Boolean(v)))).sort()]);
  const regions = $derived(['All regions', ...Array.from(new Set(universities.map((u) => u.region.value).filter((v): v is string => Boolean(v)))).sort()]);
  const recentlyViewed = $derived(recent.map((slug) => universities.find((u) => u.slug === slug)).filter((u): u is UniversityRecord => Boolean(u)));
  const filtered = $derived.by(() => {
    const needle = query.trim().toLowerCase();
    const matches = universities.filter((u) => {
      const searchable = [u.official_name.value, u.short_name.value, u.country.value, u.city.value, u.region.value, u.agreement_scope_raw.value].filter(Boolean).join(' ').toLowerCase();
      const terms = (u.available_terms.value ?? []).join(' ').toLowerCase();
      return (!needle || searchable.includes(needle))
        && (country === 'All countries' || u.country.value === country)
        && (region === 'All regions' || u.region.value === region)
        && (level === 'All levels' || (u.agreement_student_levels.value ?? []).includes(level))
        && (term === 'Any term' || terms.includes(term.toLowerCase()));
    });
    matches.sort((a, b) => {
      if (sortBy === 'qs') {
        const aRank = qsSortKey(rankings[a.slug]?.rank);
        const bRank = qsSortKey(rankings[b.slug]?.rank);
        return (aRank === bRank ? 0 : aRank < bRank ? -1 : 1) || (a.official_name.value ?? '').localeCompare(b.official_name.value ?? '');
      }
      if (sortBy === 'country') return `${a.country.value ?? ''} ${a.official_name.value ?? ''}`.localeCompare(`${b.country.value ?? ''} ${b.official_name.value ?? ''}`);
      if (sortBy === 'city') return `${a.city.value ?? ''} ${a.official_name.value ?? ''}`.localeCompare(`${b.city.value ?? ''} ${b.official_name.value ?? ''}`);
      return (a.official_name.value ?? '').localeCompare(b.official_name.value ?? '');
    });
    return matches;
  });
  const activeFilters = $derived(query.trim() !== '' || country !== 'All countries' || region !== 'All regions' || level !== 'All levels' || term !== 'Any term');
  function resetFilters() { query = ''; country = 'All countries'; region = 'All regions'; level = 'All levels'; term = 'Any term'; showCount = 18; }
  function resetCount() { showCount = 18; }

  onMount(async () => {
    recent = recentSlugs();
    try { universities = await loadUniversities(); rankings = await loadQsRankings(); }
    catch (err) { error = err instanceof Error ? err.message : 'Unable to load universities'; }
    finally { loading = false; }
  });
</script>

<svelte:head><title>Explore Universities · AUC Exchange Matcher</title></svelte:head>

<section class="space-y-6 pb-10">
  <PageHeader crumbs={['Discover', 'Explore Universities']} title="Explore Partner Universities" subtitle="Find your next exchange destination. Browse AUC partners, review the requirements, and explore life abroad." />

  <div class="rounded-2xl bg-[#0b1c30] px-5 py-6 sm:px-7 sm:py-7 text-white relative overflow-hidden">
    <div class="absolute -right-16 -top-24 w-72 h-72 rounded-full bg-[#3b5787]/30 pointer-events-none"></div>
    <div class="relative max-w-2xl space-y-2">
      <div class="inline-flex items-center gap-2 text-xs font-semibold text-[#cad9f4] uppercase tracking-wider"><Icon name="globe" class="w-4 h-4"/> AUC international opportunities</div>
      <h2 class="text-xl sm:text-2xl font-bold tracking-tight">Your world of possibilities starts here.</h2>
      <p class="text-sm text-[#d0daea] leading-relaxed">Explore university information compiled from AUC partner listings, host fact sheets, and official websites. Some details still require confirmation.</p>
      <div class="flex flex-wrap gap-2 pt-2 text-xs font-semibold">
        <span class="rounded-lg bg-white/15 px-3 py-2">{universities.length || '155'} partner records</span>
        <span class="rounded-lg bg-white/15 px-3 py-2">{countries.length - 1 || '—'} countries</span>
        <span class="rounded-lg bg-white/15 px-3 py-2">Undergraduate & graduate</span>
      </div>
    </div>
  </div>

  <section class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-4 sm:p-5 shadow-sm space-y-4" aria-label="University search and filters">
    <div class="relative">
      <span class="absolute left-3.5 top-1/2 -translate-y-1/2 text-[#515f74]"><Icon name="search" class="w-[18px] h-[18px]" /></span>
      <input aria-label="Search universities" bind:value={query} oninput={resetCount} placeholder="Search universities, countries, cities, or academic scopes..." class="w-full h-12 rounded-xl border border-[#c6c6cd]/60 bg-white pl-11 pr-4 text-sm text-[#0b1c30] placeholder:text-[#78869a] focus:border-[#0b1c30] focus:ring-0" />
    </div>
    <div class="grid grid-cols-2 lg:grid-cols-5 gap-3">
      <label class="space-y-1.5"><span class="block text-[11px] font-bold tracking-wide text-[#515f74]">COUNTRY</span>
        <select bind:value={country} onchange={resetCount} class="w-full h-10 rounded-xl border-[#c6c6cd]/60 text-xs sm:text-sm bg-white text-[#0b1c30]">{#each countries as c}<option value={c}>{c}</option>{/each}</select></label>
      <label class="space-y-1.5"><span class="block text-[11px] font-bold tracking-wide text-[#515f74]">REGION</span>
        <select bind:value={region} onchange={resetCount} class="w-full h-10 rounded-xl border-[#c6c6cd]/60 text-xs sm:text-sm bg-white text-[#0b1c30]">{#each regions as r}<option value={r}>{r}</option>{/each}</select></label>
      <label class="space-y-1.5"><span class="block text-[11px] font-bold tracking-wide text-[#515f74]">STUDENT LEVEL</span>
        <select bind:value={level} onchange={resetCount} class="w-full h-10 rounded-xl border-[#c6c6cd]/60 text-xs sm:text-sm bg-white text-[#0b1c30]"><option value="All levels">All levels</option><option value="UG">Undergraduate</option><option value="GR">Graduate</option></select></label>
      <label class="space-y-1.5"><span class="block text-[11px] font-bold tracking-wide text-[#515f74]">TERM</span>
        <select bind:value={term} onchange={resetCount} class="w-full h-10 rounded-xl border-[#c6c6cd]/60 text-xs sm:text-sm bg-white text-[#0b1c30]"><option>Any term</option><option>Fall</option><option>Spring</option><option>Summer</option><option>Academic Year</option></select></label>
      <label class="space-y-1.5"><span class="block text-[11px] font-bold tracking-wide text-[#515f74]">SORT BY</span>
        <select bind:value={sortBy} class="w-full h-10 rounded-xl border-[#c6c6cd]/60 text-xs sm:text-sm bg-white text-[#0b1c30]"><option value="name">University name</option><option value="country">Country</option><option value="city">City</option><option value="qs">QS 2027 ranking</option></select></label>
    </div>
    {#if activeFilters}<div class="flex items-center justify-between gap-3"><p class="text-xs text-[#515f74]">Showing results matching your filters</p><button type="button" onclick={resetFilters} class="text-xs font-bold text-[#003ea8] hover:underline">Clear all filters</button></div>{/if}
  </section>

  {#if !loading && recentlyViewed.length && !activeFilters}
    <section class="space-y-3">
      <h2 class="text-lg font-bold text-[#0b1c30]">Recently viewed</h2>
      <div class="flex gap-3 overflow-x-auto pb-1">
        {#each recentlyViewed as uni (uni.slug)}
          <a class="min-w-[215px] max-w-[260px] flex-1 rounded-xl border border-[#c6c6cd]/50 bg-white p-3.5 hover:border-[#8093ba] transition-colors" href={`/explore/university/${uni.slug}`}>
            <div class="flex items-center gap-2 text-sm font-semibold text-[#0b1c30]"><Icon name="globe" class="w-4 h-4 shrink-0"/><span class="line-clamp-1">{uni.official_name.value}</span></div>
            <p class="text-xs text-[#515f74] mt-1">{shown(uni.city.value)} · {shown(uni.country.value)}</p>
          </a>
        {/each}
      </div>
    </section>
  {/if}


  <div class="flex flex-wrap justify-between items-end gap-3">
    <div><h2 class="font-bold text-xl text-[#0b1c30]">Partner universities</h2><p class="text-sm text-[#515f74] mt-1">{loading ? 'Loading universities…' : `${filtered.length} universities found`}</p></div>
    <span class="rounded-lg border border-[#c6c6cd]/50 bg-white px-3 py-2 text-xs text-[#515f74]">Availability and requirements should be confirmed with IPSO</span>
  </div>
  {#if error}
    <div role="alert" class="rounded-xl border border-red-200 bg-red-50 p-5 text-red-800">{error}</div>
  {:else if loading}
    <div class="grid sm:grid-cols-2 xl:grid-cols-3 gap-4">{#each [1,2,3,4,5,6] as i}<div class="h-64 rounded-2xl border border-[#c6c6cd]/40 bg-white animate-pulse" aria-label="Loading card"></div>{/each}</div>
  {:else if filtered.length === 0}
    <div class="rounded-2xl border border-[#c6c6cd]/50 bg-white p-10 text-center space-y-3"><div class="flex justify-center text-[#515f74]"><Icon name="search" class="w-9 h-9" /></div><h3 class="text-lg font-bold">No universities found</h3><p class="text-sm text-[#515f74]">Try another search or remove some filters.</p><button type="button" onclick={resetFilters} class="rounded-xl bg-[#0b1c30] text-white px-5 py-2.5 font-semibold text-sm">Clear filters</button></div>
  {:else}
    <div class="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-4">
      {#each filtered.slice(0, showCount) as uni (uni.slug)}
        <article class="flex flex-col rounded-2xl border border-[#c6c6cd]/50 bg-white p-5 shadow-sm hover:border-[#a4b3d0] hover:shadow-md transition-all min-h-[265px]">
          <div class="flex items-start justify-between gap-3"><div class="rounded-xl bg-[#eff4ff] text-[#0b1c30] w-11 h-11 flex justify-center items-center"><Icon name="globe" class="w-5 h-5"/></div><span class="rounded-full px-2.5 py-1 text-[11px] font-semibold {uni.agreement_status.value === 'active' ? 'bg-[#edf8f1] text-[#247047]' : 'bg-[#fff6e3] text-[#986b1a]'}">{statusLabel(uni.agreement_status.value)}</span></div>
          <div class="mt-4"><h3 class="text-[16px] font-bold leading-snug text-[#0b1c30]">{uni.official_name.value}</h3><p class="mt-1.5 text-sm text-[#515f74]">{shown(uni.city.value)}, {shown(uni.country.value)}</p></div>
          <div class="mt-4 flex flex-wrap gap-1.5"><span class="rounded-lg bg-[#eff4ff] px-2.5 py-1 text-xs text-[#314c76]">{textList(uni.agreement_student_levels)}</span><span class="rounded-lg bg-[#f2f3f7] px-2.5 py-1 text-xs text-[#515f74]">{shown(uni.region.value)}</span></div>
          <div class="mt-3 text-xs text-[#515f74] space-y-1"><p><strong class="text-[#0b1c30]">Terms:</strong> {textList(uni.available_terms)}</p><p class="line-clamp-2"><strong class="text-[#0b1c30]">Agreement:</strong> {shown(uni.agreement_scope_raw.value)}</p></div>
          <div class="mt-3 rounded-xl border border-[#d9e3f5] bg-[#f3f7ff] px-3 py-2.5 flex items-center justify-between gap-3">
            <span class="text-xs font-semibold text-[#314c76]">QS World University Ranking 2027</span>
            <span class="text-sm font-bold text-[#0b1c30] text-right">{qsDisplay(rankings[uni.slug])}</span>
          </div>
          {#if !rankings[uni.slug]}<p class="mt-1 text-[11px] text-[#657087]">No confirmed QS entry; name differences may require checking.</p>{/if}
          <div class="mt-auto pt-4 flex gap-2">
            <a href={`/explore/university/${uni.slug}`} class="flex-1 inline-flex justify-center items-center gap-2 rounded-xl bg-[#0b1c30] text-white hover:bg-[#233853] py-2.5 text-sm font-semibold">View university <Icon name="arrow-right" class="w-4 h-4" /></a></div>
        </article>
      {/each}
    </div>
    {#if filtered.length > showCount}<div class="text-center"><button type="button" onclick={() => showCount += 18} class="rounded-xl bg-white border border-[#c6c6cd]/60 px-6 py-3 text-[#0b1c30] text-sm font-semibold hover:bg-[#eff4ff]">Load more universities ({filtered.length - showCount} remaining)</button></div>{/if}
  {/if}
</section>
