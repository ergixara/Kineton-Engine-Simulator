#include "../include/learning_panel.h"

#include "../include/engine_sim_application.h"
#include "../include/constants.h"
#include "../include/units.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {
    constexpr unsigned int Background = 0x111c28;
    constexpr unsigned int Card = 0x1b2b3b;
    constexpr unsigned int Divider = 0x314456;
    constexpr unsigned int White = 0xeaf3f8;
    constexpr unsigned int Muted = 0xa7bacb;
    constexpr unsigned int Accent = 0x60ddca;

    std::string number(double value, int precision = 0, const char *unit = "") {
        if (!std::isfinite(value)) return "N/A";
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value << unit;
        return stream.str();
    }

    double throttleOpening(Engine *engine) {
        return engine && engine->getIntakeCount() > 0
            ? 100.0 * engine->getThrottlePlateAngle() / (constants::pi / 2.0) : 0.0;
    }
}

LearningPanel::LearningPanel() {
    m_checkMouse = true;
}

void LearningPanel::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
}

void LearningPanel::update(float dt) {
    UiElement::update(dt);
    m_mouseBounds = m_bounds;
    m_disabled = !isVisible();
    const LearningState &state = m_app->getLearningState();
    if (state.selected != m_lastSelection || state.cylinder != m_lastCylinder) {
        // Selection can also come from a model hotspot.
        m_lastSelection = state.selected;
        m_lastCylinder = state.cylinder;
        if (m_tab == Tab::Parts) m_scroll = 0;
    }
}

void LearningPanel::setTab(Tab tab) {
    if (tab != m_tab) {
        m_tab = tab;
        m_scroll = 0;
    }
}

Bounds LearningPanel::row(float y, float height, float inset) const {
    return Bounds(m_width - inset * 2.0f, height, {m_left + inset, y}, Bounds::tl);
}

bool LearningPanel::inClip(const Bounds &bounds) const {
    return bounds.top() <= m_clip.top() + 0.1f && bounds.bottom() >= m_clip.bottom() - 0.1f;
}

void LearningPanel::box(const Bounds &bounds, unsigned int color) {
    const Bounds clipped(
        (std::max)(bounds.left(), m_clip.left()), (std::min)(bounds.right(), m_clip.right()),
        (std::max)(bounds.bottom(), m_clip.bottom()), (std::min)(bounds.top(), m_clip.top()));
    if (bounds.top() <= m_clip.bottom() || bounds.bottom() >= m_clip.top()
        || clipped.width() <= 0 || clipped.height() <= 0) return;
    drawBox(clipped, ysColor::srgbiToLinear(color));
}

std::string LearningPanel::shorten(const std::string &value, float width, float size) const {
    if (m_app->getTextRenderer()->CalculateWidth(value, size) <= width) return value;
    std::string result = value;
    while (!result.empty() && m_app->getTextRenderer()->CalculateWidth(result + "...", size) > width)
        result.pop_back();
    return result + "...";
}

void LearningPanel::text(const std::string &value, const Bounds &bounds, float size,
    unsigned int color, bool right)
{
    if (!inClip(bounds)) return;
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(color)));
    drawAlignedText(shorten(value, bounds.width(), size), bounds, size,
        right ? Bounds::rm : Bounds::lm, right ? Bounds::rm : Bounds::lm);
}

void LearningPanel::button(const std::string &label, const Bounds &bounds,
    Action action, int value, bool active)
{
    box(bounds, active ? 0x214840 : Card);
    if (active) box(Bounds(3, bounds.height(), bounds.getPosition(Bounds::tl)), Accent);
    text(label, bounds.inset(7), 13, active ? Accent : White);
    if (inClip(bounds)) m_hits.push_back({bounds, action, value});
}

float LearningPanel::paragraph(const std::string &value, float y, unsigned int color) {
    constexpr float font = 14;
    constexpr float lineHeight = 20;
    std::istringstream words(value);
    std::string word, line;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && m_app->getTextRenderer()->CalculateWidth(candidate, font) > m_width - 4) {
            text(line, row(y, lineHeight), font, color);
            y -= lineHeight;
            line = word;
        }
        else line = candidate;
    }
    if (!line.empty()) {
        text(line, row(y, lineHeight), font, color);
        y -= lineHeight;
    }
    return y;
}

