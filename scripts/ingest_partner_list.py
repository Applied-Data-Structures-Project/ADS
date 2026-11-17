#!/usr/bin/env python3
"""Stage 1 Data Ingestion & Google Drive Fact Sheet Acquisition.

Subcommands / modes (cleanly separating offline processing from Drive network I/O):
  --parse-excel     Parse Excel sheet 'Universities List' (Rows 2..163) + hyperlinks
                    into data/raw/partner_list_extracted.json (offline).
  --download-drive  Recursively crawl master Drive folder + direct Col H links into
                    data/raw/factsheets/ (network required; logs auth/404 blocks).
  --extract-text    Extract text from all downloaded fact sheets into
                    data/raw/extracted/<rel_path>.txt with [Page N] markers (offline).
  --build-manifest  Deduplicate 162 -> 155 canonical universities, normalize fields,
                    match Drive fact sheets, assign balanced shards 1..8, and write
                    data/universities/manifest.json (offline).
  --verify          Verify counts, invariants, file existence, and extraction quality
                    across all Stage 1 artifacts (offline).
"""

from __future__ import annotations

import argparse
import hashlib
import html as htmllib
import json
import re
import subprocess
import sys
import tempfile
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
COORDINATOR_STAGED_XLSX = Path(
    "/Users/Admin/.gemini/antigravity/brain/b5d8af8a-dac1-40f4-bcb8-7f9846575d5b/scratch/partner_list.xlsx"
)
LOCAL_STAGED_XLSX = Path(
    "/Users/Admin/.gemini/antigravity/brain/182976e2-d672-4485-bbee-51e077628856/scratch/partner_list.xlsx"
)

DEFAULT_EXTRACTED_JSON = REPO_ROOT / "data" / "raw" / "partner_list_extracted.json"
DEFAULT_FACTSHEETS_DIR = REPO_ROOT / "data" / "raw" / "factsheets"
DEFAULT_EXTRACTED_DIR = REPO_ROOT / "data" / "raw" / "extracted"
DEFAULT_MANIFEST_JSON = REPO_ROOT / "data" / "universities" / "manifest.json"
DEFAULT_DOWNLOAD_META = DEFAULT_FACTSHEETS_DIR / "_download_manifest.json"
DEFAULT_EXTRACTION_META = DEFAULT_EXTRACTED_DIR / "_extraction_manifest.json"

MASTER_DRIVE_FOLDER_ID = "1EP3kAAeGDpYv1lsdBjE8qqKqt50LZr0r"
MASTER_DRIVE_FOLDER_URL = f"https://drive.google.com/drive/folders/{MASTER_DRIVE_FOLDER_ID}"

HTTP_HEADERS = {"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"}
VIDEO_EXTS = {".mp4", ".mov", ".avi", ".mkv", ".webm"}
IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".tiff", ".bmp"}

# Exact 7 duplicate row pairs from the approved plan (162 -> 155 canonical universities).
DUPLICATE_ROW_PAIRS: list[tuple[int, int]] = [
    (36, 37),   # Freie Universität Berlin
    (59, 137),  # Koç University
    (64, 65),   # Louvain School of Management (UC Louvain)
    (89, 127),  # Pompeu Fabra University (UPF)
    (121, 136), # University of Groningen
    (122, 123), # Universidad Carlos III de Madrid (UC3M)
    (29, 149),  # Eberhard Karls Universität Tübingen
]

# Explicit canonical display names for merged or typo-corrected rows.
CANONICAL_NAME_OVERRIDES: dict[tuple[int, ...], str] = {
    (18,): "CETYS Universidad",
    (23,): "Constructor University",
    (29, 149): "Eberhard Karls Universität Tübingen",
    (36, 37): "Freie Universität Berlin",
    (45,): "Insper Instituto de Ensino e Pesquisa",
    (59, 137): "Koç University",
    (64, 65): "Louvain School of Management (UC Louvain)",
    (89, 127): "Pompeu Fabra University (UPF)",
    (121, 136): "University of Groningen",
    (122, 123): "Universidad Carlos III de Madrid (UC3M)",
    (129,): "University of Bratislava Eco",
    (146,): "Università degli Studi di Torino",
    (153,): "University of Palermo",
}

US_STATES: dict[str, str] = {
    "Alabama": "Alabama",
    "Arizona": "Arizona",
    "California": "California",
    "DC": "District of Columbia",
    "Florida": "Florida",
    "Illinois": "Illinois",
    "Iowa": "Iowa",
    "Kansas": "Kansas",
    "Maryland": "Maryland",
    "Massachusetts": "Massachusetts",
    "Michigan": "Michigan",
    "Minnesota": "Minnesota",
    "New Mexico": "New Mexico",
    "New York": "New York",
    "North Carolina": "North Carolina",
    "Ohio": "Ohio",
    "Oregon": "Oregon",
    "Pennsylvania": "Pennsylvania",
    "South Carolina": "South Carolina",
    "Tennessee": "Tennessee",
    "Texas": "Texas",
    "Virginia": "Virginia",
    "Washington": "Washington",
    "West Virginia": "West Virginia",
    "Wisconson": "Wisconsin",
    "Wisconsin": "Wisconsin",
}

CANADIAN_PROVINCES = {"Ontario", "Quebec"}

COUNTRY_NORMALIZATION: dict[str, str] = {
    "Brazil ": "Brazil",
    "China ": "China",
    "South Kore": "South Korea",
    "Tennessee ": "Tennessee",
    "UK": "United Kingdom",
    "UAE": "United Arab Emirates",
}

CITY_NORMALIZATION: dict[str, str] = {
    "Bermen": "Bremen",
    "Braganca": "Bragança",
    "Gunangzhou": "Guangzhou",
    "Lisboa": "Lisbon",
    "Ljubjiana": "Ljubljana",
    "Machester": "Manchester",
    "München": "Munich",
    "San Bernadino": "San Bernardino",
    "Torino": "Turin",
    "Tubingen": "Tübingen",
}

COUNTRY_TO_REGION: dict[str, str] = {
    "United States": "North America",
    "Canada": "North America",
    "Mexico": "Latin America & Caribbean",
    "Brazil": "Latin America & Caribbean",
    "Colombia": "Latin America & Caribbean",
    "Austria": "Europe",
    "Belgium": "Europe",
    "Czech Republic": "Europe",
    "Denmark": "Europe",
    "Estonia": "Europe",
    "Finland": "Europe",
    "France": "Europe",
    "Germany": "Europe",
    "Greece": "Europe",
    "Ireland": "Europe",
    "Italy": "Europe",
    "Lithuania": "Europe",
    "Netherlands": "Europe",
    "Norway": "Europe",
    "Poland": "Europe",
    "Portugal": "Europe",
    "Romania": "Europe",
    "Slovakia": "Europe",
    "Slovenia": "Europe",
    "Spain": "Europe",
    "Sweden": "Europe",
    "Switzerland": "Europe",
    "Turkey": "Europe",
    "United Kingdom": "Europe",
    "China": "East Asia",
    "Japan": "East Asia",
    "South Korea": "East Asia",
    "Taiwan": "East Asia",
    "India": "South Asia",
    "Kuwait": "Middle East & North Africa",
    "Morocco": "Middle East & North Africa",
    "Tunisia": "Middle East & North Africa",
    "United Arab Emirates": "Middle East & North Africa",
}

