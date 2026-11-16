#!/usr/bin/env python3
"""Schema validator and multi-format exporter for AUC partner university records.

Supports:
  --init-from-manifest  Initialize/refresh records/<slug>.json from manifest.json
                        and partner_list_extracted.json without clobbering
                        already-verified/partial research unless --force is set.
  --validate            Validate all records/*.json against schema.json,
                        zero-fabrication provenance invariants, FX/currency rules,
                        and exact manifest parity.
  --build               Validate first, then deterministically generate
                        universities.json, universities.csv, db/seed/universities.sql,
                        and coverage_report.md, followed by SQLite :memory: checks.
  --check-sqlite        Execute db/seed/universities.sql in sqlite3 :memory: and
                        verify row counts plus JSON/CSV/SQL parity.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sqlite3
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SCHEMA_PATH = REPO_ROOT / "data" / "universities" / "schema.json"
DEFAULT_MANIFEST_PATH = REPO_ROOT / "data" / "universities" / "manifest.json"
DEFAULT_EXTRACTED_PATH = REPO_ROOT / "data" / "raw" / "partner_list_extracted.json"
DEFAULT_RECORDS_DIR = REPO_ROOT / "data" / "universities" / "records"
DEFAULT_OUT_JSON = REPO_ROOT / "data" / "universities" / "universities.json"
DEFAULT_OUT_CSV = REPO_ROOT / "data" / "universities" / "universities.csv"
DEFAULT_OUT_SQL = REPO_ROOT / "db" / "seed" / "universities.sql"
DEFAULT_OUT_REPORT = REPO_ROOT / "data" / "universities" / "coverage_report.md"

PROVENANCE_KEYS = ("value", "status", "source_type", "source_ref", "notes")
VALID_STATUSES = ("verified", "partial", "unknown")
VALID_SOURCE_TYPES = ("excel_list", "fact_sheet", "official_web", "unknown")
NON_UNKNOWN_SOURCE_TYPES = ("excel_list", "fact_sheet", "official_web")

SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
URL_RE = re.compile(r"^https?://[^\s/$.?#].[^\s]*$")
CURRENCY_RE = re.compile(r"^[A-Z]{3}$")
DATE_RE = re.compile(r"\b20\d{2}-\d{2}(?:-\d{2})?\b")
FX_HINT_RE = re.compile(r"\b(?:fx|rate|usd|convert(?:ed|sion)?)\b|=", re.IGNORECASE)
NUMBER_RE = re.compile(r"\d+(?:\.\d+)?")

PLACEHOLDER_TEXT = {"unknown", "n/a", "na", "tbd", "none", "null", "?", "-"}

# Authoritative geographic region mapping aligned with schema.json region enum.
COUNTRY_TO_REGION: dict[str, str] = {
    # North America
    "united states": "North America",
    "usa": "North America",
    "canada": "North America",
    # Europe
    "austria": "Europe",
    "belgium": "Europe",
    "bulgaria": "Europe",
    "croatia": "Europe",
    "cyprus": "Europe",
    "czech republic": "Europe",
    "czechia": "Europe",
    "denmark": "Europe",
    "estonia": "Europe",
    "finland": "Europe",
    "france": "Europe",
    "germany": "Europe",
    "greece": "Europe",
    "hungary": "Europe",
    "iceland": "Europe",
    "ireland": "Europe",
    "italy": "Europe",
    "latvia": "Europe",
    "lithuania": "Europe",
    "luxembourg": "Europe",
    "malta": "Europe",
    "netherlands": "Europe",
    "the netherlands": "Europe",
    "norway": "Europe",
    "poland": "Europe",
    "portugal": "Europe",
    "romania": "Europe",
    "russia": "Europe",
    "serbia": "Europe",
    "slovakia": "Europe",
    "slovenia": "Europe",
    "spain": "Europe",
    "sweden": "Europe",
    "switzerland": "Europe",
    "turkey": "Europe",
    "türkiye": "Europe",
    "ukraine": "Europe",
    "united kingdom": "Europe",
    "uk": "Europe",
    "england": "Europe",
    "scotland": "Europe",
    "wales": "Europe",
    # Middle East & North Africa
    "algeria": "Middle East & North Africa",
    "bahrain": "Middle East & North Africa",
    "egypt": "Middle East & North Africa",
    "iraq": "Middle East & North Africa",
    "jordan": "Middle East & North Africa",
    "kuwait": "Middle East & North Africa",
    "lebanon": "Middle East & North Africa",
    "morocco": "Middle East & North Africa",
    "oman": "Middle East & North Africa",
    "palestine": "Middle East & North Africa",
    "qatar": "Middle East & North Africa",
    "saudi arabia": "Middle East & North Africa",
    "tunisia": "Middle East & North Africa",
    "united arab emirates": "Middle East & North Africa",
    "uae": "Middle East & North Africa",
    # East Asia
    "china": "East Asia",
    "hong kong": "East Asia",
    "japan": "East Asia",
    "macau": "East Asia",
    "south korea": "East Asia",
    "korea": "East Asia",
    "republic of korea": "East Asia",
    "taiwan": "East Asia",
    # Southeast Asia
    "indonesia": "Southeast Asia",
    "malaysia": "Southeast Asia",
    "philippines": "Southeast Asia",
    "singapore": "Southeast Asia",
    "thailand": "Southeast Asia",
    "vietnam": "Southeast Asia",
    # South Asia
    "bangladesh": "South Asia",
    "india": "South Asia",
    "pakistan": "South Asia",
    "sri lanka": "South Asia",
    # Central Asia
    "kazakhstan": "Central Asia",
    "kyrgyzstan": "Central Asia",
    "uzbekistan": "Central Asia",
    # Latin America & Caribbean
    "argentina": "Latin America & Caribbean",
    "brazil": "Latin America & Caribbean",
    "chile": "Latin America & Caribbean",
    "colombia": "Latin America & Caribbean",
    "costa rica": "Latin America & Caribbean",
    "ecuador": "Latin America & Caribbean",
    "mexico": "Latin America & Caribbean",
    "peru": "Latin America & Caribbean",
    "uruguay": "Latin America & Caribbean",
    # Sub-Saharan Africa
    "ghana": "Sub-Saharan Africa",
    "kenya": "Sub-Saharan Africa",
    "nigeria": "Sub-Saharan Africa",
    "senegal": "Sub-Saharan Africa",
    "south africa": "Sub-Saharan Africa",
    # Oceania
    "australia": "Oceania",
    "new zealand": "Oceania",
}

CSV_COLUMNS = [
    "slug",
    "excel_rows",
    "shard",
    "fact_sheet_match_status",
    "fact_sheet_files",
    "cross_references",
    "official_name",
    "short_name",
    "country",
    "state_or_province",
    "city",
    "region",
    "agreement_status",
    "agreement_student_levels",
    "min_standing_note",
    "agreement_scope_raw",
    "eligible_auc_schools",
    "eligible_majors",
    "excluded_majors",
    "min_gpa_host",
    "instruction_languages",
    "english_course_availability",
    "english_proficiency_and_rhet1020",
    "host_language_requirement",
    "available_terms",
    "fall_term_dates",
    "spring_term_dates",
    "final_semester_allowed",
    "seat_quota",
    "tuition_model",
    "est_monthly_living_currency",
    "est_monthly_living_min",
    "est_monthly_living_max",
    "living_cost_usd_est",
    "living_cost_usd_est_status",
    "mandatory_host_fees_and_insurance",
    "housing_availability",
    "housing_notes",
    "host_grants_or_stipends",
    "citizenship_restrictions",
    "visa_lead_time_and_financial_proof",
    "official_website_url",
    "exchange_page_url",
    "course_catalog_url",
    "english_course_list_url",
    "credit_system_and_load",
    "course_restrictions_note",
    "required_application_docs",
    "academic_style",
    "city_tier",
    "campus_type",
    "daily_life_english_friendliness",
    "international_support_services",
    "nomination_deadlines",
    "ext_json",
    "verified_field_count",
    "partial_field_count",
    "unknown_field_count",
]


def load_json(path: Path) -> Any:
    """Load and parse a UTF-8 JSON file."""
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def write_json(path: Path, payload: Any) -> None:
    """Write formatted, deterministic UTF-8 JSON with trailing newline."""
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    path.write_text(text, encoding="utf-8")


def compact_json(value: Any) -> str:
    """Serialize compact deterministic JSON without extra whitespace."""
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True)


def is_int(value: Any) -> bool:
    """Return True if value is an int and not a bool."""
    return isinstance(value, int) and not isinstance(value, bool)


def is_number(value: Any) -> bool:
    """Return True if value is a finite int/float and not a bool."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    return math.isfinite(float(value))


def unknown_provenance(notes: str | None = None) -> dict[str, Any]:
    """Return a clean unknown provenance envelope."""
    return {
        "value": None,
        "status": "unknown",
        "source_type": "unknown",
        "source_ref": None,
        "notes": notes,
    }


