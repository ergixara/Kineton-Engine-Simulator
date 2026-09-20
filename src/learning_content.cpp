#include "../include/learning_content.h"

#include <cstddef>

namespace {
    constexpr unsigned int Mechanical = 0xC7D3DF;
    constexpr unsigned int Intake = 0x61CBD3;
    constexpr unsigned int Exhaust = 0xE89867;
    constexpr unsigned int Ignition = 0xE9C46A;

    // Geometry is schematic; these lessons describe functions, not exact part counts
    // or failure models. Motion and valve lift still come from the live simulation.
    constexpr PartLesson Lessons[] = {
        {EnginePart::None, "Explore the engine", "Learning guide",
            "Select a component to see its role. Cyan follows the intake path, orange the exhaust, and gold the ignition and combustion areas.",
            "Follow energy from burning fuel to the piston, connecting rod, crankshaft and drivetrain.",
            "The drawing is a teaching schematic. Real parts and their positions vary with the engine design.", Mechanical},
        {EnginePart::Piston, "Piston", "Motion / mechanical",
            "The piston slides inside the cylinder. Gas pressure pushes its crown, turning the energy of burning fuel into mechanical work.",
            "The connecting rod links it to the crankshaft. Piston movement also draws in, compresses and expels gas.",
            "Piston mass affects inertia. Poor sealing lets gas escape past the rings and reduces useful pressure.", Mechanical},
        {EnginePart::ConnectingRod, "Connecting rod", "Motion / mechanical",
            "This link joins the piston to an offset journal on the crankshaft. Its pivoting ends let a sliding piston drive a rotating shaft.",
            "It carries combustion and inertia forces between the piston and crankshaft throughout the cycle.",
            "Changing rod length changes piston motion for a given crank throw. A damaged rod can stop the mechanism from working.", Mechanical},
        {EnginePart::Crankshaft, "Crankshaft", "Motion / mechanical",
            "The crankshaft gathers the pistons' pushes as turning force, or torque. Its offset journals make reciprocating motion possible.",
            "Connecting rods drive its journals. The shaft delivers torque to the flywheel and transmission and sets the timing reference.",
            "A larger crank throw increases stroke and displacement. Journal arrangement also affects balance and firing possibilities.", Mechanical},
        {EnginePart::Flywheel, "Flywheel", "Motion / mechanical",
            "This rotating mass stores energy. It smooths the changes in shaft speed between combustion events and the other strokes.",
            "It rotates with the crankshaft and helps carry the engine through compression. The clutch connects engine torque to the gearbox.",
            "More rotational inertia slows RPM changes; less makes RPM respond faster. It does not create extra steady engine power.", Mechanical},
        {EnginePart::Cylinder, "Cylinder", "Structure / mechanical",
            "The cylinder guides the piston and contains pressurized gas. Its inside diameter is the bore; piston travel is the stroke.",
            "The bore, piston rings and cylinder head form the working space. Bore and stroke together determine swept volume.",
            "Increasing bore increases displacement at the same stroke. Wear or leakage reduces sealing and changes engine performance.", Mechanical},
        {EnginePart::CylinderHead, "Cylinder head", "Structure / mechanical",
            "The head closes the cylinder at the top and provides gas passages. In this schematic it supports the valves and spark plug.",
            "Its ports link the cylinder to intake and exhaust. Its chamber volume helps determine how much the mixture is compressed.",
            "Port flow and chamber volume affect breathing and compression. The simplified shape does not reproduce a real casting.", Mechanical},
        {EnginePart::Camshaft, "Camshaft", "Timing / mechanical",
            "Cam lobes schedule valve opening and lift. In a four-stroke engine the camshaft turns once for every two crankshaft turns.",
            "The simulator uses cam profiles to drive valve lift. The displayed cams illustrate timing, not an exact valvetrain layout.",
            "Changing lift, duration or timing changes breathing across the RPM range and can alter idle quality, torque and sound.", Mechanical},
        {EnginePart::IntakeValve, "Intake valve", "Air / intake",
            "This valve opens a path into the cylinder and closes to seal it. Its opening lets fresh charge enter when pressures allow.",
            "The cam profile sets lift; lift and the head's flow curve set the opening's flow capacity. Shown valves are representative.",
            "Lift and timing changes affect cylinder filling. A valve that cannot seal loses compression; more lift is not always better.", Intake},
        {EnginePart::ExhaustValve, "Exhaust valve", "Gas / exhaust",
            "This valve releases burned gas into the exhaust path so the cylinder can accept a fresh charge on the next cycle.",
            "Its cam profile sets opening and lift. Pressure difference drives flow; the piston then helps clear remaining gas.",
            "Opening time trades expansion work against gas removal. Leakage can reduce compression. Shown valves are representative.", Exhaust},
        {EnginePart::SparkPlug, "Spark plug", "Ignition / combustion",
            "An electrical spark starts burning the compressed fuel and air. Burning takes time, so spark timing matters as RPM changes.",
            "The ignition module schedules the spark relative to crank angle. The mixture and chamber conditions determine the response.",
            "A weak or absent spark can cause misfires. Advancing or delaying ignition changes where pressure rises during the cycle.", Ignition},
        {EnginePart::CombustionChamber, "Combustion chamber", "Ignition / combustion",
            "This is the gas space above the piston, not a solid part. Burning fuel heats the gas, raising pressure that pushes the piston.",
            "Its volume changes with piston position. Valve flow, mixture and ignition together shape the pressure and temperature traces.",
            "A smaller clearance volume increases compression at fixed displacement. The glow is illustrative, not a measured flame shape.", Ignition},
        {EnginePart::IntakeRunner, "Intake runner", "Air / intake",
            "This passage carries fresh charge toward an intake valve. The schematic shows its function rather than exact pipe routing.",
            "Throttle, manifold pressure and valve opening work together to fill the cylinder. This simulator mixes fuel in the intake.",
            "Runner length, volume and flow capacity change pressure response and breathing. No separate injector mechanism is depicted.", Intake},
        {EnginePart::ExhaustRunner, "Exhaust runner", "Gas / exhaust",
            "This passage takes gas from the exhaust valve toward the collector and outlet. The moving pressure pulses also produce sound.",
            "Each cylinder sends pulses into its exhaust system. Valve events, pipe properties and the audio synthesis shape what you hear.",
            "Changing runner length or flow capacity changes gas dynamics and sound. Displayed routing is schematic.", Exhaust},
        {EnginePart::Throttle, "Throttle", "Air / intake",
            "The throttle restricts the intake path to control how much fresh charge enters. Opening it usually lets the engine make more torque.",
            "Its effect depends on RPM and load. Watch manifold pressure and airflow respond; throttle position is not a power reading.",
            "A larger opening reduces the restriction. Power also depends on fuel mixture, ignition, cylinder filling and engine speed.", Intake}
    };

    static_assert(sizeof(Lessons) / sizeof(Lessons[0]) ==
        static_cast<std::size_t>(EnginePart::Count), "Every selectable part needs a lesson");
}

const PartLesson &partLesson(EnginePart part) {
    const int index = static_cast<int>(part);
    if (index < 0 || index >= static_cast<int>(EnginePart::Count)) return Lessons[0];
    return Lessons[index];
}
