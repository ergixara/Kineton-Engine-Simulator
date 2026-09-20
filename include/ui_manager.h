#ifndef ATG_ENGINE_SIM_UI_MANAGER_H
#define ATG_ENGINE_SIM_UI_MANAGER_H

#include "ui_element.h"

#include <vector>

class EngineSimApplication;
class UiManager {
    public:
        UiManager();
        ~UiManager();

        void initialize(EngineSimApplication *app);
        void destroy();

        void update(float dt);
        void render();
        void cancelPointer();

        UiElement *getRoot() { return &m_root; }

    protected:
        UiElement m_root;

        UiElement *m_dragStart;
        UiElement *m_hover;
        Point m_mouse_p0;
        Point m_drag_p0;

        int m_lastMouseScroll;
        unsigned int m_inputResetSerial = 0;
        ysMouse::Button m_activeButton = ysMouse::Button::Count;
        bool m_dragged = false;

    protected:
        EngineSimApplication *m_app;
};

#endif /* ATG_ENGINE_SIM_UI_MANAGER_H */