def make_provenance(
    value: Any,
    status: str,
    source_type: str,
    source_ref: str | None,
    notes: str | None = None,
) -> dict[str, Any]:
    """Construct a strict provenance envelope."""
    if value is None or status == "unknown":
        return unknown_provenance(notes=notes)
    return {
        "value": value,
        "status": status,
        "source_type": source_type,
        "source_ref": source_ref,
        "notes": notes,
    }


def load_schema_spec(schema_path: Path) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    """Load schema.json and extract factual field definitions mapped by name."""
    schema = load_json(schema_path)
    props = schema.get("properties", {})
    factual_fields: dict[str, dict[str, Any]] = {}
    for name, prop in props.items():
        tier = prop.get("x-tier", 0)
        if tier >= 1 and name not in ("excel_rows", "ext"):
            factual_fields[name] = prop
    return schema, factual_fields


def extract_manifest_universities(manifest_data: Any) -> tuple[list[dict[str, Any]], int | None]:
    """Extract canonical university entries and optional canonical_count from manifest."""
    if isinstance(manifest_data, list):
        return manifest_data, None
    if not isinstance(manifest_data, dict):
        raise ValueError("manifest.json must be a JSON object or array")
    for key in ("universities", "records", "partners", "canonical_universities"):
        if key in manifest_data and isinstance(manifest_data[key], list):
            expected_count = manifest_data.get("canonical_count")
            if expected_count is None:
                expected_count = manifest_data.get("expected_count")
            return manifest_data[key], expected_count
    raise ValueError("manifest.json missing 'universities' list")


def extract_row_lookup(extracted_data: Any) -> dict[int, dict[str, Any]]:
    """Build row_number -> row_dict index from partner_list_extracted.json."""
    rows_list: list[Any]
    if isinstance(extracted_data, list):
        rows_list = extracted_data
    elif isinstance(extracted_data, dict):
        rows_list = []
        for key in ("rows", "partners", "records", "universities"):
            val = extracted_data.get(key)
            if isinstance(val, list):
                rows_list = val
                break
    else:
        rows_list = []

    lookup: dict[int, dict[str, Any]] = {}
    for item in rows_list:
        if not isinstance(item, dict):
            continue
        row_num = item.get("row", item.get("row_number", item.get("excel_row")))
        if is_int(row_num):
            lookup[int(row_num)] = item
    return lookup


def coerce_str_list(val: Any) -> list[str]:
    """Normalize string or list of strings into a clean deduplicated list."""
    if val is None:
        return []
    if isinstance(val, str):
        items = [val]
    elif isinstance(val, list):
        items = [str(x) for x in val if x is not None]
    else:
        return []
    seen: set[str] = set()
    out: list[str] = []
    for item in items:
        s = item.strip()
        if s and s not in seen:
            seen.add(s)
            out.append(s)
    return out


def normalize_fact_sheet_status(entry: dict[str, Any]) -> dict[str, Any]:
    """Extract normalized fact_sheet_status object from a manifest entry."""
    raw = (
        entry.get("fact_sheet_status")
        or entry.get("fact_sheet")
        or entry.get("fact_sheets")
        or {}
    )
    if isinstance(raw, list):
        local_files = coerce_str_list(raw)
        return {
            "match_status": "matched" if local_files else "none",
            "drive_links": [],
            "local_files": local_files,
            "extracted_files": [],
            "notes": None,
        }
    if not isinstance(raw, dict):
        raw = {}

    drive_links = [
        u
        for u in coerce_str_list(raw.get("drive_links") or raw.get("drive_urls") or raw.get("urls"))
        if URL_RE.match(u)
    ]
    local_files = coerce_str_list(raw.get("local_files") or raw.get("files") or raw.get("paths"))
    extracted_files = coerce_str_list(
        raw.get("extracted_files") or raw.get("extracted_paths") or raw.get("text_files")
    )
    status = raw.get("match_status") or raw.get("status")
    status_map = {
        "matched": "matched",
        "direct_match": "matched",
        "matched_direct": "matched",
        "name_match": "matched",
        "matched_by_name": "matched",
        "matched_name": "matched",
        "folder_matched": "matched",
        "unmatched_here": "unmatched_here",
        "unmatched": "unmatched_here",
        "here_unmatched": "unmatched_here",
        "missing_link": "unmatched_here",
        "needs_match": "unmatched_here",
        "blocked": "blocked",
        "inaccessible": "blocked",
        "failed": "blocked",
        "error": "blocked",
        "none": "none",
        "no_fact_sheet": "none",
        "not_applicable": "none",
        "na": "none",
    }
    norm_status = status_map.get(str(status).lower(), "") if status is not None else ""
    if not norm_status:
        if local_files or extracted_files:
            norm_status = "matched"
        elif drive_links:
            norm_status = "unmatched_here"
        else:
            norm_status = "none"

    notes = raw.get("notes")
    if notes is not None:
        notes = str(notes).strip() or None

    return {
        "match_status": norm_status,
        "drive_links": drive_links,
        "local_files": local_files,
        "extracted_files": extracted_files,
        "notes": notes,
    }


def normalize_terms_value(raw_terms: Any) -> list[str] | None:
    """Normalize term strings/lists into schema enum values if unambiguous."""
    if raw_terms is None:
        return None
    tokens: list[str] = []
    if isinstance(raw_terms, list):
        raw_items = [str(x) for x in raw_terms if x is not None]
    else:
        raw_items = re.split(r"[,/&+;]+", str(raw_terms))
    for item in raw_items:
        t = item.strip().lower()
        if not t:
            continue
        if "fall" in t:
            tokens.append("Fall")
        if "spring" in t or "sping" in t:
            tokens.append("Spring")
        if "academic year" in t or "full year" in t or t == "year":
            tokens.append("Academic Year")
        if "summer" in t:
            tokens.append("Summer")
    deduped = list(dict.fromkeys(tokens))
    return deduped if deduped else None


def normalize_levels_value(raw_levels: Any) -> list[str] | None:
    """Normalize student level strings/lists into schema enum values."""
    if raw_levels is None:
        return None
    allowed = {"ug": "UG", "gr": "GR", "phd": "PhD", "staff": "Staff"}
    items = raw_levels if isinstance(raw_levels, list) else re.split(r"[,/&+; ]+", str(raw_levels))
    out: list[str] = []
    for item in items:
        s = str(item).strip().lower()
        if s in allowed and allowed[s] not in out:
            out.append(allowed[s])
    return out if out else None


def normalize_agreement_status(raw_status: Any) -> str | None:
    """Normalize agreement status enum."""
    if raw_status is None:
        return None
    s = str(raw_status).strip().lower().replace("-", "_").replace(" ", "_")
    if s in ("active", "letter_of_intent", "check_slots"):
        return s
    if "intent" in s or s == "loi":
        return "letter_of_intent"
    if "slot" in s:
        return "check_slots"
    return None


REGION_ALIASES: dict[str, str] = {
    "latin america": "Latin America & Caribbean",
    "latin america and caribbean": "Latin America & Caribbean",
    "mena": "Middle East & North Africa",
    "middle east and north africa": "Middle East & North Africa",
}


def derive_region(country: str | None, manifest_region: str | None, valid_regions: set[str]) -> tuple[str | None, str | None]:
    """Return (region, note) using manifest region or authoritative country map."""
    if manifest_region:
        cleaned = manifest_region.strip()
        cleaned = REGION_ALIASES.get(cleaned.lower(), cleaned)
        if cleaned in valid_regions:
            return cleaned, "From normalized manifest country/region mapping"
    if country:
        mapped = COUNTRY_TO_REGION.get(country.strip().lower())
        if mapped in valid_regions:
            return mapped, f"Derived deterministically from normalized country '{country}'"
    return None, None


def manifest_entry_fields(entry: dict[str, Any]) -> dict[str, Any]:
    """Flatten optional 'normalized' sub-dict onto entry view without mutating input."""
    merged = dict(entry)
    norm = entry.get("normalized")
    if isinstance(norm, dict):
        for k, v in norm.items():
            merged.setdefault(k, v)
    return merged


