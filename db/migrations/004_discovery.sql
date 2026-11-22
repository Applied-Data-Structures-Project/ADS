-- 004_discovery.sql
-- Saved partner universities and student notifications (FIFO queue).
-- Loaded into DiscoveryService's HashMaps and Queues at startup.

CREATE TABLE IF NOT EXISTS saved_universities (
    student_email   TEXT NOT NULL,
    university_slug TEXT NOT NULL,
    saved_at        TEXT DEFAULT (datetime('now')),
    PRIMARY KEY (student_email, university_slug)
);

CREATE TABLE IF NOT EXISTS notifications (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    student_email TEXT NOT NULL,
    message       TEXT NOT NULL,
    created_at    TEXT DEFAULT (datetime('now')),
    is_read       INTEGER NOT NULL DEFAULT 0 CHECK (is_read IN (0, 1))
);
