# Day 2 — Continuous Integration Wrap‑Up

This note captures **where we stand** with CI at the end of Day 2 and the key decisions/artefacts that got us here.

---

## 1  Workflow in place  
**File:** `.github/workflows/build.yml`

* Builds **host‑debug** and **arm‑release** presets on every push / PR.
* Restores or (first‑run) bootstraps the *fixed* STM32MP1 SDK.
  * Uses the repository secret **`SDK_URL`** ➜ permanent URL to the SDK release asset.
  * SDK is cached under key `stm32mp1-sdk-6.6-scarthgap` — later runs skip the 1.7 GB download.
* Runs `clang-format`/`clang‑tidy` indirectly via the host build (ready for additional hooks).

Total runtime on cache‑hit: **≈3 min**.

---

## 2  SDK cache seed

* Archive **`sdk-6.6-fixed.tar.zst`** (1.7 GB) contains `/opt/st/SDK` **after** we populated the sysroot with missing libs.
* Published as a **release asset** under tag `sdk-6.6-bootstrap`.
* No installer script executed in CI; simple `tar -I zstd -xf … -C /opt` reproduces the exact local layout.

---

## 3  Secrets & keys

| Name | Purpose |
|------|---------|
| `SDK_URL` | Stable GitHub‑release link to `sdk-6.6-fixed.tar.zst` |
| Cache key | `stm32mp1-sdk-6.6-scarthgap` (bump when SDK version changes) |

---

## 4  Remaining head‑room

* **Unit‑test execution** – add GoogleTest and have CI run `ctest` for both host & ARM (future Day 3 item).
* **Static analysis gate** – hook `clang‑tidy` into the workflow once the code base grows.
* **Optional artefacts** – upload ARM binaries as job artefacts if we want nightly drops.

---

> **Status:** CI pipeline is green (first run: download+cache; subsequent runs: cache hit). We now have repeatable, minutes‑long verification for every push.

