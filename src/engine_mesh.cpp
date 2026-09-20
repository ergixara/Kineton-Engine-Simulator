#include "../include/engine_mesh.h"
#include "../include/engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    constexpr float Pi = 3.14159265359f;
    using V = EngineMesh::Vec3;
    // Stable cylinder frame, including cylinders parallel to the crankshaft.
    void frame(V axis, V &u, V &v) {
        u = (std::abs(axis.z) < 0.9f) ? V{0, 0, 1} : V{1, 0, 0};
        u = (u - axis * u.dot(axis)).normalized();
        v = axis.cross(u);
    }
    V bodyPoint(atg_scs::RigidBody &body, double x, double y, float z) {
        double wx, wy;
        body.localToWorld(x, y, &wx, &wy);
        return {static_cast<float>(wx), static_cast<float>(wy), z};
    }
}

float EngineMesh::Vec3::length() const { return std::sqrt(dot(*this)); }
EngineMesh::Vec3 EngineMesh::Vec3::normalized() const {
    const float l = length();
    return (l > 1e-8f) ? *this * (1.0f / l) : Vec3{};
}

void EngineMesh::clear() {
    for (auto &batch : vertices) batch.clear();
    setPart(EnginePart::None);
    const float inf = std::numeric_limits<float>::infinity();
    minimum = {inf, inf, inf}; maximum = {-inf, -inf, -inf};
}

EngineMesh::Hit EngineMesh::pick(V origin, V direction) const {
    Hit closest;
    if (!std::isfinite(origin.x + origin.y + origin.z)
        || !std::isfinite(direction.x + direction.y + direction.z)) return closest;
    direction = direction.normalized();
    if (direction.length() < 0.5f) return closest;
    // Two-sided Moller-Trumbore test: cutaway inner walls are selectable too.
    for (int material = 0; material < MaterialCount; ++material) {
        if (material == Grid) continue;
        const auto &batch = vertices[material];
        for (size_t i = 0; i + 2 < batch.size(); i += 3) {
            const auto &a = batch[i];
            if (a.part == EnginePart::None) continue;
            const V e1 = batch[i + 1].position - a.position;
            const V e2 = batch[i + 2].position - a.position;
            const V p = direction.cross(e2);
            const float determinant = e1.dot(p);
            if (std::abs(determinant) < 1e-12f) continue;
            const float inverse = 1.0f / determinant;
            const V t = origin - a.position;
            const float u = t.dot(p) * inverse;
            if (u < -1e-5f || u > 1.00001f) continue;
            const V q = t.cross(e1);
            const float v = direction.dot(q) * inverse;
            if (v < -1e-5f || u + v > 1.00001f) continue;
            const float distance = e2.dot(q) * inverse;
            if (distance > 1e-6f && distance < closest.distance) {
                closest = {a.part, a.cylinder, distance};
            }
        }
    }
    return closest;
}

bool EngineMesh::anchor(EnginePart part, int cylinder, V *position) const {
    if (position == nullptr || part == EnginePart::None) return false;
    const float inf = std::numeric_limits<float>::infinity();
    V lo{inf, inf, inf}, hi{-inf, -inf, -inf};
    bool found = false;
    int instance = cylinder;
    for (int material = 0; material < MaterialCount; ++material) {
        if (material == Grid) continue;
        for (const auto &vertex : vertices[material]) {
            if (vertex.part != part) continue;
            if (!found && cylinder < 0) instance = vertex.cylinder;
            if (vertex.cylinder != instance && vertex.cylinder >= 0) continue;
            found = true;
            const V p = vertex.position;
            lo = {std::min(lo.x,p.x), std::min(lo.y,p.y), std::min(lo.z,p.z)};
            hi = {std::max(hi.x,p.x), std::max(hi.y,p.y), std::max(hi.z,p.z)};
        }
    }
    if (found) *position = (lo + hi) * 0.5f;
    return found;
}

size_t EngineMesh::vertexCount() const {
    size_t count = 0;
    for (const auto &batch : vertices) count += batch.size();
    return count;
}

