# Compare + Saved frontend design

**Date:** 2026-11-21  
**Status:** Approved; implemented as a frontend prototype  
**Scope:** Student portal frontend only. No C++ or API changes.

## Intent

Add Group 2's Compare + Saved experience to the student portal:

- save partner universities from Explore
- compare up to three side by side, with the best value in each row highlighted
- see estimated cost against the student's budget
- see saved schools ordered by nearest deadline, best fit, or lowest cost
- see the top three recommended schools
- see notifications and recommendation-letter status on a Me page

The template's screens are the visual target. The real 155-school dataset does not contain most of the numeric comparison data, so this design adds a clearly labelled demo-metrics layer for the frontend prototype.

## Goals

1. One student flow from Explore to Saved to Compare to application preferences.
2. Saved schools and application preferences use the same university identity: the slug.
3. The template's six screens are represented in the existing student design system.
4. The template's named data structures appear as real, tested TypeScript modules:
   - merge sort for saved-list ordering
   - max-heap for recommendations
   - min-heap for nearest deadlines
   - queue for notifications
5. Demo data is visibly labelled and never presented as verified IPSO information.
6. Saved schools, compare selection, budget, and notification read state persist per account in `localStorage`.

## Non-goals

- No C++ services, database migrations, API routes, or real emails.
- No real tuition, seat, fit-score, or deadline feed. Values that cannot come from `universities.json` are demo estimates.
- No redesign of the application, status, letters, or eligibility-profile pages beyond the preference migration.
- No Microsoft authentication changes.
- No per-university eligibility tags beyond the demo "qualifies" check.
- No image/logo assets for 155 universities; use the existing initial/globe treatment.

## Anti-goals

- A saved list that does not actually control application preferences.
- Two university models (old numeric mock and new slug records) living side by side.
- Fake costs shown without a visible demo disclaimer.
- Mobile tab bar overflow; keep it to five tabs.
- Storage writes that overwrite another signed-in account's saved list.

## Current state

- Explore (`/explore`) loads `frontend/static/data/universities.json` (155 records, slug-keyed) and `qs-2027-matches.json` on mount. Detail pages use `slug`.
- Explore already has `save`/`compare` icons available in `Icon.svelte`, but no actions.
- `StudentStore` still has `savedIds: number[]` seeded from `DEMO_SAVED` and a 35-row numeric `UNIVERSITIES` mock. Application preferences are `[number, number, number]`.
- `/apply` step 3 selects from `store.saved`, which uses the numeric mock.
- `/apply/status` resolves preference names through `universityById`.
- No `/saved`, `/compare`, or `/me` route exists.
- The profile page (`/profile`) is the eligibility intake form, not a Me hub.
- Frontend tests already run with Vitest; `eligibility.test.ts` is the pattern.

## Approaches considered

### Option 1 — standalone Group 2 module

New saved/compare/Me pages with their own store and demo metrics; leave the application's numeric preference list untouched.

- Fast and low risk.
- Fails the core template behaviour: "Use in my application" cannot feed the real form. The portal keeps duplicate university models.

### Option 2 — unified discovery + application preferences (chosen)

Build the new pages and migrate saved schools and application preferences from numeric mock IDs to Explore slugs. The application step reads from the saved list.

- Template flow works end to end.
- Removes the duplicate university model.
- Aligns the frontend with the backend, which already stores preference slugs.
- Touches `store.svelte.ts`, `types.ts`, `mocks.ts`, `/apply`, and `/apply/status`, so those need regression checks.

### Option 3 — full student-domain rewrite

One new model and store for Explore, Saved, Compare, Apply, Profile, and Letters.

- Cleanest long term, but a large refactor with unnecessary risk to the G3 application and letters features.

**Decision:** Option 2. The rest of this document assumes it.

## Architecture

### University catalog

Add `frontend/src/lib/explore/catalog.svelte.ts`.

```ts
class UniversityCatalog {
  records = $state<UniversityRecord[]>([]);
  status = $state<'idle' | 'loading' | 'ready' | 'error'>('idle');
  error = $state('');
  load(): Promise<void>;           // idempotent, caches the fetched array
  bySlug(slug: string): UniversityRecord | undefined;
  bySlugs(slugs: string[]): UniversityRecord[];
}
export const catalog = new UniversityCatalog();
```

- Wraps the existing `loadUniversities()` fetch.
- Loaded once from `(student)/+layout.svelte` on mount. Explore also calls it, so direct visits work.
- `bySlug` / `bySlugs` are synchronous lookups over the loaded array. Build an internal `Map<string, UniversityRecord>` when records arrive, not a linear scan per card.
- Because `records` is `$state`, derived values in stores and pages update when the catalog finishes loading.
- QS rankings stay in the Explore pages; they are not needed by Saved/Compare.

