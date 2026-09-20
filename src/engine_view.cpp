#include "../include/engine_view.h"
#include "../include/engine_sim_application.h"
#include <algorithm>

EngineView::EngineView() {
    m_pan = {0, static_cast<float>(units::distance(-6, units::inch))};
    m_checkMouse = true;
    m_lastScroll = 0;
    m_zoom = 1;
    m_drawFrame = true;
}
EngineView::~EngineView() {}

void EngineView::layoutControls() {
    m_controls.clear();
    const float left=m_bounds.left()+18, top=m_bounds.top();
    const float width=(std::min)(100.0f,(m_bounds.width()-60)/4);
    const auto add=[&](int column,float y,Action action,const std::string &label,bool active=false) {
        m_controls.push_back({Bounds(width,30,{left+column*(width+8),y}),action,label,active});
    };
    add(0,top-67,Action::Mode,m_3d?"3D view  F4":"2D view  F4",m_3d);
    add(1,top-67,Action::Reset,"Reset  F3");
    if(m_3d) add(2,top-67,Action::Cutaway,m_cutaway?"Cutaway  F5":"Solid  F5",m_cutaway);
    add(3,top-67,Action::Learn,"Learn  F6",m_app->isLearningWorkspace());
    auto &learn=m_app->getLearningState();
    if(m_3d && m_app->isLearningWorkspace()) {
        add(0,top-105,Action::Labels,"Labels  F7",learn.labels);
        add(1,top-105,Action::Exploded,"Explode  F8",learn.exploded);
    }
    // The compact classic dashboard already provides engine controls and needs
    // its small viewport for the mechanism itself.
    if(!m_app->isLearningWorkspace() && m_bounds.height()<480) return;
    auto *engine=m_app->getSimulator()->getEngine();
    const bool ignition=engine && engine->getIgnitionModule()->m_enabled;
    add(0,m_bounds.bottom()+80,Action::Ignition,ignition?"Ignition ON":"Ignition OFF",ignition);
    add(1,m_bounds.bottom()+80,Action::Starter,"Hold to start",m_app->getSimulator()->m_starterMotor.m_enabled);
    add(2,m_bounds.bottom()+80,Action::Pause,m_app->isPaused()?"Resume  P":"Pause  P",m_app->isPaused());
    add(3,m_bounds.bottom()+80,Action::Slow,"Slow  F9",learn.slowMotion);
}

void EngineView::update(float dt) {
    m_mouseBounds=m_bounds;
    layoutControls();
}

bool EngineView::onControl(const Point &p) const {
    for(const auto &control:m_controls) if(control.bounds.overlaps(p)) return true;
    return false;
}

void EngineView::render() {
    layoutControls();
    const auto bg=ysColor::srgbiToLinear(0x111C28), divider=ysColor::srgbiToLinear(0x314456);
    if(m_drawFrame) drawFrame(m_bounds,1,divider,bg,false);
    auto *engine=m_app->getSimulator()->getEngine();
    // TextRenderer expects display-space colors.
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(0xEAF3F8)));
    drawText(engine?engine->getName():"No engine loaded",Bounds(m_bounds.width()-36,30,{m_bounds.left()+18,m_bounds.top()-32}),27);
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(0xA7BACB)));
    drawText(m_app->isLearningWorkspace()?"ENGINE LAB  /  Explore how motion becomes power":"SIMULATOR  /  Tab: layouts   F6: Learning workspace",
        Bounds(m_bounds.width()-36,18,{m_bounds.left()+18,m_bounds.top()-52}),13);
    for(const auto &control:m_controls) {
        drawFrame(control.bounds,1,control.active?ysColor::srgbiToLinear(0x60DDCA):divider,
            ysColor::srgbiToLinear(control.active?0x214840:0x1B2B3B));
        m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(control.active?0x60DDCA:0xEAF3F8)));
        drawAlignedText(control.text,control.bounds,14,Bounds::center,Bounds::center);
    }
    m_labels.clear();
    if(m_3d && m_app->isLearningWorkspace() && m_app->getLearningState().labels && engine) renderLabels();
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(0xA7BACB)));
    const float y=m_bounds.bottom();
    const bool compact=!m_app->isLearningWorkspace() && m_bounds.height()<480;
    drawText(m_3d?"Click a part to inspect  |  Drag to orbit  |  Wheel to zoom":"Drag to pan  |  Wheel to zoom  |  F4: return to 3D",
        Bounds(m_bounds.width()-36,18,{m_bounds.left()+18,y+(compact?13:32)}),compact?12:14);
    if(!compact) drawText(m_3d?"Cyan: intake   Orange: exhaust   Gold: selection / ignition":"The original gauges, analysis and mixer remain available with Tab.",
        Bounds(m_bounds.width()-36,16,{m_bounds.left()+18,y+13}),12);
    if(!engine) drawCenteredText("Fix assets/main.mr, then press Enter to reload.",m_bounds,19,Bounds::center);
    if(!m_app->getScriptError().empty()) {
        m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(ysColor::srgbiToLinear(0xE89867)));
        drawText("Reload failed - check error_log.log, fix main.mr, then press Enter.",
            Bounds(m_bounds.width()-36,20,{m_bounds.left()+18,m_bounds.top()-147}),14);
    }
    m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(m_app->getForegroundColor()));
}

