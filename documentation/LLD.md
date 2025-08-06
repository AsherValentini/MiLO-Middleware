# Low Level Design 

## Purpose 
Low Level Desing can get murky. Here is what this document does.

-   Maps HLD modules to concrete C++ classes and responsibilities
-   Defines data structures, threading model, memory ownership
-   Selects design patterns (Factory, FSM, Observer, Singleton, etc.)
-   Provides inter-thread communication strategy
-   Begins to define headers, public APIs, and internal logic

## Overview
I’ll break the LLD down into the following sections:

1. Module-to-Class Mapping
- One section per module from the HLD
- For each: class name, public/private methods, key responsibilities

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
- Strategy pattern for each protocol variant
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
### What I’ve done in Module-to-Class Mapping is:
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

This thread runs the active control state machine, must feal "real-time" from a logic perspective. 

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
- No accidental double-free lifetime extension bugs
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
I am once again asking you to review the architecture's modules: 
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
- Allocated on stack during `SystemCoordinator::initialize()`
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

> We do not share protocol logic between runs because I rate its better to discard and rebuild fresh per config. 

#### `RPCManager` 
- Shared between: 
    - `SystemCoordinator` (duirng init)
    - `ExperimentProtocol` (for use during run) 
- Owns: all active USB serial connections 
- Lifetime: `SystemCoordinator::initialize()` -> app shutdown 

#### `Logger`
- Owned by `SystemCoordinator`
- Runs its own internal thread
- Consumes `LogEvents` from a ring buffer 
- Shuts down cleanly at system exit via `Logger::finishRun()`

#### `UIController`
- Owned by `SystemCoordinator`
- Owns: 
    - Rotary encoder GPIO lines 
    - OLED i2c handle 
    - Its own thread 
- Writes to: `ParameterStore` (via `std::shared_ptr`) 

#### `ParameterStore`
- Shared across a few modules: 
    - `UIController` (wrt) 
    - `ExperimentProtocol` (rd) 
    - `Logger` (rd)
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


## 4. Design Patterns

This section lays down what patterns I plan to use while also addressing __why__ we are using each one, __what__ problem the pattern solves, and __how__ it fits into 
our system"s structure and goals. 

Hopefully, this section reveals: 
- The need that arises
- The pattern that solves it
- Its benefits
- Where it will appear in the project

Okay, enough chatter lets walk through them one by one. 

### Factory Pattern 
#### Problem: 
We need to instantiate different protocol classes at runtime based on: 
- JSON config file
- Possibly: user selection from the UI

#### Soln: Factory Pattern 
>Define an interface for creating an object, but let subclasses (or config) decide whcih class to instantiate. 

#### Impl (something like this) 
```
class ProtocolFactory {
public:
    using Creator = std::function<std::unique_ptr<ExperimentProtocol>()>;
    void registerProtocol(std::string name, Creator creator);
    std::unique_ptr<ExperimentProtocol> create(const std::string& name);
};
```
Why runtime decision making? Why this pattern for this system. 
- Decouples config parsing from concrete class construction 
- Easy to extend: add new protocol class and register it 
- Allows for plugin-style architecture later 
- Safer than `if (type == "lysis") return new LysisProtocol(); 

But honestly, if I am smart with how this factory works and how concrete classes generate experiment protocols, 
changes to experiment protocols will have minimal impact on object code. Maybe we can bypass generating new binaries all together. 

### State Machine (FSM) 
#### Problem: 
Experiment logic will be composed of steps that execute in sequence, probaly with: 
- Time delays 
- Waits for MCU responses
- Conditional branches (retries and aborts)

#### Soln: Finite State Machine Pattern 
> Model the experiment protocol as a sequence of states, each with transition conditions and exit criterion. 

#### Impl: 
```
enum class State : uint8_t { INIT, PREPARE, TRIGGER, WAIT, DONE, COUNT };

