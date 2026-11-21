# AUC Exchange Matcher

Applied Data Structures project. A web app that brings AUC's outgoing-exchange information into one place, **recommends** the best partner universities to each student (weighted scoring + heap), and lets admins **allocate seats optimally** with a bipartite graph (Hopcroft–Karp + Hungarian). Admins can tune the weights and preview results before publishing.

| Layer | Tech |
|---|---|
| Frontend | SvelteKit (TypeScript) + Tailwind CSS v4 |
| API | C++17 + [Crow](https://crowcpp.org) |
| Storage | SQLite (storage only; all logic runs in our own data structures) |
| Core | Our own `Vector`, `HashMap`, `Heap`, `Queue`, `BipartiteGraph` (**no STL containers**) |

---

## Prerequisites

| Tool | Version | macOS | Windows |
|---|---|---|---|
| C++ compiler | C++17 | Xcode CLT (`xcode-select --install`) | Visual Studio 2022 (Desktop C++) or MSYS2 g++ |
| CMake | ≥ 3.20 | `brew install cmake` | cmake.org installer |
| Git | any | preinstalled | git-scm.com |
| Node.js | ≥ 20 | `brew install node` | nodejs.org |

Crow, Asio and SQLite are **downloaded automatically** by CMake on the first configure, so there's nothing else to install.

## Quick start

```bash
# 1. Backend (first configure downloads dependencies, takes ~1 min)
cmake -S . -B build
cmake --build build -j
./build/ads_server                      # http://localhost:18080/api/health

# 2. Frontend (in a second terminal)
cd frontend
npm install
npm run dev                             # http://localhost:5173
```

The frontend dev server proxies every `/api/*` request to `localhost:18080`, so call the backend with relative URLs (`/api/...`) and there are no CORS issues.

### Other commands

```bash
ctest --test-dir build --output-on-failure   # C++ tests (+ the no-STL-containers check)
./build/ads_demo                             # original console demo of the data structures
cd frontend && npm run check                 # Svelte/TypeScript type check
```

On Windows with Visual Studio, binaries are under `build/Debug/` (for example `build\Debug\ads_server.exe`).

---

## Project layout

```
ADS/
├── include/            Our generic data structures & algorithms (header-only, no domain code)
│   ├── vector.hpp  hashmap.hpp  heap.hpp  queue.hpp  bipartite.hpp
├── src/                Domain code (compiled into the ads_core library)
│   ├── models/         Plain structs: Student, University, Preference, WeightConfig, MatchRun
│   │   └── admin_models.hpp   AdminStudent, AdminUniversity, ScoringWeights (admin side)
│   ├── repo/           The ONLY code that talks to SQLite
│   └── services/       Business logic: scoring, recommender, allocator, auth, LLM
│       ├── admin_scoring.hpp   AdminFitScorer class: allowed? + fit score (admin side)
│       └── admin_scoring.cpp
├── server/             Crow REST API (the ONLY code that knows HTTP/JSON)
├── db/
│   ├── migrations/     001_init.sql, 002_... (committed, applied in order)
│   ├── seed/           Collected data: universities, demo students (committed)
│   └── app.db          Local database (git-ignored, each dev has their own)
├── tests/              Each *.cpp = one CTest executable; no_stl_containers.cmake guard
│   └── test_admin_scoring.cpp   Test for AdminFitScorer
├── docs/               api.md (REST contract), design notes, report material
├── frontend/           SvelteKit + Tailwind app
│   └── src/
│       ├── lib/api/    client.ts (fetch wrapper) + mocks
│       └── routes/
│           ├── (student)/   student pages
│           └── admin/       admin console
├── main.cpp            Data-structure demo (ads_demo target)
└── CMakeLists.txt
```

### Layering rules

1. `include/` is generic. It has no Crow, no SQLite and no domain types.
2. `src/services` must **not** include Crow, so it stays unit-testable from `tests/`.
3. `server/` stays thin: parse request → call service → serialize response.
4. SQLite is storage only. Load into our structures and do filtering, ranking and matching in our DS, never in SQL.
5. **No STL data structures** (`std::vector`, `std::map`, `std::unordered_map`, `std::queue`, `std::pair`, etc.) in `include/` or `src/`. `std::string` is fine, and so is STL inside Crow. CTest enforces this.

## Admin scoring (fit score)

The admin side scores every (student, university) pair with the class `AdminFitScorer` (`src/services/admin_scoring.*`). It answers two separate questions:

1. **Is the student allowed to go there?** Yes or no, and if no, the reason is recorded. The checks run in this order and the first one that fails is the reason: application marked Ineligible, GPA below AUC's 2.8, GPA below the university's minimum, major not accepted, semester not offered, university not among the student's 3 choices (unless unranked matches are allowed).
2. **If allowed, how good is the fit?** Four factors, each turned into a number from 0 to 1: preference (1st = 1.0, 2nd = 0.7, 3rd = 0.4, unranked = 0.1), GPA (2.8 gives 0, 4.0 gives 1), statement rating / 10, and fairness (1 if the student has never studied abroad, otherwise 0).

The admin's four slider weights blend the factors: `score = (wPref*fPref + wGpa*fGpa + wStatement*fStatement + wFairness*fFairness) / (wPref + wGpa + wStatement + wFairness)`. Dividing by the sum keeps every score between 0 and 1 however the sliders are set. Negative weights, or weights that add up to 0, are refused (`setWeights` returns false and changes nothing).

The constructor runs the checks and factor conversions once and stores the four factors on each result. Moving a slider calls `setWeights`, which only redoes the blend. Unknown data never blocks a student: a missing university minimum GPA, major list or semester list counts as "passed" (flagged `minGpaUnknown`), and a missing statement rating gives factor 0 (flagged `statementMissing`).

Run the test: `ctest --test-dir build --output-on-failure -R test_admin_scoring`, or compile it directly: `g++ -std=c++17 -Wall -Wextra -Iinclude -Isrc src/services/admin_scoring.cpp tests/test_admin_scoring.cpp -o test_admin_scoring` and run `./test_admin_scoring` (exit code 0 means pass).