### Discovery store

Add `frontend/src/lib/student/discovery.svelte.ts`.

```ts
export interface StudentNotification {
  id: number;
  message: string;
  createdAt: string;   // ISO
  read: boolean;
}

class DiscoveryStore {
  savedSlugs = $state<string[]>([]);
  compareSlugs = $state<string[]>([]);     // max 3
  notifications = $state<StudentNotification[]>([]);
  initialized = $state(false);

  init(accountId: string): void;           // load + seed once
  toggleSaved(slug: string): boolean;      // returns new saved state
  isSaved(slug: string): boolean;
  toggleCompare(slug: string): boolean;    // false if already has 3
  isCompared(slug: string): boolean;
  clearCompare(): void;
  notify(message: string): void;
  markAllRead(): void;
  markOldestRead(): void;
  unreadCount = $derived(...);
  saved = $derived(catalog.bySlugs(this.savedSlugs));
  compare = $derived(catalog.bySlugs(this.compareSlugs));
}
export const discovery = new DiscoveryStore();
```

- Storage key: `auc-discovery-v1:<accountId>`.
- Stored JSON: `{ saved: string[], compare: string[], notifications: StudentNotification[] }`.
- Account key is `auth.account?.homeAccountId ?? auth.email`, same as eligibility.
- On first load for an account, seed:
  - saved: five real slugs that cover Europe/Asia/North America, for example `bocconi-university`, `sciences-po`, `aarhus-university`, `university-of-edinburgh`, `university-of-toronto` (verify slugs in the dataset).
  - compare: empty.
  - notifications: welcome message, "Your application is 43% complete", and "Review your saved universities before the deadline".
- Parse defensively: ignore non-string slugs, cap compare at 3, drop duplicate ids, default `read` to false. If storage is blocked, the session still works in memory.
- Saved changes and notification read state persist immediately.
- Avoid a circular import: `discovery` may import `catalog`, types, and storage helpers; `StudentStore` imports `discovery`.

### Demo metrics

Add `frontend/src/lib/explore/metrics.ts`, pure and deterministic.

```ts
export interface DemoMetrics {
  fit: number;                 // 35–98
  tuitionUsd: number;          // 0 when exchange tuition is waived
  livingMonthlyUsd: number;    // 550–1600
  semesterCostUsd: number;     // estimated one-semester cost
  deadline: string;            // ISO date
  seats: number;               // 1–8
  gpaRequired: number;         // real host minimum, or a seeded fallback
  ielts: number;               // 6.0, 6.5, 7.0, 7.5
  papers: number;              // 3–5
  english: string;
  qualification: boolean;      // demo "meets host requirements"
}
```

Rules:

- Seed from a stable hash of `slug` (FNV-1a). No `Math.random`, so reloads do not change numbers.
- Use real fields where they exist:
  - `tuitionUsd = 0` when `tuition_model.value === 'auc_exchange_waiver'`; otherwise a seeded 6,000–28,000 USD.
  - `livingMonthlyUsd` from `est_monthly_living_local` converted to USD with a static demo FX table; fall back to a seeded 550–1,400 USD.
  - `gpaRequired` from `min_gpa_host.value`, or a seeded 2.5 / 2.8 / 3.0 when the dataset has no value.
  - `english` from `english_proficiency_and_rhet1020.value` or "Not specified".
  - `papers` from `required_application_docs.value.length` when present, otherwise 3–5.
  - `deadline`, `seats`, and `ielts` are seeded demo values.
- `semesterCostUsd = round(tuitionUsd / 2 + livingMonthlyUsd * 5 + hostFees)` where `hostFees` is a seeded 250–900 USD. Label it "estimated semester cost"; the dataset does not contain program length.
- Demo deadline range: 2026-12-01 through 2027-02-28.
- FX table covers EUR, USD, GBP, CHF, JPY, NOK, SEK, CAD, CNY, AED, HKD, TND, PLN. Unknown currencies use 1.
- `demoMetrics(record, profile, budget)` computes fit:
  - start at 45
  - +15 when the agreement lists the student's level (or the list is empty)
  - +0–20 from GPA above the host minimum
  - +10 when the student's English requirement is complete: `englishComplete || rhet120`,
    because the UG form never sets `englishComplete`
  - +5 when the agreement is active
  - +0–8 seeded variation
  - +10 when the estimated cost is within the budget, if a budget is set
  - clamp to 35–98
- `qualification` is true when level is listed, GPA meets the host minimum, and the English requirement is met.
- Costs are in **USD**, matching the existing "Estimated budget per semester (USD)" field in `/apply` step 5. The design's earlier EUR naming was a unit mismatch; it is resolved here.
- Every page that shows these values includes: "Costs, seats, deadlines and fit are sample values for this frontend demo. IPSO confirms real figures."

