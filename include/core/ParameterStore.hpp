#pragma once
/** @file  ParameterStore.hpp
 *  @brief Thread-safe runtime parameters shared by UI & protocols.
 *
 *  © 2025 Milo Medical — MIT-licensed.
 */

#include <mutex>
#include <unordered_map>

namespace milo {
  namespace core {

    /**
 * @enum Parameter
 * @brief Strong-typed keys for every tunable setting.
 *
 *  (Full list lives in ParameterStore.cpp once we wire JSON → enum.)
 */
    enum class Parameter {
      Temp,
      FlowRate,
      Voltage,
      // …
    };

    /** @class ParameterStore
 *  @brief Lock-protected map of <Parameter → float>.
 *
 *  * R/W from multiple threads (UI encoder vs. protocol FSM).
 *  * Uses strong-typed key to avoid accidental string mismatches.
 */
    class ParameterStore {

    public:
      ParameterStore() = default;
      ~ParameterStore() = default;

      /// Atomically writes \p value under key \p p.
      void set(Parameter p, float value);

      /// Thread-safe getter; returns 0 f if key missing.
      float get(Parameter p) const;

    private:
      mutable std::mutex mtx_;
      std::unordered_map<Parameter, float>
          values_; // just for today use float primitive (strong types added on day 6)
    };

  } // namespace core
} // namespace milo
