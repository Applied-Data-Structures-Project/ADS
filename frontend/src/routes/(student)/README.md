# Student pages

`(student)` is a SvelteKit route group: the parentheses keep it out of the URL,
so `(student)/apply/+page.svelte` is served at `/apply`.

Every page here uses the frame in `+layout.svelte` (same header, sidebar, dark mode and toast as `/admin`).
Group 3's state and mock data live in `src/lib/student/` (`store.svelte.ts`, `mocks.ts`, `types.ts`).
Each store action has a `TODO` naming the backend route that will replace the mock.

| Page | URL | Owner |
|---|---|---|
| Application form (7 steps) | `/apply` | G3 Malak |
| Application status | `/apply/status` | G3 Malak |
| Recommendation letter | `/letters` | G3 Malak |
| Faculty letter requests | `/doctor` (in `(faculty)`) | G3 Malak |

To add a page (e.g. Maya's `/explore` or Muhammad's `/saved`), create `(student)/<name>/+page.svelte`
and add a nav item to `groups` in `+layout.svelte`. `PageHeader` and `Icon` in
`src/lib/components/student/` give the same look as the other pages.
