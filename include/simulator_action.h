#ifndef ATG_ENGINE_SIM_SIMULATOR_ACTION_H
#define ATG_ENGINE_SIM_SIMULATOR_ACTION_H

// Shared commands used by keyboard shortcuts and the existing UI controls.
enum class SimulatorAction {
    Mode, Reset, Cutaway, Learn, Labels, Exploded, Ignition, Starter, Pause, Slow
};

enum class ActionSource { Pointer, Keyboard };

#endif /* ATG_ENGINE_SIM_SIMULATOR_ACTION_H */
