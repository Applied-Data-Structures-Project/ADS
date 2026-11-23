-- 006_course_plans.sql
-- Draft List of Courses for Study Abroad per student and host university.

CREATE TABLE IF NOT EXISTS course_plans (
    student_email     TEXT NOT NULL,
    university_slug   TEXT NOT NULL,
    university_name   TEXT NOT NULL DEFAULT '',
    banner_inst_code  TEXT NOT NULL DEFAULT '',
    banner_inst_name  TEXT NOT NULL DEFAULT '',
    study_term        TEXT NOT NULL DEFAULT 'Fall 2027',
    updated_at        TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (student_email, university_slug)
);

CREATE TABLE IF NOT EXISTS course_plan_items (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    student_email     TEXT NOT NULL,
    university_slug   TEXT NOT NULL,
    position          INTEGER NOT NULL,
    host_course_code  TEXT NOT NULL DEFAULT '',
    host_course_title TEXT NOT NULL DEFAULT '',
    auc_equivalent    TEXT NOT NULL DEFAULT '',
    effective_term    TEXT NOT NULL DEFAULT '',
    source            TEXT NOT NULL DEFAULT 'manual'
);

CREATE INDEX IF NOT EXISTS idx_course_plan_items_owner
    ON course_plan_items(student_email, university_slug, position);