def build_baseline_record(
    entry: dict[str, Any],
    row_lookup: dict[int, dict[str, Any]],
    factual_fields: dict[str, dict[str, Any]],
    schema: dict[str, Any],
) -> dict[str, Any]:
    """Build a baseline schema-compliant university record from manifest + extracted Excel."""
    view = manifest_entry_fields(entry)
    slug = str(view.get("slug") or view.get("id") or "").strip()
    if not slug:
        raise ValueError(f"Manifest entry missing slug/id: {entry}")

    raw_rows = view.get("excel_rows") or view.get("rows") or []
    excel_rows = sorted({int(r) for r in raw_rows if is_int(r)})
    if not excel_rows:
        raise ValueError(f"Manifest entry '{slug}' missing excel_rows")

    shard = view.get("shard")
    if not is_int(shard) or not (1 <= int(shard) <= 8):
        raise ValueError(f"Manifest entry '{slug}' has invalid shard: {shard!r}")

    cross_refs: list[str] = []
    for ref in view.get("cross_references") or []:
        if isinstance(ref, dict):
            ref_slug = ref.get("slug") or ref.get("target_slug") or ref.get("related_slug")
        else:
            ref_slug = ref
        if isinstance(ref_slug, str) and ref_slug.strip() and ref_slug.strip() != slug:
            if ref_slug.strip() not in cross_refs:
                cross_refs.append(ref_slug.strip())

    excel_ref = f"data/raw/partner_list_extracted.json#Universities List!rows:{','.join(str(r) for r in excel_rows)}"
    matched_rows = [row_lookup[r] for r in excel_rows if r in row_lookup]

    def first_nonempty(*keys: str) -> str | None:
        for k in keys:
            val = view.get(k)
            if isinstance(val, str) and val.strip():
                return val.strip()
        for row_dict in matched_rows:
            for k in keys:
                val = row_dict.get(k)
                if isinstance(val, str) and val.strip():
                    return val.strip()
        return None

    official_name = first_nonempty("official_name", "name", "university", "institution_name")
    short_name = first_nonempty("short_name")
    country = first_nonempty("country")
    state_or_province = first_nonempty("state_or_province", "state", "province")
    city = first_nonempty("city")
    valid_regions = set(
        schema["properties"]["region"]["allOf"][1]["properties"]["value"]["enum"]
        - {None}
        if isinstance(schema["properties"]["region"]["allOf"][1]["properties"]["value"]["enum"], set)
        else [
            x
            for x in schema["properties"]["region"]["allOf"][1]["properties"]["value"]["enum"]
            if x is not None
        ]
    )
    region, region_note = derive_region(country, first_nonempty("region"), valid_regions)

    ag_status = normalize_agreement_status(view.get("agreement_status"))
    ag_levels = normalize_levels_value(view.get("agreement_student_levels") or view.get("student_levels"))
    if view.get("agreement_staff_eligible") and ag_levels is not None and "Staff" not in ag_levels:
        ag_levels = [*ag_levels, "Staff"]
    ag_levels_note = first_nonempty("agreement_student_levels_note")
    ag_scope_raw = first_nonempty("agreement_scope_raw", "agreement_scope", "scope", "notes_col_f")
    min_standing_note = first_nonempty("min_standing_note")
    avail_terms = normalize_terms_value(view.get("available_terms") or view.get("terms"))
    terms_notes = " | ".join(
        p for p in (first_nonempty("available_terms_raw"), first_nonempty("term_dates_note")) if p
    ) or None

    raw_url = first_nonempty("official_website_url", "website_url", "website")
    url_val = raw_url if (raw_url and URL_RE.match(raw_url) and "drive.google.com" not in raw_url.lower()) else None
    url_note = (
        f"Excel website value '{raw_url}' is not a direct HTTP(S) university URL; requires verification"
        if raw_url and url_val is None
        else None
    )

    record: dict[str, Any] = {
        "schema_version": schema["properties"]["schema_version"]["const"],
        "slug": slug,
        "excel_rows": excel_rows,
        "shard": int(shard),
        "fact_sheet_status": normalize_fact_sheet_status(view),
        "cross_references": cross_refs,
    }

    for field_name in factual_fields:
        record[field_name] = unknown_provenance()

    excel_populated: list[tuple[str, Any, str, str | None]] = [
        ("official_name", official_name, "verified", None),
        ("short_name", short_name, "verified", None),
        ("country", country, "verified", None),
        ("state_or_province", state_or_province, "verified", None),
        ("city", city, "verified", None),
        ("region", region, "verified", region_note),
        ("agreement_status", ag_status, "verified", None),
        ("agreement_student_levels", ag_levels, "verified", ag_levels_note),
        ("min_standing_note", min_standing_note, "verified", None),
        ("agreement_scope_raw", ag_scope_raw, "verified", None),
        ("available_terms", avail_terms, "verified", terms_notes),
        ("official_website_url", url_val, "verified", url_note),
    ]

    for field_name, val, status, note in excel_populated:
        if val is not None:
            record[field_name] = make_provenance(
                value=val,
                status=status,
                source_type="excel_list",
                source_ref=excel_ref,
                notes=note,
            )
        elif note is not None:
            record[field_name] = unknown_provenance(notes=note)

    record["ext"] = {}
    return record


def merge_preserving_research(existing: dict[str, Any], baseline: dict[str, Any], factual_fields: dict[str, dict[str, Any]]) -> dict[str, Any]:
    """Merge baseline manifest metadata into an existing record without clobbering researched fields."""
    merged = dict(baseline)
    for field_name in factual_fields:
        prev = existing.get(field_name)
        if isinstance(prev, dict) and prev.get("status") in ("verified", "partial"):
            merged[field_name] = prev
        elif isinstance(prev, dict) and merged[field_name].get("status") == "unknown":
            # Preserve existing unknown notes if baseline had none.
            if prev.get("notes") and not merged[field_name].get("notes"):
                merged[field_name]["notes"] = prev["notes"]
    if isinstance(existing.get("ext"), dict):
        merged["ext"] = existing["ext"]
    return merged


def init_from_manifest(
    schema_path: Path,
    manifest_path: Path,
    extracted_path: Path,
    records_dir: Path,
    force: bool,
) -> int:
    """Create or update records/<slug>.json from manifest.json + partner_list_extracted.json."""
    schema, factual_fields = load_schema_spec(schema_path)
    manifest_data = load_json(manifest_path)
    entries, _ = extract_manifest_universities(manifest_data)
    row_lookup = extract_row_lookup(load_json(extracted_path)) if extracted_path.exists() else {}

    records_dir.mkdir(parents=True, exist_ok=True)
    created = 0
    updated = 0
    for entry in entries:
        baseline = build_baseline_record(entry, row_lookup, factual_fields, schema)
        out_path = records_dir / f"{baseline['slug']}.json"
        if out_path.exists() and not force:
            existing = load_json(out_path)
            final_rec = merge_preserving_research(existing, baseline, factual_fields)
            updated += 1
        else:
            final_rec = baseline
            created += 1
        write_json(out_path, final_rec)

    print(
        f"Initialized {len(entries)} records in {records_dir} "
        f"(created={created}, merged_existing={updated}, force={force})."
    )
    return 0


def resolve_field_value_schema(prop_spec: dict[str, Any], defs: dict[str, Any]) -> dict[str, Any]:
    """Extract the 'value' sub-schema from a factual property specification in schema.json."""
    if "$ref" in prop_spec:
        def_name = prop_spec["$ref"].split("/")[-1]
        return resolve_field_value_schema(defs[def_name], defs)
    if "allOf" in prop_spec:
        merged: dict[str, Any] = {}
        for sub in prop_spec["allOf"]:
            if "properties" in sub and "value" in sub["properties"]:
                merged.update(sub["properties"]["value"])
            elif "$ref" in sub:
                merged.update(resolve_field_value_schema(sub, defs))
        return merged
    return prop_spec.get("properties", {}).get("value", {})


