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
│   ├── repo/           The ONLY code that talks to SQLite
│   └── services/       Business logic: scoring, recommender, allocator, auth, LLM
├── server/             Crow REST API (the ONLY code that knows HTTP/JSON)
├── db/
│   ├── migrations/     001_init.sql, 002_... (committed, applied in order)
│   ├── seed/           Collected data: universities, demo students (committed)
│   └── app.db          Local database (git-ignored, each dev has their own)
├── tests/              Each *.cpp = one CTest executable; no_stl_containers.cmake guard
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