float LearningPanel::section(const std::string &label, float y) {
    text(label, row(y, 22), 12, Accent);
    return y - 28;
}

void LearningPanel::render() {
    if (!isVisible()) return;
    m_hits.clear();
    m_clip = m_bounds;
    m_left = m_bounds.left() + 18;
    m_width = (std::max)(100.0f, m_bounds.width() - 36);
    box(m_bounds, Background);
    box(Bounds(1, m_bounds.height(), m_bounds.getPosition(Bounds::tl)), Divider);

    Simulator *simulator = m_app->getSimulator();
    Engine *engine = simulator ? simulator->getEngine() : nullptr;
    const float top = m_bounds.top() - 18;
    text("ENGINE LAB", row(top, 20), 12, Accent);
    text(engine ? engine->getName() : "No engine loaded", row(top - 23, 28), 22, White);

    const double rpm = engine ? std::abs(engine->getRpm()) : 0;
    const bool ignition = engine && engine->getIgnitionModule()->m_enabled;
    const bool starting = simulator && simulator->m_starterMotor.m_enabled;
    const std::string status = !engine ? "CHECK SCRIPT" : m_app->isPaused() ? "PAUSED"
        : starting ? "STARTER ENGAGED" : rpm > 100 ? (ignition ? "RUNNING" : "COASTING")
        : ignition ? "IGNITION READY" : "ENGINE STOPPED";
    text(status, row(top - 54, 18), 12, rpm > 100 && ignition ? Accent : 0xf3c77d);

    const Bounds heroes = row(top - 80, 64);
    box(heroes, Card);
    const Bounds first = heroes.horizontalSplit(0, 0.52f).inset(10);
    const Bounds second = heroes.horizontalSplit(0.52f, 1).inset(10);
    text(engine ? number(rpm) : "--", Bounds(first.width(), 29, first.getPosition(Bounds::tl)), 27, White);
    text("RPM / crankshaft speed", Bounds(first.width(), 16, first.getPosition(Bounds::bl), Bounds::bl), 11, Muted);
    text(engine && engine->getIntakeCount() > 0 ? number(throttleOpening(engine), 0, "%") : "--",
        Bounds(second.width(), 29, second.getPosition(Bounds::tl)), 27, White);
    text("Throttle plate opening", Bounds(second.width(), 16, second.getPosition(Bounds::bl), Bounds::bl), 11, Muted);

    const Bounds actions = row(top - 154, 30);
    button(m_app->isPaused() ? "Resume" : "Pause", actions.horizontalSplit(0, 0.315f), Action::Pause, 0, m_app->isPaused());
    button(m_app->getLearningState().slowMotion ? "Slow: 1/40" : "Slow motion",
        actions.horizontalSplit(0.335f, 0.65f), Action::Slow, 0, m_app->getLearningState().slowMotion);
    button("Classic UI", actions.horizontalSplit(0.67f, 1), Action::Classic);

    const Bounds tabs = row(top - 196, 34);
    const char *labels[] = {"PARTS", "LIVE DATA", "CONTROLS"};
    for (int i = 0; i < 3; ++i) {
        const Bounds tab = tabs.horizontalSplit(i / 3.0f, (i + 1) / 3.0f).inset(2);
        button(labels[i], tab, Action::Tab, i, (int)m_tab == i);
    }

    const float bodyTop = top - 246;
    const float available = bodyTop - m_bounds.bottom() - 30;
    if (available > 10) {
        m_clip = Bounds(m_bounds.left() + 12, m_bounds.right() - 12, m_bounds.bottom() + 30, bodyTop);
        const float start = bodyTop + m_scroll;
        float end = start;
        if (m_tab == Tab::Parts) end = renderParts(start, engine);
        else if (m_tab == Tab::Telemetry) end = renderTelemetry(start, engine);
        else end = renderControls(start);
        m_maxScroll = (std::max)(0.0f, start - end - available + 8);
        m_scroll = (std::min)(m_maxScroll, m_scroll);
    }
    else m_scroll = m_maxScroll = 0;
    m_clip = m_bounds;
    const Bounds footer(m_width, 20, {m_left, m_bounds.bottom() + 23}, Bounds::tl);
    text(m_maxScroll > 0 ? "Scroll for more  |  Click a row to explore" : "Select a part or metric to explore", footer, 11, Muted);
    if (m_maxScroll > 0) {
        const float trackHeight = (std::max)(1.0f, available);
        const float thumbHeight = (std::max)(25.0f, trackHeight * available / (available + m_maxScroll));
        box(Bounds(3, trackHeight, {m_bounds.right() - 6, bodyTop}), Divider);
        box(Bounds(3, thumbHeight, {m_bounds.right() - 6,
            bodyTop - (trackHeight - thumbHeight) * m_scroll / m_maxScroll}), Accent);
    }
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(m_app->getForegroundColor()));
}

