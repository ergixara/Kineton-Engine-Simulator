#include "../include/ui_manager.h"

#include "../include/engine_sim_application.h"

UiManager::UiManager() {
    m_app = nullptr;
    m_dragStart = nullptr;
    m_hover = nullptr;
    m_lastMouseScroll = 0;
}

UiManager::~UiManager() {
    /* void */
}

void UiManager::initialize(EngineSimApplication *app) {
    m_app = app;
    m_root.initialize(app);
}

void UiManager::destroy() {
    m_root.destroy();
    m_hover = nullptr;
    m_dragStart = nullptr;
    m_app = nullptr;
    m_activeButton = ysMouse::Button::Count;
    m_dragged = false;
}

void UiManager::update(float dt) {
    m_root.update(dt);
    auto *engine=m_app->getEngine();
    const auto resetSerial=engine->GetInputResetSerial();
    if(resetSerial!=m_inputResetSerial || !engine->GetGameWindow()->IsActive()) {
        cancelPointer();
        m_inputResetSerial=resetSerial;
    }
    int mouse_x=0,mouse_y=0;
    engine->GetOsMousePos(&mouse_x,&mouse_y);
    const Point mousePos{(float)mouse_x,(float)mouse_y};
    const auto hoverAt=[&](const Point &point) {
        UiElement *next=m_root.mouseOver(point);
        if(next!=m_hover) {
            if(m_hover) m_hover->onMouseLeave();
            if(next) next->onMouseOver(next->worldToLocal(point));
            m_hover=next;
        }
        return next;
    };
    const auto dragTo=[&](const Point &point) {
        if(!m_dragStart) return;
        m_dragged=m_dragged || (point-m_mouse_p0).lengthSquared()>25;
        if(m_activeButton==ysMouse::Button::Left)
            m_dragStart->onDrag(m_drag_p0,m_mouse_p0,point);
        else if(auto *view=dynamic_cast<EngineView *>(m_dragStart))
            view->dragPan(view->worldToLocal(point));
    };
    ysMouse::ButtonEvent event;
    while(engine->PollMouseButtonEvent(&event)) {
        if(!engine->GetGameWindow()->IsActive()) continue;
        const Point point{(float)event.x,(float)event.y};
        UiElement *target=hoverAt(point);
        if(event.button==ysMouse::Button::Count) {
            dragTo(point);
            continue;
        }
        if(event.state==ysMouse::ButtonState::DownTransition) {
            if(m_activeButton!=ysMouse::Button::Count) continue;
            if(event.button!=ysMouse::Button::Left) {
                auto *view=dynamic_cast<EngineView *>(target);
                if((event.button!=ysMouse::Button::Right && event.button!=ysMouse::Button::Middle)
                    || !view || !view->isCameraSurface(view->worldToLocal(point))) continue;
                view->beginPan(view->worldToLocal(point));
            }
            m_activeButton=event.button; m_dragStart=target; m_mouse_p0=point; m_dragged=false;
            if(target) {
                m_drag_p0=target->getLocalPosition();
                if(event.button==ysMouse::Button::Left) target->onMouseDown(target->worldToLocal(point));
            }
        }
        else if(event.state==ysMouse::ButtonState::UpTransition && event.button==m_activeButton) {
            dragTo(point);
            if(m_dragStart && m_activeButton==ysMouse::Button::Left) {
                m_dragStart->onMouseUp(m_dragStart->worldToLocal(point));
                if(!m_dragged && target==m_dragStart && m_dragStart->isVisible())
                    m_dragStart->onMouseClick(m_dragStart->worldToLocal(point));
            }
            m_dragStart=nullptr; m_activeButton=ysMouse::Button::Count;
        }
    }
    hoverAt(mousePos);
    if(engine->GetGameWindow()->IsActive()) dragTo(mousePos);
    const int newMouseScroll = engine->GetMouseWheel();
    if (m_lastMouseScroll != newMouseScroll) {
        auto *view=dynamic_cast<EngineView *>(m_hover);
        if (engine->GetGameWindow()->IsActive() && m_hover != nullptr
            && (!view || view->isCameraSurface(view->worldToLocal(mousePos)))) {
            m_hover->onMouseScroll(newMouseScroll - m_lastMouseScroll);
        }

        m_lastMouseScroll = newMouseScroll;
    }

}

void UiManager::cancelPointer() {
    if(m_dragStart) {
        if(auto *view=dynamic_cast<EngineView *>(m_dragStart)) view->cancelInteraction();
        else m_dragStart->onMouseUp(m_dragStart->worldToLocal(m_mouse_p0));
    }
    m_dragStart=nullptr; m_activeButton=ysMouse::Button::Count; m_dragged=false;
    m_app->setStarterHeld(false);
}

void UiManager::render() {
    m_root.render();
}
