# API: Group 3 (application, letters, doctor)

The C++ server (Crow) listens on `http://localhost:18080`. In development the Svelte app calls
`/api/...` and Vite forwards it there.

**Who is calling:** every request sends the signed-in AUC email in a header:

```
X-User-Email: salma@aucegypt.edu
```

Without it (or with a non-`@aucegypt.edu` address) the answer is `401`. For the course project the
front end sends the email from the Microsoft sign-in; a real deployment would check Microsoft's
token on the server instead.

**Errors** always look like `{"error": "a sentence the page can show"}`.

| Code | Meaning |
|---|---|
| 400 | The body is not valid JSON / the file is empty |
| 401 | No signed-in AUC email |
| 403 | Not eligible (GPA, credits, declared major) |
| 404 | Unknown document type |
| 409 | A rule refused it: submitted already, deadline passed, missing items, same doctor twice... |
| 413 | File bigger than 5 MB |
| 415 | File is not a PDF |

---

## Student Profile, Eligibility & Budget

### `GET /api/me`
The signed-in student's profile, eligibility intake fields, and per-semester budget.
`profile` is `null` until the student saves at least once.

```json
{
  "profile": {
    "email": "salma@aucegypt.edu",
    "name": "Salma Hassan",
    "aucId": "900214512",
    "phone": "+201001234567",
    "level": "UG",
    "major": "Computer Science",
    "gpa": 3.5,
    "credits": 60,
    "majorDeclared": true,
    "rhet110": true,
    "rhet120": true,
    "finalSemester": false,
    "academicProbation": false,
    "disciplinaryProbation": false,
    "englishComplete": false,
    "firstSemesterNoGpa": false,
    "eligibilitySaved": true,
    "budget": 5000,
    "hasBudget": true,
    "updatedAt": "2026-11-22T19:35:00"
  },
  "eligibility": { "eligible": true, "provisional": false, "reason": "" }
}
```

### `PUT /api/me`
Creates or partially updates the signed-in student's profile. Only fields included in the JSON body are updated (so `/signup`, `/profile`, and `/me` budget updates do not overwrite each other).
Answer: the same shape as `GET /api/me`. `400` on invalid GPA, credits, level, or incomplete eligibility submission.

---

## Saved Universities & Notifications

### `GET /api/saved`
The signed-in student's saved university slugs, in the order they were saved.
```json
{ "saved": ["bocconi-university", "aarhus-university"] }
```

### `POST /api/saved/<slug>`
Saves a university slug (idempotent, `O(1)` membership check via `HashMap`).
Answer: `{ "saved": [...] }`. `400` on invalid slug.

### `DELETE /api/saved/<slug>`
Removes a saved university slug.
Answer: `{ "saved": [...] }`.

### `GET /api/notifications`
The signed-in student's notifications in FIFO order (oldest first), plus `unreadCount` from the server's `Queue<int>`. Seeds one welcome notification on first visit.
```json
{
  "notifications": [
    { "id": 1, "message": "Welcome to the AUC exchange portal...", "createdAt": "2026-11-22T20:00:00", "read": false }
  ],
  "unreadCount": 1
}
```

### `POST /api/notifications`
Body: `{ "message": "..." }`. Appends a notification to SQLite and pushes its ID onto the student's unread FIFO `Queue`. Answer `201`: `{ "id": 2, "notifications": [...], "unreadCount": 2 }`.

### `POST /api/notifications/read-oldest`
Dequeues the front (oldest unread) notification from the student's `Queue` and marks it read.
Answer: `{ "readId": 1, "notifications": [...], "unreadCount": 1 }`.

### `POST /api/notifications/read-all`
Marks all unread notifications for the student as read and empties the unread `Queue`.
Answer: `{ "notifications": [...], "unreadCount": 0 }`.

---

## Application

### `GET /api/application`
My application and what is still missing.

```json
{
  "application": { "studentEmail": "salma@aucegypt.edu", "firstName": "Salma", "...": "...",
                   "pref": ["aarhus-university", "kyoto-university", ""], "status": "Draft" },
  "documents": ["courses", "transcript"],
  "letterReceived": false,
  "missing": ["Upload your cv", "Upload your passport", "Receive a recommendation letter"]
}
```
`application` is `null` until the student saves once. `pref` holds university **slugs**
(`""` = not chosen). The field names are the same as `Application` in
`frontend/src/lib/student/types.ts`.

### `PUT /api/application`
Save my draft. Body: the application fields (same names as above). `status`, `prior`,
`goodStanding` and the dates are set by the server and ignored if sent.
Answer: the same shape as `GET /api/application`.
`409` once the application is submitted.

### `POST /api/application/eligibility`
Evaluates full IPSO eligibility directly from the student's saved `/api/me` record in `O(1)` (falls back to `{ "level", "declared", "credits", "gpa" }` in the request body only if no profile is saved yet).
Answer:
```json
{ "eligible": false, "provisional": false, "reason": "Your GPA is below the 2.8 minimum for UG students." }
```

### `POST /api/application/submit`
Body: `{}` (or legacy `{ "level", "declared", "credits", "gpa" }` if no `/api/me` profile exists). Checks full IPSO eligibility from `/api/me`, the deadline, every required field, the 4 PDFs and a received letter, then locks the application and puts the student in IPSO's review queue.
Answer: the same shape as `GET /api/application`, with `"status": "Submitted"`.
`403` not eligible, `409` with the first missing item or "deadline has passed".

