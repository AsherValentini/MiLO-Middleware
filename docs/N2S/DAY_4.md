# Day 4 – IO Stubs + First Deploy to STM32MP1

**Date:** 2025-08-19  
**Focus:** I/O scaffolding, systemd service, cross-deploy pipeline bring-up  
**Status:** Complete

---

## What We Achieved

### Codebase
- Stubbed and committed all I/O module headers under `include/io/`:
  - `SerialChannel.hpp`
  - `RotaryEncoderGPIO.hpp`
  - `OLEDDisplay.hpp`
  - `FileLogger.hpp`

- Created corresponding empty `.cpp` files under `src/io/` with placeholder logic to keep linker happy.

### Testing
- Added fake implementations to `tests/`:
  - Minimal test coverage to validate API shape and catch signature drift.
  - All fakes linked cleanly into `milo_tests` and pass on `host-debug`.

### Build & Toolchain
- Verified `arm-release` preset now cross-compiles successfully:
  - Used ST SDK v6.6 (Scarthgap) + patched sysroot from Starter rootfs.
  - Confirmed binary output: `build/arm-release/milo-experimentd`

### Systemd Bring-Up
- Implemented `--hello` flag in `main.cpp`:
  - Logs “hello from stub” and exits successfully.
- Created and deployed working `milo-experimentd.service` to:
  ```
  /etc/systemd/system/milo-experimentd.service
  ```
- Copied binary to board:
  ```
  /usr/local/bin/milo-experimentd
  ```
- Started and verified the daemon via:
  ```
  systemctl start milo-experimentd
  journalctl -u milo-experimentd
  ```

### Networking
- Manually configured direct Ethernet link (host ↔ STM32MP1)
  - Host: `192.168.50.1` (via `eno1`)
  - Board: `192.168.50.2` (via `end0`)
- Verified with `ping`, `ssh`, and `scp`

---

## Exit Criteria Met

I/O headers + impl stubs committed  
Host + cross builds working  
Fake tests pass  
`milo-experimentd` deploys to STM32MP1  
systemd starts the service and logs cleanly  

---

## Tag

```
git commit -am "Day 4 – IO stubs + first deploy"
git tag day4-io-stubs
```

---

## Next Up: Week 2 – Real IO Implementations
- Implement `SerialChannel`, `RotaryEncoderGPIO`, `OLEDDisplay`, `FileLogger`
- Validate with fakes and host-side test harness