## Data-structure modules

Add `frontend/src/lib/explore/structures.ts`:

- `mergeSort<T>(items: readonly T[], compare: (a: T, b: T) => number): T[]`
  - stable, O(n log n), does not mutate the input.
- `MinHeap<T>` with `push`, `pop`, `peek`, `size`
  - used for "nearest deadline" ordering.
- `MaxHeap<T>` with `push`, `pop`, `peek`, `size`
  - used for recommendation top-k.
- `topK<T>(items, score, k)`:
  - max-heap by score, pop k items.
- `NotificationQueue`:
  - FIFO `enqueue`, `dequeue`, `peek`, `size`, `toArray`.
  - Discovery store uses array state for persistence, but the queue module is the documented/tested structure behind ordering.

Add `frontend/src/lib/explore/compare.ts`:

- Row definitions for the comparison table.
- `bestValues(universities, rows)` returns which column is best per row.
- Rules:
  - fit: highest
  - tuition, living, semester cost: lowest
  - host GPA, IELTS, papers: lowest
  - deadline: latest (more time)
  - seats: highest
  - qualification: true
  - city and "vs. your budget": informational, never highlighted
- Ties highlight every tied best value. Missing values never win.

## Application preference migration

### Types and mocks

- `Application.pref` becomes `[string, string, string]`; `''` means not chosen.
- `DEMO_APPLICATION.pref` uses real slugs, e.g. `['sciences-po', 'bocconi-university', '']`.
- Remove `DEMO_SAVED`, `UNIVERSITIES`, `universityById`, and `acceptsLevel` once nothing imports them.
- `DEMO_STUDENT` stays.

### Student store

- `saved` becomes `$derived(discovery.saved)`.
- `prefErrors`: empty string, duplicate, not in saved, level not listed.
- `setPref(slot, slug)` uses a string tuple.
- `stepsDone` and `missing` keep their current logic; the step-3 error messages now come from slug validation.
- Add budget persistence:
  - key `auc-budget-v1:<accountId>`
  - `loadBudget(accountId)` sets `application.budget` when a stored number exists
  - `saveBudget(accountId, amount)` writes it
  - the existing step-5 budget input remains bound to `application.budget`; its Save & continue also persists the budget.

### Apply and status pages

- Step 3 `<select>` options use slugs and `catalog.bySlug`.
- Labels use `official_name.value`, `country.value`, and terms from the record.
- The "not open to level" check uses `agreement_student_levels.value`.
- Review step lines and `/apply/status` resolve preference names through the catalog and show a dash while loading.
- The "Manage saved list" count uses `discovery.savedSlugs.length`.

## Pages

### Explore (`/explore`)

- Use `catalog` instead of its own `loadUniversities()` fetch.
- Add Save and Compare actions to every card and to the detail page.
- `Save` toggles a filled bookmark; `Compare` toggles a selected state and refuses a fourth selection with a toast.
- Show a compact "You qualify" chip when `qualification` is true.
- Show a sticky compare tray when at least one school is selected:
  - three small school initials/names
  - "Compare N programs →" (disabled below two, enabled at two or three)
  - clear control.
- Keep search, filters, QS sorting, recent schools, and load-more behaviour.
- Add the demo-data disclaimer near the results header.

### Saved (`/saved`)

- Header: "Saved universities", count, and "Your 3 application preferences are chosen from this list."
- Sort control: nearest deadline (default), best fit, lowest cost. Sorting uses `mergeSort`; nearest deadline uses `MinHeap`.
- Cards show:
  - school, city/country, fit percentage
  - qualification chip
  - estimated semester cost and budget status
  - days left with deadline date
  - Save/remove, Compare, and rank badge when used as a preference
- Global actions:
  - "Use saved schools in my application": fills the three preference slots from the current sorted list (or asks for at least three), then navigates to `/apply?step=3`.
  - "Compare" tray as on Explore.
- Empty state: explain saving from Explore, with a button back to Explore.
- Recommendations section: top three by fit among saved? No — template says "not saved yet", so top three **unsaved** schools; the recommendation cards show fit and a Save action.
- Recommendations use `topK` with a `MaxHeap`; show a small "Max-heap" footnote.

### Compare (`/compare`)

- Header and back link. If fewer than two schools, show an empty state with a link to Saved/Explore.
- Three columns with initial avatars, names, countries, and fit.
- Rows:
  - You qualify
  - Fit
  - City
  - Tuition
  - Living (monthly)
  - Whole program / estimated semester cost
  - Vs. your budget
  - GPA needed
  - IELTS needed
  - Deadline
  - Seats
  - Papers
