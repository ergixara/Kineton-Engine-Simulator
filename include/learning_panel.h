#ifndef ATG_ENGINE_SIM_LEARNING_PANEL_H
#define ATG_ENGINE_SIM_LEARNING_PANEL_H

#include "ui_element.h"
#include "learning_content.h"

#include <string>
#include <vector>

class Engine;

// A native inspector. Its telemetry reads the same simulation as the classic gauges.
class LearningPanel : public UiElement {
    friend class EngineSimApplication; // Integration checks use the same rendered hit regions as input.
public:
    enum class Tab { Parts, Telemetry, Controls };

    LearningPanel();
    void initialize(EngineSimApplication *app) override;
    void update(float dt) override;
    void render() override;
    void onMouseDown(const Point &mouseLocal) override;
    void onMouseClick(const Point &mouseLocal) override;
    void onMouseScroll(int mouseScroll) override;

    void setTab(Tab tab);
    Tab getTab() const { return m_tab; }

private:
    enum class Action { Tab, Part, CylinderPrevious, CylinderNext,
        Pause, Slow, Labels, Explode, Classic, Metric };
    struct Hit { Bounds bounds; Action action; int value; };
    struct Metric {
        std::string group, label, value, explanation;
        EnginePart part;
    };

    Bounds row(float y, float height, float inset = 0) const;
    bool inClip(const Bounds &bounds) const;
    void box(const Bounds &bounds, unsigned int color);
    void text(const std::string &value, const Bounds &bounds, float size,
        unsigned int color, bool right = false);
    void button(const std::string &label, const Bounds &bounds,
        Action action, int value = 0, bool active = false);
    float paragraph(const std::string &value, float y, unsigned int color = 0xb8c8d8);
    float section(const std::string &label, float y);
    float renderParts(float y, Engine *engine);
    float renderTelemetry(float y, Engine *engine);
    float renderControls(float y);
    std::vector<Metric> metrics(Engine *engine) const;
    std::string shorten(const std::string &value, float width, float size) const;

    Tab m_tab = Tab::Parts;
    std::vector<Hit> m_hits;
    Bounds m_clip;
    float m_left = 0;
    float m_width = 0;
    float m_scroll = 0;
    float m_maxScroll = 0;
    int m_selectedMetric = 0;
    Point m_pressedAt;
    EnginePart m_lastSelection = EnginePart::Piston;
    int m_lastCylinder = 0;
};

#endif
