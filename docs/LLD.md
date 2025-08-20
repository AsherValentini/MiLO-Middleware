# Low Level Design 

## 1. Purpose 
Low Level Design can get murky. Here is what this document does.

-   Maps HLD modules to concrete C++ classes and responsibilities
-   Defines data structures, threading model, memory ownership
-   Selects design patterns (Factory, FSM, Observer, Singleton, etc.)
-   Provides inter-thread communication strategy
-   Begins to define headers, public APIs, and internal logic

## 2. Overview
I’ll break the LLD down into the following sections:

**Module-to-Class Mapping**
- One section per module from the HLD
- For each: class name, public/private methods, key responsibilities

**Threading Model**
- Which modules run in which threads
- Queues, buffer handoff points, synchronization strategy

**Data Ownership + Lifetimes**
- Who owns the protocol, log buffers, serial ports, config data, etc.
- Use of unique_ptr, shared_ptr, or raw references

**Design Patterns**
- Factory for protocol instantiation
- State machine for protocol lifecycle
- SPSC queues or ring buffers for inter-thread messaging
- Strategy pattern for each protocol variant
- Observer for loggers 

**Error Handling & Watchdogs**
- Retry logic
- Fail-safe state transitions
- MCU re-handshake logic

**I/O Abstractions**
- Serial port wrapper class (non-blocking)
- GPIO input class (rotary encoder + debounce)
- SPI OLED display class (text rendering, menu updates)

**File & Directory Layout**
- /src/middleware/
- /src/ui/
- /src/protocols/
- /src/io/
- /include/, etc.

## 3. Module-to-Class Mapping 
### 3.1 What I’ve done in Module-to-Class Mapping is:
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


### 3.2 SystemCoordinator
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
### 3.3 ConfigLoader 
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
### 3.4 ProtocolFactory 
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
### 3.5 ExperimentProtocol (abstract base + concrete subclass)
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
### 3.6 RPCManager 
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
### 3.7 Logger 
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
### 3.8 UIController 
Role: Emits events to the `SystemCoordinator` or the `ParameterStore`. Talks to the OLED via SPI-dev. 
```
class UIController {
public:
    void start();  // Launch UI loop in its own thread
    void registerCallback(std::function<void(UIEvent)>);

    void setDisplayState(SystemState state);
    void showParameterValue(Parameter param, float value);

private:
    void handleEncoderInput();   // Read GPIO
    void updateOLED();          // Write to SPI
};
```
### 3.9 ParameterStore
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
### 3.10 ErrorMonitor
Role: Monitors RPC timeouts, diconnections and escalates to `SystemCoordinator` when needed. 
```
class ErrorMonitor {
public:
    void watch(std::function<void()> failureCallback);
    void notifyFailure(const std::string& msg);
};
```
## 4. Threading Model
### 4.1 System Goals Recap and Context for Threading Choices
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
### 4.2 Proposed Threading Model 
```
[ UI Thread ]          ---> updates --->   [ ParameterStore ]   ---> read by ---> [ Protocol Thread ]

[ Serial I/O Thread ]  <--- blocks/reads --->  [ USB MCUs ]

[ Logger Thread ]      <--- enqueues from ---> [ Protocol Thread ]
```
### 4.3 UI Thread
- Reads GPIO rotary encoder 
- Pushes updates to the `ParameterStore`
- Updates OLED display based on state

> **Why a seperate thread?** 

> Because GPIO polling and OLED updates are **slow** (~SPI wrote latency + debounce timing). 
> We do not want that garbage anywhere near the protocol FSM logic. This keeps input and rendering **off the hot path**. 

### 4.4 Protocol Execution Thread
- Core FSM loop: executes steps, and computes commands 
- Reads parameters as needed 
- Dispatches to RPCManager
- Pushes log events to logger queue

> **Why a seperate thread?** 

> Protocols will have internal timing logic that may block. 
> They will be the "brains" of the system and should not **be blocked** either. 
> So they will block and they should not be blocked by: 

- SD card access (logging) 
- SPI/GPIO ISR (UI updates)
- Serial I/O (Remote Procedure Calls to distributed devices) 