# Explicit verified mapping from master Google Drive top-level folder ID -> Excel primary row.
# Verified by inspecting folder names AND contained fact-sheet filenames.
EXPLICIT_FOLDER_ID_TO_EXCEL_ROW: dict[str, int] = {
    # Direct Col H folder IDs and/or exact partner folders in master Drive folder:
    "1pZbiOQ1mMgJb8JHj9gJk_R16E6tcUiZq": 5,    # (AUK)- American University in Kuwait -> AUK
    "1J29DWUv_CmjK1xaPC6q7oQLy2khntHOi": 2,    # Aarhus University
    "1__nVFZjtm1gAiawSCoAa1bsbbNghI7pD": 109,  # ACG- The American College of Greece
    "1p2mwNxpqw9ye76JjCdmDe7DMwl30MiBY": 3,    # Akita -> Akita International University
    "1AuQ5ya38rqEUK_WMXfndMlLO7h9ASkRe": 4,    # Al Akhawayn University
    "19LHON_BRXv3VnqFr3MgPyCn59NRlsGNG": 128,  # Antwerp -> University of Antwerp
    "1ZoWb0Xwlp58EN2k7xBMsEhdPuMYJL7e5": 8,    # Auburn -> Auburn University
    "1u-q2O2-dk4Geiosp6zb8w8-h4xwfvlIT": 110,  # AUEmirates -> The American University of the Emirates
    "1kiQpcteeg8hrI5Jbr5Vd91qwL4XiKooQ": 9,    # Augsburg -> Augsburg University
    "1XdeC9IQRiT0WV6OqoOZHqJxrJ_hP1Spd": 10,   # Bard College
    "1QXqoof0eDYuXvZnBxZSnZuRN1KaK04C9": 11,   # Bocconi -> Bocconi University
    "1ahLatRfYa7MkpPh2QYMis6kyQIZq8ZfY": 12,   # Bogazici -> Boğaziçi University
    "16Zp8bvPapiKkicFAWFwAYISON45EuZ14": 14,   # Brandeis -> Brandeis University
    "13z9N8-B050E-wJirR8faXQT5RvDoPBbg": 111,  # Catholic University of America
    "1bhNb35-Gv9qecbsRq5bi9zGNmdSIErLK": 16,   # Catholic University of the Sacred Heart
    "14CNwnYXZw8YyJm3mAsH7G1Sk91xJLUwW": 17,   # Central European University (CEU)
    "1wTieJCFwXjV65YEJbEdpCq9YAs9aikRq": 18,   # CETYS -> CETYS Universidad
    "1_65Iw7x1PpUJF8sWCvV8Bu08r_2-kGIc": 20,   # Chung-Ang University
    "17sEOXfy1jRpK7SayyiCBpaFt0l9-Rx1h": 21,   # College of Charleston
    "1x86w0dkfzugY4Psm-Cx_DC5kmhk27vS4": 22,   # Concordia University
    "1TpkP-qd3Isp53RoDbvXhnTO7DQtyV80n": 24,   # Copenhagen Business School
    "158kL6-59DIMUkqLCRNxE78KhkdiWK9o4": 26,   # Danish School of Media and Journalism (DMJX)
    "1Rs-5F9X8Lu-n0nJG98E8rSxWxh2F-lcN": 27,   # EADA Business School
    "19NqIX4-rGNaTRaXlEPC-Z3OCEJC9t2V9": 32,   # EDHEC Business School
    "18rksp7HXz0MQyK88ksBmlCeFeh9rt0bc": 33,   # Elon University
    "1Yx-wbVUZMdpLolPbOjhFAohvsuWXLMO_": 31,   # EM Normandie
    "15qgbIy01U-loR70Yhhay9z3fEswn3wCn": 34,   # Emlyon Business School
    "1hYkLDA896DBrzHduic16B6V6zD7WO3tW": 35,   # Frankfurt School of Finance & Management
    "1s1UPTvrn81gxWfUkqtkfM2cVZ05dJqUB": 36,   # FUBerlin -> Freie Universität Berlin (36,37)
    "1EPeCUtSgeYB8_vhNMkhzoCU9WR3GoQo_": 112,  # Geneva Graduate Institute
    "1UxIwZU3YHRbXh8DSuQkrNC8U4WVQfT9x": 112,  # The Geneva Graduate Institute
    "1QfOMFjYJE6QTKBdAnENcvcB0OLkSBWgf": 121,  # Groningen -> University of Groningen (121,136)
    "1LJyGXsQSdLRz5EMf7zwTz5DfDRanqPok": 121,  # UoGronigen -> University of Groningen (121,136)
    "152mbKfihE9FpT3jLRbwTp98HcoFyuN1J": 113,  # GW (Elliott School) -> GWU Elliott School
    "1QxTTHQ29L5f1bv07KMkQN6MrN49cHpBq": 113,  # The George Washington University (Elliott School)
    "1l9wLwgfFBDt9Elw60gZKqygBkYYi5yuG": 39,   # Heilbronn University of Applied Sciences
    "1aGvpU8d_CwLq0JyfDiSyWWlxHbdh1qda": 40,   # Hertie School of Governance
    "1UBiESwHjY6orytBwMnYK9GYnbFhW1Wr7": 47,   # IBEI
    "1QXh2DQooF5s9VZjmhukWkVDrCBBjdKMt": 41,   # ICN Business School
    "1Hq2Uskyx3z1i69tKpu4WoJ9D6tgLkGyU": 43,   # Indian Institute of Management Bangalore (IIM-B)
    "1hWfYxCu9L0JCyaVt4tlyfznjru_YkCOC": 44,   # Indian Institute of Technology- Delhi (IIT- Delhi)
    "11gRnIMav69wjl2iKSniS41NX0bSz2OZi": 46,   # Institut Polytechnique de Sciences Avancees (IPSA)
    "1lvHANLKzh2VwOn7NCB2PRE4Y3JNUIkbK": 46,   # IPSA France -> IPSA
    "1UyxxCQAXpjqWK7eUiYdVIDnxid6j54ut": 48,   # IULM
    "1dpoW_YuoXPBHWmrUQK64EWDD8TnZ9SBW": 63,   # ISEG - Lisbon School of Economics and Management
    "1MdLAadIqe5iZ60AKf6SYbbKri6pM42_x": 42,   # IÉSEG School of Management
    "1h3DAfmkOFlUG6Y-Yy6aUscmiY8uDjFIv": 50,   # Jagiellonian University
    "15gTFpw9ymsXcJqwoeELtHQeMBrjS1VlN": 51,   # James Madison University
    "1L-vvV-n6H5NNHxPuIZ-eWYHP-L3bcp-m": 52,   # Jinan University
    "1Dstuhj7JqNcXlKMUpigLJQKQzLVALnhP": 53,   # John Cabot University
    "1E0mfllVPRBHJNPK5EDpGLJ6EREqKbhBf": 117,  # Kansas University -> The University of Kansas
    "1tjG3DKqsOkJbLORzXBf6VnwsItCP9Oef": 57,   # Kent State University
    "1taehKFp8Q9AqbReXy5E-glzGiOUc18OM": 58,   # King's College London
    "1aG_di-d7cOr6x-QTaKQMxgZSgePZnSXE": 138,  # Konstanz -> University of Konstanz
    "1sDv8eX-Wb_l0uWc3dVOLUtTgLE3hA7_f": 60,   # Kozminski University
    "1q526MEN1cewFiemdKPVUjVU3bpRc3OR7": 59,   # Koç University (59,137)
    "1odSL8Uob294gFhArjjEQ3EoQ1RD4xeDt": 61,   # Leiden University
    "1B2E8hnb80gUR5vcFkNSG3dSE-xGyRZ2r": 62,   # Lingnan University
    "17zaSXHRHf5JVvBVvLIqTCY7EUfz0PuKg": 66,   # Lund University
    "1Rv4Bcv1ReGyPF-xeukkS9jontDpQ5GZ_": 68,   # Marquette University
    "1A7gOjCJfa56JA9bZz_qPj-nOciVLnQ3P": 69,   # MCI- Austria -> MCI Management Center Innsbruck
    "1OAGtbnxbndwAX5DhdXeGt4Os6f6MBxx5": 71,   # Mediterranean School of Business
    "1gQ0h90V9DbVr1fOw5W2ZYgjjTZtrR_a3": 72,   # Meiji Gakuin University (folder 1)
    "1sdMlQJSEcdPV4ElcFlw99Yxx1fubmuN-": 72,   # Meiji Gakuin University (folder 2)
    "1o8e3FfflmH3lN2gzJuI0dknEfIFTT_O1": 74,   # Nagoya University of Commerce and Business (NUCB)
    "1nYjTZVZQohliB2TzrV7NSqNneS9oWsRi": 75,   # National Chengchi University
    "1FbBbBXA8JPXOt3XyKW08jDiuzLYMti6S": 118,  # New Mexico -> The University of New Mexico
    "1mo26pB5cCITvJwYW0yEyyucIT-4kPUXF": 118,  # TUo New Mexico -> The University of New Mexico
    "1IBynKwXHDmRmE682e8CEz0-0aYyaqsj0": 77,   # North Carolina State -> NC State
    "1sV9gdJGwzso49C69VnnpzRUjIpjHfsaf": 78,   # Northern Arizona University
    "1gShlhWt5JSAPGMGp-6K_8j22VKeiKZK-": 80,   # Nova School of Business and Economics (Nova SBE)
    "1l5gA_uHaXpKP-bCR7-7T2MNUXvtdDCDF": 81,   # Oberlin College
    "1EgyRE_BoLTSwsrrxv5JexZ2J8k1EZBVT": 83,   # Ozyegin University
    "1G_zeOnKRNiaiq55tUXM04iM-b3FPnS51": 84,   # Paris School of Business
    "1PXVKfwtF1DBACWY6pVSktsdaT5AVtaQe": 85,   # Peking University
    "1fv1MgJi1J-G3rma1rUwxk8T2zqLnOkaz": 86,   # Pforzeim University -> Pforzheim University
    "1jp4StnKDMQrojpoVaJmvNkHdO1SvNSwK": 90,   # Queens University -> Queen's University at Kingston
    "1UoV56g8WQKWBfETCqAfUh_DqewZ3h0cM": 91,   # Radboud University
    "1qY6tugesQQt5SzP5AO8_b1KRgVgC371N": 92,   # Ramon Llull University
    "1XyLRXL4qDbuj5fY6EDwj5bouG996qzmo": 93,   # Reed College
    "18hNmYQXTGbcNzNpjoQVzc4fFpXWufJg5": 94,   # Rennes School of Business
    "1qwWDMW1SQmpZBI2fZdZnQItyTosn9IFh": 95,   # Richmond American University, London
    "1siMEcHUDuUraUfd6d0-_vYo2zW5XSrhw": 95,   # Richmond University (London Study Abroad - verified in extraction)
    "1084OFy2nA2ZBYOzhoHm4q55reC4LqJKT": 96,   # Ritsumeikan Asia Pacific University (APU)
    "1c020HMSHDYohrOyJ6m5-g0o8Jv9Rdoqm": 98,   # Sathyabama Institute of Science and Technology
    "1JYs-MDRtqMEpnyVY5ttIZk-ndC5qsD2j": 99,   # Sciences Po
    "1MIJHqNDeAxX4PGB7Tl2F8eazrZrdQONA": 100,  # Soka University
    "1HbwBGDWem64xnuD4qL5dHCq6o-WYHeLT": 144,  # St. Andrews -> University of St. Andrews
    "1STeYKk01QP6JZrXuhZgh5ZBETNGve5nn": 144,  # UoSt. Andrews -> University of St. Andrews
    "1JjBqs1Y0krydAeIM9G_TKVn5pQ47z1KF": 101,  # St. Olaf College
    "1HDA1DMeKUls3WCdYXpr_Jxo2QJxDRr8Y": 104,  # Stony Brook University
    "1X9p2jzjSWtIID2RcMI7D6gwH4dS_wuiW": 102,  # SUNY - Cortland
    "1mSCERJFYMpai0vR8O5Lokp4XMkYA2k2x": 103,  # SUNY - New Paltz
    "1J9_Zam7a2XrQKkFTKm9Wq26UQbgvIxDa": 105,  # Technical University of Darmstadt
    "1RFFj9ufVwc_5oV1jFVhXbCR0AvNcYfNz": 106,  # Technical University of Munich
    "12rkhOnevpfEjBwlMIuqWeBZzeEBHs_jc": 108,  # Technische Universiteit Eindhoven (TUE)
    "1di9M17DQ8Egh4wV8Czy7WmiemLUm0J0N": 115,  # The Polytechnic Institute of Braganca
    "1ONP_TUdhjh7V-JJ_uy0aXskHS42geDRA": 120,  # Trinity College (TCD Global Mobility Guide) -> Trinity College Dublin
    "1Zyh_TXdTVHtoimGtE30kwxQMgz1TJw29": 119,  # TUo Texas at Austin
    "1QwaXQy-u-q78gUYWo_650nQSJBr7Zk-4": 64,   # UC Louvain -> Louvain School of Management (64,65)
    "1xKI7jzrZpX3OtbfwBCtjOsqP7hf2EDYM": 132,  # UCF -> University of Central Florida
    "1Fpy02d0nsJgTNWi2n-LFzxDAXTRnFBxq": 150,  # UCollege Twente -> University of Twente
    "1cKjf2JEzkySoHo9-EVY_HcJ79IoFfLUE": 122,  # UC3M (122,123)
    "1R21bwA0-3wfkfXQ90e8yPlb19Uyc0oRI": 126,  # Universitat Autònoma de Barcelona (UAB)
    "1DTgMuIpl6PT01y5L1lAx3S8mhyT4AL1t": 89,   # Universitat Pompeu Fabra (UPF) (89,127)
    "1VYRymN3qrBbb2CO3Tz1xpoorOg7WDUuf": 116,  # University of Akron
    "1mBQ_Wj4FFDewx5N7s4H354gFhYqziWGh": 114,  # Folder named 'University of Manchester' contains 'Manchester Met Fact Sheet 2025-26.docx' -> MMU!
    "1u-GzRe71c2uIDADFw0bUwVzFhdMbI43D": 141,  # University of Oslo
    "1PiOzWGfe6YSc8ItV20IP9HoDlGinpMBY": 145,  # University of Tartu
    "1Pu-u4v2Vw44Rs4e8uIUfCwZAhcEvtkLY": 133,  # UoCopenhagen -> University of Copenhagen
    "1HVniR10ZqU1C_sx8Sqryk6I1aSFTcUdJ": 134,  # UoEdinburgh -> University of Edinburgh
    "1CAJDtwmcGwvoXT5e0z4eoCmoGrjFkgO7": 135,  # UoGeneva -> University of Geneva
    "17WSmKDHsL7sqH_XtZgqeE1wHwwMRWwz-": 151,  # UoHelsinki -> University of Helsinki
    "1Svyw9SiRCO-4o2qUgPC3MZtaZMcXiK1t": 142,  # UoPennsylvania -> University of Pennsylvania
    "1jfj51MpbI5U018hhIebpyUPcvVHwSqAO": 143,  # UoSouthern Denmark -> University of Southern Denmark
    "1lZ24KZrJntD1dQGCjpTCll5zPS10p6bK": 147,  # UoT Incarnate Word
    "1xPtJjgai-04k4QGi6ns4v7-pKE4iYbji": 148,  # UoT Pacific -> University of the Pacific
    "1mSPm1n1F39q8ZvNfbT9pKcT-9jXAWC-i": 29,   # UoTübingen -> Eberhard Karls Universität Tübingen (29,149)
    "14_s7m6Nmw3zAFtxjDqCOcK1YV-5pzn3A": 152,  # Vienna University of Economics and Business (WU)
    "1tLHTTH7Pw49ukpbEb0Ua0XTEdhX3-_ZK": 154,  # Virginia Tech
    "1DVfch3snb59SC9DRceuOKR-qh9Ncarf9": 155,  # Vrije Universiteit Amsterdam
    "1gUVt9VvHy4kCZ7Ko0_pp6CthuJpK4jzk": 159,  # Washington State University
    "1DvhloqNa6JA69ID5MBVaT2rImnD77YNL": 160,  # Wayne State University
    "1Dy3msT6bpUpqQufyn3nR8drc5fSEA7yZ": 161,  # Wellesley College
    "18wnQRAU5hYFgOEFkRAtUjk4qk6d6WypU": 162,  # West Virginia -> West Virginia University
}