- Best value in each row gets a green cell background and a small "Best" label.
- Responsive: horizontal scroll on small screens with the row-label column sticky.
- Footer: "Ask an advisor which to choose" (mailto `studyabroad@aucegypt.edu` with the three school names in the subject) and the demo disclaimer.
- Small muted footnote: "Calculated with a hash map" near the table, matching the template's data-structure notes.

### Me (`/me`)

- Header: "Me", subtitle "Profile, notifications and recommendation letters."
- Profile card: GPA, level, major, English requirement status, and "Edit eligibility profile" link to `/profile`.
- Budget card: numeric input, Save budget, success toast. Reads/writes `store.application.budget` and per-account storage.
- Notifications card:
  - oldest first
  - unread dot
  - unread count
  - "Mark all as read" and "Mark oldest as read"
  - queue footnote
- Recommendation letters card:
  - count and status from `store.myLetters` / `store.hasLetter`
  - link to `/letters`
  - optional note matching the template.
- Bottom compare CTA if compare selection is non-empty.

## Navigation

Sidebar groups:

- Discover: Explore Universities, Saved Universities
- My Application: Application Form, Application Status, Recommendation Letter
- Account: Me, Eligibility Profile

Mobile tabs (max five):

`Explore · Saved · Apply · Status · Me`

Letters remains reachable from the sidebar and the Me page. Compare is reached from the tray and Saved page, not a tab.

Badges:

- Saved: saved count (optional; use 0/undefined when empty)
- Me: unread notification count.

Update the route README table with `/saved`, `/compare`, `/me`, and the new owners.

## Persistence summary

| Key | Owner | Content |
|---|---|---|
| `auc-eligibility:<account>` | existing | eligibility profile |
| `auc-discovery-v1:<account>` | new | saved slugs, compare slugs, notifications |
| `auc-budget-v1:<account>` | new | application budget |
| `auc-explore-recent-v1` | existing | recently viewed schools |

All keys are browser-local. No data leaves the machine.

## Edge cases

- Catalog still loading: show skeletons on Saved/Compare and a dash for preference names; do not write misleading validation errors until the catalog is ready.
- Saved slug no longer in the dataset: drop it from the displayed list and from storage on next save, but do not crash.
- Compare selection containing a removed school: prune to three valid slugs.
- More than three saved: allowed; only the first three in the current sort can be applied automatically, and the user can reorder in Apply.
- Fewer than three saved: "Use in application" is disabled with an explanatory hint.
- No eligibility profile: layout redirects to `/profile`; demo metrics fall back to the demo student record only if a page is opened directly.
- Budget zero or blank: comparison shows "Set a budget on Me" instead of "within budget".
- Storage full or blocked: discovery state and notification read state remain in memory for the session.
- Deadline passed: show "Closed" instead of negative days.

## Testing

Unit tests (Vitest):

- `metrics.test.ts`
  - deterministic metrics for the same slug
  - different slugs produce different values
  - fit stays within 35–98
  - higher GPA does not lower fit
  - level-listed and English rules affect qualification
  - waiver schools have zero tuition
- `structures.test.ts`
  - merge sort correctness, stability, and no input mutation
  - min-heap returns ascending values
  - max-heap and `topK` return the highest scores
  - queue preserves FIFO and notification marked-read helpers
- `compare.test.ts`
  - best-value selection per direction
  - ties highlight all best values
  - missing values never win

Manual verification:

- Demo student sign-in → Explore → save and compare three schools.
- Compare page shows three columns with green best values.
- Saved page default order is nearest deadline; changing sort reorders.
- "Use saved schools in my application" fills step 3 and shows names/levels.
- Status page shows the same preference names.
- Me page budget updates Compare's "vs. budget".
- Notifications: unread badge, mark oldest, mark all read, survive reload.
- Refresh: saved, compare, notifications, budget persist.
- Console has no errors; `npm run check`, `npm test`, `npm run build` pass.

## Risks

- **Demo values may look authoritative.** Mitigation: repeated visible disclaimer and "sample" wording on costs, seats, fit, and deadlines.
- **Application regression from slug migration.** Mitigation: keep the change mechanical, update review/status in the same pass, and manually walk all seven steps.
- **2.4 MB dataset load.** Already exists on Explore; the catalog makes it one shared fetch instead of repeated fetches. Do not duplicate the JSON in the bundle.
- **localStorage drift between accounts.** Mitigation: every key includes the account id; test a sign-out/sign-in switch with the demo roles.

## What is left to the builder

- Exact wording and spacing of the new cards, tray, and table.
- Exact seeded saved schools and seeded notification text.
- Mobile table scroll treatment and sticky row labels.
- Whether to show the structure footnotes on every page or only Saved/Compare.
- Minor icon choices from the existing set.
