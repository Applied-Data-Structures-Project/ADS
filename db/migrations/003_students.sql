-- 003_students.sql
-- Student academic profile, eligibility intake fields, and per-semester budget.
-- Loaded into StudentService's HashMap<email, StudentRecord> at startup.

CREATE TABLE IF NOT EXISTS students (
    email                  TEXT PRIMARY KEY,
    name                   TEXT,
    auc_id                 TEXT,
    phone                  TEXT,
    level                  TEXT CHECK (level IS NULL OR level IN ('UG', 'GR')),
    major                  TEXT,
    gpa                    REAL,
    has_gpa                INTEGER NOT NULL DEFAULT 0 CHECK (has_gpa IN (0, 1)),
    credits                INTEGER,
    has_credits            INTEGER NOT NULL DEFAULT 0 CHECK (has_credits IN (0, 1)),
    major_declared         INTEGER NOT NULL DEFAULT 0 CHECK (major_declared IN (0, 1)),
    rhet110                INTEGER NOT NULL DEFAULT 0 CHECK (rhet110 IN (0, 1)),
    rhet120                INTEGER NOT NULL DEFAULT 0 CHECK (rhet120 IN (0, 1)),
    final_semester         INTEGER NOT NULL DEFAULT 0 CHECK (final_semester IN (0, 1)),
    academic_probation     INTEGER NOT NULL DEFAULT 0 CHECK (academic_probation IN (0, 1)),
    disciplinary_probation INTEGER NOT NULL DEFAULT 0 CHECK (disciplinary_probation IN (0, 1)),
    english_complete       INTEGER NOT NULL DEFAULT 0 CHECK (english_complete IN (0, 1)),
    first_semester_no_gpa  INTEGER NOT NULL DEFAULT 0 CHECK (first_semester_no_gpa IN (0, 1)),
    eligibility_saved      INTEGER NOT NULL DEFAULT 0 CHECK (eligibility_saved IN (0, 1)),
    budget                 INTEGER NOT NULL DEFAULT 0,
    has_budget             INTEGER NOT NULL DEFAULT 0 CHECK (has_budget IN (0, 1)),
    updated_at             TEXT DEFAULT (datetime('now'))
);