def resolve_default_excel_path() -> Path:
    """Return the first accessible staged partner_list.xlsx path."""
    if COORDINATOR_STAGED_XLSX.exists():
        try:
            with COORDINATOR_STAGED_XLSX.open("rb"):
                return COORDINATOR_STAGED_XLSX
        except OSError:
            pass
    if LOCAL_STAGED_XLSX.exists():
        return LOCAL_STAGED_XLSX
    return COORDINATOR_STAGED_XLSX


def ascii_slug(text: str) -> str:
    """Convert an institution name into a deterministic ASCII kebab-case slug."""
    normalized = unicodedata.normalize("NFKD", text)
    ascii_str = normalized.encode("ascii", "ignore").decode("ascii").lower()
    ascii_str = ascii_str.replace("&", " and ")
    return re.sub(r"[^a-z0-9]+", "-", ascii_str).strip("-")


def sanitize_path_component(name: str) -> str:
    """Sanitize a folder or filename component safely for macOS/POSIX filesystems."""
    cleaned = name.strip().replace("/", "_").replace("\\", "_").replace("\x00", "")
    cleaned = re.sub(r"\s+", " ", cleaned)
    return cleaned or "unnamed"


def extract_drive_id_and_kind(url: str | None) -> tuple[str | None, str | None]:
    """Extract Google Drive/Docs ID and resource kind from a URL."""
    if not url:
        return None, None
    patterns = [
        (r"drive\.google\.com/drive/folders/([A-Za-z0-9_-]+)", "folder"),
        (r"drive\.google\.com/open\?id=([A-Za-z0-9_-]+)", "open"),
        (r"drive\.google\.com/file/d/([A-Za-z0-9_-]+)", "file"),
        (r"docs\.google\.com/document/d/([A-Za-z0-9_-]+)", "gdoc"),
        (r"docs\.google\.com/spreadsheets/d/([A-Za-z0-9_-]+)", "gsheet"),
        (r"docs\.google\.com/presentation/d/([A-Za-z0-9_-]+)", "gslides"),
    ]
    for pat, kind in patterns:
        m = re.search(pat, url)
        if m:
            return m.group(1), kind
    return None, None


def write_json(path: Path, data: Any) -> None:
    """Write deterministic formatted UTF-8 JSON."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# Stage 1A: Offline Excel Parsing
# ---------------------------------------------------------------------------

def parse_excel_universities_list(xlsx_path: Path) -> dict[str, Any]:
    """Parse 'Universities List' rows 2..163 and all embedded hyperlinks using stdlib XML."""
    ns_main = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
    ns_r = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
    ns = {"main": ns_main}

    with zipfile.ZipFile(xlsx_path, "r") as zf:
        wb_root = ET.fromstring(zf.read("xl/workbook.xml"))
        wb_rels = {
            el.attrib["Id"]: el.attrib["Target"]
            for el in ET.fromstring(zf.read("xl/_rels/workbook.xml.rels"))
        }

        target_sheet = None
        for sheet_el in wb_root.findall(".//main:sheet", ns):
            if sheet_el.attrib.get("name") == "Universities List":
                rid = sheet_el.attrib[f"{{{ns_r}}}id"]
                target_sheet = wb_rels[rid]
                if not target_sheet.startswith("xl/"):
                    target_sheet = f"xl/{target_sheet}"
                break
        if target_sheet is None:
            raise ValueError("Worksheet 'Universities List' not found in workbook")

        shared_strings: list[str] = []
        if "xl/sharedStrings.xml" in zf.namelist():
            ss_root = ET.fromstring(zf.read("xl/sharedStrings.xml"))
            for si in ss_root.findall("main:si", ns):
                shared_strings.append(
                    "".join((t.text or "") for t in si.iter(f"{{{ns_main}}}t"))
                )

        sheet_root = ET.fromstring(zf.read(target_sheet))
        rels_path = str(Path(target_sheet).parent / "_rels" / f"{Path(target_sheet).name}.rels")
        sheet_rels: dict[str, str] = {}
        if rels_path in zf.namelist():
            for rel in ET.fromstring(zf.read(rels_path)):
                sheet_rels[rel.attrib["Id"]] = rel.attrib.get("Target", "")

        hyperlinks: dict[str, str | None] = {}
        for hl in sheet_root.findall(".//main:hyperlink", ns):
            ref = hl.attrib.get("ref")
            hl_rid = hl.attrib.get(f"{{{ns_r}}}id")
            if ref:
                hyperlinks[ref] = sheet_rels.get(hl_rid) if hl_rid else hl.attrib.get("location")

        headers: dict[str, str] = {}
        rows: list[dict[str, Any]] = []
        for row_el in sheet_root.findall(".//main:sheetData/main:row", ns):
            row_num = int(row_el.attrib["r"])
            cell_vals: dict[str, str | None] = {}
            for cell_el in row_el.findall("main:c", ns):
                ref = cell_el.attrib["r"]
                col = "".join(ch for ch in ref if ch.isalpha())
                ctype = cell_el.attrib.get("t")
                v_el = cell_el.find("main:v", ns)
                is_el = cell_el.find("main:is", ns)
                val: str | None = None
                if ctype == "s" and v_el is not None and v_el.text is not None:
                    val = shared_strings[int(v_el.text)]
                elif ctype == "inlineStr" and is_el is not None:
                    val = "".join((t.text or "") for t in is_el.iter(f"{{{ns_main}}}t"))
                elif v_el is not None:
                    val = v_el.text
                cell_vals[col] = val

            if row_num == 1:
                headers = {k: (v.strip() if v else "") for k, v in cell_vals.items() if k in "ABCDEFGH"}
                continue

            if not (2 <= row_num <= 163):
                continue

            name_raw = cell_vals.get("A")
            if name_raw is None or not str(name_raw).strip():
                continue

            web_text = cell_vals.get("G")
            web_hl = hyperlinks.get(f"G{row_num}")
            resolved_web = web_hl.strip() if web_hl else None
            if not resolved_web and web_text:
                cand = web_text.strip()
                if cand.startswith(("http://", "https://")):
                    resolved_web = cand

            fs_text = cell_vals.get("H")
            fs_hl = hyperlinks.get(f"H{row_num}")
            drive_id, drive_kind = extract_drive_id_and_kind(fs_hl)

            rows.append(
                {
                    "row": row_num,
                    "institution_name_raw": name_raw,
                    "country_raw": cell_vals.get("B"),
                    "city_raw": cell_vals.get("C"),
                    "agreement_student_level_raw": cell_vals.get("D"),
                    "agreement_terms_raw": cell_vals.get("E"),
                    "agreement_scope_raw": cell_vals.get("F"),
                    "website_text": web_text,
                    "website_hyperlink": web_hl,
                    "website_url": resolved_web,
                    "fact_sheet_text": fs_text,
                    "fact_sheet_hyperlink": fs_hl,
                    "fact_sheet_drive_id": drive_id,
                    "fact_sheet_drive_kind": drive_kind,
                }
            )

    rows.sort(key=lambda r: r["row"])
    rows_by_row = {str(r["row"]): r for r in rows}
    return {
        "source_file": str(xlsx_path),
        "sheet_name": "Universities List",
        "extracted_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total_rows": len(rows),
        "columns": headers,
        "rows": rows,
        "rows_by_row": rows_by_row,
    }


# ---------------------------------------------------------------------------
# Stage 1B: Google Drive Traversal & File Download (Network step)
# ---------------------------------------------------------------------------

class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    """Prevent automatic redirect following when probing Drive auth status."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def fetch_embedded_folder(folder_id: str) -> dict[str, Any]:
    """Fetch entries from a public Google Drive folder via embeddedfolderview."""
    url = f"https://drive.google.com/embeddedfolderview?id={folder_id}#list"
    req = urllib.request.Request(url, headers=HTTP_HEADERS)
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            final_url = resp.geturl()
            body = resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        return {"ok": False, "status": exc.code, "error": f"HTTP {exc.code}: {exc.reason}", "entries": []}
    except Exception as exc:
        return {"ok": False, "status": None, "error": str(exc), "entries": []}

    if "accounts.google.com/ServiceLogin" in final_url or "<title>Sign in" in body:
        return {"ok": False, "status": 401, "error": "ServiceLogin_auth_required", "entries": []}

    entries: list[dict[str, Any]] = []
    for part in body.split('<div class="flip-entry"')[1:]:
        fid_m = re.search(r'id="entry-([^"]+)"', part)
        href_m = re.search(r'<a href="([^"]+)"', part)
        aria_m = re.search(r'aria-label="([^"]+)"', part)
        title_m = re.search(r'class="flip-entry-title"[^>]*>(.*?)</div>', part, re.S)
        mod_m = re.search(r'class="flip-entry-last-modified"[^>]*><div>(.*?)</div>', part, re.S)
        href = htmllib.unescape(href_m.group(1)) if href_m else ""
        is_folder = "/drive/folders/" in href or (aria_m is not None and aria_m.group(1) == "Folder")
        entries.append(
            {
                "id": fid_m.group(1) if fid_m else None,
                "name": htmllib.unescape(title_m.group(1)).strip() if title_m else "",
                "href": href,
                "is_folder": bool(is_folder),
                "last_modified": htmllib.unescape(mod_m.group(1)).strip() if mod_m else None,
            }
        )
    return {"ok": True, "status": 200, "entries": entries}


