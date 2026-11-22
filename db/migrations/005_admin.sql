-- 005_admin.sql
-- Admin-owned tables for IPSO seat quota overrides, application review annotations,
-- and persisted matching run snapshots (runs, placed assignments, unplaced analyses).
-- Safe to run before db/seed/universities.sql and idempotent across restarts.

CREATE TABLE IF NOT EXISTS admin_seat_overrides (
    slug        TEXT PRIMARY KEY,
    seats       INTEGER NOT NULL CHECK (seats >= 0),
    updated_at  TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS admin_application_reviews (
    student_email        TEXT PRIMARY KEY,
    statement_rating     REAL,
    has_statement_rating INTEGER NOT NULL DEFAULT 0 CHECK (has_statement_rating IN (0, 1)),
    notes                TEXT,
    is_demo              INTEGER NOT NULL DEFAULT 0 CHECK (is_demo IN (0, 1)),
    updated_at           TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS admin_match_runs (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    run_code             TEXT UNIQUE NOT NULL,
    cycle                TEXT NOT NULL,
    title                TEXT NOT NULL,
    mode                 TEXT NOT NULL,
    allow_unranked       INTEGER NOT NULL CHECK (allow_unranked IN (0, 1)),
    w_pref               REAL NOT NULL,
    w_gpa                REAL NOT NULL,
    w_statement          REAL NOT NULL,
    w_fairness           REAL NOT NULL,
    default_seats        INTEGER NOT NULL,
    status               TEXT NOT NULL CHECK (status IN ('Draft', 'Published', 'Archived')),
    placed_count         INTEGER NOT NULL DEFAULT 0,
    eligible_count       INTEGER NOT NULL DEFAULT 0,
    hk_max               INTEGER NOT NULL DEFAULT 0,
    first_choice_count   INTEGER NOT NULL DEFAULT 0,
    second_choice_count  INTEGER NOT NULL DEFAULT 0,
    third_choice_count   INTEGER NOT NULL DEFAULT 0,
    unranked_count       INTEGER NOT NULL DEFAULT 0,
    avg_score            REAL NOT NULL DEFAULT 0.0,
    created_by           TEXT NOT NULL,
    created_at           TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS admin_match_assignments (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id           INTEGER NOT NULL,
    student_email    TEXT NOT NULL,
    student_name     TEXT NOT NULL,
    student_auc_id   TEXT NOT NULL,
    student_major    TEXT NOT NULL,
    student_gpa      REAL NOT NULL,
    university_slug  TEXT NOT NULL,
    university_name  TEXT NOT NULL,
    seat_label       TEXT NOT NULL,
    pref_rank        INTEGER NOT NULL,
    score            REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_admin_match_assignments_run
    ON admin_match_assignments(run_id);

CREATE TABLE IF NOT EXISTS admin_match_unplaced (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id         INTEGER NOT NULL,
    student_email  TEXT NOT NULL,
    student_name   TEXT NOT NULL,
    student_gpa    REAL NOT NULL,
    prefs_summary  TEXT NOT NULL,
    bottleneck     TEXT NOT NULL,
    reason         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_admin_match_unplaced_run
    ON admin_match_unplaced(run_id);
