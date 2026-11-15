-- 002_application.sql
--  study abroad applications, uploaded documents, and
-- recommendation letters.
---- Most form fields allow NULL on purpose: students save half-finished drafts.
-- The C++ service checks that every required field is filled on Submit.
-- Columns follow IPSO's real Fall 2027 Study Abroad application form.
-- Depends on 001_init.sql for the students and universities tables.

CREATE TABLE IF NOT EXISTS applications (
    -- Key
    student_email        TEXT PRIMARY KEY REFERENCES students(email),  -- whose application

    -- Personal info (real form)
    first_name           TEXT,                     -- as on passport
    surname              TEXT,                     -- as on passport
    date_of_birth        TEXT,                     -- MM/DD/YYYY
    nationality          TEXT,
    second_nationality   TEXT,                     -- optional, can stay empty
    gender               TEXT,                     -- 'Male' / 'Female'
    phone                TEXT,                     -- with country code
    auc_id               TEXT,                     -- e.g. 900123456
    gpa                  REAL,                     -- most recent semester
    level                TEXT CHECK (level IN ('UG', 'GR')),
    major                TEXT,
    rhet1020             TEXT,                     -- 'Yes' / 'Currently Enrolled' / 'Not Applicable'
    expected_graduation  TEXT,                     -- date
    accommodation        INTEGER DEFAULT 0 CHECK (accommodation IN (0, 1)),  -- needs accessibility support?
    scholarship          TEXT,                     -- 'None' / 'Tomorrow''s Leaders' / 'Other'
    program_type         TEXT,                     -- 'Exchange' / 'Direct Enrollment'

    -- Preferences: 3 ranked choices from the student's saved list
    pref1                INTEGER REFERENCES universities(id),  -- 1st choice
    pref2                INTEGER REFERENCES universities(id),  -- 2nd choice
    pref3                INTEGER REFERENCES universities(id),  -- 3rd choice

    -- Parent / guardian (real form)
    guardian_name        TEXT,                     -- full name
    guardian_relation    TEXT,                     -- e.g. 'Father'
    guardian_phone       TEXT,                     -- with country code
    guardian_email       TEXT,
    guardian_language    TEXT,                     -- optional: preferred language

    -- Questionnaire (real form)
    q_preparedness       TEXT,                     -- academic preparedness
    q_goals              TEXT,                     -- fit with academic goals, 300-500 words (admin scores this)
    travel_experience    TEXT,                     -- 'Yes, Study Abroad' / 'Yes, Non-Study Abroad' / 'No' / 'Other'
    q_adaptability       TEXT,                     -- adapting to new cultures, 300-5000 words
    q_teamwork           TEXT,                     -- teamwork and handling conflict
    q_reflection         TEXT,                     -- what you hope to gain
    q_finance            TEXT,                     -- how you will finance the semester

    -- Extras for the admin side's matching
    budget               INTEGER,                  -- estimated budget
    prior                INTEGER DEFAULT 0 CHECK (prior IN (0, 1)),  -- 1 if travel_experience = 'Yes, Study Abroad'

    -- Submit and review
    declaration          INTEGER DEFAULT 0 CHECK (declaration IN (0, 1)),  -- must be 1 to submit
    status               TEXT DEFAULT 'Draft'
                         CHECK (status IN ('Draft', 'Submitted', 'In Review', 'Verified', 'Ineligible')),
    good_standing        INTEGER CHECK (good_standing IN (0, 1)),  -- set by IPSO on the admin side
    submitted_at         TEXT,                     -- set once on submit
    created_at           TEXT DEFAULT (datetime('now')),  -- when the draft was started
    updated_at           TEXT DEFAULT (datetime('now'))   -- last save of the draft
);
-- application_documents: one uploaded PDF per document type per student
-- Uploading again replaces the old file (same primary key).
-- The recommendation letter is not here: it lives in the letters table.

CREATE TABLE IF NOT EXISTS application_documents (
    student_email  TEXT REFERENCES students(email),
    doc_type       TEXT CHECK (doc_type IN ('courses', 'transcript', 'cv', 'passport')),
    file_path      TEXT NOT NULL,                  -- e.g. uploads/900123456/cv.pdf
    uploaded_at    TEXT DEFAULT (datetime('now')),
    PRIMARY KEY (student_email, doc_type)
);
-- letters: recommendation letter requests from a student to AUC faculty
-- A student can ask at most 2 doctors for letters, and each doctor can only be asked once per student.
CREATE TABLE IF NOT EXISTS letters (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    student_email  TEXT REFERENCES students(email),
    doctor_name    TEXT NOT NULL,
    doctor_email   TEXT NOT NULL,                  -- must end in @aucegypt.edu
    status         TEXT DEFAULT 'Pending'
                   CHECK (status IN ('Pending', 'Received', 'Declined')),
    letter_text    TEXT,                           -- filled when the doctor answers
    requested_at   TEXT DEFAULT (datetime('now')),
    answered_at    TEXT,
    UNIQUE (student_email, doctor_email)           -- same doctor only once per student
);