void EngineView::renderLabels() {
    const auto &learn=m_app->getLearningState();
    const EnginePart parts[]={learn.selected,EnginePart::Piston,EnginePart::ConnectingRod,EnginePart::Crankshaft,
        EnginePart::IntakeValve,EnginePart::ExhaustValve,EnginePart::Camshaft};
    for(const auto part:parts) {
        bool duplicate=false; for(const auto &l:m_labels) if(l.part==part) duplicate=true;
        if(duplicate || part==EnginePart::None) continue;
        Point anchor;
        if(!m_app->getPartAnchor(part,learn.cylinder,&anchor)) continue;
        const auto &lesson=partLesson(part);
        const float width=m_app->getTextRenderer()->CalculateWidth(lesson.name,14)+20;
        const float x=anchor.x<m_bounds.center_h()?anchor.x-width-35:anchor.x+35;
        float y=std::clamp(anchor.y+13,m_bounds.bottom()+130,m_bounds.top()-165);
        Bounds box(width,26,{std::clamp(x,m_bounds.left()+18,m_bounds.right()-width-18),y});
        for(int retry=0;retry<8;++retry) {
            bool overlap=false;
            for(const auto &l:m_labels) if(box.left()<l.bounds.right()+8 && box.right()>l.bounds.left()-8
                && box.bottom()<l.bounds.top()+8 && box.top()>l.bounds.bottom()-8) overlap=true;
            if(!overlap) break;
            box.move({0,-34});
        }
        if(box.bottom()<m_bounds.bottom()+110) continue;
        const bool selected=part==learn.selected;
        const auto color=ysColor::srgbiToLinear(selected?0xFFE0A0:lesson.color);
        auto *gen=m_app->getGeometryGenerator();
        const Point p=getRenderPoint(anchor),q=getRenderPoint(box.getPosition(anchor.x<box.left()?Bounds::lm:Bounds::rm));
        GeometryGenerator::Line2dParameters line{};
        line.x0=p.x;line.y0=p.y;line.x1=q.x;line.y1=q.y;line.lineWidth=1;
        GeometryGenerator::GeometryIndices indices;
        gen->startShape();gen->generateLine2d(line);
        GeometryGenerator::Circle2dParameters dot{};dot.center_x=p.x;dot.center_y=p.y;dot.radius=3;dot.maxEdgeLength=2;
        gen->generateCircle2d(dot);gen->endShape(&indices);
        resetShader();m_app->getShaders()->SetBaseColor(color);m_app->drawGeneratedUi(indices,0x11);
        drawFrame(box,1,color,ysColor::srgbiToLinear(0x162432));
        m_app->getTextRenderer()->SetColor(ysColor::linearToSrgb(color));
        drawAlignedText(lesson.name,box,14,Bounds::center,Bounds::center);
        m_labels.push_back({box,part});
    }
}