### `PUT /api/application/documents/<type>`
`<type>` is `courses`, `transcript`, `cv` or `passport`. The request **body is the PDF file**
(`Content-Type: application/pdf`), at most 5 MB. Uploading again replaces the old file.
Answer: the same shape as `GET /api/application`.

```js
await fetch('/api/application/documents/cv', {
  method: 'PUT',
  headers: { 'X-User-Email': email, 'Content-Type': 'application/pdf' },
  body: file            // the File from <input type="file">
});
```

### `DELETE /api/application/documents/<type>`
Removes one PDF. `409` once submitted.

### `GET /api/admin/review`
IPSO's review queue (first submitted = first reviewed).
```json
{ "waiting": 2, "next": { "...the application of the student at the front..." } }
```
`next` is `null` when nobody is waiting.

---

## Letters (student)

### `GET /api/letters`
My requests (at most 2). The letter text itself is never sent to the student.
```json
[ { "id": 1, "studentEmail": "salma@aucegypt.edu", "doctorName": "Dr. Hany",
    "doctorEmail": "hany@aucegypt.edu", "status": "Pending",
    "requestedAt": "2026-11-21T09:00:00", "answeredAt": "" } ]
```
`status` is `Pending`, `Received` or `Declined`.

### `POST /api/letters`
```json
{ "doctorName": "Dr. Hany", "doctorEmail": "hany@aucegypt.edu" }
```
Answer `201`: `{ "id": 1 }`. The doctor gets an email with instructions.
`409`: not an `@aucegypt.edu` email, asking yourself, the same doctor twice, or a third request.

---

## Letters (doctor)

### `GET /api/doctor/letters`
Requests waiting for the signed-in doctor, **oldest first** (the doctor's queue), with the
student's details:
```json
[ { "id": 1, "studentEmail": "salma@aucegypt.edu", "studentName": "Salma Ali",
    "studentMajor": "Computer Science", "studentGpa": 3.4, "status": "Pending", "...": "..." } ]
```

### `POST /api/doctor/letters/<id>`
```json
{ "send": true, "letterText": "Salma is a hard-working student..." }
```
or `{ "send": false }` to decline. Answer: `{ "ok": true }`. The student gets an email.
`409`: not your request, already answered, or an empty letter.

---

## Admin Portal (`/api/admin/*`)

Every admin endpoint is handled cleanly by `server/routes_admin.cpp` and backed by `AdminService` (`src/services/admin_service.*`), which orchestrates `AdminFitScorer`, `AdminSeatTable`, `AdminAllocator`, and `AdminRepo` with zero STL containers. Requires `X-User-Email` (`@aucegypt.edu`).

- `GET /api/admin/overview?cycle=...` — Cohort KPI counts, partner seat totals, review queue count, and Hopcroft–Karp feasibility upper bound (`hkMaxFeasible`).
- `GET /api/admin/applications` — Joined student applications (with `students`, `application_documents`, `letters`, and `admin_application_reviews`), filterable in C++ via `cohort`, `search`, `standing`, `level`, `gpaMin`, `gpaMax`, `uni`, `prior`, `major`, `status`.
- `GET /api/admin/applications/<email>` — Full applicant dossier including guardian info and all questionnaire essays.
- `PUT /api/admin/applications/<email>/review` — Update audit `status`, `goodStanding`, `statementRating` (`0.0..10.0` or `null`), and `notes`; syncs `ApplicationService` and emits student notification on `Verified` / `Ineligible`.
- `GET /api/admin/universities` — All 155 seeded partner universities with effective seat quotas (`Default` vs `Custom` override) and live applicant demand counts, filterable via `search`, `region`, `status`, `term`, `level`, `quota`.
- `GET /api/admin/universities/<slug>` — Complete partner university dossier (deadlines, term dates, eligibility rules, language, living cost, housing, visa, links, fact-sheet provenance).
- `PUT /api/admin/universities/<slug>/seats` — Body `{ "seats": 6 }`. Persists IPSO seat quota override (`admin_seat_overrides`).
- `POST /api/admin/matching/simulate` — Body `{ "cycle", "mode": "best_fit" | "highest_score" | "most_placed" | "shortlists", "weights": { "pref", "gpa", "llm", "equity", "allowUnranked" }, "shortlistLimit", "compareRunId" }`. Executes C++ `AdminAllocator` + unplaced diagnostics (`AdminFitScorer`) + optional baseline diff.
- `GET /api/admin/runs` — Lists saved matching run summaries.
- `GET /api/admin/runs/<id>` — Full saved run snapshot (summary + placed assignments + unplaced analyses).
- `POST /api/admin/runs` — Executes simulation and persists a named snapshot run (`Draft`) to SQLite.
- `POST /api/admin/runs/<id>/publish` — Marks the run `Published`, archives prior published runs, and sends placement notifications to non-demo matched students.
- `GET /api/admin/runs/diff?a=<id>&b=<id>` — Computes student-level placement diff (`gained`, `lost`, `shifted`, deltas) in C++.
- `POST /api/admin/demo/seed` — Transactionally seeds 500 realistic varied demo applicants (`is_demo = 1`) matching student-side schemas across real slugs from the 155 partners.
- `DELETE /api/admin/demo/seed` — Removes only `is_demo = 1` demo records while keeping real student applications intact.

