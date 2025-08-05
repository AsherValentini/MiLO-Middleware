# Low Level Design 

## Purpose 
Low Level Desing can get murky. Here is what you stand to gain by reading this document document.

-   Map HLD modules to concrete C++ classes and responsibilities
-   Defines data structures, threading model, memory ownership
-   Selects design patterns (Factory, FSM, Observer, Singleton, etc.)
-   Provides inter-thread communication strategy
-   Begins to define headers, public APIs, and internal logic

## Overview
I’ll break the LLD down into the following sections:

1. Module-to-Class Mapping
- One section per module from the HLD
- For each: class name, public methods, key responsibilities

2. Threading Model
- Which modules run in which threads
- Queues, buffer handoff points, synchronization strategy

3. Data Ownership + Lifetimes
- Who owns the protocol, log buffers, serial ports, config data, etc.
- Use of unique_ptr, shared_ptr, or raw references

4. Design Patterns
- Factory for protocol instantiation
- State machine for protocol lifecycle
- SPSC queues or ring buffers for inter-thread messaging
- Strategy or interface pattern for each protocol variant
- Observer for loggers 

5. Error Handling & Watchdogs
- Retry logic
- Fail-safe state transitions
- MCU re-handshake logic

6. I/O Abstractions
- Serial port wrapper class (non-blocking)
- GPIO input class (rotary encoder + debounce)
- I2C OLED display class (text rendering, menu updates)

7. File & Directory Layout
- /src/middleware/
- /src/ui/
- /src/protocols/
- /src/io/
- /include/, etc.

## 1. Module-to-Class Mapping 
### What we’ve done in Module-to-Class Mapping is:
- Sketch a first-pass API for each module based on its HLD role
- Define their core responsibilities and collaborators
- Focus on clarity over optimal flexibility

DO NOT use these headers as definitive. They will be refined throughout development. Especially for: 
- Factory injection mechanics
- Protocol interface polymorphism
- Ownership, DI (dependency injection), lifecycle

Use the following mappings as a peak into the general direction I want to take each of these classes, a white board brainstorm if you will. 
The code base will be self documenting. Therefore, if you want to review production implementation for any module, review the 
header files in the Include directory, not the rough sketches throughout this document. 

Here is the module overview recap from the HLD: 
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


### SystemCoordinator
Role: Orchestrates lifecycle, state transitions, component startup/shutdown 

