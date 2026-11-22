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

### `GET /api/saved` (optional `?sort=deadline|fit|cost`)
Without `?sort=`, returns the signed-in student's saved university slugs in the order they were saved:
```json
{ "saved": ["bocconi-university", "aarhus-university"] }
```
With `?sort=deadline`, `?sort=fit`, or `?sort=cost`, scores and orders the student's saved universities in C++ using `Heap` (`include/heap.hpp`, min-heap for `deadline`) or stable `mergeSort` (`include/mergesort.hpp`, for `fit` and `cost`):
```json
{
  "sort": "deadline",
  "algorithm": "cpp-min-heap",
  "saved": ["bocconi-university", "aarhus-university"],
  "items": [
    {
      "slug": "aarhus-university",
      "name": "Aarhus University",
      "country": "Denmark",
      "city": "Aarhus",
      "region": "Europe",
      "fitScore": 84,
      "eligible": true,
      "provisional": false,
      "eligibilityReason": "",
      "minGpaHost": 2.8,
      "estimatedSemesterCostUsd": 5400,
      "withinBudget": true,
      "deadline": "2026-10-15",
      "matchSummary": "Strong GPA match and within your budget"
    }
  ]
}
```

### `POST /api/saved/<slug>`
Saves a university slug (idempotent, `O(1)` membership check via `HashMap`).
Answer: `{ "saved": [...] }`. `400` on invalid slug.

### `DELETE /api/saved/<slug>`
Removes a saved university slug.
Answer: `{ "saved": [...] }`.

### `GET /api/notifications`
The signed-in student's notifications ordered oldest-first by C++ `mergeSort` (`include/mergesort.hpp`), plus `unreadCount` from the server's FIFO `Queue<int>` (`include/queue.hpp`). Seeds one welcome notification on first visit.
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

### `GET /api/recommendations?k=3`
Computes the top `k` (`1..20`, default `3`) unsaved partner universities for the signed-in student in C++ using our `Heap` (`include/heap.hpp`, `O(n)` `buildFromArray` + `O(k log n)` pops) over `UniversityService`, excluding saved slugs in `O(1)` via `DiscoveryService`.
```json
{
  "algorithm": "cpp-max-heap",
  "count": 3,
  "recommendations": [
    {
      "slug": "bocconi-university",
      "name": "Bocconi University",
      "country": "Italy",
      "city": "Milan",
      "region": "Europe",
      "fitScore": 92,
      "eligible": true,
      "provisional": false,
      "eligibilityReason": "",
      "minGpaHost": 3.0,
      "estimatedSemesterCostUsd": 5200,
      "withinBudget": true,
      "deadline": "2026-10-15",
      "matchSummary": "Direct major match and within your budget"
    }
  ]
}
```

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
