# 1. High-Level Design (HLD) – MiLO Middleware on Linux MPU

## System Role

The Linux MPU serves as the central brain of a medical device, orchestrating three external MCU-controlled hardware modules via USB serial. It executes experiment protocols, parses SD card config files, monitors physical button input, and logs experiment results with low latency and HFT-grade system architecture.

## Core Responsibilities of the Linux MPU

    Loads an experiment protocol configuration from an SD card (.json)

    Waits for physical user input (tactile buttons via GPIO)

    Sends high-level control commands to MCU devices over /dev/ttyUSBx

    Receives acknowledgments and measurements from MCUs

    Executes protocol as a state machine

    Logs timestamped results and system events to CSV files on the SD card

    Maintains SD card storage via automatic log rotation

## Interfacing Overview

| Interface             | Connection Type            | Direction      | Purpose              |
| --------------------- | -------------------------- | -------------- | -------------------- |
| MCU 1 (PSU)           | USB-Serial                 | Bi-Directional | Power enable/disable |
| MCU 2 (PG)            | USB-Serial                 | Bi-Directional | Pulse triggering     |
| MCU 3 (Pump)          | USB-Serial                 | Bi-Directional | Syringe control      |
| Tactile Buttons       | GPIO                       | Input          | Start/Stop commands  |
| RGB LEDs (on buttons) | GPIO or LED driver         | Output         | State indication     |
| SD Card               | SDIO/MMC                   | Read/Write     | Config + logs        |

# 2. HLD Module Breakdown

| Module                              | Description                                                                  |
| ----------------------------------- | ---------------------------------------------------------------------------- |
| **SystemCoordinator**               | Manages overall system lifecycle: boot, protocol execution, shutdown.        |
| **ConfigLoader**                    | Loads and parses JSON protocol file from `/mnt/sdcard/config.json`.          |
| **ProtocolFactory**                 | Dynamically instantiates the appropriate protocol class from config.         |
| **ExperimentProtocol (base class)** | Defines interface for all protocols: `run(RPCManager&, Logger&)`.            |
| **RPCManager**                      | Manages serial communication with all MCUs; non-blocking and low-latency.    |
| **ButtonController**                | Monitors GPIO edges for button presses; applies software debounce.           |
| **LEDController**                   | Controls GPIO-based RGB LEDs for UI feedback based on system state.          |
| **Logger**                          | Asynchronously logs events to CSV; handles log rotation based on disk quota. |
| **ErrorMonitor**                    | Tracks timeouts, communication faults, and fallback/retry logic.             |


## 3. State Machine (SystemCoordinator)

[BOOT]
  ↓
[INIT]
  - Mount SD card
  - Load config
  - Open USB serial connections
  - Handshake with MCUs
  ↓
[IDLE]
  - Wait for "Start" button press
  - Set LED to blue
  ↓
[RUNNING]
  - Execute protocol (via state transitions)
  - Send high-level commands to MCUs
  - Receive responses
  - Log events
  - Set LED to red 
  ↓
[FINISHED]
  - Log summary
  - Flash LED to green
  ↓
[IDLE]
  - Await next experiment
  - Set LED to blue
 
 Fallback Paths:
    On serial failure or invalid config → [ERROR] state
    On emergency button press → [ABORT] → [IDLE]

# 4. External and Internal Interfaces 

## External (to hardware) 

| Component  | API                                                                           |
| ---------- | ----------------------------------------------------------------------------- |
| SD Card    | Standard Linux mount (`/mnt/sdcard`)                                          |
| Buttons    | `libgpiod` edge detection                                                     |
| LEDs       | GPIO writes or PWM driver                                                     |
| USB Serial | `/dev/ttyUSBx`, managed via VID/PID udev-mapped symlinks (`/dev/pump1`, etc.) |

## Internal (between modules) 

| From → To                            | Communication                           |
| ------------------------------------ | --------------------------------------- |
| ButtonController → SystemCoordinator | Debounced press events                  |
| SystemCoordinator → ProtocolFactory  | Config-driven instantiation             |
| Protocol → RPCManager                | Async command dispatch + response await |
| Protocol → Logger                    | Step-by-step event logging              |
| ErrorMonitor → Logger + StateMachine | Error escalations and logging           |


# 5. Performance and Real-Time Considerations

- Target <10ms end-to-end latency from user input to MCU acknowledgment
- Use SPSC ring buffers or double buffers for handoff between:
    GPIO ISR → Protocol FSM
    Protocol FSM → Logger
- Avoid dynamic memory allocation during critical path execution
- Reduce heap churn via custom allocators if applicable 
- Use std::chrono::steady_clock for timestamping events


# 6. Filesystem Layout (on SD card)

/mnt/sdcard/
├── config.json
└── logs/
    ├── 2025-07-17T09-30-01_run001.csv
    ├── 2025-07-17T11-14-29_run002.csv
    └── manifest.json (optional index of all runs)

# 7. Log Rotation Policy

- Maximum total log size: e.g. 512MB
- When exceeded, delete oldest run folder
- Logger maintains log size counters and triggers cleanup
- Logs stored in .csv per run, flushed periodically



