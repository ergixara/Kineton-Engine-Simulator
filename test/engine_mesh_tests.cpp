#include <gtest/gtest.h>
#include "../include/engine_mesh.h"
#include "../include/engine.h"
#include "../include/constants.h"
#include "../include/standard_valvetrain.h"
#include <algorithm>
#include <cmath>

namespace {
void checkSurface(const EngineMesh &mesh) {
    ASSERT_GT(mesh.vertexCount(),0u);
    for(const auto &batch:mesh.vertices) {
        ASSERT_EQ(batch.size()%3,0u);
        for(size_t i=0;i<batch.size();i+=3) {
            const auto &a=batch[i],&b=batch[i+1],&c=batch[i+2];
            for(const auto &v:{a,b,c}) {
                EXPECT_TRUE(std::isfinite(v.position.x+v.position.y+v.position.z));
                EXPECT_NEAR(v.normal.length(),1,1e-5);
            }
            const auto cross=(b.position-a.position).cross(c.position-a.position);
            EXPECT_GT(cross.length(),1e-9f);
            EXPECT_GT(cross.dot(a.normal+b.normal+c.normal),0);
        }
    }
}
}

TEST(EngineMeshTests, SolidCylindersHaveFiniteOutwardSurfacesOnEveryAxis) {
    for(const EngineMesh::Vec3 axis: {EngineMesh::Vec3{1,0,0}, {0,1,0}, {0,0,1}, {1,2,3}}) {
        EngineMesh mesh; mesh.clear();
        mesh.cylinder({0,0,0},axis,0.1f,EngineMesh::Steel);
        checkSurface(mesh);
        EXPECT_GT(mesh.radius(),axis.length()*0.49f);
    }
}

TEST(EngineMeshTests, CutawayTubeClosesCutEdgesAndPreservesEmptyBore) {
    EngineMesh mesh; mesh.clear();
    mesh.tube({0,0,0},{0,1,0},0.1f,0.12f,EngineMesh::BankBlue,1,4);
    checkSurface(mesh);
    for(const auto &v:mesh.vertices[EngineMesh::BankBlue]) {
        const float radius=std::hypot(v.position.x,v.position.z);
        EXPECT_GE(radius,0.09999f); EXPECT_LE(radius,0.12001f);
    }
}

TEST(EngineMeshTests, InvalidPrimitivesAndMissingEngineProduceNoGeometry) {
    EngineMesh mesh; mesh.clear();
    mesh.cylinder({},{},0.1f,EngineMesh::Steel);
    mesh.tube({},{0,1,0},0.2f,0.1f,EngineMesh::Steel);
    mesh.beam({},{},1,1,EngineMesh::Steel);
    mesh.beam({},{0,1,0},0,1,EngineMesh::Steel);
    mesh.beam({},{0,1,0},1,-1,EngineMesh::Steel);
    EXPECT_EQ(mesh.vertexCount(),0u);
    mesh.build(nullptr,true);
    EXPECT_EQ(mesh.vertexCount(),0u);
    EXPECT_EQ(mesh.radius(),0);
}

TEST(EngineMeshTests, PickingReturnsClosestComponentAndCylinderInWorldUnits) {
    EngineMesh mesh; mesh.clear();
    mesh.setPart(EnginePart::Piston, 3);
    mesh.cylinder({0,0,0},{0,1,0},0.1f,EngineMesh::Aluminium);
    mesh.setPart(EnginePart::ConnectingRod, 2);
    mesh.cylinder({0,-2,0},{0,-1,0},0.1f,EngineMesh::Steel);
    const auto hit=mesh.pick({0,3,0},{0,-7,0});
    EXPECT_EQ(hit.part,EnginePart::Piston);
    EXPECT_EQ(hit.cylinder,3);
    EXPECT_NEAR(hit.distance,2.0f,1e-5f);
    EXPECT_EQ(mesh.pick({0,3,0},{0,1,0}).part,EnginePart::None);
    EXPECT_EQ(mesh.pick({1,3,0},{0,-1,0}).part,EnginePart::None);
    EXPECT_EQ(mesh.pick({0,3,0},{}).part,EnginePart::None);
    // The far cylinder is reachable through another ray, not hidden in a hit table.
    const auto reverse=mesh.pick({0,-3,0},{0,1,0});
    EXPECT_EQ(reverse.part,EnginePart::ConnectingRod);
    EXPECT_EQ(reverse.cylinder,2);
}