def validate_typed_value(
    field_name: str,
    value: Any,
    val_schema: dict[str, Any],
    is_url_field: bool,
    errors: list[str],
) -> None:
    """Validate a non-null factual value against its schema.json type & domain constraints."""
    allowed_types = val_schema.get("type", [])
    if isinstance(allowed_types, str):
        allowed_types = [allowed_types]
    non_null_types = [t for t in allowed_types if t != "null"]
    primary_type = non_null_types[0] if non_null_types else None

    if primary_type == "string":
        if not isinstance(value, str):
            errors.append(f"{field_name}.value: expected string, got {type(value).__name__}")
            return
        stripped = value.strip()
        if not stripped:
            errors.append(f"{field_name}.value: string must not be empty or whitespace")
            return
        enum_vals = val_schema.get("enum")
        if enum_vals is not None:
            valid_enums = [e for e in enum_vals if e is not None]
            if value not in valid_enums:
                errors.append(f"{field_name}.value: {value!r} not in allowed enum {valid_enums}")
        else:
            if stripped.lower() in PLACEHOLDER_TEXT:
                errors.append(f"{field_name}.value: placeholder string {value!r} is forbidden")
        if is_url_field:
            if not URL_RE.match(value):
                errors.append(f"{field_name}.value: invalid http(s) URL {value!r}")
            elif field_name == "official_website_url" and "drive.google.com" in value.lower():
                errors.append(
                    f"{field_name}.value: Google Drive URL belongs in fact_sheet_status, not official_website_url"
                )
    elif primary_type == "boolean":
        if not isinstance(value, bool):
            errors.append(f"{field_name}.value: expected boolean, got {type(value).__name__}")
    elif primary_type == "integer":
        if not is_int(value):
            errors.append(f"{field_name}.value: expected integer, got {type(value).__name__}")
            return
        minimum = val_schema.get("minimum")
        if minimum is not None and value < minimum:
            errors.append(f"{field_name}.value: {value} < minimum {minimum}")
    elif primary_type == "number":
        if not is_number(value):
            errors.append(f"{field_name}.value: expected finite number, got {type(value).__name__}")
            return
        minimum = val_schema.get("minimum")
        maximum = val_schema.get("maximum")
        if minimum is not None and value < minimum:
            errors.append(f"{field_name}.value: {value} < minimum {minimum}")
        if maximum is not None and value > maximum:
            errors.append(f"{field_name}.value: {value} > maximum {maximum}")
    elif primary_type == "array":
        if not isinstance(value, list):
            errors.append(f"{field_name}.value: expected array, got {type(value).__name__}")
            return
        min_items = val_schema.get("minItems", 0)
        if len(value) < min_items:
            errors.append(f"{field_name}.value: expected at least {min_items} item(s), got {len(value)}")
        item_spec = val_schema.get("items", {})
        item_enum = item_spec.get("enum")
        seen: set[str] = set()
        for idx, item in enumerate(value):
            if not isinstance(item, str) or not item.strip():
                errors.append(f"{field_name}.value[{idx}]: expected non-empty string")
                continue
            if item in seen:
                errors.append(f"{field_name}.value[{idx}]: duplicate item {item!r}")
            seen.add(item)
            if item_enum is not None and item not in item_enum:
                errors.append(f"{field_name}.value[{idx}]: {item!r} not in {item_enum}")
            elif item_enum is None and item.strip().lower() in PLACEHOLDER_TEXT:
                errors.append(f"{field_name}.value[{idx}]: placeholder string {item!r} is forbidden")
    elif primary_type == "object":
        if not isinstance(value, dict):
            errors.append(f"{field_name}.value: expected object, got {type(value).__name__}")
            return
        expected_keys = set(val_schema.get("required", []))
        if set(value.keys()) != expected_keys:
            errors.append(f"{field_name}.value: keys {sorted(value.keys())} must match {sorted(expected_keys)}")
            return
        cur = value.get("currency")
        if not isinstance(cur, str) or not CURRENCY_RE.match(cur):
            errors.append(f"{field_name}.value.currency: must be 3-letter uppercase ISO code, got {cur!r}")
        lo = value.get("min")
        hi = value.get("max")
        if not is_number(lo) or float(lo) < 0:
            errors.append(f"{field_name}.value.min: must be finite number >= 0, got {lo!r}")
        if not is_number(hi) or float(hi) < 0:
            errors.append(f"{field_name}.value.max: must be finite number >= 0, got {hi!r}")
        if is_number(lo) and is_number(hi) and float(hi) < float(lo):
            errors.append(f"{field_name}.value: max ({hi}) must be >= min ({lo})")


def validate_living_cost_invariant(record: dict[str, Any], errors: list[str]) -> None:
    """Enforce special currency, FX rate, and status invariants on living_cost_usd_est."""
    usd_env = record.get("living_cost_usd_est")
    local_env = record.get("est_monthly_living_local")
    if not isinstance(usd_env, dict):
        return
    usd_val = usd_env.get("value")
    if usd_val is None:
        return

    local_val = local_env.get("value") if isinstance(local_env, dict) else None
    usd_status = usd_env.get("status")
    notes = usd_env.get("notes")

    if usd_status == "verified":
        if not isinstance(local_val, dict) or local_val.get("currency") != "USD":
            errors.append(
                "living_cost_usd_est: status='verified' is only allowed when officially stated in USD "
                "(est_monthly_living_local.value.currency must be 'USD')"
            )
        return

    if usd_status == "partial":
        if not isinstance(local_val, dict):
            errors.append(
                "living_cost_usd_est: non-null partial estimate must be backed by non-null est_monthly_living_local"
            )
            return
        if not isinstance(notes, str) or not notes.strip():
            errors.append(
                "living_cost_usd_est: notes must document official local figure, FX rate, and date (YYYY-MM-DD)"
            )
            return
        if not DATE_RE.search(notes):
            errors.append(
                "living_cost_usd_est.notes: must include conversion/verification date in YYYY-MM or YYYY-MM-DD format"
            )
        local_cur = str(local_val.get("currency", ""))
        if local_cur != "USD":
            has_cur = local_cur in notes.upper()
            has_fx_hint = bool(FX_HINT_RE.search(notes)) and bool(NUMBER_RE.search(notes))
            if not (has_cur and has_fx_hint):
                errors.append(
                    f"living_cost_usd_est.notes: must state local currency ({local_cur}) and numeric FX rate"
                )


def validate_record(
    record: Any,
    expected_slug: str,
    schema: dict[str, Any],
    factual_fields: dict[str, dict[str, Any]],
) -> list[str]:
    """Validate a single university record dictionary and return a list of errors."""
    errors: list[str] = []
    if not isinstance(record, dict):
        return ["root must be a JSON object"]

    required_keys = set(schema["required"])
    actual_keys = set(record.keys())
    missing = sorted(required_keys - actual_keys)
    extra = sorted(actual_keys - required_keys)
    if missing:
        errors.append(f"missing required keys: {missing}")
    if extra:
        errors.append(f"unexpected extra keys: {extra}")
    if missing:
        return errors

    expected_ver = schema["properties"]["schema_version"]["const"]
    if record["schema_version"] != expected_ver:
        errors.append(f"schema_version: expected {expected_ver!r}, got {record['schema_version']!r}")

    slug = record["slug"]
    if not isinstance(slug, str) or not SLUG_RE.match(slug) or not (2 <= len(slug) <= 120):
        errors.append(f"slug: invalid kebab-case identifier {slug!r}")
    elif slug != expected_slug:
        errors.append(f"slug: {slug!r} does not match filename stem {expected_slug!r}")

    excel_rows = record["excel_rows"]
    if not isinstance(excel_rows, list) or not excel_rows:
        errors.append("excel_rows: must be a non-empty list of integers >= 2")
    else:
        if any(not is_int(r) or r < 2 for r in excel_rows):
            errors.append(f"excel_rows: all items must be integers >= 2, got {excel_rows!r}")
        if len(set(excel_rows)) != len(excel_rows):
            errors.append(f"excel_rows: contains duplicate row numbers {excel_rows!r}")

    shard = record["shard"]
    if not is_int(shard) or not (1 <= shard <= 8):
        errors.append(f"shard: must be integer in 1..8, got {shard!r}")

    fs = record["fact_sheet_status"]
    if not isinstance(fs, dict):
        errors.append("fact_sheet_status: must be an object")
    else:
        fs_req = {"match_status", "drive_links", "local_files", "extracted_files"}
        fs_allowed = fs_req | {"notes"}
        if not fs_req.issubset(fs.keys()) or not set(fs.keys()).issubset(fs_allowed):
            errors.append(f"fact_sheet_status: invalid keys {sorted(fs.keys())}")
        if fs.get("match_status") not in ("matched", "unmatched_here", "blocked", "none"):
            errors.append(f"fact_sheet_status.match_status: invalid value {fs.get('match_status')!r}")
        for list_key in ("drive_links", "local_files", "extracted_files"):
            lst = fs.get(list_key)
            if not isinstance(lst, list) or any(not isinstance(x, str) or not x.strip() for x in lst):
                errors.append(f"fact_sheet_status.{list_key}: must be list of non-empty strings")
            elif list_key == "drive_links" and any(not URL_RE.match(x) for x in lst):
                errors.append(f"fact_sheet_status.drive_links: invalid URL in {lst!r}")
        if fs.get("notes") is not None and not isinstance(fs.get("notes"), str):
            errors.append("fact_sheet_status.notes: must be string or null")

    xrefs = record["cross_references"]
    if not isinstance(xrefs, list) or any(not isinstance(x, str) or not SLUG_RE.match(x) for x in xrefs):
        errors.append(f"cross_references: must be list of valid slugs, got {xrefs!r}")
    elif len(set(xrefs)) != len(xrefs):
        errors.append(f"cross_references: duplicate slugs {xrefs!r}")
    elif isinstance(slug, str) and slug in xrefs:
        errors.append("cross_references: record must not cross-reference itself")

    if not isinstance(record["ext"], dict):
        errors.append("ext: must be a JSON object")

    defs = schema.get("$defs", {})
    for field_name, prop_spec in factual_fields.items():
        env = record[field_name]
        if not isinstance(env, dict):
            errors.append(f"{field_name}: must be a provenance object")
            continue
        if set(env.keys()) != set(PROVENANCE_KEYS):
            errors.append(f"{field_name}: provenance keys {sorted(env.keys())} must equal {list(PROVENANCE_KEYS)}")
            continue

        val = env["value"]
        status = env["status"]
        source_type = env["source_type"]
        source_ref = env["source_ref"]
        notes = env["notes"]

        if status not in VALID_STATUSES:
            errors.append(f"{field_name}.status: invalid {status!r}")
        if source_type not in VALID_SOURCE_TYPES:
            errors.append(f"{field_name}.source_type: invalid {source_type!r}")
        if notes is not None and not isinstance(notes, str):
            errors.append(f"{field_name}.notes: must be string or null")

        # Strict zero-fabrication provenance invariants
        if status == "unknown":
            if val is not None:
                errors.append(f"{field_name}: status='unknown' requires value=null, got {val!r}")
            if source_type != "unknown":
                errors.append(f"{field_name}: status='unknown' requires source_type='unknown', got {source_type!r}")
            if source_ref is not None:
                errors.append(f"{field_name}: status='unknown' requires source_ref=null, got {source_ref!r}")
        elif status in ("verified", "partial"):
            if val is None:
                errors.append(f"{field_name}: status={status!r} forbids value=null")
            if source_type not in NON_UNKNOWN_SOURCE_TYPES:
                errors.append(
                    f"{field_name}: status={status!r} forbids source_type={source_type!r}"
                )
            if not isinstance(source_ref, str) or not source_ref.strip():
                errors.append(f"{field_name}: status={status!r} requires non-empty source_ref")
            elif source_type == "official_web" and not URL_RE.match(source_ref):
                errors.append(
                    f"{field_name}.source_ref: source_type='official_web' requires valid http(s) URL, got {source_ref!r}"
                )

        if val is not None:
            val_schema = resolve_field_value_schema(prop_spec, defs)
            is_url_field = prop_spec.get("$ref") == "#/$defs/provenance_url"
            validate_typed_value(field_name, val, val_schema, is_url_field, errors)

    validate_living_cost_invariant(record, errors)
    return errors


