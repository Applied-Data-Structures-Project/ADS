# src/repo
The ONLY code that talks to SQLite. Loads rows into src/models structs and writes them back.
No algorithmic work in SQL (no ORDER BY / JOIN doing ranking) - that happens in our DS in src/services.