TEST(EngineMeshTests, GridAndUnlabelledSurfacesDoNotInterceptSelection) {
    EngineMesh mesh; mesh.clear();
    mesh.setPart(EnginePart::Cylinder, 4);
    mesh.cylinder({0,0,0},{0,1,0},0.2f,EngineMesh::BankBlue);
    mesh.setPart(EnginePart::Flywheel);
    mesh.cylinder({0,2,0},{0,3,0},0.2f,EngineMesh::Grid);
    mesh.setPart(EnginePart::None);
    mesh.cylinder({0,4,0},{0,5,0},0.2f,EngineMesh::Steel);
    EXPECT_EQ(mesh.pick({0,6,0},{0,-1,0}).part,EnginePart::Cylinder);
    EXPECT_NEAR(mesh.pick({0,6,0},{0,-1,0}).distance,5,1e-5f);
    mesh.clear();
    mesh.cylinder({0,0,0},{0,1,0},0.2f,EngineMesh::Steel);
    EXPECT_EQ(mesh.pick({0,3,0},{0,-1,0}).part,EnginePart::None);
}

TEST(EngineMeshTests, CutawayPickingPreservesOpenSpaceAndAnchorsSpecificInstances) {
    EngineMesh mesh; mesh.clear();
    mesh.setPart(EnginePart::Cylinder, 0);
    mesh.tube({0,0,0},{0,1,0},0.1f,0.12f,EngineMesh::BankBlue);
    EXPECT_EQ(mesh.pick({0,2,0},{0,-1,0}).part,EnginePart::None);
    EXPECT_EQ(mesh.pick({0.11f,2,0},{0,-1,0}).part,EnginePart::Cylinder);
    mesh.setPart(EnginePart::Cylinder, 1);
    mesh.cylinder({2,0,0},{2,1,0},0.1f,EngineMesh::BankBlue);
    EngineMesh::Vec3 anchor;
    ASSERT_TRUE(mesh.anchor(EnginePart::Cylinder,1,&anchor));
    EXPECT_NEAR(anchor.x,2,1e-5f);
    EXPECT_NEAR(anchor.y,0.5f,1e-5f);
    ASSERT_TRUE(mesh.anchor(EnginePart::Cylinder,-1,&anchor));
    EXPECT_NEAR(anchor.x,0,1e-5f);
    EXPECT_FALSE(mesh.anchor(EnginePart::Cylinder,8,&anchor));
    EXPECT_FALSE(mesh.anchor(EnginePart::Piston,0,&anchor));
    EXPECT_FALSE(mesh.anchor(EnginePart::Cylinder,0,nullptr));
}

namespace {
// A real Engine graph without a running solver isolates presentation invariants.
class LearningMeshFixture : public ::testing::Test {
protected:
    Engine engine;
    Function lift;
    Camshaft intakeCam, exhaustCam;
    StandardValvetrain valvetrain;