This thread runs the active control state machine, must feal "real-time" from a logic perspective. 

### 4.5 Serial I/O Thread 
- Uses `poll()` or `select()` to wait on several `/dev/ttyUSBx`
- Reads/writes data using `RPCManager`
- Buffers input lines and matches protocol requests 

> **Why a seperate thread** 

> Because `read()` on a serial port is a blocking syscall, and IO latency is unpredictable. 
> By isolating it, the main protocol thread can continue working independently, and just await a res. 

### 4.6 Logger Thread
- Consumes LogEvent structs from a lock-free ring buffer (yes thats overkill but I like the pattern) 
- Flushes periodically to CSV 
- Rotates logs if disk space exceeded

> **Why a seperate thread**
> Logging simply cannot delay control flow..dah. 
> Okay but for real for real, writing to disk is non-deterministic (SD card wear leveling and OS syncs blah blah) 
> Miss me with that, we gonna log events into a buffer and handle flushing in the background. 
> smooth.  

### 4.7 Why I Like This Threading Model 

| Benefit              | How                                               |
| -------------------- | ------------------------------------------------- |
| Pipeline-ready       | Each thread handles one stage, decoupled          |
| Fail-safe            | A stuck logger doesn’t break the protocol         |
| Predictable latency  | The protocol thread is free from I/O jitter       |
| Testable             | Each module can be tested in isolation with mocks |
| Easy to reason about | Responsibilities are clearly isolated             |

### 4.8 Okay, Quick Summary

| Thread            | Purpose                     | Blocking Ops?  | Communication                                                  |
| ----------------- | --------------------------- | -------------- | -------------------------------------------------------------- |
| `UI Thread`       | Reads GPIO, updates display | Yes (SPI)      | Writes to `ParameterStore`                                     |
| `Protocol Thread` | Runs FSM, manages commands  | No             | Reads from `ParameterStore`, sends to Serial, pushes to logger |
| `Serial Thread`   | Reads/writes `/dev/ttyUSBx` | Yes            | Notifies protocol FSM via future/promise or callback           |
| `Logger Thread`   | Flushes log entries to disk | Yes (disk I/O) | Pulls from lock-free buffer                                    |


## 5. Data Ownership + Lifetimes

Memory leaks...pshshsh 
This section answers the question: 
> Who owns what? 
> Who creates what?
> Who cleans up what? 
> How long do these things live for...?

### 5.1 Why This Matters
The system is going to be multithreaded, so ownership clarity ensures: 
- No dangling pointers
- No data races
- No accidental double-free lifetime extension bugs
- Deterministic shutdown logic 
- Minimal surprises when reading or modifying code

### 5.2 Guiding Principles 
| Topic                                           | Rule                                                          |
| ----------------------------------------------- | ------------------------------------------------------------- |
| Long-lived systems (e.g., coordinator, loggers) | Use `std::shared_ptr` or lifetime-managed singletons          |
| Single-owner resources (e.g., config, protocol) | Use `std::unique_ptr`                                         |
| Shared memory buffers (e.g., ParameterStore)    | Shared via `std::shared_ptr` with internal mutex (or atomics) |
| Thread modules (UI, Logger)                     | Own their own thread + clean shutdown (RAII)                         |
| Event/data queues                               | Owned by producer, referenced by consumer                     |


### 5.3 Ownership Map (Per Module)
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


#### 5.3.1 `SystemCoordinator`
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

#### 5.3.2 `ConfigLoader`
- Short lived 
- Allocated on stack during `SystemCoordinator::initialize()`
- Return JSOn data byvaly (copy/move) 

#### 5.3.3 `ProtocolFactory` 
- Stateless singleton 
- Returns `std::unique_ptr<ExperimentProtocol>`

> **Why unique_ptr** 
> I expect that each protocol is used exactly once for a run. Clean ownership, no sharing. 

#### 5.3.4 `ExperimentProtocol`
- Owned by: `SystemCoordinator`
- Holds reference to: `ParameterStore` (as a `std::shared_ptr`) 
- Lifetime: per experiment run. 

> We do not share protocol logic between runs because I rate its better to discard and rebuild fresh per config. 

