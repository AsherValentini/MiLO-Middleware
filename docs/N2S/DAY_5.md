# Day 5 – SerialChannel MVP

**Date:** 2025‑08‑18  
**Focus:** Implement & unit‑test the non‑blocking UART wrapper (`SerialChannel`)  
**Status:** Complete

---

## What We Built

| Component | Highlights |
|-----------|------------|
| `SerialChannel::open()` | • Opens `/dev/tty*` with `O_RDWR \| O_NOCTTY \| O_NONBLOCK`  <br>• Configures raw mode, 8‑N‑1, no flow control <br>• Supports any `speed_t` baud via `cfsetispeed/ospeed` |
| `writeLine()` | • Appends CR‑LF if missing <br>• Handles short writes, `EINTR`, `EAGAIN` retry loop |
| `readLine()` | • `poll()`‑driven timeout <br>• Internal `rx_buffer_` until `\r\n` detected <br>• Handles disconnect (`read()==0`) & transient errors |
| `close()` / dtor | Safe RAII cleanup |
| Header updates | Added `rx_buffer_` & moved destructor logic |
| Build flags | Passes `-Wall -Wextra -Wpedantic -Wconversion` |

---

## Testing

*Set up tomorrow (Day 6) with `openpty()` harness.*

- Build: `cmake --preset host-debug` 
- Cross‑build: `cmake --preset arm-release` 

---

## Known Follow‑ups

1. Replace busy‑retry in `writeLine()` with `poll(POLLOUT)` to avoid busy loop.  
2. Unit tests: happy‑path, timeout, disconnect (planned Day 6).

---

## Commit

```
[Day 5] SerialChannel MVP – open/close, writeLine, readLine, cleanup
Tag: day5-serial-mvp
```

---

## Notes

Today was tough but productive: we wrestled with POSIX quirks, strict `-Wconversion` flags, and clarified layer boundaries. Despite frustration, we now have a solid serial transport to build the RPC layer on.
