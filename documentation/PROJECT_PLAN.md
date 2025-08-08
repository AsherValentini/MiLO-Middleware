# 8‑Week Build Plan (Markdown)  
**Project start:** **Tue, 12 Aug 2025** (Europe/Vienna)  
**Cadence:** 5 days/week (Week 1 has 4 days due to Tuesday start)

---

## Assumptions (kept tight to your LLD/HLD)
- **Lang/Build:** C++17+, CMake, clang-tidy/format, GTest; cross‑compile for STM32MP1; reproducible builds.  
- **Runtime:** `experimentd` (systemd), udev stable device names, SD card layout for configs/logs.  
- **Arch:** Threads: UI, Serial/RPC, Logger, Coordinator; ring buffers/SPSC queues; FSM-driven protocols; observer-based error monitoring.  
- **Perf:** Target typical 3–5 ms hot‑path latency, worst‑case <10 ms; zero new allocations in hot path.

---

## Week 1 — Bootstrap & Scaffolding  
**Dates:** Tue **12 Aug** → Fri **15 Aug** (4 days)

**Goals:** Clean repo skeleton, toolchain, CI, headers in place so work can parallelize.

- **Day 1–2:**  
  - Repo layout per LLD (`include/{core,io,protocols,ui}`, `src/...`, `tests`, `cmake`, `scripts`).  
  - Baseline CMake targets, cross toolchain file, preset profiles (host/arm).  
  - Coding standard + clang‑format + clang‑tidy config checked into repo.
- **Day 3:**  
  - Stub public headers: `SystemCoordinator`, `Logger`(+CSV schema), `RPCManager`, `ParameterStore`, `UIController`, `ErrorMonitor`, `ExperimentProtocol` + `LysisProtocol`, `ProtocolFactory`, `ConfigLoader`.  
  - Empty unit tests to gate builds.
- **Day 4:**  
  - IO stubs + fakes: `SerialChannel`, `RotaryEncoderGPIO`, `OLEDDisplay`, `FileLogger`.  
  - “Hello‑service” systemd unit starts & exits cleanly on dev board (no logic yet).

**Exit criteria:** Builds on host + cross; CI green; service runs; headers compile and are owned by modules.

---

## Week 2 — IO Layer & Test Harness  
**Dates:** Mon **18 Aug** → Fri **22 Aug**

**Goals:** Realistic IO with mocks so upper layers can progress unblocked.

- Implement `SerialChannel` (non‑blocking line IO, `poll()`/timeouts, CRC hook).  
- Begin `RPCManager::connect()/send/await()` with retry/backoff policy.  
- `RotaryEncoderGPIO` with 20–30 ms debounce + click/press abstractions.  
- `OLEDDisplay` minimal text pages + frame API.  
- `FileLogger` with buffered CSV writes and periodic flush; log dir on `/mnt/sdcard/logs`.  
- Unit tests: serial disconnect, timeouts, SD missing/full, OLED mock surfaces.

**Exit criteria:** Host sim proves serial read/write; mocks pass; log files rotate on size/date in tests.

---

## Week 3 — Concurrency Backbone & State Shell  
**Dates:** Mon **25 Aug** → Fri **29 Aug**

**Goals:** Threads alive; messages flowing; coordinator FSM skeleton.

- SPSC `RingBuffer<LogEvent>` + `Logger` worker thread; backpressure behavior tested.  
- `ParameterStore` (typed getters/setters, thread‑safe); change notifications to UI.  
- `RPCManager` serial loop with request/response correlation + timeouts.  
- `ErrorMonitor` (observer) + `ThreadHeartbeat`; escalation queue to Coordinator.  
- `SystemCoordinator` FSM: BOOT→INIT→IDLE→RUNNING→FINISHED/ERROR with handlers.

**Exit criteria:** End‑to‑end “idle loop” on host; parameter edits show on UI mock and log to CSV.

---

## Week 4 — Config, Protocols, and First E2E on Host  
**Dates:** Mon **1 Sep** → Fri **5 Sep**

**Goals:** Run a protocol from SD config, with logging and basic UI.

- `ConfigLoader` (JSON schema + validation; defaults; schema tests).  
- `ProtocolFactory` (registry pattern) + DI of IO/params/logger.  
- Implement `LysisProtocol` v1 (step timings, retries, guards, ABORT path).  
- Manifest and log naming (run IDs, timestamps), basic rotation policy.  
- UI pages: IDLE/RUNNING/ERROR; encoder navigates; live param view.

**Exit criteria:** From config→factory→`LysisProtocol` runs in host sim; CSV written; ERROR path returns to IDLE.

---

## Week 5 — Hardware‑in‑the‑Loop Bring‑Up  
**Dates:** Mon **8 Sep** → Fri **12 Sep**

**Goals:** Talk to real devices; meet reliability basics.

- Cross‑compile + deploy to STM32MP1; udev rules for stable `/dev/pump1`, `/dev/psu1`, `/dev/pg1`.  
- `experimentd.service` with restart/Watchdog; cold‑boot to IDLE.  
- Latency probes (timestamps at enqueue/send/ack/log) — verify typical 3–5 ms, <10 ms worst.  
- Robustness tests: unplug/replug USB, SD full/absent, serial noise; verify recovery policies.  
- UI polish: clear status badges, error toasts, long‑press ABORT.