#### 5.4.5 `RPCManager` 
- Shared between: 
    - `SystemCoordinator` (duirng init)
    - `ExperimentProtocol` (for use during run) 
- Owns: all active USB serial connections 
- Lifetime: `SystemCoordinator::initialize()` -> app shutdown 

#### 5.3.6 `Logger`
- Owned by `SystemCoordinator`
- Runs its own internal thread
- Consumes `LogEvents` from a ring buffer 
- Shuts down cleanly at system exit via `Logger::finishRun()`

#### 5.3.7 `UIController`
- Owned by `SystemCoordinator`
- Owns: 
    - Rotary encoder GPIO lines 
    - OLED SPI handle 
    - Its own thread 
- Writes to: `ParameterStore` (via `std::shared_ptr`) 

#### 5.3.8 `ParameterStore`
- Shared across a few modules: 
    - `UIController` (wrt) 
    - `ExperimentProtocol` (rd) 
    - `Logger` (rd)
- Critical sections and internal data guarded by mutex/RAII guards 

#### 5.3.9 `ErrorMonitor` 
- Shared across: 
    - `RPCManager` (report failures) 
    - `SystemCoordinator` (escalations) 
> **Pattern options:** thinking about signal or observer patern depending on how the callbacks are designed...

### 5.4 Inter-Thread Buffers 
So these are heavily subject to change based on how I am feeling when I get into the meet and bones of this. 
One thing is for certain, they will be typical queue like memory structures/patterns. Double buffer, ring buffer, thread safe blocking queue built
over std::queue<T>. All good options. Here is my initial draft: 

| Buffer                                 | Owner           | Reader          |
| -------------------------------------- | --------------- | --------------- |
| `RingBuffer<LogEvent>`                 | Protocol Thread | Logger Thread   |
| `RingBuffer<SerialMessage>` (optional) | Serial Thread   | Protocol Thread |
| `ParameterStore` (mutexed map)         | Shared          | Shared          |


### 5.5 Takeaways
| Decision                                      | Motivation                                          |
| --------------------------------------------- | --------------------------------------------------- |
| `unique_ptr<ExperimentProtocol>`              | Clean separation per run, exclusive ownership       |
| `shared_ptr<Logger/UIController/ParamStore>`  | Shared by multiple threads with clear shutdown path |
| Internal mutex in `ParameterStore`            | Simplicity over lock-free complexity                |
| `std::thread` member in thread-owning modules | Ensures joinable shutdown                           |
| Value-returning `ConfigLoader`                | No leaks, easy testing                              |
| Clear start/stop API on threaded modules      | Professional-grade lifecycle handling               |


## 6. Design Patterns

This section lays down what patterns I plan to use while also addressing __why__ we are using each one, __what__ problem the pattern solves, and __how__ it fits into 
our system"s structure and goals. 

Hopefully, this section reveals: 
- The need that arises
- The pattern that solves it
- Its benefits
- Where it will appear in the project

Okay, enough chatter lets walk through them one by one. 

### 6.1 Factory Pattern 
#### 6.1.2 Problem: 
We need to instantiate different protocol classes at runtime based on: 
- JSON config file
- Possibly: user selection from the UI

#### 6.1.3 Soln: Factory Pattern 
>Define an interface for creating an object, but let subclasses (or config) decide whcih class to instantiate. 

#### 6.1.4 Impl (something like this) 
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

### 6.2 State Machine (FSM) 
#### 6.2.1 Problem: 
Experiment logic will be composed of steps that execute in sequence, probaly with: 
- Time delays 
- Waits for MCU responses
- Conditional branches (retries and aborts)

#### 6.2.2 Soln: Finite State Machine Pattern 
> Model the experiment protocol as a sequence of states, each with transition conditions and exit criterion. 

#### 6.2.3 Impl: 
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

#### 6.2.4 Why this fits: 
- Experiment logic is naturally stateful and sequential
- Easy to debug, test, simulate, log, its just good okay 
- Works well with timers, retries, and error transitions. (Think error fallback STATE) 

### 6.3 Observer Pattern (Lightweight Usage) 
#### 6.3.1 Problem: 
We want to notify the `SystemCoordinator` or error logger when something fails. 
#### 6.3.2 Soln: Observer Pattern 
>Set the `ErrorMonitor` as a subject and allow it to publish events without really caring who will handle them. 