float LearningPanel::renderParts(float y, Engine *engine) {
    LearningState &state = m_app->getLearningState();
    const PartLesson &lesson = partLesson(state.selected);
    // Keep the visual teaching controls visible before the longer lesson content.
    const Bounds modes = row(y, 32);
    button("Labels", modes.horizontalSplit(0, 0.49f), Action::Labels, 0, state.labels);
    button("Exploded view", modes.horizontalSplit(0.51f, 1), Action::Explode, 0, state.exploded);
    y -= 40;
    const int count = engine ? engine->getCylinderCount() : 0;
    const Bounds cylinder = row(y, 30);
    button("<", cylinder.horizontalSplit(0, 0.12f), Action::CylinderPrevious);
    text(count > 0 ? "Cylinder " + std::to_string(state.cylinder + 1) + " / " + std::to_string(count) : "No cylinders",
        cylinder.horizontalSplit(0.16f, 0.84f), 14, White);
    button(">", cylinder.horizontalSplit(0.88f, 1), Action::CylinderNext);
    y -= 42;
    y = section(lesson.category, y);
    text(lesson.name, row(y, 28), 22, White);
    y -= 33;
    y = paragraph(lesson.description, y, White) - 14;
    y = section("CONNECTED TO", y);
    y = paragraph(lesson.interaction, y) - 14;
    y = section("IF CHANGED OR DAMAGED", y);
    y = paragraph(lesson.changeEffect, y) - 18;

    y = section("EXPLORE A COMPONENT", y);
    constexpr int parts = (int)EnginePart::Count - 1;
    for (int i = 0; i < parts; ++i) {
        const EnginePart part = (EnginePart)(i + 1);
        const Bounds cell = row(y - (i / 2) * 33.0f, 29).horizontalSplit(
            (i % 2) * 0.51f, (i % 2) == 0 ? 0.49f : 1.0f);
        button(partLesson(part).name, cell, Action::Part, (int)part, state.selected == part);
        box(Bounds(3, 15, {cell.right() - 5, cell.center_v()}, Bounds::rm), partLesson(part).color);
    }
    y -= ((parts + 1) / 2) * 33.0f + 12;
    return paragraph("Click a marker on the model, or choose a component here. The highlighted region follows the selected cylinder. Exploded spacing changes the drawing only.", y);
}

