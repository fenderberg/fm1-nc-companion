# Copilot instructions

Follow **AGENTS.md** at the repo root (the one source of the project's rules) and read **docs/HANDOFF.md** for where
the work stands. The essentials:

- Answer Remco in **Dutch**; code, comments, commits and docs in English, in the upstream (Felucca) style.
- Firmware for a small chip: integers only, no float, no 64-bit division, guard every divisor, nothing slow in the
  audio ISR, keep RAM and the image within the size report's limits.
- NCC's changes live in new files behind `FELUCCA_LIVEUI=1`; edits to Felucca's files are small and marked `/* NC: */`.
- Run `sh tools/ncc_dev.sh test` after every change; add a test to `tests/ncc_test.c` for new behaviour.
- **Never flash the FM-1, push, release or publish the site without Remco's explicit OK.**