def detect_magic_extension(data: bytes) -> str | None:
    """Infer file extension from magic bytes when filename lacks one."""
    if data.startswith(b"%PDF-"):
        return ".pdf"
    if data.startswith(b"PK\x03\x04"):
        if b"word/" in data[:4096]:
            return ".docx"
        if b"xl/" in data[:4096]:
            return ".xlsx"
        if b"ppt/" in data[:4096]:
            return ".pptx"
        return ".zip"
    if data.startswith(b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"):
        return ".doc"
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return ".png"
    if data.startswith(b"\xff\xd8\xff"):
        return ".jpg"
    return None


def looks_like_html(data: bytes, content_type: str | None) -> bool:
    """Return True if payload appears to be an HTML page rather than a document."""
    head = data[:512].lstrip().lower()
    if head.startswith((b"<!doctype html", b"<html", b"<head", b"<script")):
        return True
    if content_type and "text/html" in content_type.lower() and not data.startswith((b"%PDF-", b"PK\x03\x04")):
        return True
    return False


def parse_drive_confirm_url(html_bytes: bytes) -> str | None:
    """Extract Drive large-file virus-scan confirmation URL from HTML warning page."""
    text = html_bytes.decode("utf-8", errors="replace")
    form_m = re.search(r'<form[^>]+id="download-form"[^>]+action="([^"]+)"[^>]*>(.*?)</form>', text, re.S)
    if not form_m:
        return None
    action = htmllib.unescape(form_m.group(1))
    inputs = dict(re.findall(r'<input[^>]+type="hidden"[^>]+name="([^"]+)"[^>]+value="([^"]*)"', form_m.group(2)))
    if not inputs:
        return None
    sep = "&" if "?" in action else "?"
    return f"{action}{sep}{urllib.parse.urlencode(inputs)}"


def parse_content_disposition_filename(header_val: str | None) -> str | None:
    """Extract filename from Content-Disposition header if present."""
    if not header_val:
        return None
    m_utf8 = re.search(r"filename\*\s*=\s*UTF-8''([^;]+)", header_val, re.I)
    if m_utf8:
        return urllib.parse.unquote(m_utf8.group(1)).strip()
    m_plain = re.search(r'filename\s*=\s*"?([^";]+)"?', header_val, re.I)
    if m_plain:
        return m_plain.group(1).strip()
    return None


def fetch_url_bytes(url: str) -> tuple[bytes, str, dict[str, str]]:
    """Fetch URL bytes with headers and handle Drive virus-scan confirm form."""
    req = urllib.request.Request(url, headers=HTTP_HEADERS)
    with urllib.request.urlopen(req, timeout=45) as resp:
        final_url = resp.geturl()
        headers = {k.lower(): v for k, v in resp.headers.items()}
        data = resp.read()

    if "accounts.google.com/ServiceLogin" in final_url:
        raise PermissionError("ServiceLogin_auth_required")

    if looks_like_html(data, headers.get("content-type")):
        confirm_url = parse_drive_confirm_url(data)
        if confirm_url:
            req2 = urllib.request.Request(confirm_url, headers=HTTP_HEADERS)
            with urllib.request.urlopen(req2, timeout=60) as resp2:
                final_url = resp2.geturl()
                headers = {k.lower(): v for k, v in resp2.headers.items()}
                data = resp2.read()
                if "accounts.google.com/ServiceLogin" in final_url:
                    raise PermissionError("ServiceLogin_auth_required")
    return data, final_url, headers


def download_single_drive_item(item: dict[str, Any], factsheets_dir: Path) -> dict[str, Any]:
    """Download one file/Google Doc from Drive or skip non-document video media cleanly."""
    fid = item["id"]
    orig_name = item["name"]
    href = item["href"]
    rel_dir = item["rel_dir"]
    ext_lower = Path(orig_name).suffix.lower()

    base_record = {
        "id": fid,
        "name": orig_name,
        "drive_url": href,
        "folder_path": item["folder_path"],
        "top_folder_id": item["top_folder_id"],
        "top_folder_name": item["top_folder_name"],
        "last_modified": item.get("last_modified"),
    }

    if ext_lower in VIDEO_EXTS:
        return {
            **base_record,
            "status": "skipped_media_video",
            "local_path": None,
            "size_bytes": None,
            "sha256": None,
            "error": None,
        }

    is_gdoc = "docs.google.com/document/" in href
    is_gsheet = "docs.google.com/spreadsheets/" in href
    is_gslides = "docs.google.com/presentation/" in href

    if is_gdoc:
        download_url = f"https://docs.google.com/document/d/{fid}/export?format=docx"
    elif is_gsheet:
        download_url = f"https://docs.google.com/spreadsheets/d/{fid}/export?format=xlsx"
    elif is_gslides:
        download_url = f"https://docs.google.com/presentation/d/{fid}/export?format=pptx"
    else:
        download_url = f"https://drive.google.com/uc?export=download&id={fid}"

    try:
        data, final_url, headers = fetch_url_bytes(download_url)
    except PermissionError as exc:
        return {**base_record, "status": "blocked_auth", "local_path": None, "size_bytes": None, "sha256": None, "error": str(exc)}
    except urllib.error.HTTPError as exc:
        status = "blocked_auth" if exc.code in (401, 403) else f"http_{exc.code}"
        return {**base_record, "status": status, "local_path": None, "size_bytes": None, "sha256": None, "error": f"HTTP {exc.code}: {exc.reason}"}
    except Exception as exc:
        return {**base_record, "status": "error", "local_path": None, "size_bytes": None, "sha256": None, "error": str(exc)}

    if looks_like_html(data, headers.get("content-type")):
        return {
            **base_record,
            "status": "error_html_response",
            "local_path": None,
            "size_bytes": len(data),
            "sha256": None,
            "error": "Drive returned HTML page instead of document payload",
            "final_url": final_url,
        }

    if not data:
        return {**base_record, "status": "error_empty_file", "local_path": None, "size_bytes": 0, "sha256": None, "error": "Empty payload"}

    cd_name = parse_content_disposition_filename(headers.get("content-disposition"))
    magic_ext = detect_magic_extension(data)
    safe_name = sanitize_path_component(orig_name)

    # Ensure proper document extension without mangling original title.
    known_doc_exts = {".pdf", ".docx", ".doc", ".xlsx", ".xls", ".pptx", ".ppt", ".txt", ".csv", ".png", ".jpg", ".jpeg", ".zip"}
    current_ext = Path(safe_name).suffix.lower()
    if current_ext not in known_doc_exts:
        cd_ext = Path(cd_name).suffix.lower() if cd_name else ""
        chosen_ext = magic_ext or (cd_ext if cd_ext in known_doc_exts else "")
        if not chosen_ext and is_gdoc:
            chosen_ext = ".docx"
        if chosen_ext:
            safe_name = f"{safe_name}{chosen_ext}"

    if item.get("disambiguate"):
        stem = Path(safe_name).stem
        suffix = Path(safe_name).suffix
        safe_name = f"{stem}__{fid[:6]}{suffix}"

    target_path = factsheets_dir / rel_dir / safe_name
    target_path.parent.mkdir(parents=True, exist_ok=True)
    target_path.write_bytes(data)

    rel_repo_path = str(target_path.relative_to(REPO_ROOT))
    sha256 = hashlib.sha256(data).hexdigest()
    return {
        **base_record,
        "status": "downloaded",
        "download_url": download_url,
        "content_type": headers.get("content-type"),
        "content_disposition_name": cd_name,
        "detected_extension": Path(safe_name).suffix.lower(),
        "local_path": rel_repo_path,
        "size_bytes": len(data),
        "sha256": sha256,
        "error": None,
    }


def download_all_drive_assets(
    extracted_json_path: Path,
    factsheets_dir: Path,
    download_meta_path: Path,
) -> dict[str, Any]:
    """Crawl master Drive folder tree + check all direct Col H Drive hyperlinks."""
    extracted = json.loads(extracted_json_path.read_text(encoding="utf-8"))
    factsheets_dir.mkdir(parents=True, exist_ok=True)

    # 1. BFS traversal of master folder.
    root_res = fetch_embedded_folder(MASTER_DRIVE_FOLDER_ID)
    if not root_res["ok"]:
        raise RuntimeError(f"Failed to read master Drive folder: {root_res}")

    # Track duplicate sanitized top-level folder names (e.g., two 'Meiji Gakuin University' folders).
    top_entries = [e for e in root_res["entries"] if e.get("is_folder") and e.get("id")]
    top_name_counts = Counter(sanitize_path_component(e["name"]) for e in top_entries)

    folder_records: dict[str, dict[str, Any]] = {}
    all_files: list[dict[str, Any]] = []
    seen_folders: set[str] = {MASTER_DRIVE_FOLDER_ID}
    queue: list[tuple[str, list[str], str, str, Path]] = []

    for ent in root_res["entries"]:
        if ent.get("is_folder") and ent.get("id"):
            san = sanitize_path_component(ent["name"])
            top_dir_name = f"{san}__{ent['id'][:6]}" if top_name_counts[san] > 1 else san
            queue.append((ent["id"], [ent["name"]], ent["id"], ent["name"], Path(top_dir_name)))
        elif ent.get("id"):
            all_files.append(
                {
                    "id": ent["id"],
                    "name": ent["name"],
                    "href": ent["href"],
                    "folder_path": [],
                    "top_folder_id": MASTER_DRIVE_FOLDER_ID,
                    "top_folder_name": "ROOT",
                    "rel_dir": Path("_root"),
                    "last_modified": ent.get("last_modified"),
                }
            )

    while queue:
        batch = []
        while queue:
            fid, path_names, top_id, top_name, rel_dir = queue.pop(0)
            if fid not in seen_folders:
                seen_folders.add(fid)
                batch.append((fid, path_names, top_id, top_name, rel_dir))
        if not batch:
            break
        with ThreadPoolExecutor(max_workers=10) as pool:
            futs = {pool.submit(fetch_embedded_folder, fid): (fid, path_names, top_id, top_name, rel_dir) for fid, path_names, top_id, top_name, rel_dir in batch}
            for fut in as_completed(futs):
                fid, path_names, top_id, top_name, rel_dir = futs[fut]
                res = fut.result()
                folder_records[fid] = {
                    "id": fid,
                    "path": path_names,
                    "top_folder_id": top_id,
                    "top_folder_name": top_name,
                    "rel_dir": str(rel_dir),
                    "ok": res["ok"],
                    "status": res["status"],
                    "error": res.get("error"),
                    "entry_count": len(res["entries"]),
                }
                for ent in res["entries"]:
                    if ent.get("is_folder") and ent.get("id"):
                        sub_san = sanitize_path_component(ent["name"])
                        queue.append((ent["id"], path_names + [ent["name"]], top_id, top_name, rel_dir / sub_san))
                    elif ent.get("id"):
                        all_files.append(
                            {
                                "id": ent["id"],
                                "name": ent["name"],
                                "href": ent["href"],
                                "folder_path": path_names,
                                "top_folder_id": top_id,
                                "top_folder_name": top_name,
                                "rel_dir": rel_dir,
                                "last_modified": ent.get("last_modified"),
                            }
                        )

    # Mark filename collisions within the same folder so files never overwrite each other.
    dir_name_counts = Counter((str(f["rel_dir"]), sanitize_path_component(f["name"])) for f in all_files)
    for f in all_files:
        if dir_name_counts[(str(f["rel_dir"]), sanitize_path_component(f["name"]))] > 1:
            f["disambiguate"] = True

    # 2. Download all discovered Drive files concurrently.
    download_results: list[dict[str, Any]] = []
    with ThreadPoolExecutor(max_workers=10) as pool:
        futs = [pool.submit(download_single_drive_item, item, factsheets_dir) for item in all_files]
        for fut in as_completed(futs):
            download_results.append(fut.result())

    download_results.sort(key=lambda r: (r["top_folder_name"].lower(), "/".join(r["folder_path"]), r["name"], r["id"]))

    # 3. Check direct Col H hyperlinks not present in the master Drive tree.
    seen_file_ids = {r["id"] for r in download_results}
    direct_col_h_checks: list[dict[str, Any]] = []
    extra_direct_rows = [
        r for r in extracted["rows"]
        if r.get("fact_sheet_drive_id")
        and r["fact_sheet_drive_id"] not in seen_folders
        and r["fact_sheet_drive_id"] not in seen_file_ids
    ]

    def check_col_h_link(row: dict[str, Any]) -> dict[str, Any]:
        did = row["fact_sheet_drive_id"]
        dkind = row["fact_sheet_drive_kind"]
        url = row["fact_sheet_hyperlink"]
        if dkind == "folder":
            res = fetch_embedded_folder(did)
            return {
                "excel_row": row["row"],
                "institution_name": row["institution_name_raw"].strip(),
                "drive_id": did,
                "drive_kind": dkind,
                "url": url,
                "status": "ok" if res["ok"] else f"http_{res['status']}",
                "error": res.get("error"),
                "local_path": None,
            }
        # Probe/attempt download for open?id= or file/d/ links.
        probe_item = {
            "id": did,
            "name": f"row_{row['row']}_{sanitize_path_component(row['institution_name_raw'])}",
            "href": url,
            "folder_path": ["_direct_col_h"],
            "top_folder_id": "_direct_col_h",
            "top_folder_name": "_direct_col_h",
            "rel_dir": Path("_direct_col_h"),
            "disambiguate": True,
        }
        dl_res = download_single_drive_item(probe_item, factsheets_dir)
        return {
            "excel_row": row["row"],
            "institution_name": row["institution_name_raw"].strip(),
            "drive_id": did,
            "drive_kind": dkind,
            "url": url,
            "status": dl_res["status"],
            "error": dl_res.get("error"),
            "local_path": dl_res.get("local_path"),
            "size_bytes": dl_res.get("size_bytes"),
            "sha256": dl_res.get("sha256"),
        }

    with ThreadPoolExecutor(max_workers=8) as pool:
        for res in pool.map(check_col_h_link, extra_direct_rows):
            direct_col_h_checks.append(res)

    top_level_folders = [
        {"id": fid, "name": info["path"][0], "rel_dir": info["rel_dir"], "ok": info["ok"], "entry_count": info["entry_count"]}
        for fid, info in folder_records.items()
        if len(info["path"]) == 1
    ]
    top_level_folders.sort(key=lambda x: x["name"].lower())

    status_counts = Counter(r["status"] for r in download_results)
    payload = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "master_folder_id": MASTER_DRIVE_FOLDER_ID,
        "master_folder_url": MASTER_DRIVE_FOLDER_URL,
        "top_level_folder_count": len(top_level_folders),
        "total_folder_count": len(folder_records),
        "total_discovered_files": len(download_results),
        "download_status_counts": dict(status_counts),
        "top_level_folders": top_level_folders,
        "folders": folder_records,
        "files": download_results,
        "direct_col_h_external_checks": direct_col_h_checks,
    }
    write_json(download_meta_path, payload)
    return payload