std::vector<LearningPanel::Metric> LearningPanel::metrics(Engine *engine) const {
    Simulator *sim = m_app->getSimulator();
    const auto value = [engine](double v, int p, const char *unit) {
        return engine ? number(v, p, unit) : "N/A";
    };
    const double rpm = engine ? std::abs(engine->getRpm()) : 0;
    const bool dyno = engine && sim && sim->m_dyno.m_enabled;
    const double torque = dyno ? sim->getFilteredDynoTorque() : 0;
    const double afr = engine ? engine->getIntakeAfr() : 0;
    const int count = engine ? engine->getCylinderCount() : 0;
    const int selectedCylinder = (std::max)(0, (std::min)(count - 1, m_app->getLearningState().cylinder));
    CombustionChamber *chamber = count > 0 ? engine->getChamber(selectedCylinder) : nullptr;
    const std::string cylinder = " (cyl " + std::to_string(selectedCylinder + 1) + ")";
    double intakeTemperature = 0;
    const int intakeCount = engine ? engine->getIntakeCount() : 0;
    for (int i = 0; i < intakeCount; ++i) intakeTemperature += engine->getIntake(i)->m_system.temperature();
    if (intakeCount > 0) intakeTemperature /= intakeCount;
    const double flow = engine ? engine->getIntakeFlowRate() : 0;
    const double theoreticalFlow = engine ? 0.5 * engine->getDisplacement() * units::atm
        / (constants::R * units::celcius(25)) * rpm / 60.0 : 0;
    const bool metricSpeed = m_app->getAppSettings()->speedUnits != "mph";
    const bool metricPower = m_app->getAppSettings()->powerUnits == "kW";
    const bool metricTorque = m_app->getAppSettings()->torqueUnits == "Nm";
    const std::string gear = sim && sim->getTransmission()
        ? sim->getTransmission()->getGear() < 0 ? "N" : std::to_string(sim->getTransmission()->getGear() + 1) : "N/A";

    return {
        {"ENGINE CORE", "Engine speed", value(rpm, 0, " rpm"),
            "The crankshaft's revolutions per minute. More throttle can raise RPM; drivetrain or dyno load resists it. Compare with this engine's redline, not a universal safe range.", EnginePart::Crankshaft},
        {"ENGINE CORE", "Redline", value(engine ? units::toRpm(engine->getRedline()) : 0, 0, " rpm"),
            "The configured upper engine-speed reference. The ignition limiter may have its own setting in the engine script. High speed increases piston acceleration and valvetrain demands.", EnginePart::Crankshaft},
        {"ENGINE CORE", "Throttle opening", intakeCount > 0 ? number(throttleOpening(engine), 0, "%") : "N/A",
            "Throttle plate angle from closed toward fully open, including its idle opening. It controls restriction into the intake. Opening it usually raises manifold pressure; it is not a direct power percentage.", EnginePart::Throttle},
        {"AIR / FUEL", "Manifold pressure (MAP)", intakeCount > 0 ? number(engine->getManifoldPressure() / units::kPa, 1, " kPa abs") : "N/A",
            "Absolute pressure in the intake, averaged across plenums. About 101 kPa is standard atmosphere. A closed throttle often lowers MAP; opening it reduces that restriction. This is not a gauge-vacuum reading.", EnginePart::IntakeRunner},
        {"AIR / FUEL", "Air / fuel ratio", engine && afr > 0 ? number(afr, 2, ":1") : "N/A",
            "Mass of air per mass of fuel in the intake mixture. A larger number is leaner, a smaller number richer. The ideal ratio depends on fuel; this legacy readout uses the simulator's octane-based calculation. No fuel present means N/A.", EnginePart::IntakeRunner},
        {"AIR / FUEL", "Intake airflow", value(flow / units::scfm, 1, " scfm"),
            "Air entering the intakes, expressed as standard cubic feet per minute. Engine speed, throttle, valves and runner restrictions all affect it. Pressure-wave reversion can briefly reverse flow.", EnginePart::IntakeValve},
        {"AIR / FUEL", "Volumetric efficiency", engine && theoreticalFlow > 1e-3 ? number(100 * flow / theoreticalFlow, 1, "%") : "N/A",
            "Actual intake flow divided by the four-stroke displacement flow at 25 C and 1 atmosphere. Around 100% means that reference filling rate. Resonance or boost can exceed it. It is undefined near zero RPM.", EnginePart::IntakeRunner},
        {"AIR / FUEL", "Fuel used since reload", value(engine ? engine->getTotalVolumeFuelConsumed() / units::L : 0, 3, " L"),
            "Cumulative injected fuel volume since loading the engine or resetting consumption. This is fuel consumed, not fuel remaining in a tank. More airflow and richer mixtures generally increase consumption.", EnginePart::Throttle},
        {"PERFORMANCE / DYNO", "Brake torque", dyno ? number(torque / (metricTorque ? units::Nm : units::ft_lb), 1, metricTorque ? " Nm" : " lb-ft") : "DYNO OFF",
            "Filtered torque resisted by the dynamometer at the crankshaft. Enable D to measure it. Torque is twisting force; cylinder pressure, crank leverage and mechanical losses shape it. It is not wheel torque.", EnginePart::Crankshaft},
        {"PERFORMANCE / DYNO", "Brake power", dyno ? number(sim->getDynoPower() / (metricPower ? units::kW : units::hp), 1, metricPower ? " kW" : " hp") : "DYNO OFF",
            "Dyno torque multiplied by shaft angular speed. The same torque produces more power at higher RPM. Use D for the dynamometer and H for speed hold; power is not measured by free-revving an unloaded engine.", EnginePart::Crankshaft},
        {"PERFORMANCE / DYNO", "BMEP (from dyno)", dyno && engine->getDisplacement() > 0 ? number(4 * constants::pi * torque / engine->getDisplacement() / units::bar, 2, " bar") : "N/A",
            "Brake mean effective pressure: 4 pi times dyno torque divided by displacement for a four-stroke engine. It compares torque relative to engine size. It is an equivalent cycle average, not instantaneous cylinder pressure.", EnginePart::CombustionChamber},
        {"TEMPERATURE / PRESSURE", "Intake gas temperature", intakeCount > 0 ? number(intakeTemperature - units::K0, 1, " C") : "N/A",
            "Mean gas temperature across intake plenums. Cooler gas is denser at the same pressure. This is the simulation's intake gas state, not a modeled coolant or oil sensor.", EnginePart::IntakeRunner},
        {"TEMPERATURE / PRESSURE", "Chamber gas" + cylinder, chamber ? number(chamber->m_system.temperature() - units::K0, 0, " C") : "N/A",
            "Instantaneous gas temperature in the selected cylinder, not head metal or coolant temperature. Compression and combustion raise it; expansion and gas exchange reduce it. It changes quickly through each cycle.", EnginePart::CombustionChamber},
        {"TEMPERATURE / PRESSURE", "Cylinder pressure" + cylinder, chamber ? number(chamber->m_system.pressure() / units::bar, 2, " bar abs") : "N/A",
            "Instantaneous absolute gas pressure in the selected cylinder. The pressure difference across the piston creates force. Compression and burning raise it; expansion lowers it. It is different from BMEP.", EnginePart::Piston},
        {"TEMPERATURE / PRESSURE", "Exhaust gas" + cylinder, chamber ? number(chamber->m_exhaustRunnerAndPrimary.temperature() - units::K0, 0, " C") : "N/A",
            "Instantaneous exhaust-runner gas temperature for the selected cylinder. Exhaust pulses make it fluctuate. It is a model gas state rather than a thermocouple measurement or exhaust-metal temperature.", EnginePart::ExhaustRunner},
        {"DRIVETRAIN", "Vehicle speed", sim && sim->getVehicle() ? number(std::abs(sim->getVehicle()->getSpeed()) / (metricSpeed ? units::km / units::hour : units::mile / units::hour), 1, metricSpeed ? " km/h" : " mph") : "N/A",
            "Road speed from the simulated vehicle. Gear ratios and clutch engagement connect crankshaft RPM to wheel speed, while drag and rolling resistance oppose motion. RPM can rise in neutral without vehicle movement.", EnginePart::Flywheel},
        {"DRIVETRAIN", "Selected gear", gear,
            "N is neutral; numbered gears connect the engine to the vehicle through different ratios. Lower gears multiply more torque. Use Up/Down to shift and Shift to disengage the clutch.", EnginePart::Flywheel},
        {"DRIVETRAIN", "Clutch engagement", sim && sim->getTransmission() ? number(sim->getTransmission()->getClutchPressure() * 100, 0, "%") : "N/A",
            "Applied clutch pressure connecting the flywheel to the gearbox. Zero releases the engine from the drivetrain; 100% requests full engagement. It can still slip when torque exceeds its configured capacity.", EnginePart::Flywheel},
        {"SIMULATION", "Physics frequency", sim ? number(sim->getSimulationFrequency() / 1000.0, 1, " kHz") : "N/A",
            "Physics steps per simulated second. Increasing it costs CPU time and can improve numerical resolution. Hold N and scroll to adjust; hold Space too for fine control. This is not engine RPM or audio sample rate.", EnginePart::None}
    };
}