void EngineView::onMouseDown(const Point &mouseLocal) {
    UiElement::onMouseDown(mouseLocal);
    m_dragStart=m_pan; m_mouseDown=mouseLocal;m_dragYaw=m_yaw;m_dragPitch=m_pitch;
    m_dragged=false; m_pressedControl=false; m_pressedLabel=false;
    for(const auto &control:m_controls) if(control.bounds.overlaps(mouseLocal)) {
        m_pressedControl=true; m_pressedAction=control.action;
        if(control.action==Action::Starter) m_app->performAction(Action::Starter,true);
        break;
    }
    if(!m_pressedControl) for(const auto &label:m_labels) if(label.bounds.overlaps(mouseLocal)) {
        m_pressedLabel=true; m_pressedPart=label.part; break;
    }
}
void EngineView::onMouseUp(const Point &mouseLocal) {
    UiElement::onMouseUp(mouseLocal);m_app->performAction(Action::Starter,false);
}
void EngineView::onDrag(const Point &p0,const Point &mouse0,const Point &mouse) {
    const Point delta=mouse-mouse0;
    m_dragged = m_dragged || delta.lengthSquared()>25;
    if(!m_dragged || m_pressedControl || m_pressedLabel) return;
    if(m_3d) {
        m_yaw=m_dragYaw-delta.x*0.006f;
        m_pitch=std::clamp(m_dragPitch-delta.y*0.006f,-1.35f,1.35f);
    } else m_pan=m_dragStart+Point(m_app->pixelsToUnits(delta.x),m_app->pixelsToUnits(delta.y));
}
void EngineView::onMouseScroll(int scroll) {
    for(const auto key:{ysKey::Code::Z,ysKey::Code::X,ysKey::Code::C,ysKey::Code::V,ysKey::Code::B,
        ysKey::Code::N,ysKey::Code::G,ysKey::Code::Space}) if(m_app->getEngine()->IsKeyDown(key)) return;
    const float f=std::pow(2.0f,static_cast<float>(scroll)/800);
    if(m_3d) {m_zoom3d=std::clamp(m_zoom3d*f,0.3f,2.3f);return;}
    const Point previous=getCenter();m_zoom=std::clamp(m_zoom*f,0.05f,20.0f);
    const Point diff=getCenter()-previous;m_pan+=diff*m_zoom;m_dragStart+=diff*m_zoom;
}
void EngineView::onMouseClick(const Point &mouseLocal) {
    if(m_dragged || (mouseLocal-m_mouseDown).lengthSquared()>25) return;
    for(const auto &control:m_controls) if(control.bounds.overlaps(mouseLocal)) {
        if(m_pressedControl && m_pressedAction==control.action && control.action!=Action::Starter)
            m_app->performAction(control.action);
        return;
    }
    if(m_pressedControl) return;
    for(const auto &label:m_labels) if(label.bounds.overlaps(mouseLocal)) {
        if(m_pressedLabel && m_pressedPart==label.part) m_app->openPartLesson(label.part);
        return;
    }
    if(m_3d && !m_pressedLabel) m_app->selectEnginePart(localToWorld(mouseLocal));
}
bool EngineView::isCameraSurface(const Point &point) const {
    if(!m_bounds.overlaps(point) || onControl(point)) return false;
    for(const auto &label:m_labels) if(label.bounds.overlaps(point)) return false;
    return true;
}
void EngineView::beginPan(const Point &point) {
    m_panStart=point; m_dragPan3d=m_pan3d; m_dragStart=m_pan;
}
void EngineView::dragPan(const Point &point) {
    const Point delta=point-m_panStart;
    if(m_3d) m_pan3d=m_dragPan3d+m_app->cameraPanDisplacement(delta);
    else m_pan=m_dragStart+Point(m_app->pixelsToUnits(delta.x),m_app->pixelsToUnits(delta.y));
}
void EngineView::cancelInteraction() {
    m_mouseHeld=false; m_dragged=true; m_pressedControl=false; m_pressedLabel=false;
    m_app->performAction(Action::Starter,false);
}
void EngineView::resetCamera() {
    m_yaw=-0.7f;m_pitch=0.5f;m_zoom3d=1;
    m_pan3d={};
    m_pan={0,static_cast<float>(units::distance(-6,units::inch))};m_zoom=1;
}
void EngineView::setBounds(const Bounds &bounds) {m_bounds=bounds;}
Point EngineView::getCenter() const {return getCameraPosition();}
Point EngineView::getCameraPosition() const {return Point(-m_pan/m_zoom);}
