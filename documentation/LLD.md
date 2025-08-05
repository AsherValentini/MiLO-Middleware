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

## Module-to-Class Mapping 
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
### 1. SystemCoordinator
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
### 2. ConfigLoader 
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
### 3. ProtocolFactory 
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
### 4. ExperimentProtocl (abstract base + concrete subclass)
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
### 5. RPCManager 
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
### 6. Logger 
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
### 7. UIController 
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
### 8. ParameterStore
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
### 9. ErrorMonitor
Role: Monitors RPC timeouts, diconnections and escalates to `SystemCoordinator` when needed. 
```
class ErrorMonitor {
public:
    void watch(std::function<void()> failureCallback);
    void notifyFailure(const std::string& msg);
};
```
## Threading Model
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

> **Why a seperate thread?** Because GPIO polling and OLED updates are **slow** (~i2c wrote latency + debounce timing). 
> You do not want that anywhere near the protocol FSM logic. This keeps input an rendering **off the hot path**. 