So why not the signal-slot pattern? Because this is not a Qt based project.
Go lecture someone else about how easy event driven architecture is with Qt.  
We are gonna go Subject -> Subject Interface -> Observer Interface -> Observer. 
The way GoF intended pub-sub to work.
Don't like it, take it up with Design Patterns: Elements of Reusable Code. 

#### 6.2.3 Impl (read it and weap) 
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

### 6.3 SPSC Queue / Ring Buffer 
#### 6.3.1 Problem: 
We need to pass log events from the Protocol thread to the Logger thread without locking or blocking. 
Honestly, there will be more cross stage inter thread memory hand offs and we gonna go with Ring Buffers. 

#### 6.3.2 Soln: Single-Producer, Single-Consumer (SPSC) Ring Buffer 
> A fixed sized lock-fre buffer where one thread produces and another consumes. 

> Open Q: overwrite policy...not clear. 

#### 6.3.3 Example Application between `Protocol` and `Logger`: 
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


#### 6.3.4 Why this fits: 
- No contention
- Fixed memory = real-time safety
- Pattern used in HFT engines and embedded logging subsystems

### 6.4 Strategy Pattern 
#### 6.4.1 Problem:
Different protocols have different control logic, but share a common structure (start, run steps, log, etc.)
#### 6.4.2 Soln: Strategy Pattern
>Define a family of algorithms (protocols), encapsulate each one, and make them interchangeable.
#### 6.4.3 Already kinda implicitly implied via:
class ExperimentProtocol {
    virtual void run(...) = 0;
};
The entire protocol system is a Strategy, chosen at runtime via Factory.
#### 6.4.4 Why it fits:

- Enables treating all protocols uniformly in SystemCoordinator
- Encourages clean encapsulation of each protocol's logic

### 6.5 RAII (Resource Management)

Use RAII (in destructors) to clean up (terrible acronym-great practice):
-   SerialChannel connections
-   Threads (std::thread::join())
-   SPI file handles
-   Log file handles

Perhaps I will introduce scoped guards (e.g., RunGuard for safe FSM exit logging). Lets see, how much I can do before the deadline. 

### 6.6 Summary 
| Pattern       | Used In                   | Motivation                                    |
| ------------- | ------------------------- | --------------------------------------------- |
| Factory       | `ProtocolFactory`         | Runtime flexibility, decoupled construction   |
| State Machine | `ExperimentProtocol`      | Deterministic step-based control              |
| Observer      | `ErrorMonitor`            | Loose coupling for error propagation          |
| SPSC Queue    | `Logger`, `Protocol`      | Lock-free async event streaming               |
| Strategy      | `ExperimentProtocol`      | Swappable protocols under a uniform interface |
| RAII          | Threads, Files, SPI, etc. | Resource safety, no leaks                     |


## 7. Error Handling & Watchdogs

So this section addresses how I plan to: 
- Detect failures in real time 
- Recover 
- Escalate to the right module (e.g., `SystemCoordinator`)
- Ensure system does not hang or silently fail

### 7.1 Error Categories (MiLO SW specific)
| Category            | Examples                                           | Detection Mechanism      |
| ------------------- | -------------------------------------------------- | ------------------------ |
| Thread-level issues | Logger thread crashes, UI thread stalls            | Watchdog, liveness check |
| Serial failures     | USB disconnection, timeout, corrupt response       | `RPCManager`, CRC check  |
| SD/storage issues   | SD card missing, write failure, config parse error | `Logger`, `ConfigLoader` |
| UI failures         | Encoder stops responding, OLED SPI failure         | `UIController`           |
| Protocol runtime    | Unexpected response, user abort, invalid state     | `ExperimentProtocol`     |

### 7.2 Guiding Philosophy 
- System must never silently fail 
- All errors should be: 
    - Logged 
    - Escalated to the `SystemCoordinator`
    - Possibly displayed on the OLED (e.g., sd card corrupt/missing)
- The device should always return to a safe fallbackstate: `IDLE`