def validate_dataset(
    schema_path: Path,
    manifest_path: Path,
    records_dir: Path,
    expected_count: int | None = None,
) -> tuple[list[dict[str, Any]], dict[str, list[str]]]:
    """Validate all records/*.json against schema, provenance invariants, and manifest.json."""
    schema, factual_fields = load_schema_spec(schema_path)
    manifest_data = load_json(manifest_path)
    manifest_entries, manifest_expected_count = extract_manifest_universities(manifest_data)
    target_count = expected_count if expected_count is not None else manifest_expected_count

    errors_by_file: dict[str, list[str]] = defaultdict(list)

    manifest_by_slug: dict[str, dict[str, Any]] = {}
    manifest_rows_all: list[int] = []
    for entry in manifest_entries:
        view = manifest_entry_fields(entry)
        slug = str(view.get("slug") or view.get("id") or "").strip()
        if not slug:
            errors_by_file["manifest.json"].append(f"entry missing slug/id: {entry}")
            continue
        if slug in manifest_by_slug:
            errors_by_file["manifest.json"].append(f"duplicate slug in manifest: {slug}")
        manifest_by_slug[slug] = view
        rows = [int(r) for r in (view.get("excel_rows") or view.get("rows") or []) if is_int(r)]
        manifest_rows_all.extend(rows)

    if target_count is not None and len(manifest_by_slug) != target_count:
        errors_by_file["manifest.json"].append(
            f"manifest slug count {len(manifest_by_slug)} != expected_count {target_count}"
        )

    if not records_dir.exists():
        errors_by_file[str(records_dir)].append("records directory does not exist")
        return [], dict(errors_by_file)

    record_files = sorted(
        p for p in records_dir.iterdir() if p.is_file() and p.suffix == ".json" and not p.name.startswith(".")
    )
    file_slugs = {p.stem for p in record_files}
    manifest_slugs = set(manifest_by_slug.keys())

    missing_slugs = sorted(manifest_slugs - file_slugs)
    extra_slugs = sorted(file_slugs - manifest_slugs)
    if missing_slugs:
        errors_by_file["manifest_parity"].append(f"missing record files for manifest slugs: {missing_slugs}")
    if extra_slugs:
        errors_by_file["manifest_parity"].append(f"unexpected record files not in manifest: {extra_slugs}")

    loaded_records: list[dict[str, Any]] = []
    seen_excel_rows: dict[int, str] = {}

    for rec_path in record_files:
        rel_name = rec_path.name
        try:
            rec = load_json(rec_path)
        except json.JSONDecodeError as exc:
            errors_by_file[rel_name].append(f"invalid JSON: {exc}")
            continue

        rec_errors = validate_record(rec, rec_path.stem, schema, factual_fields)
        if rec_errors:
            errors_by_file[rel_name].extend(rec_errors)
            continue

        slug = rec["slug"]
        if slug in manifest_by_slug:
            m_entry = manifest_by_slug[slug]
            m_rows = sorted({int(r) for r in (m_entry.get("excel_rows") or m_entry.get("rows") or []) if is_int(r)})
            if sorted(rec["excel_rows"]) != m_rows:
                errors_by_file[rel_name].append(
                    f"excel_rows {rec['excel_rows']} != manifest excel_rows {m_rows}"
                )
            m_shard = m_entry.get("shard")
            if is_int(m_shard) and rec["shard"] != int(m_shard):
                errors_by_file[rel_name].append(
                    f"shard {rec['shard']} != manifest shard {m_shard}"
                )

        for r in rec["excel_rows"]:
            if r in seen_excel_rows:
                errors_by_file[rel_name].append(
                    f"duplicate excel_row {r} already claimed by {seen_excel_rows[r]}"
                )
            else:
                seen_excel_rows[r] = slug

        for xref in rec["cross_references"]:
            if xref not in manifest_slugs:
                errors_by_file[rel_name].append(f"cross_references contains unknown slug {xref!r}")

        loaded_records.append(rec)

    if not missing_slugs and not extra_slugs and not errors_by_file:
        if set(seen_excel_rows.keys()) != set(manifest_rows_all):
            errors_by_file["manifest_parity"].append(
                "union of excel_rows across records does not match manifest.json"
            )

    loaded_records.sort(key=lambda r: r["slug"])
    return loaded_records, dict(errors_by_file)


def csv_cell(value: Any) -> str:
    """Format a scalar, bool, list, or null value for CSV export."""
    if value is None:
        return ""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (list, dict)):
        return compact_json(value)
    return str(value)


def record_to_csv_row(rec: dict[str, Any], factual_fields: dict[str, dict[str, Any]]) -> dict[str, str]:
    """Flatten a canonical university record into a CSV row dictionary."""
    monthly = rec["est_monthly_living_local"]["value"] or {}
    counts = Counter(rec[f]["status"] for f in factual_fields)
    row = {
        "slug": rec["slug"],
        "excel_rows": compact_json(rec["excel_rows"]),
        "shard": str(rec["shard"]),
        "fact_sheet_match_status": rec["fact_sheet_status"]["match_status"],
        "fact_sheet_files": compact_json(rec["fact_sheet_status"]["local_files"]),
        "cross_references": compact_json(rec["cross_references"]),
        "official_name": csv_cell(rec["official_name"]["value"]),
        "short_name": csv_cell(rec["short_name"]["value"]),
        "country": csv_cell(rec["country"]["value"]),
        "state_or_province": csv_cell(rec["state_or_province"]["value"]),
        "city": csv_cell(rec["city"]["value"]),
        "region": csv_cell(rec["region"]["value"]),
        "agreement_status": csv_cell(rec["agreement_status"]["value"]),
        "agreement_student_levels": csv_cell(rec["agreement_student_levels"]["value"]),
        "min_standing_note": csv_cell(rec["min_standing_note"]["value"]),
        "agreement_scope_raw": csv_cell(rec["agreement_scope_raw"]["value"]),
        "eligible_auc_schools": csv_cell(rec["eligible_auc_schools"]["value"]),
        "eligible_majors": csv_cell(rec["eligible_majors"]["value"]),
        "excluded_majors": csv_cell(rec["excluded_majors"]["value"]),
        "min_gpa_host": csv_cell(rec["min_gpa_host"]["value"]),
        "instruction_languages": csv_cell(rec["instruction_languages"]["value"]),
        "english_course_availability": csv_cell(rec["english_course_availability"]["value"]),
        "english_proficiency_and_rhet1020": csv_cell(rec["english_proficiency_and_rhet1020"]["value"]),
        "host_language_requirement": csv_cell(rec["host_language_requirement"]["value"]),
        "available_terms": csv_cell(rec["available_terms"]["value"]),
        "fall_term_dates": csv_cell(rec["fall_term_dates"]["value"]),
        "spring_term_dates": csv_cell(rec["spring_term_dates"]["value"]),
        "final_semester_allowed": csv_cell(rec["final_semester_allowed"]["value"]),
        "seat_quota": csv_cell(rec["seat_quota"]["value"]),
        "tuition_model": csv_cell(rec["tuition_model"]["value"]),
        "est_monthly_living_currency": csv_cell(monthly.get("currency")),
        "est_monthly_living_min": csv_cell(monthly.get("min")),
        "est_monthly_living_max": csv_cell(monthly.get("max")),
        "living_cost_usd_est": csv_cell(rec["living_cost_usd_est"]["value"]),
        "living_cost_usd_est_status": rec["living_cost_usd_est"]["status"],
        "mandatory_host_fees_and_insurance": csv_cell(rec["mandatory_host_fees_and_insurance"]["value"]),
        "housing_availability": csv_cell(rec["housing_availability"]["value"]),
        "housing_notes": csv_cell(rec["housing_notes"]["value"]),
        "host_grants_or_stipends": csv_cell(rec["host_grants_or_stipends"]["value"]),
        "citizenship_restrictions": csv_cell(rec["citizenship_restrictions"]["value"]),
        "visa_lead_time_and_financial_proof": csv_cell(rec["visa_lead_time_and_financial_proof"]["value"]),
        "official_website_url": csv_cell(rec["official_website_url"]["value"]),
        "exchange_page_url": csv_cell(rec["exchange_page_url"]["value"]),
        "course_catalog_url": csv_cell(rec["course_catalog_url"]["value"]),
        "english_course_list_url": csv_cell(rec["english_course_list_url"]["value"]),
        "credit_system_and_load": csv_cell(rec["credit_system_and_load"]["value"]),
        "course_restrictions_note": csv_cell(rec["course_restrictions_note"]["value"]),
        "required_application_docs": csv_cell(rec["required_application_docs"]["value"]),
        "academic_style": csv_cell(rec["academic_style"]["value"]),
        "city_tier": csv_cell(rec["city_tier"]["value"]),
        "campus_type": csv_cell(rec["campus_type"]["value"]),
        "daily_life_english_friendliness": csv_cell(rec["daily_life_english_friendliness"]["value"]),
        "international_support_services": csv_cell(rec["international_support_services"]["value"]),
        "nomination_deadlines": csv_cell(rec["nomination_deadlines"]["value"]),
        "ext_json": compact_json(rec["ext"]),
        "verified_field_count": str(counts["verified"]),
        "partial_field_count": str(counts["partial"]),
        "unknown_field_count": str(counts["unknown"]),
    }
    return row