# ---------------------------------------------------------------------------
# Stage 1C: Local Text Extraction (Offline)
# ---------------------------------------------------------------------------

def extract_pdfs_batch_jxa(pdf_paths: list[Path], ocr_fallback: bool = True) -> dict[str, dict[str, Any]]:
    """Extract per-page text from PDFs using macOS PDFKit (+ Vision OCR fallback if empty)."""
    if not pdf_paths:
        return {}

    jxa_script = r'''
ObjC.import("PDFKit");
ObjC.import("Foundation");
ObjC.import("Vision");
ObjC.import("CoreGraphics");

function readJson(path) {
  var data = $.NSData.dataWithContentsOfFile(path);
  var str = $.NSString.alloc.initWithDataEncoding(data, $.NSUTF8StringEncoding);
  return JSON.parse(ObjC.unwrap(str));
}

function ocrCgImage(cgImg) {
  var handler = $.VNImageRequestHandler.alloc.initWithCGImageOptions(cgImg, $({}));
  var req = $.VNRecognizeTextRequest.alloc.init;
  req.recognitionLevel = $.VNRequestTextRecognitionLevelAccurate;
  req.usesLanguageCorrection = true;
  var err = Ref();
  handler.performRequestsError($.NSArray.arrayWithObject(req), err);
  var results = req.results;
  var lines = [];
  if (results) {
    var count = results.count;
    for (var i = 0; i < count; i++) {
      var obs = results.objectAtIndex(i);
      var cands = obs.topCandidates(1);
      if (cands && cands.count > 0) {
        lines.push(ObjC.unwrap(cands.objectAtIndex(0).string));
      }
    }
  }
  return lines.join("\n");
}

function run(argv) {
  var inputPath = argv[0];
  var outputPath = argv[1];
  var doOcr = argv[2] === "1";
  var paths = readJson(inputPath);
  var out = {};
  for (var idx = 0; idx < paths.length; idx++) {
    var p = paths[idx];
    try {
      var url = $.NSURL.fileURLWithPath(p);
      var doc = $.PDFDocument.alloc.initWithURL(url);
      if (!doc) {
        out[p] = { ok: false, error: "PDFDocument failed to open", pages: 0, text: "" };
        continue;
      }
      var pageCount = doc.pageCount;
      var chunks = [];
      var totalChars = 0;
      for (var i = 0; i < pageCount; i++) {
        var page = doc.pageAtIndex(i);
        var ptext = page ? ObjC.unwrap(page.string) : "";
        if (!ptext) ptext = "";
        totalChars += ptext.replace(/\s+/g, "").length;
        chunks.push("[Page " + (i + 1) + "]\n" + ptext);
      }
      var usedOcr = false;
      if (doOcr && totalChars === 0 && pageCount > 0) {
        usedOcr = true;
        chunks = [];
        var maxPages = Math.min(pageCount, 20);
        for (var i = 0; i < maxPages; i++) {
          var page = doc.pageAtIndex(i);
          var rect = page.boundsForBox($.kPDFDisplayBoxMediaBox);
          var scale = 2.0;
          var w = Math.max(100, Math.round(rect.size.width * scale));
          var h = Math.max(100, Math.round(rect.size.height * scale));
          var cs = $.CGColorSpaceCreateDeviceRGB();
          var ctx = $.CGBitmapContextCreate(null, w, h, 8, w * 4, cs, $.kCGImageAlphaPremultipliedLast);
          $.CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
          $.CGContextFillRect(ctx, $.CGRectMake(0, 0, w, h));
          $.CGContextScaleCTM(ctx, scale, scale);
          page.drawWithBoxToContext($.kPDFDisplayBoxMediaBox, ctx);
          var cgImg = $.CGBitmapContextCreateImage(ctx);
          var otext = cgImg ? ocrCgImage(cgImg) : "";
          chunks.push("[Page " + (i + 1) + "]\n" + otext);
        }
      }
      out[p] = { ok: true, pages: pageCount, ocr: usedOcr, text: chunks.join("\n\n") };
    } catch (e) {
      out[p] = { ok: false, error: String(e), pages: 0, text: "" };
    }
  }
  var jsonStr = $(JSON.stringify(out));
  jsonStr.writeToFileAtomicallyEncodingError(outputPath, true, $.NSUTF8StringEncoding, null);
  return "ok";
}
'''
    results: dict[str, dict[str, Any]] = {}
    batch_size = 40
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        script_file = tmp / "extract_pdfs.js"
        script_file.write_text(jxa_script, encoding="utf-8")

        for i in range(0, len(pdf_paths), batch_size):
            batch = pdf_paths[i : i + batch_size]
            in_json = tmp / f"in_{i}.json"
            out_json = tmp / f"out_{i}.json"
            in_json.write_text(json.dumps([str(p) for p in batch]), encoding="utf-8")
            proc = subprocess.run(
                ["osascript", "-l", "JavaScript", str(script_file), str(in_json), str(out_json), "1" if ocr_fallback else "0"],
                capture_output=True,
                text=True,
                timeout=240,
            )
            if proc.returncode != 0 or not out_json.exists():
                for p in batch:
                    results[str(p)] = {"ok": False, "error": f"osascript rc={proc.returncode}: {proc.stderr.strip()}", "pages": 0, "text": ""}
                continue
            batch_out = json.loads(out_json.read_text(encoding="utf-8"))
            results.update(batch_out)
    return results