### 7.3 Error Detection Mechanisms
#### 7.3.1 Thread Watchdogs (Lighweight)
For: Logger thread, UI thread, Serial thread im thinking something like: 
```
class ThreadHeartbeat {
public:
    void tick();
    bool isAlive(std::chrono::milliseconds timeout) const;
};
```
- Each thread calls `tick()` periodically 
- `SystemCoordinator` checks `.isAlive()` every second 
- if not alive -> log error, abort experiment, reset state

#### 7.3.2 Serial Communication Errors 
For: Disconneted FTDI, malformed responses, timeout hits
Handled inside the `RPCManager`
```
Response awaitResponse(..., Timeout t) {
    if (!port_.isConnected()) errorMonitor_.notify(...);
    if (crcFail || parseError) errorMonitor_.notify(...);
}
```
These get pushed into a ring buffer (maybe an observer call) for the `SystemCoordinator`. 

#### 7.3.3 SD Card & Config Failures 
- `ConfigLoader::load() will throw or return error codes 
- `Logger::startNewRun() checks file open/write sucess
- Both escalate via `ErrorMonitor` or return to `SystemCoordinator::handleError()`

### 7.4 Error Escalation Mechanism 
Handled via GoF Obs Pattern (`ErrorMonitor -> SystemCoordinator`) as I mentioned earlier. 
With the adition of the messages buffered in `RingBuffer<ErrorEvent>` and polled by FSM loop. 

All escallations trigger: 
- State change to `ERROR`
- Loggin of failure cause 
- Display of failure reason on OLED 
- Reset to `IDLE` after user ack via rotary encoder button press (perhaps a timout as well) 

### 7.5 Error handling in `SystemCoordinator`
```
void SystemCoordinator::handleError(const std::string& msg) {
    logger_->log(LogEvent::SystemError{msg});
    ui_->setDisplayState(SystemState::ERROR, msg);
    abortCurrentProtocol();
    transitionTo(State::IDLE);
}
```

### 7.6 Recovery Strategy 
| Scenario             | Action                                            |
| -------------------- | ------------------------------------------------- |
| USB device unplugged | Log, retry for 5s, then abort                     |
| SD full              | Log warning, skip file writes                     |
| SD Corrupt/Missing   | Log warning, then abort                           |
| Protocol failure     | Abort run, return to IDLE                         |
| Logger thread stuck  | Write error to `/tmp/failsafe.log`, reboot device |

## 8. IO Abstractions 
In this section I will define how the middleware interacts with the hardware-level interfaces: 
- USB serial ports 
- GPIO lines (rotary encoder + button) 
- SPI ILED display 
- File system (SD card) 

So I am thinking to design these as modular wrapper classes to encapsulate raw Linux APIs, hide complexity, and 
make sure the logic is clean, testable, and portable. 

### 8.1 USB Serial - `SerialChannel`
#### 8.1.1 Purspose: 
- Abstract `/dev/ttyUSBx` device
- Provide non-blocking read/write with line framing and optional CRC 
- Handle reconnection, flushing, and identity tagging 

#### 8.1.2 Class Sketch 
```
class SerialChannel {
public:
    SerialChannel(const std::string& devicePath);
    bool open();
    void close();
    bool isConnected() const;

    void writeLine(const std::string& line);
    std::optional<std::string> readLine();  // Non-blocking if possible

private:
    int fd_;
    std::string path_;
    std::string rxBuffer_;
};
```
But I also want to support device VID/PID tagging and line CRC verification and `poll()` integration for async event loop
(in `RPCManager`)

### 8.2 GPIO - 'GPIOInput` - `GPIOButton` (derived) - `RotaryEncoderGPIO` (composite) 
So just a brief intro to whats going on here. In our system decomp we got a rotary encoder that also acts as a tactile switch. 
Then we got a standalone button. My proposil is to create an GPIOInput base class. Then, we got a button IO module that
will inherit from that. Instead of making the rotary encoder an 'is a' lets make it a 'has a', where its channels are GPIOInputs. 

#### 8.2.1 Purpose:
- Read GPIO input ~ button press  
- Read rotary encoder A/B signals
- Provide high-level events like `Direction::Left`, `Direction::Right`, `Pressed`

