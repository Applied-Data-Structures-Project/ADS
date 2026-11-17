# AUC Exchange Matcher — frontend

SvelteKit 3 + Svelte 5 + Tailwind CSS v4 + TypeScript.

## Commands

```sh
npm install
npm run dev       # http://localhost:5173 (proxies /api/* to the C++ server on 18080)
npm run check     # svelte-check: TypeScript + Svelte diagnostics
npm test          # vitest: pure auth logic (roles, profile validation)
npm run build     # production build
```

## Sign-in (real AUC Microsoft / Entra ID)

Students, IPSO administrators and faculty sign in with their AUC Microsoft
account. There are no passwords in this app: the client uses the OAuth 2.0
authorization-code flow with PKCE (`@azure/msal-browser`), so there is no client
secret anywhere.

### Configuration

Copy `frontend/.env.example` to `frontend/.env` and fill in:

| Variable | Meaning |
|---|---|
| `PUBLIC_MSAL_CLIENT_ID` | Application (client) ID of the Entra app registration |
| `PUBLIC_MSAL_TENANT_ID` | Directory (tenant) ID of the AUC tenant |
| `PUBLIC_MSAL_REDIRECT_URI` | Optional; defaults to `<origin>/auth/callback` |

These values are public by design. Restart `npm run dev` after changing them.

### App registration checklist (AUC IT, or a free dev tenant)

1. Platform: **Single-page application (SPA)**.
2. Redirect URI: `http://localhost:5173/auth/callback` (plus the deployed URL).
3. App roles: `Admin`, `Faculty`. Students need no role assignment.
4. Assign staff users to those roles in Entra.

Roles arrive in the ID token's `roles` claim (`src/lib/auth/roles.ts`). An
account with no staff role is treated as a student. Accounts outside
`@aucegypt.edu` are rejected.

Without the two ids, `/login` shows a configuration notice instead of a dead
button, and every authenticated route redirects there. The rest of the frontend
(health check page, static admin mock) still renders, but there is no sign-in.

### Flow

| Route | Purpose |
|---|---|
| `/login` | Student / Administrator tabs, Microsoft button, real error states |
| `/auth/callback` | Finishes the redirect and routes by role (admin → `/admin`, faculty → `/doctor`, student → `/signup` or `/apply`) |
| `/signup` | One-time profile completion (level, major, AUC ID, phone) |

The student, faculty and admin surfaces are guarded: signed-out visitors go to
`/login?next=…`, and the wrong role gets a "go to my area" card.

### Demo mode (until the IDs arrive)

No app registration yet? Set `PUBLIC_DEMO_AUTH=true` in `frontend/.env` and
restart the dev server. `/login` then offers **Student**, **Administrator** and
**Faculty** demo identities, each landing on its normal home with a sample
profile. A "Demo mode" badge shows in the header so it is never mistaken for
real sign-in.

Demo mode is presentation only: no Microsoft request is made, nothing is
authenticated, and a production build ignores the flag entirely. To switch back
to real sign-in, set `PUBLIC_DEMO_AUTH=false` (or delete the line) and fill in
the two MSAL ids.

### Limitation (until the backend catches up)

The C++ server only has `/api/health`, and role checks currently happen in the
browser. Anyone who can edit JavaScript could reach `/admin`. The backend
milestone must verify the Microsoft token and enforce roles server-side.

The application, document and letter data on the student pages is still Group
3's mock data; only the identity is real.