**Exit criteria:** Real lysis run completes; survives fault injections; latency goals met on target.

---

## Week 6 — Hardening & Observability  
**Dates:** Mon **15 Sep** → Fri **19 Sep**

**Goals:** Make it boring: faults handled, metrics visible, logs useful.

- Fault‑injection harness: serial corruption, delayed ACKs, thread stall, config errors.  
- Structured CSV/TSV + optional JSON event logs; include run‑ID, device‑ID, protocol step.  
- Metrics page: queue depths, loop periods, error counters; lightweight periodic snapshot.  
- Performance: tune ring buffer sizes, logger flush cadence; eliminate hot‑path allocations.  
- Add watchdog trip tests + graceful shutdown (flush & mark run as incomplete).

**Exit criteria:** Fault matrix passes; metrics page works; perf stable across 1‑hour soak.

---

## Week 7 — Second Protocol + CI/CD + Ops Docs  
**Dates:** Mon **22 Sep** → Fri **26 Sep**

**Goals:** Prove extensibility; productionize delivery.

- Implement **Protocol #2** (choose the next most valuable — e.g., “StainCycle” or “Calibration”).  
- CI: split host vs cross jobs; artifact publishing (tarball with binaries, udev, systemd, configs).  
- Release scripts: versioning, changelog, `install.sh` (idempotent), rollback script.  
- Operator Guide: SD card layout, service management commands, daily checks, log retrieval.  
- Developer Guide: how to add a new protocol; interfaces, gotchas, perf checklist.

**Exit criteria:** One‑command deploy; second protocol runs; docs reviewed by a fresh pair of eyes.

---

## Week 8 — Soak, Compliance, Handoff  
**Dates:** Mon **29 Sep** → Fri **3 Oct**

**Goals:** Final polish and evidence of readiness.

- **48‑hour soak** (can overlap nights): log size growth, memory, CPU, temps; alarms on drift.  
- Edge‑case matrix: power loss mid‑run, config mismatch, corrupted SD file, partial USB reconnect.  
- Packaging: sign artifacts (hashes), bundle recovery image or “factory reset” tarball.  
- Final perf report (plots, percentiles, worst‑case); incident playbooks (what to do if X).  
- Handoff: recorded demo, architecture/readme review, backlog triage for post‑Week‑8.

**Exit criteria:** Signed acceptance on target; artifact bundle + docs delivered; backlog labeled/prioritized.

---

## Deliverables Checklist
- **Source:** Clean CMake project with profiles, tidy/format configs, unit/integration tests.  
- **Runtime:** `experimentd` with systemd/udev; boots to IDLE; UI+encoder functional.  
- **Protocols:** `ExperimentProtocol` base; `LysisProtocol` v1; **Protocol #2** implemented.  
- **IO:** `SerialChannel`, `RPCManager` with retries/backoff; OLED + encoder; `FileLogger`.  
- **Infra:** SPSC ring buffers; `ParameterStore`; `ErrorMonitor` with heartbeats.  
- **Perf:** Typical 3–5 ms hot‑path; <10 ms worst‑case; no hot‑path heap churn.  
- **Observability:** Structured logs, metrics page, latency probes, soak tests.  
- **Ops:** CI/CD pipelines, signed artifacts, install/rollback scripts, Operator + Developer guides.  
- **Reports:** Final performance + fault‑injection summary; incident playbooks.

---

## Calendar View (high‑level)

- **Week 1 (Aug 12–15):** Skeleton, toolchain, headers, IO stubs, service skeleton  
- **Week 2 (Aug 18–22):** IO implementations, RPC skeleton, tests  
- **Week 3 (Aug 25–29):** Threads/concurrency backbone, FSM shell  
- **Week 4 (Sep 1–5):** Config loader, factory, `LysisProtocol` v1, first E2E (host)  
- **Week 5 (Sep 8–12):** HIL bring‑up, latency + robustness on device  
- **Week 6 (Sep 15–19):** Hardening, metrics/observability, perf tuning  
- **Week 7 (Sep 22–26):** Protocol #2, CI/CD, Ops + Dev docs  
- **Week 8 (Sep 29–Oct 3):** Soak, compliance polish, final handoff

---

## Definition of Done (per module)
- **Coordinator:** FSM incl. ERROR/ABORT; heartbeat supervision; graceful shutdown.  
- **Protocols:** Base + concrete implementations; retries/guards; ABORT path; easy to add more.  
- **RPC/Serial:** Stable device mapping; reconnect; timeouts; correlated req/resp.  
- **Logger:** Background writer; rotation; structured schema; minimal jitter.  
- **UI/Params:** Debounced inputs; status pages; live parameter rendering.  
- **Errors:** Observer‑based notifications; escalations to Coordinator; user‑visible status.  
- **Deploy:** systemd + udev; cold boot to IDLE; watchdog‑safe; single‑command install.  
- **Perf:** Proven with probes + report; no hot‑path allocations; capacity tuned.  
- **Docs:** Operator & Developer guides; playbooks; release/rollback process.  

---

### Notes
- If Week 1’s 4‑day start leaves a tiny gap, pull a light task (docs/tests) forward from Week 2.  
- Swap in your preferred **Protocol #2** during Week 7; the slot is deliberately generic so we can pick the highest‑value one.  