#### 8.2.2 Classes Sketch: 
GPIOInput: 
```
    class GPIOInput {
    public:
      enum class Edge { Rising, Falling };

      using Callback = std::function<void(Edge)>;

      GPIOInput() = default;
      virtual ~GPIOInput(); ///< auto-release line

      /** @returns false if the GPIO chip/line cannot be opened. */
      bool open(const std::string& chip, ///< e.g. "/dev/gpiochip0"
                unsigned int line,       ///< pin number
                bool activeLow = false);

      /** Polls the line and emits debounced edge events.  To be called from the
      owner’s loop (UI thread or epoll mux). */
      virtual void poll(std::chrono::milliseconds now) = 0;

      void registerCallback(Callback cb) { cb_ = std::move(cb); }

      void close();

    protected:
      /** Derived classes call this when they detect a debounced edge. */
      void emit(Edge e) {
        if (cb_)
          cb_(e);
      }

      int fd_{ -1 }; ///< gpiod line FD (-1 = closed)
      Callback cb_{};

      // simple debounce helpers
      bool lastState_{ false };
      uint32_t lastTick_{ 0 }; ///< msec timestamp of last change
    };
```
GPIOButton (derived): 
```
    class ButtonGPIO : public GPIOInput {

    public:
      enum class Event { ShortPress, LongPress };

      using Callback = std::function<void(Event)>;

      explicit ButtonGPIO(std::chrono::milliseconds longPressThresh = std::chrono::seconds{ 1 })
          : longThreshold_{ longPressThresh } {}

      void registerCallback(Callback cb) { cbButton_ = std::move(cb); }

      /** Called by owner loop (e.g., UI thread) every ~10 ms. */
      void poll(std::chrono::milliseconds now) override;

    private:
      void emitPress(Event e) {
        if (cbButton_) {
          if (cbButton_)
            cbButton_(e);
        }
      }

      Callback cbButton_{};

      std::chrono::milliseconds longThreshold_{ 1000 };
      bool presed_{ false };
      std::uint32_t pressTick_{ 0 };
    };
```
RotaryEncoderGPIO (composite) 
```
enum class Direction { None, Left, Right };

class RotaryEncoderGPIO {
public:
    void initialize();
    Direction poll();  // Called periodically from UI thread
    bool isButtonPressed();

private:
    GPIOInput pinA_, pinB_, pinBtn_; ///<something like this-right where we make the channels GPIO as well
    int lastState_;
    std::chrono::steady_clock::time_point lastDebounceTime_;
};
```
Use `libgpiod` or `/sys/class/gpio` depending on system setup 

### 8.3 SPI OLED Display - `OLEDDisplay`
#### Purpose: 
- Render parameter name + value 
- Show status screens (IDLE, RUNNING, ERROR) 
- Optional: animation or flashing effects, maybe Cellectric Logo 

#### 8.3.1 Class Sketch: 
```
class OLEDDisplay {
public:
    void initialize();
    void showParameter(const std::string& name, float value);
    void showState(SystemState state, const std::string& optionalMsg = "");

private:
    int Fd_;
    uint8_t Address_;
    FrameBuffer frame_;
    void flush();
};
```
#### 8.3.2 Internal Deets:  
- Use existing Cpp Libs or talk to `/dev/spidev0.0` directly 
- Frame buffering ensures no flickering 

### 8.4 File System - `FileLogger` (ussed by `Logger`)
#### 8.4.1 Purpose: 
- Abstract CSV logging to SD card 
- Handle auto-flushing and log rotation 

#### 8.4.2 Class Sketch 
```
class FileLogger {
public:
    bool startRun(const std::string& experimentId);
    void log(const LogEvent& e);
    void finishRun();
    void enforceRotationLimit(size_t maxBytes);

private:
    std::ofstream file_;
    std::string baseDir_;
};
```
- `Logger` thread calls `FileLogger::log()` from the background thread
- Uses `std::thread::steady_clock` for timestamps 
- Manages log naming and `manifest.json` if needs be. 