float LearningPanel::renderTelemetry(float y, Engine *engine) {
    const std::vector<Metric> values = metrics(engine);
    m_selectedMetric = (std::max)(0, (std::min)((int)values.size() - 1, m_selectedMetric));
    const Metric &selected = values[m_selectedMetric];
    y = section("UNDERSTAND THE READING", y);
    text(selected.label, row(y, 26), 18, White);
    y -= 30;
    text(selected.value, row(y, 29), 25,
        selected.value == "N/A" || selected.value == "DYNO OFF" ? Muted : Accent);
    y -= 35;
    y = paragraph(selected.explanation, y) - 18;
    std::string group;
    for (size_t i = 0; i < values.size(); ++i) {
        const Metric &metric = values[i];
        if (group != metric.group) {
            y -= 7;
            y = section(metric.group, y);
            group = metric.group;
        }
        const Bounds metricBounds = row(y, 42);
        box(metricBounds, (int)i == m_selectedMetric ? 0x214840 : Card);
        const float labelSize = 12;
        text(metric.label, Bounds(m_width - 16, 16, {m_left + 8, y - 4}), labelSize, Muted);
        text(metric.value, Bounds(m_width - 16, 22, {m_left + 8, y - 18}), 18,
            metric.value == "N/A" || metric.value == "DYNO OFF" ? Muted : White, true);
        if (inClip(metricBounds)) m_hits.push_back({metricBounds, Action::Metric, (int)i});
        y -= 47;
    }
    return y;
}

