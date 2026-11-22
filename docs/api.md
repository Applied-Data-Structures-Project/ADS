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
```json
{ "level": "UG", "declared": true, "credits": 30, "gpa": 2.5 }
```
Answer:
```json
{ "eligible": false, "reason": "Your GPA is below the minimum for UG students." }
```

### `POST /api/application/submit`
Body: the same 4 fields as eligibility (from the student's profile). Checks eligibility, the
deadline, every required field, the 4 PDFs and a received letter, then locks the application and
puts the student in IPSO's review queue.
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