void EngineMesh::triangle(V a, V b, V c, V na, V nb, V nc, Material material) {
    // Consistent outward winding, regardless of primitive orientation.
    if ((b-a).cross(c-a).dot(na + nb + nc) < 0) {
        std::swap(b, c); std::swap(nb, nc);
    }
    auto &out = vertices[material];
    out.push_back({a, na, m_part, m_cylinder});
    out.push_back({b, nb, m_part, m_cylinder});
    out.push_back({c, nc, m_part, m_cylinder});
    for (const V p : {a, b, c}) {
        minimum = {std::min(minimum.x,p.x), std::min(minimum.y,p.y), std::min(minimum.z,p.z)};
        maximum = {std::max(maximum.x,p.x), std::max(maximum.y,p.y), std::max(maximum.z,p.z)};
    }
}

void EngineMesh::quad(V a, V b, V c, V d, V n, Material m) {
    triangle(a,b,c,n,n,n,m); triangle(a,c,d,n,n,n,m);
}

void EngineMesh::cylinder(V a, V b, float r, Material m, int segments) {
    if (r <= 0 || segments < 3 || (b-a).length() < 1e-7f) return;
    const V axis = (b-a).normalized();
    V u, v; frame(axis, u, v);
    for (int i = 0; i < segments; ++i) {
        const float t0 = 2*Pi*i/segments, t1 = 2*Pi*(i+1)/segments;
        const V n0 = u*std::cos(t0) + v*std::sin(t0);
        const V n1 = u*std::cos(t1) + v*std::sin(t1);
        const V p = a+n0*r, q = a+n1*r, s = b+n1*r, t = b+n0*r;
        triangle(p,q,s,n0,n1,n1,m); triangle(p,s,t,n0,n1,n0,m);
        triangle(a,q,p,axis*-1,axis*-1,axis*-1,m);
        triangle(b,t,s,axis,axis,axis,m);
    }
}

void EngineMesh::tube(V a, V b, float ri, float ro, Material m,
    float start, float sweep, int segments) {
    if (ri <= 0 || ro <= ri || segments < 3 || sweep <= 0 || (b-a).length() < 1e-7f) return;
    const V axis = (b-a).normalized();
    V u,v; frame(axis,u,v);
    for (int i = 0; i < segments; ++i) {
        const float t0 = start+sweep*i/segments, t1 = start+sweep*(i+1)/segments;
        const V n0 = u*std::cos(t0)+v*std::sin(t0), n1 = u*std::cos(t1)+v*std::sin(t1);
        const V p=a+n0*ro, q=a+n1*ro, s=b+n1*ro, t=b+n0*ro;
        const V ip=a+n0*ri, iq=a+n1*ri, is=b+n1*ri, it=b+n0*ri;
        triangle(p,q,s,n0,n1,n1,m); triangle(p,s,t,n0,n1,n0,m);
        triangle(ip,is,iq,n0*-1,n1*-1,n1*-1,m); triangle(ip,it,is,n0*-1,n0*-1,n1*-1,m);
        quad(p,ip,iq,q,axis*-1,m); quad(t,s,is,it,axis,m);
        if (sweep < 2*Pi-0.001f && i == 0) quad(p,t,it,ip,axis.cross(n0)*-1,m);
        if (sweep < 2*Pi-0.001f && i == segments-1) quad(q,iq,is,s,axis.cross(n1),m);
    }
}

void EngineMesh::beam(V a, V b, float width, float depth, Material m) {
    if (width <= 0 || depth <= 0 || (b-a).length() < 1e-7f) return;
    const V axis=(b-a).normalized(); V u,v; frame(axis,u,v);
    // Rods lie in XY; u is the crankshaft/depth direction.
    const V du=u*(depth*0.5f), dv=v*(width*0.5f);
    const V p=a+du+dv,q=a-du+dv,r=a-du-dv,s=a+du-dv;
    const V t=b+du+dv,w=b-du+dv,x=b-du-dv,y=b+du-dv;
    quad(p,q,r,s,axis*-1,m); quad(t,y,x,w,axis,m);
    quad(p,t,w,q,v,m); quad(r,x,y,s,v*-1,m);
    quad(p,s,y,t,u,m); quad(q,w,x,r,u*-1,m);
}