def write_universities_csv(path: Path, records: list[dict[str, Any]], factual_fields: dict[str, dict[str, Any]]) -> None:
    """Write flat relational CSV export sorted by slug."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=CSV_COLUMNS, lineterminator="\n")
        writer.writeheader()
        for rec in records:
            writer.writerow(record_to_csv_row(rec, factual_fields))


def sql_literal(val: Any) -> str:
    """Safely format a Python scalar as a single-line SQLite literal."""
    if val is None:
        return "NULL"
    if isinstance(val, bool):
        return "1" if val else "0"
    if isinstance(val, int):
        return str(val)
    if isinstance(val, float):
        return repr(val)
    s = str(val)
    if "\x00" in s:
        raise ValueError("NUL byte forbidden in SQL string literal")
    s = s.replace("\r\n", "\n")
    parts: list[str] = []
    for idx, line in enumerate(s.split("\n")):
        if idx > 0:
            parts.append("char(10)")
        # Handle bare \r if any remained.
        subparts = line.split("\r")
        sub_sql = " || char(13) || ".join("'" + sp.replace("'", "''") + "'" for sp in subparts)
        parts.append(sub_sql)
    return " || ".join(parts)


SQL_UNIVERSITIES_DDL = """CREATE TABLE IF NOT EXISTS universities (
  slug TEXT PRIMARY KEY,
  excel_rows_json TEXT NOT NULL,
  shard INTEGER NOT NULL CHECK (shard BETWEEN 1 AND 8),
  fact_sheet_match_status TEXT NOT NULL,
  fact_sheet_files_json TEXT NOT NULL,
  cross_references_json TEXT NOT NULL,
  official_name TEXT,
  short_name TEXT,
  country TEXT,
  state_or_province TEXT,
  city TEXT,
  region TEXT,
  agreement_status TEXT,
  agreement_student_levels_json TEXT,
  min_standing_note TEXT,
  agreement_scope_raw TEXT,
  eligible_auc_schools_json TEXT,
  eligible_majors_json TEXT,
  excluded_majors_json TEXT,
  min_gpa_host REAL,
  instruction_languages_json TEXT,
  english_course_availability TEXT,
  english_proficiency_and_rhet1020 TEXT,
  host_language_requirement TEXT,
  available_terms_json TEXT,
  fall_term_dates TEXT,
  spring_term_dates TEXT,
  final_semester_allowed INTEGER CHECK (final_semester_allowed IN (0, 1) OR final_semester_allowed IS NULL),
  seat_quota INTEGER,
  tuition_model TEXT,
  est_monthly_living_currency TEXT,
  est_monthly_living_min REAL,
  est_monthly_living_max REAL,
  living_cost_usd_est REAL,
  living_cost_usd_est_status TEXT NOT NULL,
  mandatory_host_fees_and_insurance TEXT,
  housing_availability TEXT,
  housing_notes TEXT,
  host_grants_or_stipends TEXT,
  citizenship_restrictions TEXT,
  visa_lead_time_and_financial_proof TEXT,
  official_website_url TEXT,
  exchange_page_url TEXT,
  course_catalog_url TEXT,
  english_course_list_url TEXT,
  credit_system_and_load TEXT,
  course_restrictions_note TEXT,
  required_application_docs_json TEXT,
  academic_style TEXT,
  city_tier TEXT,
  campus_type TEXT,
  daily_life_english_friendliness TEXT,
  international_support_services TEXT,
  nomination_deadlines TEXT,
  ext_json TEXT NOT NULL,
  metadata_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS university_provenance (
  university_slug TEXT NOT NULL REFERENCES universities(slug) ON DELETE CASCADE,
  field_name TEXT NOT NULL,
  tier INTEGER NOT NULL CHECK (tier BETWEEN 1 AND 4),
  status TEXT NOT NULL CHECK (status IN ('verified', 'partial', 'unknown')),
  source_type TEXT NOT NULL CHECK (source_type IN ('excel_list', 'fact_sheet', 'official_web', 'unknown')),
  source_ref TEXT,
  notes TEXT,
  value_json TEXT,
  PRIMARY KEY (university_slug, field_name)
);

CREATE INDEX IF NOT EXISTS idx_universities_country ON universities(country);
CREATE INDEX IF NOT EXISTS idx_universities_region ON universities(region);
CREATE INDEX IF NOT EXISTS idx_universities_agreement_status ON universities(agreement_status);
CREATE INDEX IF NOT EXISTS idx_universities_shard ON universities(shard);
CREATE INDEX IF NOT EXISTS idx_provenance_field_status ON university_provenance(field_name, status);"""

SQL_UNIVERSITY_COLUMNS = [
    "slug",
    "excel_rows_json",
    "shard",
    "fact_sheet_match_status",
    "fact_sheet_files_json",
    "cross_references_json",
    "official_name",
    "short_name",
    "country",
    "state_or_province",
    "city",
    "region",
    "agreement_status",
    "agreement_student_levels_json",
    "min_standing_note",
    "agreement_scope_raw",
    "eligible_auc_schools_json",
    "eligible_majors_json",
    "excluded_majors_json",
    "min_gpa_host",
    "instruction_languages_json",
    "english_course_availability",
    "english_proficiency_and_rhet1020",
    "host_language_requirement",
    "available_terms_json",
    "fall_term_dates",
    "spring_term_dates",
    "final_semester_allowed",
    "seat_quota",
    "tuition_model",
    "est_monthly_living_currency",
    "est_monthly_living_min",
    "est_monthly_living_max",
    "living_cost_usd_est",
    "living_cost_usd_est_status",
    "mandatory_host_fees_and_insurance",
    "housing_availability",
    "housing_notes",
    "host_grants_or_stipends",
    "citizenship_restrictions",
    "visa_lead_time_and_financial_proof",
    "official_website_url",
    "exchange_page_url",
    "course_catalog_url",
    "english_course_list_url",
    "credit_system_and_load",
    "course_restrictions_note",
    "required_application_docs_json",
    "academic_style",
    "city_tier",
    "campus_type",
    "daily_life_english_friendliness",
    "international_support_services",
    "nomination_deadlines",
    "ext_json",
    "metadata_json",
]


def opt_json(val: Any) -> str | None:
    """Serialize non-null list/object as compact JSON or return None for SQL NULL."""
    return None if val is None else compact_json(val)


def write_universities_sql(path: Path, records: list[dict[str, Any]], factual_fields: dict[str, dict[str, Any]]) -> None:
    """Write deterministic SQLite DDL + idempotent seed script."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = [
        "-- Auto-generated by scripts/export_universities.py. Do not edit by hand.",
        "BEGIN TRANSACTION;",
        "",
        SQL_UNIVERSITIES_DDL,
        "",
        "-- Idempotent reset for deterministic canonical seed state.",
        "DELETE FROM university_provenance;",
        "DELETE FROM universities;",
        "",
    ]

    cols_sql = ", ".join(SQL_UNIVERSITY_COLUMNS)
    for rec in records:
        monthly = rec["est_monthly_living_local"]["value"] or {}
        values = [
            rec["slug"],
            compact_json(rec["excel_rows"]),
            rec["shard"],
            rec["fact_sheet_status"]["match_status"],
            compact_json(rec["fact_sheet_status"]["local_files"]),
            compact_json(rec["cross_references"]),
            rec["official_name"]["value"],
            rec["short_name"]["value"],
            rec["country"]["value"],
            rec["state_or_province"]["value"],
            rec["city"]["value"],
            rec["region"]["value"],
            rec["agreement_status"]["value"],
            opt_json(rec["agreement_student_levels"]["value"]),
            rec["min_standing_note"]["value"],
            rec["agreement_scope_raw"]["value"],
            opt_json(rec["eligible_auc_schools"]["value"]),
            opt_json(rec["eligible_majors"]["value"]),
            opt_json(rec["excluded_majors"]["value"]),
            rec["min_gpa_host"]["value"],
            opt_json(rec["instruction_languages"]["value"]),
            rec["english_course_availability"]["value"],
            rec["english_proficiency_and_rhet1020"]["value"],
            rec["host_language_requirement"]["value"],
            opt_json(rec["available_terms"]["value"]),
            rec["fall_term_dates"]["value"],
            rec["spring_term_dates"]["value"],
            rec["final_semester_allowed"]["value"],
            rec["seat_quota"]["value"],
            rec["tuition_model"]["value"],
            monthly.get("currency"),
            monthly.get("min"),
            monthly.get("max"),
            rec["living_cost_usd_est"]["value"],
            rec["living_cost_usd_est"]["status"],
            rec["mandatory_host_fees_and_insurance"]["value"],
            rec["housing_availability"]["value"],
            rec["housing_notes"]["value"],
            rec["host_grants_or_stipends"]["value"],
            rec["citizenship_restrictions"]["value"],
            rec["visa_lead_time_and_financial_proof"]["value"],
            rec["official_website_url"]["value"],
            rec["exchange_page_url"]["value"],
            rec["course_catalog_url"]["value"],
            rec["english_course_list_url"]["value"],
            rec["credit_system_and_load"]["value"],
            rec["course_restrictions_note"]["value"],
            opt_json(rec["required_application_docs"]["value"]),
            rec["academic_style"]["value"],
            rec["city_tier"]["value"],
            rec["campus_type"]["value"],
            rec["daily_life_english_friendliness"]["value"],
            rec["international_support_services"]["value"],
            rec["nomination_deadlines"]["value"],
            compact_json(rec["ext"]),
            compact_json(rec),
        ]
        vals_sql = ", ".join(sql_literal(v) for v in values)
        lines.append(f"INSERT OR REPLACE INTO universities ({cols_sql}) VALUES ({vals_sql});")

    lines.append("")
    prov_cols = "university_slug, field_name, tier, status, source_type, source_ref, notes, value_json"
    for rec in records:
        slug = rec["slug"]
        for field_name in sorted(factual_fields):
            tier = int(factual_fields[field_name]["x-tier"])
            env = rec[field_name]
            val_json = None if env["value"] is None else compact_json(env["value"])
            prov_vals = ", ".join(
                sql_literal(v)
                for v in (
                    slug,
                    field_name,
                    tier,
                    env["status"],
                    env["source_type"],
                    env["source_ref"],
                    env["notes"],
                    val_json,
                )
            )
            lines.append(f"INSERT OR REPLACE INTO university_provenance ({prov_cols}) VALUES ({prov_vals});")

    lines.append("")
    lines.append("COMMIT;")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def pct(num: int, den: int) -> str:
    """Format percentage string cleanly."""
    if den == 0:
        return "0.0%"
    return f"{(100.0 * num / den):.1f}%"