```
class SystemCoordinator {
public:
    void initialize();            // Mount SD, load config, init subsystems
    void run();                   // Main system loop (FSM)
    void handleStart();           // Called when user starts experiment
    void handleAbort();           // Emergency stop handler
    void handleError(const std::string& reason);  // Enter error state
private:
    void enterIdleState();
    void enterRunningState();
    void enterFinishedState();
    void transitionTo(State next);

    State currentState_;
    std::unique_ptr<ExperimentProtocol> protocol_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<UIController> ui_;
    std::shared_ptr<ParameterStore> paramStore_;
    std::shared_ptr<RPCManager> rpc_;
    std::shared_ptr<ErrorMonitor> errorMonitor_;
};
```
### ConfigLoader 
Role: Parse system configs allowing runtime decision making. Used by the `SystemCoordinator` during the `INIT` state.
```
class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& configPath);
    nlohmann::json load();  // Parses and returns config JSON

private:
    std::string path_;
};
```
### ProtocolFactory 
Role: Consumes parsed configs and returns appropriate protocol instances. Injects `ParameterStore` so protocol can read user-defined values. 
```
class ProtocolFactory {
public:
    using Creator = std::function<std::unique_ptr<ExperimentProtocol>()>;

    void registerProtocol(std::string name, Creator);
    std::unique_ptr<ExperimentProtocol> create(const std::string& name);

private:
    std::unordered_map<std::string, Creator> creators_;
};
```
### ExperimentProtocl (abstract base + concrete subclass)
Role: Pulls live values from `ParameterStore` and executes FSM-style step logic inside `run()` method. 
```
class ExperimentProtocol {
public:
    virtual ~ExperimentProtocol() = default;
    virtual void run(RPCManager& rpc, Logger& logger) = 0;
};

class LysisProtocol : public ExperimentProtocol {
public:
    void run(RPCManager& rpc, Logger& logger) override;
private:
    void step1();
    void step2();
};
```
### RPCManager 
Role: Abstracts raw serial I/O. May use `poll()` or `select()` in a background thread. 
Devices will be scoped enumerations: PSU, PG, and Pump, with compile time safety rails. eg. Lock the number of enum values. 
```
class RPCManager {
public:
    void connect();  // Opens serial connections and identifies roles
    void sendCommand(Device device, const Command& cmd);
    Response awaitResponse(Device device, std::chrono::milliseconds timeout);

private:
    std::unordered_map<Device, SerialChannel> channels_;
};
```
### Logger 
Role: Async logger with internal thread. Writes to SD, and ratates storage based on quota. LogEven = timestamp, type, key/value
```
class Logger {
public:
    void startNewRun();
    void log(const LogEvent& event);
    void finishRun();
private:
    std::ofstream csvFile_;
    RingBuffer<LogEvent> buffer_;
    std::thread worker_;
    std::atomic<bool> running_;
};
```
### UIController 
Role: Emits events to the `SystemCoordinator` or the `ParameterStore`. Talks to the OLED via i2c-dev. 
```
class UIController {
public:
    void start();  // Launch UI loop in its own thread
    void registerCallback(std::function<void(UIEvent)>);

    void setDisplayState(SystemState state);
    void showParameterValue(Parameter param, float value);

private:
    void handleEncoderInput();   // Read GPIO
    void updateOLED();          // Write to I2C
};
```
### ParameterStore
Role: Thread safety parameter store with strong types. Shared resource between UIController (wrt) and ExperimentProtocol (rd). 
```
class ParameterStore {
public:
    void set(Parameter key, float value);
    float get(Parameter key) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<Parameter, float> values_;
};
```
### ErrorMonitor
Role: Monitors RPC timeouts, diconnections and escalates to `SystemCoordinator` when needed. 
```
class ErrorMonitor {
public:
    void watch(std::function<void()> failureCallback);
    void notifyFailure(const std::string& msg);
};
```
## 2. Threading Model
### System Goals Recap and Context for Threading Choices
Given: 
- Sub-10ms latency target 
- Multi-stage processing (UI->protocol->serial->log)
- Modular HFT-esc design 
- Focus on clean memory and ownership boundaries 

...the threading model needs to prioritize: 
| Concern           | Implication                                        |
| ------------------| -------------------------------------------------- |
| Low Latency       | Avoid locks and avoid blocking                     |
| Isolation         | Ensure one module doesn’t block others             |
| Streaming         | Pipe data across stages in predictable time        |
| Testability       | Keep logic in isolated units for mocking           |
| Developer clarity | Easy to reason about: who owns what and runs where |

So the threading model leans on dedicated threads + lock-free memory hand offs for inter thread/stage comms. 
### Proposed Threading Model 
```
[ UI Thread ]          ---> updates --->   [ ParameterStore ]   ---> read by ---> [ Protocol Thread ]

[ Serial I/O Thread ]  <--- blocks/reads --->  [ USB MCUs ]

[ Logger Thread ]      <--- enqueues from ---> [ Protocol Thread ]
```
### UI Thread
- Reads GPIO rotary encoder 
- Pushes updates to the `ParameterStore`
- Updates OLED display based on state

> **Why a seperate thread?** 
> Because GPIO polling and OLED updates are **slow** (~i2c wrote latency + debounce timing). 
> We do not want that garbage anywhere near the protocol FSM logic. This keeps input and rendering **off the hot path**. 