float LearningPanel::renderControls(float y) {
    y = section("START HERE", y);
    y = paragraph("Click the engine view so the app has focus. Press A for ignition, then hold S until the engine starts. Release S. Hold W for a small throttle input, or R for full input.", y, White) - 16;
    const auto control = [this, &y](const char *keys, const char *description) {
        text(keys, row(y, 21), 14, White);
        y -= 22;
        y = paragraph(description, y) - 13;
    };
    y = section("ENGINE / DRIVETRAIN", y);
    control("A / hold S", "Toggle ignition / engage the starter motor.");
    control("Hold Q / W / E / R", "Speed-control input: 1% / 10% / 20% / 100%. The engine script maps this input to throttle.");
    control("Hold Space + scroll", "Fine throttle input. Keep Space held to retain the setting after releasing a throttle key; releasing Space returns the input to zero.");
    control("Up / Down", "Shift up / down. Neutral sits below first gear.");
    control("Shift / T / U / Y", "Disengage clutch / gradually release / gradually engage / hold clutch setting. Space smooths clutch changes.");
    y = section("DYNAMOMETER / ANALYSIS", y);
    control("D / H / G + scroll", "Toggle dyno / toggle RPM hold / change hold RPM. Torque and power appear in Live Data when the dyno is enabled.");
    control("Classic UI / F6 / Tab", "F6 switches between Engine Lab and the original dashboard. All original gauges, scopes, dyno analysis and audio mixer remain available. Tab cycles all four layouts, including Engine Lab.");
    y = section("VIEW / LEARNING", y);
    control("Mouse camera controls", "Left drag orbits; the wheel zooms. Right or middle drag pans. A short click selects a component. F1/F2 rotate left or right, and F3 resets orbit, zoom and pan. Scroll over this panel to read more.");
    control("F3 / F4 / F5", "Reset camera / switch 2D and 3D / toggle cylinder cutaway.");
    control("F7 / F8 / F9", "Toggle component labels / exploded spacing / persistent slow motion. These controls also have buttons in Engine Lab.");
    control("M / comma", "Increase / decrease the visible cylinder layer in the original 2D view.");
    control("Hold 1 / 2 / 3 / 4 / 5", "Simulation speed: 1/10, 1/100, 1/200, 1/500 or 1/1000. Slow motion keeps 1/40 speed active when no speed key is held. Slowing the physics also slows the synthesized sound.");
    control("P / Right", "Pause or resume simulation. While paused, Right advances one simulation frame. The Pause button does the same thing.");
    y = section("AUDIO / QUALITY", y);
    control("Hold Z / X / C + scroll", "Adjust volume / convolution amount / high-frequency gain. Add Space for fine changes.");
    control("Hold V / B / N + scroll", "Adjust air noise / input noise / physics frequency. Existing sound controls remain available.");
    y = section("ENGINE SCRIPTS / WINDOW", y);
    control("Enter", "Reload assets/main.mr. Select another engine by changing its script import and main call in that file. Reload restarts simulation and fuel counters.");
    control("F / Escape", "Toggle fullscreen / exit the app.");
    y = paragraph("The 3D shapes and exploded spacing are teaching aids generated from engine dimensions. They do not change the mechanics, airflow model, dyno, or sound synthesis.", y);
    return y;
}

