# Functional and Non-Functional Requirements — Medical Device Middleware - v/1.0

## Project Overview

The system is a central controller for a medical device, running on an STM32MP157D-DK1 MPU board with Embedded Linux. It orchestrates three peripheral MCU-based hardware modules over USB serial (FTDI bridges), controls physical buttons with RGB feedback, executes runtime-selectable experiment protocols, and logs experiment results to an SD card.

---

## Functional Requirements

### Input Handling
- Detect physical button presses via GPIO and rotary encoder button
- Debounce button input in software (≥20ms stabilization window)
- Decode rotary encoder input to cycle and select parameters

### UI Parameter Configuration
- Allow user to adjust experiment parameters at runtime using a rotary encoder with tactile switch.
- Display current editable parameter (e.g., voltage, frequency, flow rate, syringe diameter) on a small I2C OLED display.
- Save selected parameters in memory for duration of the session.
- Apply user-defined values when constructing high-level commands sent to MCUs.


### Protocol Control
- Load experiment protocol config from SD card (`.json` format)
- Instantiate and execute experiment protocol as a runtime-generated object
- Dispatch high-level commands to each connected MCU, populated with user-adjusted parameters (e.g., `StartPump`, `EnablePSU`):
    - Voltage settings to MCU 1 (PSU)
    - Frequency settings to MCU 2 (Pulse Generator)
    - Flow rate and syringe diameter to MCU 3 (Pump)
- Wait for and validate responses from MCUs

### Experiment Execution
- Protocol FSM should support multi-step experiments with timing control
- Support different protocol types (e.g., lysis, calibration, rinse)
- Run a complete experiment cycle from Start → Finished → Ready state

## USB Serial Communication
- Detect and identify USB serial devices via VID/PID
- Maintain persistent logical mapping to devices (`/dev/pump1`, `/dev/psu1`, etc.)
- Send/receive non-blocking messages over USB serial (FTDI UART bridges)
- Handle retries, timeouts, and communication failures

### Logging
- Log experiment results, command/response pairs, errors, and timestamps to CSV files
- Store logs on SD card under `/mnt/sdcard/logs/`
- Apply rotation policy: delete oldest logs when size exceeds configurable quota (e.g., 512MB)

---

## Non-Functional Requirements

### Performance
- Total system latency from button press → command → MCU response → log ≤10ms
- Target: 3–5ms typical response time under nominal conditions
- Internal handoff between threads via lock-free structures (e.g., SPSC ring buffer)

### Software Architecture
- Use modern C++17 or newer
- Apply modular architecture: separation of protocol, comms, logging, and I/O logic
- Protocol execution logic must be testable in isolation from hardware
- Use factory pattern for runtime protocol generation
- Use state machine to manage experiment lifecycle
- UI components must run in an isolated thread or coroutine to prevent blocking protocol execution.
- Parameter selections must be updated in shared memory (with appropriate synchronization).

### Concurrency Model
- At least 3 primary threads: Protocol executor, Logger, Serial I/O
- Optional additional threads for button polling or debounce filtering
- Thread-safe communication via pre-allocated buffers or queues
- Avoid dynamic allocation during protocol execution

### System Constraints (based on STM32MP157D-DK1)
- Internal storage (eMMC): 4GB available
- RAM: 512MB DDR3L shared with GPU (if not disabled)
- CPU: Dual Cortex-A7 @ 650MHz — target 1 core for app, 1 for system
- SD card interface: MicroSD (mounted at `/mnt/sdcard`), supports FAT32/ext4
- GPIOs available via 40-pin header; usable for button + LED control
- USB Host mode supported via USB-A port — up to 3 FTDI serial devices
- I2C OLED display interface supported via Linux I2C bus (connected via STM32MP1 header).
- Rotary encoder connected to GPIO pins with software debounce and edge detection.

### Reliability
- Safe fallback if SD card is missing or full
- Handle USB disconnects gracefully
- Retry failed commands with bounded retry policy
- Abort on unrecoverable protocol error; log before shutdown

### Storage Policy
- Logs must be readable without external toolchain (CSV preferred)
- Logs must include run timestamp, protocol type, and unique identifier
- Manifest (optional): index of all completed runs

### Deployability
- Application to be built with CMake
- Cross-compiled using ST-provided toolchain (`arm-linux-gnueabihf`)
- Deployed as a systemd service (`experimentd.service`)
- Must start automatically on boot and resume after watchdog reset