def write_coverage_report(path: Path, records: list[dict[str, Any]], factual_fields: dict[str, dict[str, Any]]) -> None:
    """Generate deterministic Markdown coverage report across tiers, fields, sources, and shards."""
    n_univ = len(records)
    n_fields = len(factual_fields)
    total_slots = n_univ * n_fields
    excel_row_total = sum(len(r["excel_rows"]) for r in records)

    status_counts: Counter[str] = Counter()
    source_counts: Counter[str] = Counter()
    tier_fields: dict[int, list[str]] = defaultdict(list)
    for fname, prop in factual_fields.items():
        tier_fields[int(prop["x-tier"])].append(fname)

    for rec in records:
        for fname in factual_fields:
            env = rec[fname]
            status_counts[env["status"]] += 1
            source_counts[env["source_type"]] += 1

    lines: list[str] = [
        "# AUC Exchange Partner Universities — Coverage Report",
        "",
        "## Summary",
        "",
        "| Metric | Value |",
        "|---|---:|",
        f"| Canonical Universities | {n_univ} |",
        f"| Excel Source Rows Covered | {excel_row_total} |",
        f"| Factual Fields per Record | {n_fields} |",
        f"| Total Factual Field Slots | {total_slots} |",
        f"| Verified Slots | {status_counts['verified']} ({pct(status_counts['verified'], total_slots)}) |",
        f"| Partial Slots | {status_counts['partial']} ({pct(status_counts['partial'], total_slots)}) |",
        f"| Unknown Slots | {status_counts['unknown']} ({pct(status_counts['unknown'], total_slots)}) |",
        f"| Non-Unknown Completeness | {status_counts['verified'] + status_counts['partial']} ({pct(status_counts['verified'] + status_counts['partial'], total_slots)}) |",
        "",
        "## Source Type Breakdown",
        "",
        "| Source Type | Field Slots | Share |",
        "|---|---:|---:|",
    ]
    for st in VALID_SOURCE_TYPES:
        lines.append(f"| `{st}` | {source_counts[st]} | {pct(source_counts[st], total_slots)} |")

    lines.extend([
        "",
        "## Coverage by Tier",
        "",
        "| Tier | Fields | Total Slots | Verified | Partial | Unknown | Non-Unknown % |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ])
    for tier in sorted(tier_fields):
        fnames = tier_fields[tier]
        t_slots = n_univ * len(fnames)
        tc = Counter(rec[f]["status"] for rec in records for f in fnames)
        non_unk = tc["verified"] + tc["partial"]
        lines.append(
            f"| Tier {tier} | {len(fnames)} | {t_slots} | {tc['verified']} | {tc['partial']} | "
            f"{tc['unknown']} | {pct(non_unk, t_slots)} |"
        )

    lines.extend([
        "",
        "## Coverage by Field",
        "",
        "| Tier | Field | Verified | Partial | Unknown | Non-Unknown % | `excel_list` | `fact_sheet` | `official_web` |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ])
    for fname, prop in factual_fields.items():
        tier = int(prop["x-tier"])
        fc = Counter(rec[fname]["status"] for rec in records)
        sc = Counter(rec[fname]["source_type"] for rec in records)
        non_unk = fc["verified"] + fc["partial"]
        lines.append(
            f"| Tier {tier} | `{fname}` | {fc['verified']} | {fc['partial']} | {fc['unknown']} | "
            f"{pct(non_unk, n_univ)} | {sc['excel_list']} | {sc['fact_sheet']} | {sc['official_web']} |"
        )

    lines.extend([
        "",
        "## Breakdown by Fact Sheet Match Status",
        "",
        "| Fact Sheet Match Status | Universities | Share | Avg Non-Unknown Fields |",
        "|---|---:|---:|---:|",
    ])
    fs_groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for rec in records:
        fs_groups[rec["fact_sheet_status"]["match_status"]].append(rec)
    for status_key in ("matched", "unmatched_here", "blocked", "none"):
        grp = fs_groups.get(status_key, [])
        avg = (
            sum(
                sum(1 for f in factual_fields if r[f]["status"] != "unknown")
                for r in grp
            )
            / len(grp)
            if grp
            else 0.0
        )
        lines.append(f"| `{status_key}` | {len(grp)} | {pct(len(grp), n_univ)} | {avg:.1f} / {n_fields} |")

    lines.extend([
        "",
        "## Breakdown by Agreement Status",
        "",
        "| Agreement Status | Universities | Share |",
        "|---|---:|---:|",
    ])
    ag_counts = Counter(rec["agreement_status"]["value"] or "unknown" for rec in records)
    for ag_key in ("active", "letter_of_intent", "check_slots", "unknown"):
        lines.append(f"| `{ag_key}` | {ag_counts[ag_key]} | {pct(ag_counts[ag_key], n_univ)} |")

    lines.extend([
        "",
        "## Breakdown by Shard (1–8)",
        "",
        "| Shard | Universities | Fact-Sheet Matched | Verified Slots | Partial Slots | Unknown Slots | Completeness |",
        "|---:|---:|---:|---:|---:|---:|---:|",
    ])
    by_shard: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for rec in records:
        by_shard[rec["shard"]].append(rec)
    for shard_id in range(1, 9):
        grp = by_shard.get(shard_id, [])
        s_slots = len(grp) * n_fields
        matched_n = sum(1 for r in grp if r["fact_sheet_status"]["match_status"] == "matched")
        sc = Counter(r[f]["status"] for r in grp for f in factual_fields)
        non_unk = sc["verified"] + sc["partial"]
        lines.append(
            f"| {shard_id} | {len(grp)} | {matched_n} | {sc['verified']} | {sc['partial']} | "
            f"{sc['unknown']} | {pct(non_unk, s_slots)} |"
        )

    lines.extend([
        "",
        "## Breakdown by Region",
        "",
        "| Region | Universities | Share |",
        "|---|---:|---:|",
    ])
    reg_counts = Counter(rec["region"]["value"] or "unknown" for rec in records)
    for reg, cnt in sorted(reg_counts.items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"| {reg} | {cnt} | {pct(cnt, n_univ)} |")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def check_sqlite_roundtrip(
    sql_path: Path,
    json_path: Path,
    csv_path: Path,
    schema_path: Path,
) -> list[str]:
    """Load sql_path into sqlite3 :memory: and verify exact row counts + JSON/CSV/SQL parity."""
    errors: list[str] = []
    _, factual_fields = load_schema_spec(schema_path)
    json_payload = load_json(json_path)
    json_records: list[dict[str, Any]] = json_payload["universities"]

    with csv_path.open("r", encoding="utf-8", newline="") as fh:
        csv_rows = list(csv.DictReader(fh))

    sql_text = sql_path.read_text(encoding="utf-8")
    conn = sqlite3.connect(":memory:")
    try:
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON;")
        conn.executescript(sql_text)

        integrity = conn.execute("PRAGMA integrity_check;").fetchone()[0]
        if integrity != "ok":
            errors.append(f"SQLite integrity_check failed: {integrity}")

        sql_rows = conn.execute("SELECT * FROM universities ORDER BY slug ASC;").fetchall()
        prov_count = conn.execute("SELECT COUNT(*) FROM university_provenance;").fetchone()[0]
    finally:
        conn.close()

    expected_n = len(json_records)
    if json_payload.get("record_count") != expected_n:
        errors.append(f"universities.json record_count={json_payload.get('record_count')} != {expected_n}")
    if len(csv_rows) != expected_n:
        errors.append(f"CSV row count {len(csv_rows)} != JSON count {expected_n}")
    if len(sql_rows) != expected_n:
        errors.append(f"SQLite row count {len(sql_rows)} != JSON count {expected_n}")
    expected_prov_n = expected_n * len(factual_fields)
    if prov_count != expected_prov_n:
        errors.append(f"SQLite university_provenance count {prov_count} != expected {expected_prov_n}")

    if errors:
        return errors

    for idx, rec in enumerate(json_records):
        slug = rec["slug"]
        crow = csv_rows[idx]
        srow = sql_rows[idx]

        if crow["slug"] != slug or srow["slug"] != slug:
            errors.append(f"row {idx}: slug parity mismatch json={slug}, csv={crow['slug']}, sql={srow['slug']}")
            continue

        expected_crow = record_to_csv_row(rec, factual_fields)
        for col in CSV_COLUMNS:
            if crow.get(col) != expected_crow[col]:
                errors.append(f"{slug}: CSV column '{col}' mismatch: {crow.get(col)!r} != {expected_crow[col]!r}")

        parsed_meta = json.loads(srow["metadata_json"])
        if parsed_meta != rec:
            errors.append(f"{slug}: SQLite metadata_json does not round-trip identically to JSON record")

        scalar_checks = [
            ("official_name", rec["official_name"]["value"]),
            ("country", rec["country"]["value"]),
            ("state_or_province", rec["state_or_province"]["value"]),
            ("city", rec["city"]["value"]),
            ("region", rec["region"]["value"]),
            ("agreement_status", rec["agreement_status"]["value"]),
            ("min_gpa_host", rec["min_gpa_host"]["value"]),
            ("living_cost_usd_est", rec["living_cost_usd_est"]["value"]),
            ("housing_availability", rec["housing_availability"]["value"]),
            ("official_website_url", rec["official_website_url"]["value"]),
            ("seat_quota", rec["seat_quota"]["value"]),
        ]
        for col, exp_val in scalar_checks:
            if srow[col] != exp_val:
                errors.append(f"{slug}: SQLite column '{col}' mismatch: {srow[col]!r} != {exp_val!r}")

        sql_final = srow["final_semester_allowed"]
        exp_final = (
            None
            if rec["final_semester_allowed"]["value"] is None
            else (1 if rec["final_semester_allowed"]["value"] else 0)
        )
        if sql_final != exp_final:
            errors.append(f"{slug}: SQLite final_semester_allowed {sql_final!r} != {exp_final!r}")

    return errors


def run_validate(args: argparse.Namespace) -> int:
    """Execute validation and print per-file errors."""
    records, errors_by_file = validate_dataset(
        schema_path=args.schema,
        manifest_path=args.manifest,
        records_dir=args.records_dir,
        expected_count=args.expected_count,
    )
    if errors_by_file:
        print(f"VALIDATION FAILED ({len(errors_by_file)} file/section error group(s)):", file=sys.stderr)
        for fname, errs in sorted(errors_by_file.items()):
            for err in errs:
                print(f"  [{fname}] {err}", file=sys.stderr)
        return 1
    print(f"Validation passed: {len(records)} canonical records verified cleanly.")
    return 0


def run_build(args: argparse.Namespace) -> int:
    """Validate records, emit JSON/CSV/SQL/Markdown, and run SQLite :memory: verification."""
    records, errors_by_file = validate_dataset(
        schema_path=args.schema,
        manifest_path=args.manifest,
        records_dir=args.records_dir,
        expected_count=args.expected_count,
    )
    if errors_by_file:
        print(f"BUILD ABORTED — validation failed ({len(errors_by_file)} group(s)):", file=sys.stderr)
        for fname, errs in sorted(errors_by_file.items()):
            for err in errs:
                print(f"  [{fname}] {err}", file=sys.stderr)
        return 1

    schema, factual_fields = load_schema_spec(args.schema)
    excel_row_total = sum(len(r["excel_rows"]) for r in records)

    out_payload = {
        "schema_version": schema["properties"]["schema_version"]["const"],
        "record_count": len(records),
        "excel_row_count": excel_row_total,
        "generated_by": "scripts/export_universities.py",
        "universities": records,
    }
    write_json(args.out_json, out_payload)
    write_universities_csv(args.out_csv, records, factual_fields)
    write_universities_sql(args.out_sql, records, factual_fields)
    write_coverage_report(args.out_report, records, factual_fields)

    sqlite_errors = check_sqlite_roundtrip(
        sql_path=args.out_sql,
        json_path=args.out_json,
        csv_path=args.out_csv,
        schema_path=args.schema,
    )
    if sqlite_errors:
        print("SQLITE ROUNDTRIP CHECK FAILED:", file=sys.stderr)
        for err in sqlite_errors:
            print(f"  {err}", file=sys.stderr)
        return 1

    print(
        f"Build succeeded ({len(records)} records, {excel_row_total} Excel rows): "
        f"{args.out_json}, {args.out_csv}, {args.out_sql}, {args.out_report} (SQLite :memory: parity OK)."
    )
    return 0


def run_check_sqlite(args: argparse.Namespace) -> int:
    """Run standalone SQLite :memory: load and parity verification."""
    errors = check_sqlite_roundtrip(
        sql_path=args.out_sql,
        json_path=args.out_json,
        csv_path=args.out_csv,
        schema_path=args.schema,
    )
    if errors:
        print("SQLite check failed:", file=sys.stderr)
        for err in errors:
            print(f"  {err}", file=sys.stderr)
        return 1
    payload = load_json(args.out_json)
    print(f"SQLite :memory: check passed ({payload['record_count']} universities, JSON/CSV/SQL parity OK).")
    return 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(
        description="Initialize, validate, and export AUC Exchange Partner University records."
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--init-from-manifest", action="store_true", help="Initialize baseline records from manifest.json")
    mode.add_argument("--validate", action="store_true", help="Validate records/*.json against schema & invariants")
    mode.add_argument("--build", action="store_true", help="Validate and build JSON, CSV, SQL, coverage_report.md + SQLite check")
    mode.add_argument("--check-sqlite", action="store_true", help="Verify db/seed/universities.sql in sqlite3 :memory:")

    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA_PATH, help="Path to schema.json")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST_PATH, help="Path to manifest.json")
    parser.add_argument("--extracted", type=Path, default=DEFAULT_EXTRACTED_PATH, help="Path to partner_list_extracted.json")
    parser.add_argument("--records-dir", type=Path, default=DEFAULT_RECORDS_DIR, help="Directory containing <slug>.json records")
    parser.add_argument("--out-json", type=Path, default=DEFAULT_OUT_JSON, help="Output path for universities.json")
    parser.add_argument("--out-csv", type=Path, default=DEFAULT_OUT_CSV, help="Output path for universities.csv")
    parser.add_argument("--out-sql", type=Path, default=DEFAULT_OUT_SQL, help="Output path for universities.sql")
    parser.add_argument("--out-report", type=Path, default=DEFAULT_OUT_REPORT, help="Output path for coverage_report.md")
    parser.add_argument("--expected-count", type=int, default=None, help="Enforce exact canonical record count")
    parser.add_argument("--force", action="store_true", help="Overwrite existing verified/partial records during --init-from-manifest")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    """CLI entry point."""
    args = parse_args(argv)
    if args.init_from_manifest:
        return init_from_manifest(
            schema_path=args.schema,
            manifest_path=args.manifest,
            extracted_path=args.extracted,
            records_dir=args.records_dir,
            force=args.force,
        )
    if args.validate:
        return run_validate(args)
    if args.build:
        return run_build(args)
    if args.check_sqlite:
        return run_check_sqlite(args)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
