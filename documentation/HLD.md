# 1. High-Level Design (HLD) - MiLO Middleware on Linux MPU - v/1.0

## System Role

The Linux MPU serves as the central brain of the medical device, orchestrating three external MCU-controlled hardware modules via USB serial. 
It executes experiment protocols, parses SD card config files, monitors rotary encoder input, updates an OLED display, and logs experiment results with low latency system architecture.

## Core Responsibilities of the Linux MPU

    Loads an experiment protocol configuration from an SD card (.json)

    Waits for physical user input (rotary encoder)

    Updates UI

    Sends high-level control commands to MCU devices over /dev/ttyUSBx

    Receives acknowledgments and measurements from MCUs

    Executes protocol as a state machine

    Logs timestamped results and system events to CSV files on the SD card

    Maintains SD card storage via automatic log rotation

## Interfacing Overview

| Interface                   | Connection Type            | Direction      | Purpose                                                   |
|-----------------------------|----------------------------|----------------|-----------------------------------------------------------|
| MCU 1 (PSU)                 | USB-Serial                 | Bi-Directional | Power enable/disable                                      |
| MCU 2 (PG)                  | USB-Serial                 | Bi-Directional | Pulse triggering                                          |
| MCU 3 (Pump)                | USB-Serial                 | Bi-Directional | Syringe control                                           |
| Rotary Encoder + Switch     | GPIO                       | Input          | Menu navigation, parameter adjustment, Start/Stop trigger |
| OLED Display                | I2C                        | Output         | Show system state and user-selected parameters            |
| SD Card                     | SDIO/MMC                   | Read/Write     | Load config and save experiment logs                      |


# 2. HLD Module Breakdown

| Module                              | Description                                                                  |
| ----------------------------------- | ---------------------------------------------------------------------------- |
| **SystemCoordinator**               | Manages overall system lifecycle: boot, protocol execution, shutdown.        |
| **ConfigLoader**                    | Loads and parses JSON protocol file from `/mnt/sdcard/config.json`.          |
| **ProtocolFactory**                 | Dynamically instantiates the appropriate protocol class from config.         |
| **ExperimentProtocol (base class)** | Defines interface for all protocols: `run(RPCManager&, Logger&)`.            |
| **RPCManager**                      | Manages serial communication with all MCUs; non-blocking and low-latency.    |
| **Logger**                          | Asynchronously logs events to CSV; handles log rotation based on disk quota. |
| **ErrorMonitor**                    | Tracks timeouts, communication faults, and fallback/retry logic.             |
| **UIController**                    | Manages rotary encoder + OLED display for user input of parameters.          |
| **ParameterStore**                  | Holds live user-defined parameters shared with Protocol execution logic.     |


## 3. State Machine (SystemCoordinator)

``` 
[BOOT]
  ↓
[INIT]
  - Mount SD card
  - Load config and initialize ParameterStore with defaults
  - Open USB serial connections
  - Handshake with MCUs
  ↓
[IDLE]
  - Allow user to edit parameters via UI while idle
  - Wait for "Start" button press
  - Update UI screen to [RUNNING]
  ↓
[RUNNING]
  - Execute protocol (via state transitions)
  - Send high-level commands to MCUs using values from ParameterStore
  - Receive responses
  - Log events
  ↓
[FINISHED]
  - Log summary
  - Udate UI screen to [IDLE] 
  ↓
[IDLE]
  - Await next experiment
 
 Fallback Paths:
    On serial failure or invalid config → [ERROR] state
    On emergency button press → [ABORT] → [IDLE]
```

# 4. External and Internal Interfaces 

## External (to hardware) 

| Component         | API                                                                           |
|------------------|--------------------------------------------------------------------------------|
| SD Card          | Standard Linux mount (`/mnt/sdcard`)                                           |
| USB Serial       | `/dev/ttyUSBx`, managed via VID/PID udev-mapped symlinks (`/dev/pump1`, etc.)  |
| OLED Display     | `/dev/i2c-1` using I2C user-space API (e.g., `ioctl` + write)                  |
| Rotary Encoder   | GPIO lines using `libgpiod` edge detection and polling                         |

## Internal (between modules) 

| From → To                            | Communication                                  |
| ------------------------------------ | -----------------------------------------------|
| UIController → ParameterStore        | Thread-safe write of user-selected parameters  |
| Protocol → ParameterStore            | Thread-safe read of selected parameters        |               
| SystemCoordinator → ProtocolFactory  | Config-driven instantiation                    |
| Protocol → RPCManager                | Async command dispatch + response await        |
| Protocol → Logger                    | Step-by-step event logging                     |
| ErrorMonitor → Logger + StateMachine | Error escalations and logging                  |


# 5. Performance and Real-Time Considerations

- Target <10ms end-to-end latency from user input to MCU acknowledgment
- Use SPSC ring buffers or double buffers for handoff between:
    - GPIO ISR → Protocol FSM
    - Protocol FSM → Logger
- Avoid dynamic memory allocation during critical path execution
- Reduce heap churn via custom allocators if applicable 
- Use std::chrono::steady_clock for timestamping events


# 6. Filesystem Layout (on SD card)

```
/mnt/sdcard/
├── config.json
└── logs/
    ├── 2025-07-17T09-30-01_run001.csv
    ├── 2025-07-17T11-14-29_run002.csv
    └── manifest.json (optional index of all runs)
```

# 7. Log Rotation Policy

- Maximum total log size: e.g. 512MB
- When exceeded, delete oldest run folder
- Logger maintains log size counters and triggers cleanup
- Logs stored in .csv per run, flushed periodically