void EngineMesh::build(Engine *engine, bool cutaway, bool exploded) {
    clear();
    if (engine == nullptr || engine->getCylinderCount() == 0) {
        minimum = maximum = {}; return;
    }
    float bore = 0.05f;
    int maxLayer = 0;
    for (int i=0;i<engine->getCylinderCount();++i) {
        bore = std::max(bore, static_cast<float>(engine->getPiston(i)->getCylinderBank()->getBore()));
        maxLayer = std::max(maxLayer, engine->getConnectingRod(i)->getLayer());
    }
    const float pitch = bore*1.12f;
    const auto journalZ = [=](int layer) { return (layer-maxLayer*0.5f)*pitch; };
    const float rear = journalZ(0)-pitch*0.65f, front = journalZ(maxLayer)+pitch*0.65f;

    for (int k=0;k<engine->getCrankshaftCount();++k) {
        setPart(EnginePart::Crankshaft);
        auto *crank=engine->getCrankshaft(k);
        const V origin{static_cast<float>(crank->m_body.p_x),static_cast<float>(crank->m_body.p_y),0};
        cylinder(origin+V{0,0,rear},origin+V{0,0,front},bore*0.16f,Steel);
        for (int j=0;j<crank->getRodJournalCount();++j) {
            double x,y; crank->getRodJournalPositionLocal(j,&x,&y);
            const float z=journalZ(j);
            // The legacy global-journal helper only translates the initial pose.
            // Apply the live rigid-body rotation so pins stay attached to rods.
            const V pin=bodyPoint(crank->m_body,x,y,z);
            cylinder(pin+V{0,0,-bore*0.19f},pin+V{0,0,bore*0.19f},bore*0.14f,Steel);
            for (float side : {-1.0f,1.0f}) {
                const float webZ=z+side*bore*0.23f;
                const V main=origin+V{0,0,webZ}, end{pin.x,pin.y,webZ};
                beam(main,end,bore*0.35f,bore*0.11f,DarkSteel);
                cylinder(main+V{0,0,-bore*0.055f},main+V{0,0,bore*0.055f},bore*0.24f,DarkSteel);
            }
        }
        setPart(EnginePart::Flywheel);
        cylinder(origin+V{0,0,rear-bore*0.18f},origin+V{0,0,rear},bore*0.78f,DarkSteel,48);
        cylinder(origin+V{0,0,rear-bore*0.2f},origin+V{0,0,rear-bore*0.18f},bore*0.55f,Aluminium,48);
        // Flywheel bolts and a visible timing mark rotate with the actual shaft.
        for (int j=0;j<8;++j) {
            const float a=static_cast<float>(crank->m_body.theta)+j*Pi/4;
            const V p=origin+V{std::cos(a)*bore*0.4f,std::sin(a)*bore*0.4f,rear-bore*0.22f};
            cylinder(p,p+V{0,0,bore*0.025f},bore*0.045f,Brass,6);
        }
        const float angle=static_cast<float>(crank->m_body.theta);
        const V mark=origin+V{std::cos(angle)*bore*0.65f,std::sin(angle)*bore*0.65f,rear-bore*0.2f};
        cylinder(mark,mark+V{0,0,bore*0.03f},bore*0.07f,BankRed,12);
    }

    std::vector<Intake *> representedIntakes;
    for (int i=0;i<engine->getCylinderCount();++i) {
        auto *piston=engine->getPiston(i); auto *rod=piston->getRod(); auto *bank=piston->getCylinderBank();
        const float r=static_cast<float>(bank->getBore())*0.5f;
        float z=journalZ(rod->getLayer());
        int paired=0,preceding=0;
        for (int j=0;j<engine->getCylinderCount();++j) {
            auto *other=engine->getConnectingRod(j);
            if (other->getCrankshaft()==rod->getCrankshaft() && other->getLayer()==rod->getLayer()) {
                ++paired; if(j<i) ++preceding;
            }
        }
        z+=(preceding-(paired-1)*0.5f)*bore*0.22f;
        const V axis{static_cast<float>(bank->getDx()),static_cast<float>(bank->getDy()),0};
        const V side{axis.y,-axis.x,0};
        const V origin{static_cast<float>(bank->getX()),static_cast<float>(bank->getY()),z};
        const V deck=origin+axis*static_cast<float>(bank->getDeckHeight());
        const V pin=bodyPoint(piston->m_body,0,piston->getWristPinLocation(),z);
        const V crown=bodyPoint(piston->m_body,0,-piston->getCompressionHeight(),z);
        const V skirt=crown-axis*(r*0.95f);
        setPart(EnginePart::Piston, i);
        cylinder(skirt,crown,r*0.965f,Aluminium,32);
        for (float h : {0.12f,0.23f,0.34f})
            cylinder(crown-axis*(r*h),crown-axis*(r*(h+0.035f)),r*0.986f,DarkSteel);
        cylinder(pin+V{0,0,-r*0.97f},pin+V{0,0,r*0.97f},r*0.13f,Steel);

        setPart(EnginePart::ConnectingRod, i);
        const V big=bodyPoint(rod->m_body,0,rod->getBigEndLocal(),z);
        const V small=bodyPoint(rod->m_body,0,rod->getLittleEndLocal(),z);
        beam(big,small,r*0.32f,r*0.2f,Aluminium);
        beam(big,small,r*0.12f,r*0.24f,Steel);
        tube(big+V{0,0,-r*0.18f},big+V{0,0,r*0.18f},bore*0.142f,bore*0.21f,Aluminium);
        cylinder(small+V{0,0,-r*0.15f},small+V{0,0,r*0.15f},r*0.23f,Aluminium);

        const Material bankColor = bank->getIndex()%2==0 ? BankBlue : BankRed;
        const float start=cutaway ? Pi*0.43f : 0, sweep=cutaway ? Pi*1.14f : Pi*2;
        const V sleeveOffset=axis*(exploded ? r*0.35f : 0);
        const V headDeck=deck+axis*(exploded ? r*1.5f : 0);
        const V base=deck-axis*(r*1.85f);
        setPart(EnginePart::Cylinder, i);
        tube(base+sleeveOffset,deck+sleeveOffset,r,r*1.1f,bankColor,start,sweep);
        tube(base+sleeveOffset,deck+sleeveOffset,r*0.999f,r*1.012f,Steel,start,sweep);
        // A machined rim and separate head fins make the sleeve/head boundary legible.
        setPart(EnginePart::CylinderHead, i);
        tube(headDeck,headDeck+axis*(r*0.30f),r*0.83f,r*1.2f,Aluminium,start,sweep,24);
        for (float h : {0.04f,0.20f})
            tube(headDeck+axis*(r*h),headDeck+axis*(r*(h+0.035f)),r*1.19f,r*1.29f,bankColor,start,sweep,24);

        // This rim identifies the chamber volume without hiding the moving piston.
        // Its hot color follows actual combustion state, never a decorative timer.
        setPart(EnginePart::CombustionChamber, i);
        const bool lit=engine->getChamber(i)->isLit();
        tube(deck-axis*(r*0.11f),deck-axis*(r*0.07f),r*0.72f,r*0.81f,
            lit ? CombustionGlow : Combustion,start,sweep,24);
        if (lit) {
            const V flame=(crown+deck)*0.5f;
            cylinder(flame-axis*(r*0.025f),flame+axis*(r*0.025f),r*0.20f,CombustionGlow,16);
        }

        // Representative valves/cams and ports show their function, not a claimed
        // manufacturer-specific head layout. Lift and cam angles come from the model.
        auto *head=engine->getHead(bank->getIndex());
        for (int valve=0;valve<2;++valve) {
            const bool intake=valve==0;
            const float sign=intake ? -1.0f : 1.0f;
            const Material portColor=intake ? IntakeTeal : ExhaustOrange;
            setPart(intake ? EnginePart::IntakeValve : EnginePart::ExhaustValve, i);
            const float lift=static_cast<float>(valve==0 ? head->intakeValveLift(piston->getCylinderIndex())
                : head->exhaustValveLift(piston->getCylinderIndex()));
            const V seat=headDeck+side*(sign*r*0.42f)-axis*lift;
            cylinder(seat-axis*(r*0.055f),seat,r*0.29f,portColor,20);
            cylinder(seat,seat+axis*(r*0.62f),r*0.052f,Steel,12);
            tube(seat+axis*(r*0.32f),seat+axis*(r*0.53f),r*0.09f,r*0.115f,DarkSteel,0,2*Pi,12);
            cylinder(seat+axis*(r*0.54f),seat+axis*(r*0.59f),r*0.16f,portColor,16);

            setPart(EnginePart::Camshaft, i);
            const V camCenter=headDeck+axis*(r*0.89f)+side*(sign*r*0.42f);
            cylinder(camCenter+V{0,0,-pitch*0.51f},camCenter+V{0,0,pitch*0.51f},r*0.105f,CamPurple,16);
            Camshaft *cam=intake ? head->getIntakeCamshaft() : head->getExhaustCamshaft();
            const float camAngle=static_cast<float>(cam->getAngle()+cam->getLobeCenterline(piston->getCylinderIndex()));
            const V lobeCenter=camCenter-axis*(std::cos(camAngle)*r*0.10f)+side*(std::sin(camAngle)*r*0.10f);
            cylinder(lobeCenter+V{0,0,-r*0.10f},lobeCenter+V{0,0,r*0.10f},r*0.20f,CamPurple,24);

            setPart(intake ? EnginePart::IntakeRunner : EnginePart::ExhaustRunner, i);
            const V port=headDeck+side*(sign*r*0.90f)+axis*(r*0.12f);
            const V bend=headDeck+side*(sign*r*1.65f)+axis*(r*0.24f);
            const V mouth=bend+axis*(r*0.57f);
            tube(port,bend,r*0.15f,r*0.20f,portColor,0,2*Pi,16);
            tube(bend,mouth,r*0.15f,r*0.20f,portColor,0,2*Pi,16);
            tube(mouth-axis*(r*0.035f),mouth+axis*(r*0.035f),r*0.15f,r*0.24f,portColor,0,2*Pi,16);

            Intake *intakeModel=head->getIntake(piston->getCylinderIndex());
            if (intake && intakeModel != nullptr
                && std::find(representedIntakes.begin(),representedIntakes.end(),intakeModel)==representedIntakes.end()) {
                representedIntakes.push_back(intakeModel);
                setPart(EnginePart::Throttle, i);
                const V throttle=mouth+axis*(r*0.32f);
                tube(mouth,throttle+axis*(r*0.24f),r*0.24f,r*0.29f,IntakeTeal,
                    cutaway ? start : 0,cutaway ? sweep : 2*Pi,20);
                const float plateAngle=static_cast<float>((1.0-intakeModel->getThrottlePlatePosition())*Pi*0.5);
                const V plateNormal=axis*std::cos(plateAngle)+side*std::sin(plateAngle);
                cylinder(throttle-plateNormal*(r*0.018f),throttle+plateNormal*(r*0.018f),r*0.235f,Brass,24);
                cylinder(throttle+V{0,0,-r*0.32f},throttle+V{0,0,r*0.32f},r*0.035f,Steel,8);
            }
        }

        setPart(EnginePart::SparkPlug, i);
        const V plug=headDeck+axis*(r*0.15f);
        cylinder(plug-axis*(r*0.16f),plug,r*0.045f,Steel,10);
        cylinder(plug,plug+axis*(r*0.26f),r*0.11f,Steel,12);
        cylinder(plug+axis*(r*0.26f),plug+axis*(r*0.38f),r*0.155f,Steel,6);
        cylinder(plug+axis*(r*0.38f),plug+axis*(r*0.90f),r*0.10f,Ceramic,16);
        for (float h : {0.48f,0.61f,0.74f})
            cylinder(plug+axis*(r*h),plug+axis*(r*(h+0.055f)),r*0.13f,Ceramic,16);
        cylinder(plug+axis*(r*0.90f),plug+axis*(r*1.06f),r*0.065f,Brass,12);
    }
    // A reference grid under the engine makes orbit and depth easy to read.
    const Vec3 lo=minimum,hi=maximum;
    setPart(EnginePart::None);
    const float floor=lo.y-bore*0.3f;
    for (int i=-5;i<=5;++i) {
        const float p=i*bore;
        cylinder({p,floor,rear-bore},{p,floor,front+bore},bore*0.004f,Grid,4);
        cylinder({lo.x-bore,floor,p},{hi.x+bore,floor,p},bore*0.004f,Grid,4);
    }
    // Frame the engine rather than the decorative grid.
    minimum=lo; maximum=hi;
}