void LearningPanel::onMouseDown(const Point &mouseLocal) {
    UiElement::onMouseDown(mouseLocal);
    m_pressedAt = mouseLocal;
}

void LearningPanel::onMouseClick(const Point &mouseLocal) {
    if ((mouseLocal - m_pressedAt).lengthSquared() > 36) return;
    for (const Hit &hit : m_hits) {
        if (!hit.bounds.overlaps(mouseLocal)) continue;
        LearningState &state = m_app->getLearningState();
        switch (hit.action) {
        case Action::Tab: setTab((Tab)hit.value); break;
        case Action::Part:
            state.selected = (EnginePart)hit.value;
            m_lastSelection = state.selected;
            m_scroll = 0;
            break;
        case Action::CylinderPrevious:
        case Action::CylinderNext: {
            Simulator *sim = m_app->getSimulator();
            Engine *engine = sim ? sim->getEngine() : nullptr;
            const int count = engine ? engine->getCylinderCount() : 0;
            if (count > 0) state.cylinder = (state.cylinder + count
                + (hit.action == Action::CylinderNext ? 1 : -1)) % count;
            break;
        }
        case Action::Pause: m_app->performAction(SimulatorAction::Pause); break;
        case Action::Slow: m_app->performAction(SimulatorAction::Slow); break;
        case Action::Labels: m_app->performAction(SimulatorAction::Labels); break;
        case Action::Explode: m_app->performAction(SimulatorAction::Exploded); break;
        case Action::Classic: m_app->performAction(SimulatorAction::Learn); break;
        case Action::Metric: {
            m_selectedMetric = hit.value;
            Simulator *sim = m_app->getSimulator();
            const auto readings = metrics(sim ? sim->getEngine() : nullptr);
            if (readings[m_selectedMetric].part != EnginePart::None)
                state.selected = readings[m_selectedMetric].part;
            m_scroll = 0;
            break;
        }
        }
        break;
    }
}

void LearningPanel::onMouseScroll(int mouseScroll) {
    // Keep the simulator's modifier-plus-wheel tuning gestures available here too.
    const ysKey::Code tuningKeys[] = {ysKey::Code::Space, ysKey::Code::Z, ysKey::Code::X,
        ysKey::Code::C, ysKey::Code::V, ysKey::Code::B, ysKey::Code::N, ysKey::Code::G};
    for (ysKey::Code key : tuningKeys) {
        if (m_app->getEngine()->IsKeyDown(key)) return;
    }
    // Windows wheel units vary by input backend; one notch should move three text lines.
    const float notches = std::abs(mouseScroll) >= 120 ? mouseScroll / 120.0f : (float)mouseScroll;
    m_scroll = (std::max)(0.0f, (std::min)(m_maxScroll, m_scroll - notches * 60.0f));
}