### Protocol Execution Thread
- Core FSM loop: executes steps, and computes commands 
- Reads parameters as needed 
- Dispatches to RPCManager
- Pushes log events to logger queue

> **Why a seperate thread?** 
> Protocols will have internal timing logic that may block. 
> They will be the "brains" of the system and should not **be blocked** either. 
> So they will block and they should not be blocked by: 

- SD card access (logging) 
- i2C/GPIO ISR (UI updates)
- Serial I/O (Remote Procedure Calls to distributed devices) 

This thread runs the active control state machine, must feal "real-time from a logic perspective. 

### Serial I/O Thread 
- Uses `poll()` or `select()` to wait on several `/dev/ttyUSBx`
- Reads/writes data using `RPCManager`
- Buffers input lines and matches protocol requests 

> **Why a seperate thread** 
> Because `read()` on a serial port is a blocking syscall, and IO latency is unpredictable. 
> By isolating it, the main protocol thread can continue working independently, and just await a res. 

### Logger Thread
- Consumes LogEvent structs from a lock-free ring buffer (yes thats overkill but I like the pattern) 
- Flushes periodically to CSV 
- Rotates logs if disk space exceeded

> **Why a seperate thread**
> Logging simply cannot delay control flow..dah. 
> Okay but for real for real, writing to disk is non-deterministic (SD card wear leveling and OS syncs blah blah) 
> Miss me with that, we gonna log events into a buffer and handle flushing in the background. 
> smooth.  

### Why I Like This Threading Model 

| Benefit              | How                                               |
| -------------------- | ------------------------------------------------- |
| Pipeline-ready       | Each thread handles one stage, decoupled          |
| Fail-safe            | A stuck logger doesn’t break the protocol         |
| Predictable latency  | The protocol thread is free from I/O jitter       |
| Testable             | Each module can be tested in isolation with mocks |
| Easy to reason about | Responsibilities are clearly isolated             |

### Okay, Quick Summary

| Thread            | Purpose                     | Blocking Ops?  | Communication                                                  |
| ----------------- | --------------------------- | -------------- | -------------------------------------------------------------- |
| `UI Thread`       | Reads GPIO, updates display | Yes (I2C)      | Writes to `ParameterStore`                                     |
| `Protocol Thread` | Runs FSM, manages commands  | No             | Reads from `ParameterStore`, sends to Serial, pushes to logger |
| `Serial Thread`   | Reads/writes `/dev/ttyUSBx` | Yes            | Notifies protocol FSM via future/promise or callback           |
| `Logger Thread`   | Flushes log entries to disk | Yes (disk I/O) | Pulls from lock-free buffer                                    |


## 3. Data Ownership + Lifetimes

Memory leaks...pshshsh 
This section answers the question: 
> Who owns what? 
> Who creates what?
> Who cleans up what? 
> How long do these things live for...?

### Why This Matters
The system is going to be multithreaded, so ownership clarity ensures: 
- No dangling pointers
- No data races
- No accidental double-free -r lifetime extension bugs
- Deterministic shutdown logic 
- Minimal surprises when reading or modifying code

### Guiding Principles 
| Topic                                           | Rule                                                          |
| ----------------------------------------------- | ------------------------------------------------------------- |
| Long-lived systems (e.g., coordinator, loggers) | Use `std::shared_ptr` or lifetime-managed singletons          |
| Single-owner resources (e.g., config, protocol) | Use `std::unique_ptr`                                         |
| Shared memory buffers (e.g., ParameterStore)    | Shared via `std::shared_ptr` with internal mutex (or atomics) |
| Thread modules (UI, Logger)                     | Own their own thread + clean shutdown (RAII)                         |
| Event/data queues                               | Owned by producer, referenced by consumer                     |


### Ownership Map (Per Module)
I am once again asking you to review the architectures modules: 
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


