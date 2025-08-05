# Low Level Design 

## Purpose 
Low Level Desing can get murky. Here is why I wrote this document. Let these points guide your motivations for readind this document. 

-   Map HLD modules to concrete C++ classes and responsibilities
-   Defines data structures, threading model, memory ownership
-   Selects design patterns (Factory, FSM, Observer, Singleton, etc.)
-   Provides inter-thread communication strategy
-   Begins to define headers, public APIs, and internal logic

## Overview

I’ll break my LLD down into the following sections:

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

Do not use these headers as definitive. They will be refined in Step 4. Especially for: 
- Factory injection mechanics
- Protocol interface polymorphism
- Ownership, DI (dependency injection), lifecycle

So yest, I will clean up these interfaces once we lock in the design patterns in later steps.
I would say use the following mappings as a peak into the general direction I want to take each of these classes. 
The code base will be self documenting. Therefore, if you want to review production implementation for any module, review the 
header files in the Include directory. 