static_assert(State::COUNT == 5, "State enum count changed-update code that depends on it); 

void run(...) {
    switch (state_) {
        case INIT: prepare(); state_ = PREPARE; break;
        ...
    }
}
```
Just to get the SW MVP out this pattern will hold. But future releases should evolve to an event driven FSM or maybe even 
the fabled generator-style protocols with coroutines. Fancy fancy. 

#### Why this fits: 
- Experiment logic is naturally stateful and sequential
- Easy to debug, test, simulate, log, its just good okay 
- Works well with timers, retries, and error transitions. (Think error fallback STATE) 

### Observer Pattern (Lightweight Usage) 
#### Problem: 
We want to notify the `SystemCoordinator` or error logger when something fails. 
#### Soln: Observer Pattern 
>Set the `ErrorMonitor` as a subject and allow it to publish events without really caring who will handle them. 

So why not the signal-slot pattern? Because this is not a Qt based project.
Go lecture someone else about how easy event driven architecture is with Qt.  
We are gonna go Subject -> Subject Interface -> Observer Interface -> Observer. 
The way GoF intended pub-sub to work.
Don't like it, take it up with Design Patterns: Elements of Reusable Code. 

#### Impl (read it and weap) 
`IObserver` interface 
```
class IObserver {
public:
    virtual void onNotify(const std::string& message) = 0;
    virtual ~IObserver() = default;
};
```
`ISubject` interface 
```
class ISubject {
public:
    virtual void registerObserver(IObserver* obs) = 0;
    virtual void removeObserver(IObserver* obs) = 0;
    virtual void notifyAll(const std::string& msg) = 0;
    virtual ~ISubject() = default;
};
```
Then we got our concrete subject (just the core module `ErrorMonitor`) 
```
class ErrorMonitor : public ISubject {
public:
    void registerObserver(IObserver* obs) override {
        observers_.insert(obs);
    }

    void removeObserver(IObserver* obs) override {
        observers_.erase(obs);
    }

    void notifyAll(const std::string& msg) override {
        for (auto* obs : observers_) {
            obs->onNotify(msg);
        }
    }

private:
    std::unordered_set<IObserver*> observers_;
};
```
Then the concrete observer (again just the core module `SystemCoordinator`) 
```
class SystemCoordinator : public IObserver {
public:
    void onNotify(const std::string& message) override {
        handleError(message);
    }

    void handleError(const std::string& msg) {
        // transition to ERROR state
    }
};
```

Buuut the above is just a quick scaffold of the basic observer pattern. In truth the `ErrorMonitor` subject will live in 
a different thread to the `SystemCoordinator`. This means we need to queue notifications for the right thread. 
So instead of directly calling `observer->onNotify(...)` we will do something like: `observer->postToMainThread(msg)`
where `postToMainThread()` pushes the message onto a `RingBuffer<ErrorEvent>` owned by the protocol thread. 

> This introduces a message queue from `ErrorMonitor -> SystemCoordinator`, keeping the latter on its own thread. 

Clean. Safe. Real-Time.

### SPSC Queue / Ring Buffer 
#### Problem: 
We need to pass log events from the Protocol thread to the Logger thread without locking or blocking. 
Honestly, there will be more cross stage inter thread memory hand offs and we gonna go with Ring Buffers. 

#### Soln: Single-Producer, Single-Consumer (SPSC) Ring Buffer 
> A fixed sized lock-fre buffer where one thread produces and another consumes. 

> Open Q: overwrite policy...not clear. 

#### Example Application between `Protocol` and `Logger`: 
```
RingBuffer<LogEvent> buffer_; 
```
Log producer (protocol) does: 
```
buffer_.push(LogEvent{...});
```
Log consumer (logger thread) does something like: 
```
while (running_) {
    LogEvent e;
    if (buffer_.pop(e)) writeToDisk(e);
}
```


#### Why this fits: 
- No contention
- Fixed memory = real-time safety
- Pattern used in HFT engines and embedded logging subsystems

### Strategy Pattern 
#### Problem:
Different protocols have different control logic, but share a common structure (start, run steps, log, etc.)
#### Soln: Strategy Pattern
>Define a family of algorithms (protocols), encapsulate each one, and make them interchangeable.
#### Already kinda implicitly implied via:
class ExperimentProtocol {
    virtual void run(...) = 0;
};
The entire protocol system is a Strategy, chosen at runtime via Factory.
#### Why it fits:

- Enables treating all protocols uniformly in SystemCoordinator
- Encourages clean encapsulation of each protocol's logic

### RAII (Resource Management)

Use RAII (in destructors) to clean up (terrible acronym-great practice):
-   SerialChannel connections
-   Threads (std::thread::join())
-   i2c file handles
-   Log file handles

Perhaps I will introduce scoped guards (e.g., RunGuard for safe FSM exit logging). Lets see, how much I can do before the deadline. 

### Summary 
| Pattern       | Used In                   | Motivation                                    |
| ------------- | ------------------------- | --------------------------------------------- |
| Factory       | `ProtocolFactory`         | Runtime flexibility, decoupled construction   |
| State Machine | `ExperimentProtocol`      | Deterministic step-based control              |
| Observer      | `ErrorMonitor`            | Loose coupling for error propagation          |
| SPSC Queue    | `Logger`, `Protocol`      | Lock-free async event streaming               |
| Strategy      | `ExperimentProtocol`      | Swappable protocols under a uniform interface |
| RAII          | Threads, Files, I2C, etc. | Resource safety, no leaks                     |


## 5. Error Handling & Watchdogs

So thi section addresses how I plan to : 
- Detect failures in real time 
- Recover 
- Escalate to the right module (e.g., `SystemCoordinator`)
- Ensure system does not hang or silently fail

### Error Categories (MiLO SW specific)
| Category            | Examples                                           | Detection Mechanism      |
| ------------------- | -------------------------------------------------- | ------------------------ |
| Thread-level issues | Logger thread crashes, UI thread stalls            | Watchdog, liveness check |
| Serial failures     | USB disconnection, timeout, corrupt response       | `RPCManager`, CRC check  |
| SD/storage issues   | SD card missing, write failure, config parse error | `Logger`, `ConfigLoader` |
| UI failures         | Encoder stops responding, OLED I2C failure         | `UIController`           |
| Protocol runtime    | Unexpected response, user abort, invalid state     | `ExperimentProtocol`     |