def extract_docx_text(path: Path) -> str:
    """Extract readable paragraphs, tables, headers, and footers from a DOCX archive."""
    ns_w = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"

    def para_text(p_el: ET.Element) -> str:
        parts: list[str] = []
        for elem in p_el.iter():
            tag = elem.tag.split("}")[-1]
            if tag == "t" and elem.text:
                parts.append(elem.text)
            elif tag == "tab":
                parts.append("\t")
            elif tag in ("br", "cr"):
                parts.append("\n")
        return "".join(parts).strip()

    def walk_xml(xml_bytes: bytes) -> list[str]:
        root = ET.fromstring(xml_bytes)
        lines: list[str] = []
        body = root.find(f"{{{ns_w}}}body")
        container = body if body is not None else root
        for child in list(container):
            tag = child.tag.split("}")[-1]
            if tag == "p":
                txt = para_text(child)
                if txt:
                    lines.append(txt)
            elif tag == "tbl":
                for tr in child.findall(f"./{{{ns_w}}}tr"):
                    cells: list[str] = []
                    for tc in tr.findall(f"./{{{ns_w}}}tc"):
                        cell_paras = [para_text(p) for p in tc.findall(f".//{{{ns_w}}}p")]
                        cell_str = " / ".join(cp for cp in cell_paras if cp)
                        if cell_str:
                            cells.append(cell_str)
                    if cells:
                        lines.append(" | ".join(cells))
        return lines

    try:
        with zipfile.ZipFile(path, "r") as zf:
            names = zf.namelist()
            sections: list[str] = []
            for hname in sorted(n for n in names if re.match(r"word/header\d*\.xml$", n)):
                hlines = walk_xml(zf.read(hname))
                if hlines:
                    sections.extend(hlines)
            if "word/document.xml" in names:
                sections.extend(walk_xml(zf.read("word/document.xml")))
            for fname in sorted(n for n in names if re.match(r"word/footer\d*\.xml$", n)):
                flines = walk_xml(zf.read(fname))
                if flines:
                    sections.extend(flines)
            text = "\n".join(sections).strip()
            if text:
                return text
    except Exception:
        pass

    # Fallback to macOS textutil if XML extraction produced nothing or failed.
    proc = subprocess.run(
        ["/usr/bin/textutil", "-convert", "txt", "-stdout", str(path)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if proc.returncode == 0:
        return proc.stdout.strip()
    raise RuntimeError(f"Failed to extract DOCX text: {proc.stderr.strip()}")


def extract_doc_text(path: Path) -> str:
    """Extract text from legacy binary .doc files using macOS textutil."""
    proc = subprocess.run(
        ["/usr/bin/textutil", "-convert", "txt", "-stdout", str(path)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"textutil failed: {proc.stderr.strip()}")
    return proc.stdout.strip()


def extract_xlsx_text(path: Path) -> str:
    """Extract sheet-by-sheet cell contents from an .xlsx workbook."""
    ns_main = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
    ns_r = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
    ns = {"main": ns_main}
    out_lines: list[str] = []

    with zipfile.ZipFile(path, "r") as zf:
        wb_root = ET.fromstring(zf.read("xl/workbook.xml"))
        wb_rels = {
            el.attrib["Id"]: el.attrib["Target"]
            for el in ET.fromstring(zf.read("xl/_rels/workbook.xml.rels"))
        }
        shared_strings: list[str] = []
        if "xl/sharedStrings.xml" in zf.namelist():
            ss_root = ET.fromstring(zf.read("xl/sharedStrings.xml"))
            for si in ss_root.findall("main:si", ns):
                shared_strings.append("".join((t.text or "") for t in si.iter(f"{{{ns_main}}}t")))

        for sheet_el in wb_root.findall(".//main:sheet", ns):
            sname = sheet_el.attrib.get("name", "Sheet")
            rid = sheet_el.attrib[f"{{{ns_r}}}id"]
            target = wb_rels[rid]
            if not target.startswith("xl/"):
                target = f"xl/{target}"
            if target not in zf.namelist():
                continue
            out_lines.append(f"[Sheet: {sname}]")
            sroot = ET.fromstring(zf.read(target))
            for row_el in sroot.findall(".//main:sheetData/main:row", ns):
                vals: list[str] = []
                for c in row_el.findall("main:c", ns):
                    ct = c.attrib.get("t")
                    v = c.find("main:v", ns)
                    is_el = c.find("main:is", ns)
                    val = ""
                    if ct == "s" and v is not None and v.text is not None:
                        val = shared_strings[int(v.text)]
                    elif ct == "inlineStr" and is_el is not None:
                        val = "".join((t.text or "") for t in is_el.iter(f"{{{ns_main}}}t"))
                    elif v is not None and v.text is not None:
                        val = v.text
                    val = val.strip()
                    if val:
                        vals.append(val)
                if vals:
                    out_lines.append(" | ".join(vals))
    return "\n".join(out_lines).strip()


def extract_pptx_text(path: Path) -> str:
    """Extract slide-by-slide text from a .pptx presentation."""
    ns_a = "http://schemas.openxmlformats.org/drawingml/2006/main"
    chunks: list[str] = []
    with zipfile.ZipFile(path, "r") as zf:
        slide_names = sorted(
            (n for n in zf.namelist() if re.match(r"ppt/slides/slide\d+\.xml$", n)),
            key=lambda s: int(re.search(r"slide(\d+)\.xml$", s).group(1)),
        )
        for idx, sname in enumerate(slide_names, 1):
            root = ET.fromstring(zf.read(sname))
            texts = [t.text.strip() for t in root.iter(f"{{{ns_a}}}t") if t.text and t.text.strip()]
            chunks.append(f"[Slide {idx}]\n" + "\n".join(texts))
    return "\n\n".join(chunks).strip()


def extract_image_ocr_jxa(image_paths: list[Path]) -> dict[str, str]:
    """Run macOS Vision OCR on image fact sheets/flyers."""
    if not image_paths:
        return {}
    jxa = r'''
ObjC.import("Vision");
ObjC.import("AppKit");
ObjC.import("Foundation");

function run(argv) {
  var paths = JSON.parse(ObjC.unwrap($.NSString.alloc.initWithDataEncoding($.NSData.dataWithContentsOfFile(argv[0]), $.NSUTF8StringEncoding)));
  var out = {};
  for (var i = 0; i < paths.length; i++) {
    var p = paths[i];
    try {
      var img = $.NSImage.alloc.initWithContentsOfFile(p);
      var cgImg = img ? img.CGImageForProposedRectContextHints(null, null, null) : null;
      if (!cgImg) { out[p] = ""; continue; }
      var handler = $.VNImageRequestHandler.alloc.initWithCGImageOptions(cgImg, $({}));
      var req = $.VNRecognizeTextRequest.alloc.init;
      req.recognitionLevel = $.VNRequestTextRecognitionLevelAccurate;
      handler.performRequestsError($.NSArray.arrayWithObject(req), null);
      var lines = [];
      var res = req.results;
      if (res) {
        for (var j = 0; j < res.count; j++) {
          var cands = res.objectAtIndex(j).topCandidates(1);
          if (cands && cands.count > 0) lines.push(ObjC.unwrap(cands.objectAtIndex(0).string));
        }
      }
      out[p] = lines.join("\n");
    } catch (e) {
      out[p] = "";
    }
  }
  $(JSON.stringify(out)).writeToFileAtomicallyEncodingError(argv[1], true, $.NSUTF8StringEncoding, null);
  return "ok";
}
'''
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        js = tmp / "ocr_img.js"
        inp = tmp / "in.json"
        out = tmp / "out.json"
        js.write_text(jxa, encoding="utf-8")
        inp.write_text(json.dumps([str(p) for p in image_paths]), encoding="utf-8")
        proc = subprocess.run(["osascript", "-l", "JavaScript", str(js), str(inp), str(out)], capture_output=True, text=True, timeout=120)
        if proc.returncode == 0 and out.exists():
            return json.loads(out.read_text(encoding="utf-8"))
    return {}


def extract_all_texts(
    download_meta_path: Path,
    factsheets_dir: Path,
    extracted_dir: Path,
    extraction_meta_path: Path,
) -> dict[str, Any]:
    """Extract text from all downloaded documents into data/raw/extracted/."""
    dl_meta = json.loads(download_meta_path.read_text(encoding="utf-8"))
    extracted_dir.mkdir(parents=True, exist_ok=True)

    downloaded_items = [f for f in dl_meta["files"] if f["status"] == "downloaded" and f.get("local_path")]
    pdf_items: list[tuple[dict[str, Any], Path]] = []
    img_items: list[tuple[dict[str, Any], Path]] = []
    other_items: list[tuple[dict[str, Any], Path]] = []

    for item in downloaded_items:
        local_abs = REPO_ROOT / item["local_path"]
        ext = local_abs.suffix.lower()
        if ext == ".pdf":
            pdf_items.append((item, local_abs))
        elif ext in IMAGE_EXTS:
            img_items.append((item, local_abs))
        else:
            other_items.append((item, local_abs))

    pdf_results = extract_pdfs_batch_jxa([p for _, p in pdf_items], ocr_fallback=True)
    img_results = extract_image_ocr_jxa([p for _, p in img_items])

    records: list[dict[str, Any]] = []

    def save_extracted(item: dict[str, Any], local_abs: Path, text: str, method: str, pages: int | None = None, ok: bool = True, error: str | None = None) -> None:
        rel_from_fs = local_abs.relative_to(factsheets_dir)
        out_txt = extracted_dir / f"{rel_from_fs}.txt"
        out_txt.parent.mkdir(parents=True, exist_ok=True)
        header = (
            f"Source File: {item['local_path']}\n"
            f"Drive Folder: {' / '.join(item.get('folder_path') or [])}\n"
            f"Drive URL: {item['drive_url']}\n"
            f"Extraction Method: {method}\n"
            + ("=" * 72)
            + "\n\n"
        )
        out_txt.write_text(header + text.strip() + "\n", encoding="utf-8")
        non_ws_chars = len(re.sub(r"\s+", "", text))
        records.append(
            {
                "id": item["id"],
                "local_path": item["local_path"],
                "extracted_path": str(out_txt.relative_to(REPO_ROOT)),
                "method": method,
                "pages": pages,
                "ok": ok,
                "char_count": len(text.strip()),
                "non_ws_chars": non_ws_chars,
                "error": error,
            }
        )

    for item, local_abs in pdf_items:
        res = pdf_results.get(str(local_abs), {"ok": False, "error": "missing result", "pages": 0, "text": ""})
        method = "pdfkit+vision_ocr" if res.get("ocr") else "pdfkit"
        save_extracted(item, local_abs, res.get("text", ""), method=method, pages=res.get("pages"), ok=bool(res.get("ok")), error=res.get("error"))

    for item, local_abs in img_items:
        text = img_results.get(str(local_abs), "")
        save_extracted(item, local_abs, text, method="vision_ocr", ok=True)

    for item, local_abs in other_items:
        ext = local_abs.suffix.lower()
        try:
            if ext == ".docx":
                text = extract_docx_text(local_abs)
                method = "docx_xml"
            elif ext == ".doc":
                text = extract_doc_text(local_abs)
                method = "textutil"
            elif ext == ".xlsx":
                text = extract_xlsx_text(local_abs)
                method = "xlsx_xml"
            elif ext == ".pptx":
                text = extract_pptx_text(local_abs)
                method = "pptx_xml"
            elif ext in (".txt", ".csv", ".md"):
                text = local_abs.read_text(encoding="utf-8", errors="replace")
                method = "plain_text"
            else:
                text = extract_doc_text(local_abs)
                method = "textutil_fallback"
            save_extracted(item, local_abs, text, method=method, ok=True)
        except Exception as exc:
            save_extracted(item, local_abs, "", method="failed", ok=False, error=str(exc))

    non_empty = sum(1 for r in records if r["non_ws_chars"] > 0)
    payload = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total_extracted_files": len(records),
        "non_empty_count": non_empty,
        "empty_count": len(records) - non_empty,
        "failed_count": sum(1 for r in records if not r["ok"]),
        "files": records,
    }
    write_json(extraction_meta_path, payload)
    return payload


# ---------------------------------------------------------------------------
# Stage 1D: Normalization, Deduplication, Matching & Sharding
# ---------------------------------------------------------------------------

def extract_short_name(official_name: str, row_dicts: list[dict[str, Any]]) -> str | None:
    """Extract an explicit parenthetical acronym/short name from the Excel name if present."""
    for rd in row_dicts:
        raw = str(rd.get("institution_name_raw") or "").strip()
        m = re.search(r"\(([^()]{2,20})\)\s*$", raw)
        if m:
            candidate = m.group(1).strip()
            # Avoid non-acronym Italian/German full name translations inside parens
            if len(candidate) <= 18 and " " not in candidate or candidate in ("New Mexico Tech", "UC Louvain", "Nova SBE", "Virginia Tech", "Sciences Po", "EM Normandie"):
                return candidate
    if official_name == "Sciences Po (Institute d'Etudes Politiques de Paris)":
        return "Sciences Po"
    return None


def normalize_country_state_city(row_dicts: list[dict[str, Any]]) -> tuple[str, str | None, str, str, str | None]:
    """Normalize country, state_or_province, city, and region across merged Excel rows."""
    raw_countries = [str(r["country_raw"]).strip() for r in row_dicts if r.get("country_raw") and str(r["country_raw"]).strip()]
    raw_cities = [str(r["city_raw"]).strip() for r in row_dicts if r.get("city_raw") and str(r["city_raw"]).strip()]

    b = COUNTRY_NORMALIZATION.get(raw_countries[0], raw_countries[0]) if raw_countries else ""
    if b in US_STATES:
        country = "United States"
        state = US_STATES[b]
    elif b in CANADIAN_PROVINCES:
        country = "Canada"
        state = b
    else:
        country = b
        state = None

    region = COUNTRY_TO_REGION[country]

    clean_cities: list[str] = []
    for c in raw_cities:
        c_norm = CITY_NORMALIZATION.get(c, c)
        if c_norm not in clean_cities:
            clean_cities.append(c_norm)

    city_note: str | None = None
    # Special cleanups for merged rows & multi-campus notes
    if len(clean_cities) > 1:
        # e.g. Koc R59='Istanbul' + R137='Koc'; UC3M R122='Getafe, Leganés, Colmenarejo, and Madrid' + R123='Madrid'
        if "Istanbul" in clean_cities:
            city = "Istanbul"
        elif "Madrid" in clean_cities:
            city = "Madrid"
            city_note = "; ".join(clean_cities)
        else:
            city = clean_cities[0]
            city_note = "; ".join(clean_cities)
    else:
        city = clean_cities[0] if clean_cities else ""

    return country, state, city, region, city_note


def normalize_student_levels(row_dicts: list[dict[str, Any]]) -> tuple[list[str] | None, bool, str | None, str | None]:
    """Normalize Col D student levels into ['UG','GR','PhD','Staff'] or None."""
    raw_vals = [str(r["agreement_student_level_raw"]).strip() for r in row_dicts if r.get("agreement_student_level_raw") and str(r["agreement_student_level_raw"]).strip()]
    if not raw_vals:
        return None, False, None, None

    raw_joined = " | ".join(raw_vals)
    levels: list[str] = []
    staff_eligible = False
    notes: list[str] = []

    for val in raw_vals:
        low = val.lower()
        if "need to check slots" in low:
            notes.append("Excel Col D: 'Need to check slots'")
            continue
        if "exchange and erasmus" in low:
            notes.append("Excel Col D note: 'Exchange and erasmus'")
            continue
        if "ug" in low and "UG" not in levels:
            levels.append("UG")
        if "gr" in low and "GR" not in levels:
            levels.append("GR")
        if "phd" in low and "PhD" not in levels:
            levels.append("PhD")
        if "staff" in low:
            staff_eligible = True
            if "Staff" not in levels:
                levels.append("Staff")

    return (levels if levels else None), staff_eligible, raw_joined, (" | ".join(notes) if notes else None)


def normalize_terms_and_standing(row_dicts: list[dict[str, Any]]) -> tuple[list[str] | None, str | None, str | None, str | None]:
    """Normalize Col E terms into schema enum ['Fall','Spring','Academic Year','Summer']."""
    raw_vals = [str(r["agreement_terms_raw"]).strip() for r in row_dicts if r.get("agreement_terms_raw") and str(r["agreement_terms_raw"]).strip()]
    if not raw_vals:
        return None, None, None, None

    raw_joined = " | ".join(raw_vals)
    terms: list[str] = []
    term_dates_notes: list[str] = []
    standing_notes: list[str] = []

    for val in raw_vals:
        low = val.lower()
        # R144 St. Andrews has standing rules in Col E instead of terms.
        if "outgoing aucian" in low:
            standing_notes.append("For St. Andrews 3rd year; for outgoing AUCians 2nd and 3rd year (Excel Col E)")
            continue
        if "semester i:" in low or "fall semester:" in low or "academic period:" in low:
            term_dates_notes.append(val.replace("\n", " "))
        if "winter" in low:
            term_dates_notes.append("Excel Col E also notes Winter term")

        if "semester i" in low and "academic period: january" in low:
            # Southern Hemisphere / Colombian calendar: Sem I Jan-Jun (~Spring), Sem II Jul-Dec (~Fall)
            for t in ("Spring", "Fall"):
                if t not in terms:
                    terms.append(t)
            continue

        if "fall" in low or "semester1" in low or "semester i" in low:
            if "Fall" not in terms:
                terms.append("Fall")
        if "spring" in low or "sping" in low or "semester 2" in low or "semester ii" in low:
            if "Spring" not in terms:
                terms.append("Spring")
        if re.search(r"\bay\b", low):
            if "Academic Year" not in terms:
                terms.append("Academic Year")
        if "summer" in low:
            if "Summer" not in terms:
                terms.append("Summer")

    return (
        terms if terms else None,
        raw_joined,
        (" | ".join(term_dates_notes) if term_dates_notes else None),
        (" | ".join(standing_notes) if standing_notes else None),
    )


def build_manifest(
    extracted_json_path: Path,
    download_meta_path: Path,
    extraction_meta_path: Path,
    manifest_out_path: Path,
) -> dict[str, Any]:
    """Deduplicate 162 rows -> 155 canonical records, match Drive files, and assign shards 1..8."""
    extracted = json.loads(extracted_json_path.read_text(encoding="utf-8"))
    dl_meta = json.loads(download_meta_path.read_text(encoding="utf-8"))
    ex_meta = json.loads(extraction_meta_path.read_text(encoding="utf-8"))

    rows_by_row: dict[int, dict[str, Any]] = {int(r["row"]): r for r in extracted["rows"]}

    # Build lookup maps from Drive download & extraction metadata.
    extracted_by_local: dict[str, dict[str, Any]] = {
        rec["local_path"]: rec for rec in ex_meta["files"]
    }
    files_by_top_folder_id: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for f in dl_meta["files"]:
        files_by_top_folder_id[f["top_folder_id"]].append(f)

    top_folders_by_id: dict[str, dict[str, Any]] = {
        tf["id"]: tf for tf in dl_meta["top_level_folders"]
    }
    direct_checks_by_row: dict[int, dict[str, Any]] = {
        int(c["excel_row"]): c for c in dl_meta.get("direct_col_h_external_checks", [])
    }

    # Build reverse mapping: excel_row -> list of matched Drive top_folder_ids.
    row_to_folder_ids: dict[int, list[str]] = defaultdict(list)
    for fid, row_num in EXPLICIT_FOLDER_ID_TO_EXCEL_ROW.items():
        if fid in top_folders_by_id and fid not in row_to_folder_ids[row_num]:
            row_to_folder_ids[row_num].append(fid)

    # Group Excel rows into 155 canonical entities.
    merge_key_by_row: dict[int, tuple[int, ...]] = {}
    for a, b in DUPLICATE_ROW_PAIRS:
        pair = tuple(sorted((a, b)))
        merge_key_by_row[a] = pair
        merge_key_by_row[b] = pair

    grouped_rows: dict[tuple[int, ...], list[dict[str, Any]]] = {}
    for rn in sorted(rows_by_row):
        key = merge_key_by_row.get(rn, (rn,))
        grouped_rows.setdefault(key, []).append(rows_by_row[rn])

    if len(grouped_rows) != 155:
        raise ValueError(f"Expected 155 canonical universities, got {len(grouped_rows)}")

    matched_top_folder_ids: set[str] = set()
    universities: list[dict[str, Any]] = []
    used_slugs: set[str] = set()

    for key, row_dicts in grouped_rows.items():
        excel_rows = [r["row"] for r in row_dicts]
        raw_name_clean = str(row_dicts[0]["institution_name_raw"]).strip()
        official_name = CANONICAL_NAME_OVERRIDES.get(key, raw_name_clean)
        slug = ascii_slug(official_name)
        if slug in used_slugs:
            raise ValueError(f"Duplicate slug generated: {slug} for rows {excel_rows}")
        used_slugs.add(slug)

        short_name = extract_short_name(official_name, row_dicts)
        country, state, city, region, city_note = normalize_country_state_city(row_dicts)
        levels, staff_eligible, levels_raw, levels_note = normalize_student_levels(row_dicts)
        terms, terms_raw, term_dates_note, standing_note = normalize_terms_and_standing(row_dicts)

        if 153 in excel_rows:
            agreement_status = "letter_of_intent"
        elif 23 in excel_rows:
            agreement_status = "check_slots"
        else:
            agreement_status = "active"

        scopes = [
            str(r["agreement_scope_raw"]).strip().rstrip("`").strip()
            for r in row_dicts
            if r.get("agreement_scope_raw") and str(r["agreement_scope_raw"]).strip()
        ]
        # Clean trailing dash on TUM R106 'university wide -' -> keep meaningful raw or clean
        scope_raw = " | ".join(dict.fromkeys(scopes)) if scopes else None
        if scope_raw == "university wide -":
            scope_raw = "University-wide"

        # Website resolution: prefer non-trailing-dup primary URL across merged rows.
        urls = [str(r["website_url"]).strip() for r in row_dicts if r.get("website_url")]
        # Prefer faculty-specific LSM URL (R64) if present, else first non-empty URL
        official_website_url = urls[0] if urls else None
        alternate_urls = [u for u in dict.fromkeys(urls) if u != official_website_url]

        # Collect fact sheet linkages across merged rows.
        matched_fids: list[str] = []
        for rn in excel_rows:
            for fid in row_to_folder_ids.get(rn, []):
                if fid not in matched_fids:
                    matched_fids.append(fid)
                    matched_top_folder_ids.add(fid)

        drive_links: list[str] = []
        col_h_links: list[str] = []
        blocked_notes: list[str] = []
        here_marked = any(bool(r.get("fact_sheet_text")) for r in row_dicts)

        for r in row_dicts:
            hl = r.get("fact_sheet_hyperlink")
            if hl:
                col_h_links.append(hl)
                if hl not in drive_links:
                    drive_links.append(hl)
            dc = direct_checks_by_row.get(r["row"])
            if dc and dc["status"] != "ok":
                blocked_notes.append(
                    f"Col H link ({dc['drive_id']}) unreachable ({dc['status']}: {dc.get('error')})"
                )

        local_files: list[str] = []
        extracted_files: list[str] = []
        for fid in matched_fids:
            folder_url = f"https://drive.google.com/drive/folders/{fid}"
            if folder_url not in drive_links:
                drive_links.append(folder_url)
            for f in files_by_top_folder_id.get(fid, []):
                if f["status"] == "downloaded" and f.get("local_path"):
                    lp = f["local_path"]
                    if lp not in local_files:
                        local_files.append(lp)
                    ex_rec = extracted_by_local.get(lp)
                    if ex_rec and ex_rec.get("extracted_path") and ex_rec["extracted_path"] not in extracted_files:
                        extracted_files.append(ex_rec["extracted_path"])

        local_files.sort()
        extracted_files.sort()

        fs_notes: list[str] = list(blocked_notes)
        if local_files:
            match_status = "matched"
            if not here_marked:
                fs_notes.append("Excel Col H blank, but matching public folder found in Drive master folder")
            elif blocked_notes:
                fs_notes.append("Resolved via matching public folder inside Drive master folder")
        elif blocked_notes:
            match_status = "blocked"
        elif here_marked:
            match_status = "unmatched_here"
            fs_notes.append("Marked 'Here' in Excel Col H without hyperlink; no matching folder in master Drive folder")
        else:
            match_status = "none"

        if city_note:
            fs_notes_extra = f"Excel multi-campus/city note: {city_note}"
        else:
            fs_notes_extra = None

        universities.append(
            {
                "slug": slug,
                "excel_rows": excel_rows,
                "shard": 0,  # Assigned deterministically below
                "official_name": official_name,
                "short_name": short_name,
                "country": country,
                "state_or_province": state,
                "city": city,
                "region": region,
                "agreement_status": agreement_status,
                "agreement_student_levels": levels,
                "agreement_staff_eligible": staff_eligible,
                "agreement_student_levels_raw": levels_raw,
                "agreement_student_levels_note": levels_note,
                "min_standing_note": standing_note,
                "agreement_scope_raw": scope_raw,
                "available_terms": terms,
                "available_terms_raw": terms_raw,
                "term_dates_note": term_dates_note,
                "official_website_url": official_website_url,
                "alternate_website_urls": alternate_urls,
                "city_note": fs_notes_extra,
                "cross_references": [],
                "fact_sheet": {
                    "match_status": match_status,
                    "drive_links": drive_links,
                    "local_files": local_files,
                    "extracted_files": extracted_files,
                    "notes": " | ".join(fs_notes) if fs_notes else None,
                },
                "raw_excel_rows": row_dicts,
            }
        )

    # Add cross-link between Nova SBE (R80) and Universidade Nova de Lisboa (R125).
    by_row = {r: u for u in universities for r in u["excel_rows"]}
    nova_sbe = by_row[80]
    nova_lisboa = by_row[125]
    nova_sbe["cross_references"].append(
        {
            "slug": nova_lisboa["slug"],
            "official_name": nova_lisboa["official_name"],
            "relation": "parent_institution_separate_agreement",
            "note": "Kept separate because Nova SBE has a dedicated faculty-specific Business & Economics agreement (R80 vs R125).",
        }
    )
    nova_lisboa["cross_references"].append(
        {
            "slug": nova_sbe["slug"],
            "official_name": nova_sbe["official_name"],
            "relation": "dedicated_faculty_agreement",
            "note": "Kept separate because Nova SBE has a dedicated faculty-specific Business & Economics agreement (R125 vs R80).",
        }
    )

    # Sort deterministically by slug and assign balanced shards 1..8 stratified by fact_sheet presence.
    universities.sort(key=lambda u: u["slug"])
    matched_group = [u for u in universities if u["fact_sheet"]["match_status"] == "matched"]
    other_group = [u for u in universities if u["fact_sheet"]["match_status"] != "matched"]

    # Round-robin across 8 shards: matched first, continuing seamlessly into other_group
    # so every shard has 19-20 total universities AND balanced fact-sheet-backed counts.
    ordered_for_sharding = matched_group + other_group
    for idx, uni in enumerate(ordered_for_sharding):
        uni["shard"] = (idx % 8) + 1

    shard_summary: dict[str, dict[str, int]] = {}
    for s in range(1, 9):
        in_s = [u for u in universities if u["shard"] == s]
        shard_summary[str(s)] = {
            "total": len(in_s),
            "fact_sheet_matched": sum(1 for u in in_s if u["fact_sheet"]["match_status"] == "matched"),
            "without_fact_sheet": sum(1 for u in in_s if u["fact_sheet"]["match_status"] != "matched"),
        }

    unmatched_drive_folders = [
        {
            "id": tf["id"],
            "name": tf["name"],
            "url": f"https://drive.google.com/drive/folders/{tf['id']}",
            "files": [f["name"] for f in files_by_top_folder_id.get(tf["id"], [])],
            "local_files": [f["local_path"] for f in files_by_top_folder_id.get(tf["id"], []) if f.get("local_path")],
        }
        for tf in dl_meta["top_level_folders"]
        if tf["id"] not in matched_top_folder_ids
    ]

    blocked_or_failed_downloads = [
        {
            "source": "drive_tree_file",
            "id": f["id"],
            "name": f["name"],
            "folder_path": f["folder_path"],
            "status": f["status"],
            "error": f.get("error"),
            "url": f["drive_url"],
        }
        for f in dl_meta["files"]
        if f["status"] not in ("downloaded", "skipped_media_video")
    ] + [
        {
            "source": "col_h_direct_link",
            "excel_row": dc["excel_row"],
            "institution_name": dc["institution_name"],
            "id": dc["drive_id"],
            "status": dc["status"],
            "error": dc.get("error"),
            "url": dc["url"],
        }
        for dc in dl_meta.get("direct_col_h_external_checks", [])
        if dc["status"] != "ok"
    ]

    status_counts = Counter(u["fact_sheet"]["match_status"] for u in universities)
    manifest = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "source_excel_extracted": str(extracted_json_path.relative_to(REPO_ROOT)),
        "master_drive_folder_url": MASTER_DRIVE_FOLDER_URL,
        "raw_row_count": len(rows_by_row),
        "canonical_count": len(universities),
        "merged_duplicate_pairs": [
            {"excel_rows": [a, b], "slug": by_row[a]["slug"], "official_name": by_row[a]["official_name"]}
            for a, b in DUPLICATE_ROW_PAIRS
        ],
        "fact_sheet_match_counts": dict(status_counts),
        "shards": shard_summary,
        "drive_summary": {
            "top_level_folders_total": dl_meta["top_level_folder_count"],
            "top_level_folders_matched": len(matched_top_folder_ids),
            "top_level_folders_unmatched": len(unmatched_drive_folders),
            "files_downloaded": dl_meta["download_status_counts"].get("downloaded", 0),
            "files_skipped_video": dl_meta["download_status_counts"].get("skipped_media_video", 0),
            "extracted_text_total": ex_meta["total_extracted_files"],
            "extracted_text_non_empty": ex_meta["non_empty_count"],
            "blocked_or_failed_count": len(blocked_or_failed_downloads),
        },
        "unmatched_drive_items": unmatched_drive_folders,
        "blocked_or_failed_downloads": blocked_or_failed_downloads,
        "universities": universities,
    }
    write_json(manifest_out_path, manifest)
    return manifest


# ---------------------------------------------------------------------------
# Stage 1E: Offline Verification
# ---------------------------------------------------------------------------

def verify_stage1_outputs(
    extracted_json_path: Path,
    manifest_path: Path,
    download_meta_path: Path,
    extraction_meta_path: Path,
) -> int:
    """Verify exact counts, row coverage, slug format, shard balance, and file integrity."""
    errors: list[str] = []

    extracted = json.loads(extracted_json_path.read_text(encoding="utf-8"))
    if extracted["total_rows"] != 162 or len(extracted["rows"]) != 162:
        errors.append(f"Expected 162 extracted rows, got {extracted['total_rows']}")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    unis = manifest["universities"]
    if manifest["canonical_count"] != 155 or len(unis) != 155:
        errors.append(f"Expected 155 canonical universities, got {len(unis)}")

    seen_rows: list[int] = []
    seen_slugs: set[str] = set()
    slug_re = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")

    for u in unis:
        slug = u["slug"]
        if not slug_re.match(slug):
            errors.append(f"Invalid slug: {slug!r}")
        if slug in seen_slugs:
            errors.append(f"Duplicate slug: {slug!r}")
        seen_slugs.add(slug)
        seen_rows.extend(u["excel_rows"])

        if not u.get("region") or not u.get("country") or not u.get("city"):
            errors.append(f"Missing geo fields on {slug}")

        fs = u["fact_sheet"]
        for lp in fs["local_files"]:
            p = REPO_ROOT / lp
            if not p.exists() or p.stat().st_size == 0:
                errors.append(f"Missing or empty local fact sheet for {slug}: {lp}")
            else:
                head = p.read_bytes()[:64].lstrip().lower()
                if head.startswith((b"<!doctype html", b"<html")):
                    errors.append(f"HTML error page stored as fact sheet: {lp}")
        for ep in fs["extracted_files"]:
            p = REPO_ROOT / ep
            if not p.exists() or p.stat().st_size == 0:
                errors.append(f"Missing extracted text for {slug}: {ep}")

    if sorted(seen_rows) != list(range(2, 164)):
        errors.append(f"Row coverage mismatch! Covered {len(seen_rows)} rows, unique {len(set(seen_rows))}")

    for s, info in manifest["shards"].items():
        if not (19 <= info["total"] <= 20):
            errors.append(f"Unbalanced shard {s}: {info}")

    dl_meta = json.loads(download_meta_path.read_text(encoding="utf-8"))
    ex_meta = json.loads(extraction_meta_path.read_text(encoding="utf-8"))

    print("=== Stage 1 Ingestion Verification ===")
    print(f"Raw Excel rows:           {extracted['total_rows']} (rows 2..163)")
    print(f"Canonical universities:   {len(unis)}")
    print(f"Fact sheet match counts:  {manifest['fact_sheet_match_counts']}")
    print(f"Drive top-level folders:  {dl_meta['top_level_folder_count']} (matched={manifest['drive_summary']['top_level_folders_matched']}, unmatched={manifest['drive_summary']['top_level_folders_unmatched']})")
    print(f"Drive files downloaded:   {dl_meta['download_status_counts'].get('downloaded', 0)} (skipped video={dl_meta['download_status_counts'].get('skipped_media_video', 0)})")
    print(f"Extracted text files:     {ex_meta['total_extracted_files']} (non-empty={ex_meta['non_empty_count']}, empty={ex_meta['empty_count']}, failed={ex_meta['failed_count']})")
    print(f"Blocked/failed ColH/Drive:{len(manifest['blocked_or_failed_downloads'])}")
    print(f"Shards (1..8):            {manifest['shards']}")

    if errors:
        print("\nVERIFICATION ERRORS:")
        for err in errors:
            print(f"  - {err}")
        return 1
    print("\nALL VERIFICATION CHECKS PASSED.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Stage 1 AUC Partner List & Fact Sheet Ingestion")
    parser.add_argument("--excel", type=Path, default=None, help="Path to partner_list.xlsx")
    parser.add_argument("--parse-excel", action="store_true", help="Parse Excel into partner_list_extracted.json")
    parser.add_argument("--download-drive", action="store_true", help="Download public Drive fact sheets")
    parser.add_argument("--extract-text", action="store_true", help="Extract text from downloaded fact sheets")
    parser.add_argument("--build-manifest", action="store_true", help="Build data/universities/manifest.json")
    parser.add_argument("--verify", action="store_true", help="Run offline verification checks")
    parser.add_argument("--all-offline", action="store_true", help="Run --parse-excel, --extract-text, --build-manifest, --verify")
    args = parser.parse_args()

    if not any([args.parse_excel, args.download_drive, args.extract_text, args.build_manifest, args.verify, args.all_offline]):
        parser.print_help()
        return 1

    if args.parse_excel or args.all_offline:
        xlsx_path = args.excel or resolve_default_excel_path()
        extracted = parse_excel_universities_list(xlsx_path)
        write_json(DEFAULT_EXTRACTED_JSON, extracted)
        print(f"Wrote {DEFAULT_EXTRACTED_JSON} ({extracted['total_rows']} rows)")

    if args.download_drive:
        dl = download_all_drive_assets(DEFAULT_EXTRACTED_JSON, DEFAULT_FACTSHEETS_DIR, DEFAULT_DOWNLOAD_META)
        print(f"Downloaded Drive assets: {dl['download_status_counts']} -> {DEFAULT_DOWNLOAD_META}")

    if args.extract_text or args.all_offline:
        ex = extract_all_texts(DEFAULT_DOWNLOAD_META, DEFAULT_FACTSHEETS_DIR, DEFAULT_EXTRACTED_DIR, DEFAULT_EXTRACTION_META)
        print(f"Extracted texts: {ex['total_extracted_files']} total, {ex['non_empty_count']} non-empty -> {DEFAULT_EXTRACTION_META}")

    if args.build_manifest or args.all_offline:
        man = build_manifest(DEFAULT_EXTRACTED_JSON, DEFAULT_DOWNLOAD_META, DEFAULT_EXTRACTION_META, DEFAULT_MANIFEST_JSON)
        print(f"Wrote {DEFAULT_MANIFEST_JSON} ({man['canonical_count']} canonical universities)")

    if args.verify or args.all_offline:
        return verify_stage1_outputs(DEFAULT_EXTRACTED_JSON, DEFAULT_MANIFEST_JSON, DEFAULT_DOWNLOAD_META, DEFAULT_EXTRACTION_META)

    return 0


if __name__ == "__main__":
    sys.exit(main())