    void SetUp() override {
        Engine::Parameters ep{};
        ep.cylinderBanks=ep.cylinderCount=ep.crankshaftCount=ep.intakeCount=1;
        engine.initialize(ep);
        Crankshaft::Parameters cp{};
        cp.crankThrow=0.04; cp.rodJournals=1;
        engine.getCrankshaft(0)->initialize(cp);
        engine.getCrankshaft(0)->setRodJournalAngle(0,constants::pi/2);
        CylinderBank::Parameters bp{};
        bp.crankshaft=engine.getCrankshaft(0); bp.bore=0.10; bp.deckHeight=0.2;
        bp.cylinderCount=1; bp.index=0;
        engine.getCylinderBank(0)->initialize(bp);
        ConnectingRod::Parameters rp{};
        rp.length=0.14; rp.crankshaft=engine.getCrankshaft(0); rp.piston=engine.getPiston(0);
        engine.getConnectingRod(0)->initialize(rp);
        engine.getConnectingRod(0)->m_body.p_y=0.11;
        Piston::Parameters pp{};
        pp.Rod=engine.getConnectingRod(0); pp.Bank=engine.getCylinderBank(0);
        pp.CylinderIndex=0; pp.CompressionHeight=0.02;
        engine.getPiston(0)->initialize(pp);
        engine.getPiston(0)->m_body.p_y=0.18;
        engine.getPiston(0)->m_body.theta=constants::pi;
        Intake::Parameters ip{};
        ip.volume=0.002; ip.CrossSectionArea=0.002;
        ip.IdleThrottlePlatePosition=1.0;
        engine.getIntake(0)->initialize(ip);
        lift.initialize(3,1.0);
        lift.addSample(-3.14159265,0); lift.addSample(0,0.008); lift.addSample(3.14159265,0);
        Camshaft::Parameters cmp{};
        cmp.lobes=1; cmp.crankshaft=engine.getCrankshaft(0); cmp.lobeProfile=&lift;
        intakeCam.initialize(cmp); exhaustCam.initialize(cmp);
        valvetrain.initialize({&intakeCam,&exhaustCam});
        CylinderHead::Parameters hp{};
        hp.Bank=engine.getCylinderBank(0); hp.Valvetrain=&valvetrain;
        engine.getHead(0)->initialize(hp);
        engine.getHead(0)->setAllIntakes(engine.getIntake(0));
    }
    void TearDown() override {
        engine.getHead(0)->destroy();
        intakeCam.destroy(); exhaustCam.destroy(); lift.destroy(); engine.destroy();
    }
};
}

TEST_F(LearningMeshFixture, ExplodedViewSeparatesTeachingPartsWithoutMovingSimulation) {
    EngineMesh assembled, exploded;
    assembled.build(&engine,true,false);
    exploded.build(&engine,true,true);
    EngineMesh::Vec3 a,b;
    for (const auto part : {EnginePart::Piston,EnginePart::ConnectingRod,EnginePart::Crankshaft}) {
        ASSERT_TRUE(assembled.anchor(part,0,&a));
        ASSERT_TRUE(exploded.anchor(part,0,&b));
        EXPECT_NEAR((b-a).length(),0,1e-6f);
    }
    ASSERT_TRUE(assembled.anchor(EnginePart::CylinderHead,0,&a));
    ASSERT_TRUE(exploded.anchor(EnginePart::CylinderHead,0,&b));
    EXPECT_NEAR(b.y-a.y,0.075f,1e-6f);
    ASSERT_TRUE(assembled.anchor(EnginePart::Cylinder,0,&a));
    ASSERT_TRUE(exploded.anchor(EnginePart::Cylinder,0,&b));
    EXPECT_NEAR(b.y-a.y,0.0175f,1e-6f);
    EXPECT_DOUBLE_EQ(engine.getPiston(0)->m_body.p_y,0.18);
    EXPECT_DOUBLE_EQ(engine.getPiston(0)->m_body.theta,constants::pi);
    EXPECT_LT(exploded.vertexCount(),19000u);
    for (int p=static_cast<int>(EnginePart::Piston);p<static_cast<int>(EnginePart::Count);++p)
        EXPECT_TRUE(exploded.anchor(static_cast<EnginePart>(p),0,&a));
    checkSurface(exploded);
    for (int material=0;material<EngineMesh::MaterialCount;++material) {
        if (material==EngineMesh::Grid) continue;
        for (const auto &vertex:exploded.vertices[material]) {
            const auto p=vertex.position;
            EXPECT_GE(p.x,exploded.minimum.x); EXPECT_LE(p.x,exploded.maximum.x);
            EXPECT_GE(p.y,exploded.minimum.y); EXPECT_LE(p.y,exploded.maximum.y);
            EXPECT_GE(p.z,exploded.minimum.z); EXPECT_LE(p.z,exploded.maximum.z);
        }
    }
}

