#ifndef ATG_ENGINE_SIM_LEARNING_CONTENT_H
#define ATG_ENGINE_SIM_LEARNING_CONTENT_H

enum class EnginePart {
    None, Piston, ConnectingRod, Crankshaft, Flywheel, Cylinder, CylinderHead,
    Camshaft, IntakeValve, ExhaustValve, SparkPlug, CombustionChamber,
    IntakeRunner, ExhaustRunner, Throttle, Count
};

struct PartLesson {
    EnginePart part;
    const char *name;
    const char *category;
    const char *description;
    const char *interaction;
    const char *changeEffect;
    unsigned int color;
};

const PartLesson &partLesson(EnginePart part);

struct LearningState {
    EnginePart selected = EnginePart::Piston;
    int cylinder = 0;
    bool labels = true;
    bool exploded = false;
    bool slowMotion = false;
};

#endif
