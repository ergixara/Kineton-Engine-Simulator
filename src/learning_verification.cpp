#include "../include/engine_sim_application.h"

#include <cmath>
#include <ostream>

// Opt-in native integration check. Input is delivered to the real hit regions after
// rendering, so this remains usable with Delta's hardware-only keyboard backend.
bool EngineSimApplication::verifyLearningFrame(int frame, std::ostream &log) {
    bool passed = true;
    const auto check = [&](bool condition, const char *name) {
        log << (condition ? "OK " : "FAIL ") << name << std::endl;
        passed &= condition;
    };
    const auto viewClick = [&](EngineView::Action action) {
        for (const auto &control : m_engineView->m_controls) {
            if (control.action != action) continue;
            const Point point = control.bounds.getPosition(Bounds::center);
            if (m_uiManager.getRoot()->mouseOver(point) != m_engineView) return false;
            m_engineView->onMouseDown(point);
            m_engineView->onMouseUp(point);
            m_engineView->onMouseClick(point);
            return true;
        }
        return false;
    };
    const auto panelClick = [&](LearningPanel::Action action, int value) {
        // A click may change the hit list on the next render, never during this loop.
        for (const auto &hit : m_learningPanel->m_hits) {
            if (hit.action != action || hit.value != value) continue;
            const Point point = hit.bounds.getPosition(Bounds::center);
            if (m_uiManager.getRoot()->mouseOver(point) != m_learningPanel) return false;
            m_learningPanel->onMouseDown(point);
            m_learningPanel->onMouseUp(point);
            m_learningPanel->onMouseClick(point);
            return true;
        }
        return false;
    };
    static double pausedAngle = 0;
    if (frame == 245) {
        check(!m_engineView->m_labels.empty(), "projected component labels exist");
        if (!m_engineView->m_labels.empty()) {
            const auto label = m_engineView->m_labels.back();
            const Point point = label.bounds.getPosition(Bounds::center);
            m_engineView->onMouseDown(point);
            m_engineView->onMouseUp(point);
            m_engineView->onMouseClick(point);
            check(m_learning.selected == label.part, "label opens matching part lesson");
        }
    }
    if (frame == 250) {
        bool hit = false;
        const auto &b = m_engineView->m_bounds;
        for (int y = 3; y <= 7 && !hit; ++y)
            for (int x = 3; x <= 7 && !hit; ++x)
                hit = selectEnginePart({b.left()+b.width()*x/10, b.bottom()+b.height()*y/10});
        check(hit && m_learning.selected != EnginePart::None, "screen ray selects live mesh component");
    }
    if (frame == 255) {
        check(viewClick(EngineView::Action::Exploded) && m_learning.exploded, "exploded view button");
        check(viewClick(EngineView::Action::Pause) && m_paused, "pause button");
        pausedAngle = m_iceEngine->getOutputCrankshaft()->m_body.theta;
    }
    if (frame == 260) {
        check(m_iceEngine->getOutputCrankshaft()->m_body.theta == pausedAngle, "pause freezes physics");
        check(viewClick(EngineView::Action::Slow) && m_learning.slowMotion, "slow motion button");
        check(viewClick(EngineView::Action::Pause) && !m_paused, "resume button");
    }
    if (frame == 270) {
        // Exercise the existing handlers without changing subsequent reference captures.
        auto *view = m_engineView;
        const auto learning = m_learning;
        const auto tab = m_learningPanel->getTab();
        const int screen = m_screen;
        const bool mode = view->is3d(), cutaway = view->m_cutaway, paused = m_paused;
        const bool ignition = m_iceEngine->getIgnitionModule()->m_enabled;
        const bool pointerStarter = m_starterHeld, keyboardStarter = m_keyboardStarterHeld;
        const bool starterEnabled = m_simulator->m_starterMotor.m_enabled;
        const float yaw = view->m_yaw, pitch = view->m_pitch, zoom = view->m_zoom3d;
        const auto pan = view->m_pan3d;
        const Point pan2d = view->m_pan;
        const float zoom2d = view->m_zoom, displayAngle = m_displayAngle;
        const auto state = [&](SimulatorAction action) {
            switch (action) {
            case SimulatorAction::Mode: return view->is3d();
            case SimulatorAction::Cutaway: return view->m_cutaway;
            case SimulatorAction::Learn: return isLearningWorkspace();
            case SimulatorAction::Labels: return m_learning.labels;
            case SimulatorAction::Exploded: return m_learning.exploded;
            case SimulatorAction::Ignition: return m_iceEngine->getIgnitionModule()->m_enabled;
            case SimulatorAction::Pause: return m_paused;
            case SimulatorAction::Slow: return m_learning.slowMotion;
            default: return false;
            }
        };
        const auto activeMatches = [&](SimulatorAction action) {
            view->layoutControls();
            for (const auto &control : view->m_controls)
                if (control.action == action) return control.active == state(action);
            return false;
        };
        bool synchronized = true;
        for (const auto action : {SimulatorAction::Mode, SimulatorAction::Cutaway,
                SimulatorAction::Learn, SimulatorAction::Labels, SimulatorAction::Exploded,
                SimulatorAction::Ignition, SimulatorAction::Pause, SimulatorAction::Slow}) {
            view->layoutControls();
            const bool before = state(action);
            synchronized &= viewClick(action) && state(action) != before && activeMatches(action);
            performAction(action, true, ActionSource::Keyboard);
            synchronized &= state(action) == before && activeMatches(action);
            performAction(action, true, ActionSource::Keyboard);
            synchronized &= state(action) != before && activeMatches(action);
            synchronized &= viewClick(action) && state(action) == before && activeMatches(action);
        }
        check(synchronized, "all toggle buttons and keyboard actions remain synchronized with active styling");

        bool sidebarSynchronized = true;
        const LearningPanel::Action panelActions[] = {LearningPanel::Action::Labels,
            LearningPanel::Action::Explode, LearningPanel::Action::Slow, LearningPanel::Action::Pause};
        const SimulatorAction actions[] = {SimulatorAction::Labels, SimulatorAction::Exploded,
            SimulatorAction::Slow, SimulatorAction::Pause};
        for (int i = 0; i < 4; ++i) {
            const bool before = state(actions[i]);
            sidebarSynchronized &= panelClick(panelActions[i], 0) && state(actions[i]) != before;
            performAction(actions[i], true, ActionSource::Keyboard);
            sidebarSynchronized &= state(actions[i]) == before && view->m_yaw == yaw
                && view->m_pitch == pitch && (view->m_pan3d - pan).length() < 1e-6f;
        }
        check(sidebarSynchronized, "sidebar toggles share keyboard actions without changing the camera");

        if (m_engine.GetGameWindow()->IsActive()) {
            Point starterPoint;
            bool foundStarter = false;
            for (const auto &control : view->m_controls) if (control.action == SimulatorAction::Starter) {
                starterPoint = control.bounds.getPosition(Bounds::center);
                foundStarter = true;
            }
            check(foundStarter, "starter has a visible hit region");
            if (foundStarter) {
                performAction(SimulatorAction::Starter, false, ActionSource::Keyboard);
                view->onMouseDown(starterPoint);
                check(m_starterHeld && m_simulator->m_starterMotor.m_enabled,
                    "Hold to start activates on mouse press");
                performAction(SimulatorAction::Starter, true, ActionSource::Keyboard);
                view->onMouseUp(starterPoint);
                check(!m_starterHeld && m_keyboardStarterHeld && m_simulator->m_starterMotor.m_enabled,
                    "releasing pointer retains the keyboard starter hold");
                performAction(SimulatorAction::Starter, false, ActionSource::Keyboard);
                check(!m_simulator->m_starterMotor.m_enabled, "releasing both sources stops the starter");
                view->onMouseDown(starterPoint);
                performAction(SimulatorAction::Starter, true, ActionSource::Keyboard);
                performAction(SimulatorAction::Starter, false, ActionSource::Keyboard);
                check(m_starterHeld && !m_keyboardStarterHeld && m_simulator->m_starterMotor.m_enabled,
                    "releasing keyboard retains the pointer starter hold");
                view->cancelInteraction();
                check(!m_starterHeld && !m_simulator->m_starterMotor.m_enabled,
                    "cancelling a pointer gesture releases its starter hold");
            }
        }
        else log << "SKIP held-starter overlap check: simulator window is not active" << std::endl;

        // Every UI control owns its gesture, even after a pointer leaves and returns.
        bool controlsIsolated = true;
        const auto controls = view->m_controls;
        for (const auto &control : controls) {
            const Point point = control.bounds.getPosition(Bounds::center);
            const bool before = state(control.action);
            view->onMouseDown(point);
            view->onDrag({}, point, point + Point(45, 22));
            view->onDrag({}, point, point);
            view->onMouseUp(point);
            view->onMouseClick(point);
            controlsIsolated &= view->m_yaw == yaw && view->m_pitch == pitch
                && (view->m_pan3d - pan).length() < 1e-6f && state(control.action) == before;
        }
        check(controlsIsolated, "dragging any viewport button never orbits or activates its click action");
        check(!view->isCameraSurface(m_learningPanel->m_bounds.getPosition(Bounds::center)),
            "learning sidebar is excluded from the camera surface");

        Point meshPoint;
        EnginePart hitPart = EnginePart::None;
        const auto &bounds = view->m_bounds;
        for (int y = 4; y <= 16 && hitPart == EnginePart::None; ++y) {
            for (int x = 4; x <= 16 && hitPart == EnginePart::None; ++x) {
                const Point candidate(bounds.left() + bounds.width()*x/20,
                    bounds.bottom() + bounds.height()*y/20);
                if (!view->isCameraSurface(candidate) || !selectEnginePart(candidate)) continue;
                meshPoint = candidate;
                hitPart = m_learning.selected;
            }
        }
        check(hitPart != EnginePart::None, "an unobstructed live mesh click target exists");
        if (hitPart != EnginePart::None) {
            m_learning.selected = EnginePart::None;
            view->onMouseDown(meshPoint);
            view->onDrag({}, meshPoint, meshPoint + Point(2, 1));
            view->onMouseUp(meshPoint);
            view->onMouseClick(meshPoint);
            check(m_learning.selected == hitPart && view->m_yaw == yaw && view->m_pitch == pitch,
                "small pointer movement preserves component selection without orbiting");
            m_learning.selected = EnginePart::None;
            view->onMouseDown(meshPoint);
            view->onDrag({}, meshPoint, meshPoint + Point(45, 22));
            check(view->m_yaw != yaw && view->m_pitch != pitch, "viewport drag changes camera yaw and pitch");
            view->onDrag({}, meshPoint, meshPoint);
            view->onMouseUp(meshPoint);
            view->onMouseClick(meshPoint);
            check(m_learning.selected == EnginePart::None && view->m_dragged,
                "drag threshold stays latched after returning to the press position");

            view->onMouseDown(meshPoint);
            view->onDrag({}, meshPoint, meshPoint + Point(0, 10000));
            const bool lowerPitchBound = view->m_pitch >= -1.35f;
            view->onDrag({}, meshPoint, meshPoint + Point(0, -10000));
            check(lowerPitchBound && view->m_pitch <= 1.35f, "orbit pitch cannot flip the camera upside down");
            view->onMouseUp(meshPoint);
            view->m_yaw = yaw; view->m_pitch = pitch;

            view->beginPan(meshPoint);
            const Point displacement(35, -20);
            view->dragPan(meshPoint + displacement);
            const auto expected = cameraPanDisplacement(displacement);
            check(expected.length() > 0 && (view->m_pan3d - pan - expected).length() < 1e-6f
                && std::abs(expected.dot(m_cameraForward3d)) < 1e-6f,
                "pan follows the camera plane at the current zoom");
        }

        view->onMouseScroll(120);
        check(view->m_zoom3d > zoom, "wheel zooms toward the engine");
        view->onMouseScroll(100000);
        check(view->m_zoom3d <= 2.3f, "zoom has a near limit");
        view->onMouseScroll(-100000);
        check(view->m_zoom3d >= 0.3f, "zoom has a far limit");
        view->m_yaw = 1.2f; view->m_pitch = -0.8f; view->m_zoom3d = 1.7f;
        view->m_pan3d = {0.2f, 0.3f, 0.1f};
        view->m_pan = {4, 7}; view->m_zoom = 2; m_displayAngle = 0.7f;
        check(viewClick(SimulatorAction::Reset) && view->m_yaw == -0.7f && view->m_pitch == 0.5f
            && view->m_zoom3d == 1 && view->m_pan3d.length() == 0 && view->m_zoom == 1
            && m_displayAngle == 0, "Reset button restores camera rotation zoom pan and 2D display angle");
        const Point resetPan2d = view->m_pan;
        view->m_yaw = 0; view->m_pitch = 1; view->m_zoom3d = 0.4f;
        view->m_pan3d = {1, 1, 1}; view->m_pan = {8, 9}; view->m_zoom = 3; m_displayAngle = 1;
        performAction(SimulatorAction::Reset, true, ActionSource::Keyboard);
        check(view->m_yaw == -0.7f && view->m_pitch == 0.5f && view->m_zoom3d == 1
            && view->m_pan3d.length() == 0 && (view->m_pan-resetPan2d).lengthSquared() == 0
            && view->m_zoom == 1 && m_displayAngle == 0, "F3 action restores exactly the same camera as Reset click");

        m_learning = learning; m_screen = screen;
        m_learningPanel->setTab(tab);
        view->set3d(mode); view->m_cutaway = cutaway; setPaused(paused);
        m_iceEngine->getIgnitionModule()->m_enabled = ignition;
        view->m_yaw = yaw; view->m_pitch = pitch; view->m_zoom3d = zoom; view->m_pan3d = pan;
        view->m_pan = pan2d; view->m_zoom = zoom2d; m_displayAngle = displayAngle;
        view->cancelInteraction();
        m_starterHeld = pointerStarter; m_keyboardStarterHeld = keyboardStarter;
        m_simulator->m_starterMotor.m_enabled = starterEnabled;
        view->layoutControls();
    }
    if (frame == 275) check(std::abs(m_simulator->getSimulationSpeed()-0.025)<1e-9, "slow motion reaches physics at 1/40");
    if (frame == 290) check(viewClick(EngineView::Action::Labels) && !m_learning.labels, "labels toggle off");
    if (frame == 301) check(m_engineView->m_labels.empty(), "hidden labels have no click regions");
    if (frame == 305) {
        check(viewClick(EngineView::Action::Labels) && m_learning.labels, "labels toggle on");
        check(panelClick(LearningPanel::Action::Tab, (int)LearningPanel::Tab::Telemetry)
            && m_learningPanel->getTab()==LearningPanel::Tab::Telemetry, "Live Data tab");
    }
    if (frame == 310) check(panelClick(LearningPanel::Action::Metric, 3)
        && m_learningPanel->m_selectedMetric==3 && m_learning.selected==EnginePart::IntakeRunner,
        "MAP row opens explanation and highlights intake");
    if (frame == 325) m_learningPanel->onMouseScroll(-2400);
    if (frame == 330) check(panelClick(LearningPanel::Action::Metric, 17)
        && m_learningPanel->m_selectedMetric==17 && m_learningPanel->m_scroll==0,
        "lower drivetrain metric scrolls back to explanation");
    if (frame == 345) check(panelClick(LearningPanel::Action::Tab, (int)LearningPanel::Tab::Controls)
        && m_learningPanel->getTab()==LearningPanel::Tab::Controls, "Controls tab");
    if (frame == 350) m_learningPanel->onMouseScroll(-720);
    if (frame == 360) {
        m_engine.GetGameWindow()->SetWindowSize(1280, 720);
        m_learningPanel->setTab(LearningPanel::Tab::Parts);
        m_engineView->resetCamera();
        m_learning.selected=EnginePart::Piston;
    }
    if (frame == 380) {
        check(m_learningPanel->m_maxScroll>0, "compact inspector scrolls overflowing lessons");
        m_learningPanel->onMouseScroll(-2400);
    }
    if (frame == 385) check(panelClick(LearningPanel::Action::Part, (int)EnginePart::Throttle)
        && m_learning.selected==EnginePart::Throttle, "compact component list selects throttle");
    if (frame == 400) {
        check(panelClick(LearningPanel::Action::Classic, 0) && m_screen==0, "Classic UI button preserves original dashboard");
        m_engineView->set3d(false);
        m_learning.slowMotion=false;
    }
    if (frame == 420) m_screen=1;
    if (frame == 440) m_screen=2;
    if (frame == 460) {
        m_screen=3;
        m_engineView->set3d(true);
        m_learning.exploded=false;
        m_learningPanel->setTab(LearningPanel::Tab::Telemetry);
        m_simulator->m_dyno.m_enabled=true;
    }
    if (frame == 470) {
        const auto readings=m_learningPanel->metrics(m_iceEngine);
        check(readings[8].value!="DYNO OFF" && readings[9].value!="DYNO OFF"
            && readings[10].value!="N/A", "dyno torque power and BMEP remain available");
        m_learningPanel->m_selectedMetric=8;
    }
    return passed;
}