#### `SystemCoordinator`
- Owns 
    - Logger
    - UIController 
    - RPCManager
    - ErrorMonitor
    - ParameterStore
    - The **current protocol** (via unique_ptr returned from the protocol registery from the protocol factory) 
- Lifetime: app boot -> shutdown 
- Shutdown: Calls `stop()` on each owned thread-safe module 

Recalling my earlier API draft in section one, ownership for this module might look something like: 
```
std::shared_ptr<Logger> logger_;
std::shared_ptr<UIController> ui_;
std::shared_ptr<RPCManager> rpc_;
std::shared_ptr<ErrorMonitor> errorMonitor_;
std::shared_ptr<ParameterStore> paramStore_;
std::unique_ptr<ExperimentProtocol> protocol_;
```

#### `ConfigLoader`
- Short lived 
- Allocated on stack during SystemCoordinator::initialize()
- Return JSOn data byvaly (copy/move) 

#### `ProtocolFactory` 
- Stateless singleton 
- Returns `std::unique_ptr<ExperimentProtocol>`

> **Why unique_ptr** 
> I expect that each protocol is used exactly once for a run. Clean ownership, no sharing. 

#### `ExperimentProtocol`
- Owned by: `SystemCoordinator`
- Holds reference to: `ParameterStore` (as a `std::shared_ptr`) 
- Lifetime: per experiment run. 

> We do not share protocl logic between runs - I rate its better to discard and rebuild fresh per config. 

#### `RPCManager` 
- Shared between: 
    - `SystemCoordinator` (duirng init)
    - `ExperimentProtocol` (for use during run) 
- Owns: all active USB serial connections 
- Lifetime: `SystemCoordinator::initialize() -> app shutdown 

#### `Logger`
- Owned by `SystemCoordinator`
- Runs its own internal thread
- Consumes `LogEvents` from a ring buffer 
- Shuts down cleanly at system exit via Logger::finishRun()

#### `UIController`
- Owned by `SystemCoordinator`
- Owns: 
    - Rotary encoder GPIO lines 
    - OLED i2c handle 
    - Its own thread 
- Writes to: `ParameterStore` (via std::shared_ptr) 

#### `ParameterStore`
- Shared across a few modules: 
    - `UIController` (wrt) 
    - `ExperimentProtocol` (rd) 
    - `Logger` (rad, for logging parameters set 
- Critical sections and internal data guarded by mutex/RAII guards 

#### `ErrorMonitor` 
- Shared across: 
    - `RPCManager` (report failures) 
    - `SystemCoordinator` (escalations) 
> **Pattern options:** thinking about signal or observer patern depending on how the callbacks are designed...

### Inter-Thread Buffers 
So these are heavily subject to change based on how I am feeling when I get into the meet and bones of this. 
One thing is for certain, they will be typical queue like memory structures/patterns. Double buffer, ring buffer, thread safe blocking queue built
over std::queue<T>. All good options. Here is my initial draft: 

| Buffer                                 | Owner           | Reader          |
| -------------------------------------- | --------------- | --------------- |
| `RingBuffer<LogEvent>`                 | Protocol Thread | Logger Thread   |
| `RingBuffer<SerialMessage>` (optional) | Serial Thread   | Protocol Thread |
| `ParameterStore` (mutexed map)         | Shared          | Shared          |


### Takeaways
| Decision                                      | Motivation                                          |
| --------------------------------------------- | --------------------------------------------------- |
| `unique_ptr<ExperimentProtocol>`              | Clean separation per run, exclusive ownership       |
| `shared_ptr<Logger/UIController/ParamStore>`  | Shared by multiple threads with clear shutdown path |
| Internal mutex in `ParameterStore`            | Simplicity over lock-free complexity                |
| `std::thread` member in thread-owning modules | Ensures joinable shutdown                           |
| Value-returning `ConfigLoader`                | No leaks, easy testing                              |
| Clear start/stop API on threaded modules      | Professional-grade lifecycle handling               |