TEST_F(LearningMeshFixture, CrankJournalsAndWebsFollowTheLiveShaftRotation) {
    EngineMesh initial, rotated;
    initial.build(&engine,true);
    engine.getCrankshaft(0)->m_body.theta=constants::pi/2;
    rotated.build(&engine,true);
    // The asymmetric crank outline rotates a quarter turn around the shaft.
    // Flywheel and other parts are excluded from these bounds.
    const auto crankBounds=[](const EngineMesh &mesh) {
        std::array<float,4> bounds{1e6f,1e6f,-1e6f,-1e6f};
        for (const auto &batch:mesh.vertices) for (const auto &v:batch) {
            if (v.part!=EnginePart::Crankshaft) continue;
            bounds[0]=std::min(bounds[0],v.position.x);
            bounds[1]=std::min(bounds[1],v.position.y);
            bounds[2]=std::max(bounds[2],v.position.x);
            bounds[3]=std::max(bounds[3],v.position.y);
        }
        return bounds;
    };
    const auto a=crankBounds(initial), b=crankBounds(rotated);
    EXPECT_NEAR(b[0],-a[3],1e-6f);
    EXPECT_NEAR(b[1],a[0],1e-6f);
    EXPECT_NEAR(b[2],-a[1],1e-6f);
    EXPECT_NEAR(b[3],a[2],1e-6f);
    EXPECT_LT(b[0],a[0]-0.01f);
}

TEST_F(LearningMeshFixture, ValveLiftAndThrottlePlateFollowTheirSimulationInputs) {
    EngineMesh open, closed;
    engine.setThrottle(0.0);
    open.build(&engine,true);
    engine.setThrottle(1.0);
    engine.getCrankshaft(0)->m_body.theta=2*constants::pi;
    closed.build(&engine,true);
    EngineMesh::Vec3 a,b;
    ASSERT_TRUE(open.anchor(EnginePart::IntakeValve,0,&a));
    ASSERT_TRUE(closed.anchor(EnginePart::IntakeValve,0,&b));
    // Lobe peak at zero pushes the valve 8 mm into the cylinder.
    EXPECT_NEAR(b.y-a.y,0.008f,1e-6f);
    EXPECT_NEAR(b.x-a.x,0,1e-6f);
    const auto plateExtent=[](const EngineMesh &mesh) {
        float lo=1e6f,hi=-1e6f;
        for (const auto &v:mesh.vertices[EngineMesh::Brass]) {
            if (v.part!=EnginePart::Throttle) continue;
            lo=std::min(lo,v.position.y); hi=std::max(hi,v.position.y);
        }
        return hi-lo;
    };
    // Open is edge-on to the runner; closed spans its cross-section.
    EXPECT_NEAR(plateExtent(open),0.0235f,1e-6f);
    EXPECT_NEAR(plateExtent(closed),0.0018f,1e-6f);
    EXPECT_DOUBLE_EQ(engine.getThrottle(),1.0);
}

TEST_F(LearningMeshFixture, ChamberColorFollowsCombustionAndCamRespondsToCrank) {
    EngineMesh mesh;
    engine.getChamber(0)->m_lit=false;
    mesh.build(&engine,true);
    EXPECT_TRUE(mesh.vertices[EngineMesh::CombustionGlow].empty());
    const auto initialCam=mesh.vertices[EngineMesh::CamPurple];
    engine.getChamber(0)->m_lit=true;
    engine.getCrankshaft(0)->m_body.theta=1.0;
    mesh.build(&engine,true);
    EXPECT_FALSE(mesh.vertices[EngineMesh::CombustionGlow].empty());
    EXPECT_TRUE(mesh.vertices[EngineMesh::Combustion].empty());
    ASSERT_EQ(initialCam.size(),mesh.vertices[EngineMesh::CamPurple].size());
    bool camMoved=false;
    for (size_t i=0;i<initialCam.size();++i)
        camMoved|=(initialCam[i].position-mesh.vertices[EngineMesh::CamPurple][i].position).length()>1e-5f;
    EXPECT_TRUE(camMoved);
    EXPECT_DOUBLE_EQ(engine.getCrankshaft(0)->m_body.theta,1.0);
}