### 8.5 Integration Points 
| Module              | Uses This I/O Abstraction              |
| ------------------- | -------------------------------------- |
| `RPCManager`        | `SerialChannel` per device             |
| `UIController`      | `RotaryEncoderGPIO`, `OLEDDisplay`     |
| `Logger`            | `FileLogger`                           |
| `SystemCoordinator` | indirectly calls I/O via other modules |

### 8.5.1 Why This Abstraction Layer Matters 

| Benefit              | Explanation                                                       |
| -------------------- | ----------------------------------------------------------------- |
|  Testable            | Replace each I/O backend with mocks/stubs for unit testing        |
|  Isolation           | Logic code doesn’t depend on Linux file APIs                      |
|  Portability         | Easy to retarget to another board (e.g., SPI OLED instead of I2C) |
|  Professional design | Clear layering, dependency inversion in practice                  |

### 8.5.2 RAII Reminder 
- All these IO classes will: 
    - `open()` resources in `initialize()` or ctor
    - `close()` or `cleanup()` in dtor
- This keeps all system-level handles safely wrapped :)

## 9. File and Directory Layout 
This section defnes the physical structure of the code base - where each module lives, how it is grouped and what goes 
into the `include/` vs `src/`. 

The goal is to 
- Keep the project modular and navigable 
- Seperate headers and impl
- Mirror Logical domains (e.g., UI vs protocol vs middleware) 
- Support CMake builds and cross-compilation 

### 9.1 Top-Level Structure 

```
/MiLO/
├── include/
│   ├── core/
│   ├── io/
│   ├── protocols/
│   └── ui/
├── src/
│   ├── core/
│   ├── io/
│   ├── protocols/
│   └── ui/
├── tests/
├── cmake/
├── scripts/
└── CMakeLists.txt
```

### 9.2 Folder-by-Folder Breakdown 
#### 9.2.1 `/include/`
- Contains all public headers 
- Grouped by logical domain
- Mirrors `/src/` layout 

| Subfolder    | Purpose                                      |
| ------------ | -------------------------------------------- |
| `core/`      | Coordinator, Logger, Factory, ErrorMonitor   |
| `io/`        | SerialChannel, OLED, GPIO, FileLogger        |
| `protocols/` | ExperimentProtocol base + concrete protocols |
| `ui/`        | UIController, enums, ParameterStore          |

eg: 

```
/include/core/SystemCoordinator.hpp
/include/io/SerialChannel.hpp
/include/protocols/LysisProtocol.hpp
/include/ui/UIController.hpp
```

#### 9.2.2 `/src/`
- Impl files (.cpp)
- Mirrors header structure for clarity 

eg: 
```
/src/core/SystemCoordinator.cpp
/src/io/SerialChannel.cpp
/src/protocols/LysisProtocol.cpp
/src/ui/UIController.cpp
```

#### 9.2.3 `/tests/`
- Unit  and integration tests (GTest) 
- Mocks for each IO class
- Standalone test runners for FSMs, protocol steps, IO edge cases 

`/cmake/`
- Toolchain files (cross compile)
- ST toolchain config (arm-linux-gnueabinf)
- Optional: `FindXXX.cmake` files for dependencies (e.g., `nlohman_json`)

#### 9.2.4 `/scripts/` 
- Flash scripts 
- SD card image mount helpers 
- Serial port mapping tools (udev utilities) 

### 9.2.5 CMake Targets 
Suggested targets in `CMakeLists.txt`:
```
add_library(core STATIC
    src/core/SystemCoordinator.cpp
    src/core/Logger.cpp
    ...
)

add_library(io STATIC
    src/io/SerialChannel.cpp
    src/io/OLEDDisplay.cpp
    ...
)

add_executable(milo_experimentd
    src/main.cpp
)

target_link_libraries(milo_experimentd
    core
    io
    protocols
    ui
    pthread
    ...
)
```

### 9.3 Best Practices Followed 
| Practice               | Explanation                                  |
| ---------------------- | -------------------------------------------- |
| Header/Impl separation | Clean build system, test mocking, modularity |
| Folder mirroring       | Jump between header/impl instantly           |
| Logical domains        | Easy onboarding, team collaboration          |
| CMake-ready            | CI/CD and cross-compilation support          |

