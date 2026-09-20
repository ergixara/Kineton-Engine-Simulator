#ifndef ATG_ENGINE_SIM_ENGINE_VIEW_H
#define ATG_ENGINE_SIM_ENGINE_VIEW_H

#include "ui_element.h"
#include "learning_content.h"
#include "simulator_action.h"
#include "engine_mesh.h"

class EngineView : public UiElement {
    friend class EngineSimApplication; // Owner also exercises rendered hit regions in --verify-3d.
    public:
        EngineView();
        virtual ~EngineView();

        virtual void update(float dt);
        virtual void render();
        virtual void onMouseDown(const Point &mouseLocal);
        virtual void onMouseUp(const Point &mouseLocal);
        virtual void onDrag(const Point &p0, const Point &mouse0, const Point &mouse);
        virtual void onMouseScroll(int scroll);
        virtual void onMouseClick(const Point &mouseLocal);

        bool is3d() const { return m_3d; }
        void set3d(bool enabled) { m_3d = enabled; }
        void resetCamera();
        bool isCameraSurface(const Point &point) const;
        void beginPan(const Point &point);
        void dragPan(const Point &point);
        void cancelInteraction();
        EngineMesh::Vec3 m_pan3d;
        float m_yaw = -0.7f;
        float m_pitch = 0.5f;
        float m_zoom3d = 1.0f;
        bool m_cutaway = true;

        void setDrawFrame(bool drawFrame) { m_drawFrame = drawFrame; }
        void setBounds(const Bounds &bounds);

        Point getCenter() const;

        Point getCameraPosition() const;
        float m_zoom;
        
    protected:
        Point m_pan;
        Point m_dragStart;
        int m_lastScroll;
        bool m_drawFrame;
        bool m_3d = true;
        float m_dragYaw = 0, m_dragPitch = 0;
        Point m_mouseDown;
        Bounds m_modeButton, m_resetButton, m_cutButton;
        using Action = SimulatorAction;
        struct Control { Bounds bounds; Action action; std::string text; bool active; };
        struct Label { Bounds bounds; EnginePart part; };
        std::vector<Control> m_controls;
        std::vector<Label> m_labels;
        void layoutControls();
        void renderLabels();
        bool onControl(const Point &p) const;
        bool m_dragged = false;
        bool m_pressedControl = false, m_pressedLabel = false;
        Action m_pressedAction = Action::Mode;
        EnginePart m_pressedPart = EnginePart::None;
        Point m_panStart;
        EngineMesh::Vec3 m_dragPan3d;
};

#endif /* ATG_ENGINE_SIM_ENGINE_VIEW_H */
