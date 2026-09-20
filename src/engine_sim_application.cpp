#include "../include/engine_sim_application.h"

#include "../include/piston_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/constants.h"
#include "../include/units.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/ui_button.h"
#include "../include/combustion_chamber_object.h"
#include "../include/csv_io.h"
#include "../include/exhaust_system.h"
#include "../include/feedback_comb_filter.h"
#include "../include/utilities.h"
#include "../include/piston_engine_simulator.h"

#include "../scripting/include/compiler.h"

#include <chrono>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <algorithm>

#if ATG_ENGINE_SIM_DISCORD_ENABLED
#include "../discord/Discord.h"
#endif

std::string EngineSimApplication::s_buildVersion = "0.1.12a";

EngineSimApplication::EngineSimApplication() {
    m_assetPath = "";

    m_geometryVertexBuffer = nullptr;
    m_geometryIndexBuffer = nullptr;

    m_paused = false;
    m_recording = false;
    m_screenResolutionIndex = 0;
    for (int i = 0; i < ScreenResolutionHistoryLength; ++i) {
        m_screenResolution[i][0] = m_screenResolution[i][1] = 0;
    }

    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);

    m_displayHeight = (float)units::distance(2.0, units::foot);
    m_outputAudioBuffer = nullptr;
    m_audioSource = nullptr;

    m_torque = 0;
    m_dynoSpeed = 0;

    m_simulator = nullptr;
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_temperatureGauge = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    m_iceEngine = nullptr;
    m_mainRenderTarget = nullptr;

    m_vehicle = nullptr;
    m_transmission = nullptr;

    m_oscillatorSampleOffset = 0;
    m_gameWindowHeight = 256;
    m_screenWidth = 256;
    m_screenHeight = 256;
    m_screen = 3;
    m_viewParameters.Layer0 = 0;
    m_viewParameters.Layer1 = 0;

    m_displayAngle = 0.0f;
}

EngineSimApplication::~EngineSimApplication() {
    /* void */
}

void EngineSimApplication::initialize(void *instance, ysContextObject::DeviceAPI api) {
    dbasic::Path modulePath = dbasic::GetModulePath();
    dbasic::Path confPath = modulePath.Append("delta.conf");

    std::string enginePath = "../dependencies/submodules/delta-studio/engines/basic";
    m_assetPath = "../assets";
    if (confPath.Exists()) {
        std::fstream confFile(confPath.ToString(), std::ios::in);

        std::getline(confFile, enginePath);
        std::getline(confFile, m_assetPath);
        enginePath = modulePath.Append(enginePath).ToString();
        m_assetPath = modulePath.Append(m_assetPath).ToString();

        confFile.close();
    }

    m_engine.GetConsole()->SetDefaultFontDirectory(enginePath + "/fonts/");

    const std::string shaderPath = enginePath + "/shaders/";
    const std::string winTitle = "Engine Sim | AngeTheGreat | v" + s_buildVersion;
    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = true;
    settings.Instance = instance;
    settings.ShaderDirectory = shaderPath.c_str();
    settings.WindowTitle = winTitle.c_str();
    settings.WindowPositionX = 0;
    settings.WindowPositionY = 0;
    settings.WindowStyle = ysWindow::WindowStyle::Windowed;
    RECT workArea{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        // Use Windows' DPI-adjusted work area so controls stay above the taskbar.
        settings.WindowPositionX = workArea.left + 12;
        settings.WindowPositionY = workArea.top + 12;
        settings.WindowWidth = (std::min)(1600L, workArea.right - workArea.left - 24);
        settings.WindowHeight = (std::min)(1000L, workArea.bottom - workArea.top - 24);
    }
    else {
        settings.WindowWidth = 1280;
        settings.WindowHeight = 720;
    }

    m_engine.CreateGameWindow(settings);

    // The app renders its own text. The unused console shares this text batch
    // and would replace its font atlas during EndFrame, hiding Roboto glyphs.
    m_engine.SetConsoleEnabled(false);

    m_engine.GetDevice()->CreateSubRenderTarget(
        &m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        0,
        0,
        0,
        0);

    m_engine.InitializeShaderSet(&m_shaderSet);
    m_shaders.Initialize(
        &m_shaderSet,
        m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        m_engine.GetDefaultShaderProgram(),
        m_engine.GetDefaultInputLayout());
    m_engine.InitializeConsoleShaders(&m_shaderSet);
    m_engine.SetShaderSet(&m_shaderSet);

    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));

    m_assetManager.SetEngine(&m_engine);

    m_engine.GetDevice()->CreateIndexBuffer(
        &m_geometryIndexBuffer, sizeof(unsigned short) * 600000, nullptr);
    m_engine.GetDevice()->CreateVertexBuffer(
        &m_geometryVertexBuffer, sizeof(dbasic::Vertex) * 400000, nullptr);

    m_geometryGenerator.initialize(400000, 600000);

    // This freely licensed font is already bundled with Delta Studio.
    m_engine.LoadFont(&m_readableFont, (enginePath + "/fonts/roboto/Roboto-Regular.ttf").c_str(), 1024, 40);

    initialize();
}

void EngineSimApplication::initialize() {
    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));
    m_assetManager.CompileInterchangeFile((m_assetPath + "/assets").c_str(), 1.0f, true);
    m_assetManager.LoadSceneFile((m_assetPath + "/assets").c_str(), true);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_readableFont ? m_readableFont : m_engine.GetConsole()->GetFont());

    loadScript();

    m_audioBuffer.initialize(44100, 44100);
    m_audioBuffer.m_writePointer = (int)(44100 * 0.1);

    ysAudioParameters params;
    params.m_bitsPerSample = 16;
    params.m_channelCount = 1;
    params.m_sampleRate = 44100;
    m_outputAudioBuffer =
        m_engine.GetAudioDevice()->CreateBuffer(&params, 44100);

    m_audioSource = m_engine.GetAudioDevice()->CreateSource(m_outputAudioBuffer);
    m_audioSource->SetMode((m_simulator->getEngine() != nullptr)
        ? ysAudioSource::Mode::Loop
        : ysAudioSource::Mode::Stop);
    m_audioSource->SetPan(0.0f);
    m_audioSource->SetVolume(1.0f);

#ifdef ATG_ENGINE_SIM_DISCORD_ENABLED
    // Create a global instance of discord-rpc
    CDiscord::CreateInstance();

    // Enable it, this needs to be set via a config file of some sort. 
    GetDiscordManager()->SetUseDiscord(true);
    DiscordRichPresence passMe = { 0 };

    std::string engineName = (m_iceEngine != nullptr)
        ? m_iceEngine->getName()
        : "Broken Engine";

    GetDiscordManager()->SetStatus(passMe, engineName, s_buildVersion);
#endif /* ATG_ENGINE_SIM_DISCORD_ENABLED */
}

void EngineSimApplication::process(float frame_dt) {
    frame_dt = static_cast<float>(clamp(frame_dt, 1 / 200.0f, 1 / 30.0f));

    double speed = m_learning.slowMotion ? 0.025 : 1.0;
    if (m_engine.IsKeyDown(ysKey::Code::N1)) {
        speed = 1 / 10.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N2)) {
        speed = 1 / 100.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N3)) {
        speed = 1 / 200.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N4)) {
        speed = 1 / 500.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N5)) {
        speed = 1 / 1000.0;
    }

    m_simulator->setSimulationSpeed(speed);

    const double avgFramerate = clamp(m_engine.GetAverageFramerate(), 30.0f, 1000.0f);
    m_simulator->startFrame(1 / avgFramerate);

    auto proc_t0 = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        m_oscCluster->sample();
    }

    auto proc_t1 = std::chrono::steady_clock::now();

    m_simulator->endFrame();

    auto duration = proc_t1 - proc_t0;
    if (iterationCount > 0) {
        m_performanceCluster->addTimePerTimestepSample(
            (duration.count() / 1E9) / iterationCount);
    }

    const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    const SampleOffset writePosition = m_audioBuffer.m_writePointer;

    SampleOffset targetWritePosition =
        m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.1));
    SampleOffset maxWrite = m_audioBuffer.offsetDelta(writePosition, targetWritePosition);

    SampleOffset currentLead = m_audioBuffer.offsetDelta(safeWritePosition, writePosition);
    SampleOffset newLead = m_audioBuffer.offsetDelta(safeWritePosition, targetWritePosition);

    if (currentLead > 44100 * 0.5) {
        m_audioBuffer.m_writePointer = m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.05));
        currentLead = m_audioBuffer.offsetDelta(safeWritePosition, m_audioBuffer.m_writePointer);
        maxWrite = m_audioBuffer.offsetDelta(m_audioBuffer.m_writePointer, targetWritePosition);
    }

    if (currentLead > newLead) {
        maxWrite = 0;
    }

    int16_t *samples = new int16_t[maxWrite];
    const int readSamples = m_simulator->readAudioOutput(maxWrite, samples);

    for (SampleOffset i = 0; i < (SampleOffset)readSamples && i < maxWrite; ++i) {
        const int16_t sample = samples[i];
        if (m_oscillatorSampleOffset % 4 == 0) {
            m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
                m_oscillatorSampleOffset,
                sample / (float)(INT16_MAX));
        }

        m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, (int)i);

        m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (44100 / 10);
    }

    delete[] samples;

    if (readSamples > 0) {
        SampleOffset size0, size1;
        void *data0, *data1;
        m_audioSource->LockBufferSegment(
            m_audioBuffer.m_writePointer, readSamples, &data0, &size0, &data1, &size1);

        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data0), m_audioBuffer.m_writePointer, size0);
        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data1),
            m_audioBuffer.getBufferIndex(m_audioBuffer.m_writePointer, size0),
            size1);

        m_audioSource->UnlockBufferSegments(data0, size0, data1, size1);
        m_audioBuffer.commitBlock(readSamples);
    }

    m_performanceCluster->addInputBufferUsageSample(
        (double)m_simulator->getSynthesizerInputLatency() / m_simulator->getSynthesizerInputLatencyTarget());
    m_performanceCluster->addAudioLatencySample(
        m_audioBuffer.offsetDelta(m_audioSource->GetCurrentWritePosition(), m_audioBuffer.m_writePointer) / (44100 * 0.1));
}

void EngineSimApplication::render() {
    if (m_engineView->is3d()) {
        renderEngine3d();
        m_uiManager.render();
        return;
    }
    for (SimulationObject *object : m_objects) {
        object->generateGeometry();
    }

    m_viewParameters.Sublayer = 0;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_viewParameters.Sublayer = 1;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_viewParameters.Sublayer = 2;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_uiManager.render();
}

void EngineSimApplication::renderEngine3d() {
    const unsigned int colors[] = {0xCAD7E0, 0x8193A4, 0x455667, 0x506C7C, 0x677181, 0xD6AE63, 0x202D3B,
        0x61CBD3, 0xE89867, 0xA999DC, 0xE8E8D8, 0x815E38, 0xFFBC58};
    static_assert(sizeof(colors)/sizeof(colors[0]) == EngineMesh::MaterialCount, "Every material needs a color");
    auto &vars = m_shaders.m_objectVariables;
    vars = dbasic::ShaderObjectVariables{};
    vars.ColorReplace = 1; vars.Lit = 1;
    vars.Metallic = 0.12f; vars.SpecularPower = 40; vars.SpecularMix = 0.3f;
    const bool inspect = isLearningWorkspace() && m_learning.selected != EnginePart::None;
    for (int material = 0; material < EngineMesh::MaterialCount; ++material) {
        const auto &batch = m_engineMesh.vertices[material];
        for (int selectedPass=0; selectedPass<2; ++selectedPass) {
            if (!inspect && selectedPass==1) continue;
            ysVector color=ysColor::srgbiToLinear(colors[material]);
            if(selectedPass==1) color=ysColor::srgbiToLinear(0xFFE0A0);
            m_shaders.SetBaseColor(color);
            vars.Emission = selectedPass ? ysMath::LoadVector(0.12f,0.065f,0.015f,0) : ysMath::Constants::Zero;
            if(material==EngineMesh::CombustionGlow) vars.Emission=ysMath::LoadVector(0.35f,0.1f,0,0);
            vars.Lit = (material == EngineMesh::Grid) ? 0 : 1;
            GeometryGenerator::GeometryIndices indices;
            int count=0;
            m_geometryGenerator.startShape();
            const auto flush = [&]() {
                m_geometryGenerator.endShape(&indices);
                if(indices.FaceCount>0) m_engine.DrawGeneric(m_shaders.GetRegularFlags(),m_geometryIndexBuffer,m_geometryVertexBuffer,
                    sizeof(dbasic::Vertex),indices.BaseIndex,indices.BaseVertex,indices.FaceCount,true,0);
                m_geometryGenerator.startShape(); count=0;
            };
            for(size_t i=0;i<batch.size();i+=3) {
                const auto &src=batch[i];
                const bool selected=inspect && src.part==m_learning.selected && (src.cylinder<0 || src.cylinder==m_learning.cylinder);
                if(selected != (selectedPass==1)) continue;
                if(count>=60000) flush();
                dbasic::Vertex v[3];
                for(int j=0;j<3;++j) {
                    const auto &vertex=batch[i+j];
                    v[j].Pos=ysMath::LoadVector(vertex.position.x,vertex.position.y,vertex.position.z,1);
                    v[j].Normal=ysMath::LoadVector(vertex.normal.x,vertex.normal.y,vertex.normal.z,0);
                }
                if(!m_geometryGenerator.generateTriangle3d(v[0],v[1],v[2])) break;
                count+=3;
            }
            flush();
        }
    }
    vars = dbasic::ShaderObjectVariables{};
    vars.ColorReplace = 1; vars.Lit = 0;
}

void EngineSimApplication::setPaused(bool paused) {
    m_paused = paused;
    if(m_audioSource) m_audioSource->SetMode(paused ? ysAudioSource::Mode::Stop :
        (m_iceEngine ? ysAudioSource::Mode::Loop : ysAudioSource::Mode::Stop));
}

void EngineSimApplication::toggleLearningWorkspace() {
    m_screen = isLearningWorkspace() ? 0 : 3;
}

void EngineSimApplication::performAction(SimulatorAction action, bool pressed, ActionSource source) {
    // Starter is held; the other commands fire once on a key press or UI click.
    if (action == SimulatorAction::Starter) {
        if (source == ActionSource::Keyboard) m_keyboardStarterHeld = pressed;
        else m_starterHeld = pressed;

        const bool active = m_engine.GetGameWindow() && m_engine.GetGameWindow()->IsActive();
        if (!active) m_starterHeld = m_keyboardStarterHeld = false;
        if (m_simulator) {
            const bool enabled = active && (m_starterHeld || m_keyboardStarterHeld);
            const bool changed = enabled != m_simulator->m_starterMotor.m_enabled;
            m_simulator->m_starterMotor.m_enabled = enabled;
            if (changed && m_infoCluster) m_infoCluster->setLogMessage(enabled
                ? "STARTER ENABLED" : "STARTER DISABLED");
        }
        return;
    }
    if (!pressed) return;

    switch (action) {
    case SimulatorAction::Mode:
        m_engineView->set3d(!m_engineView->is3d());
        break;
    case SimulatorAction::Reset:
        m_engineView->resetCamera();
        m_displayAngle = 0;
        break;
    case SimulatorAction::Cutaway:
        m_engineView->m_cutaway = !m_engineView->m_cutaway;
        break;
    case SimulatorAction::Learn:
        toggleLearningWorkspace();
        break;
    case SimulatorAction::Labels:
        m_learning.labels = !m_learning.labels;
        break;
    case SimulatorAction::Exploded:
        m_learning.exploded = !m_learning.exploded;
        break;
    case SimulatorAction::Ignition:
        if (m_iceEngine) {
            auto *ignition = m_iceEngine->getIgnitionModule();
            ignition->m_enabled = !ignition->m_enabled;
            if (m_infoCluster) m_infoCluster->setLogMessage(ignition->m_enabled
                ? "IGNITION ENABLED" : "IGNITION DISABLED");
        }
        break;
    case SimulatorAction::Pause:
        setPaused(!m_paused);
        break;
    case SimulatorAction::Slow:
        m_learning.slowMotion = !m_learning.slowMotion;
        break;
    case SimulatorAction::Starter:
        break;
    }
}

bool EngineSimApplication::projectEnginePoint(EngineMesh::Vec3 point, Point *screen) const {
    if(!m_engineView->is3d()) return false;
    const auto delta=point-m_cameraEye3d;
    const float depth=delta.dot(m_cameraForward3d);
    if(depth<=0.001f) return false;
    const float tangent=std::tan(3.14159265359f/8);
    const float x=delta.dot(m_cameraRight3d)/(depth*tangent*m_cameraAspect3d);
    const float y=delta.dot(m_cameraUp3d)/(depth*tangent);
    const auto &bounds=m_engineView->m_bounds;
    *screen={bounds.left()+(x+1)*bounds.width()*0.5f,bounds.bottom()+(y+1)*bounds.height()*0.5f};
    return std::abs(x)<0.98f && std::abs(y)<0.95f;
}

bool EngineSimApplication::getPartAnchor(EnginePart part, int cylinder, Point *screen) const {
    EngineMesh::Vec3 anchor;
    return (m_engineMesh.anchor(part,cylinder,&anchor) || m_engineMesh.anchor(part,-1,&anchor)) && projectEnginePoint(anchor,screen);
}

bool EngineSimApplication::selectEnginePart(const Point &screen) {
    if(!m_engineView->is3d() || !m_engineView->m_bounds.overlaps(screen)) return false;
    const auto &b=m_engineView->m_bounds;
    const float x=2*(screen.x-b.left())/b.width()-1, y=2*(screen.y-b.bottom())/b.height()-1;
    const float t=std::tan(3.14159265359f/8);
    const auto ray=(m_cameraForward3d+m_cameraRight3d*(x*t*m_cameraAspect3d)+m_cameraUp3d*(y*t)).normalized();
    const auto hit=m_engineMesh.pick(m_cameraEye3d,ray);
    if(hit.part==EnginePart::None) return false;
    m_learning.selected=hit.part;
    if(hit.cylinder>=0) m_learning.cylinder=hit.cylinder;
    m_screen=3;
    m_learningPanel->setTab(LearningPanel::Tab::Parts);
    return true;
}
EngineMesh::Vec3 EngineSimApplication::cameraPanDisplacement(const Point &pixels) const {
    const float radius=(std::max)(m_engineMesh.radius(),0.1f);
    const float distance=radius*3.25f*(std::max)(1.0f,1.0f/m_cameraAspect3d)/m_engineView->m_zoom3d;
    const float scale=2*distance*std::tan(3.14159265359f/8)/(std::max)(1.0f,m_engineView->m_bounds.height());
    return m_cameraRight3d*(-pixels.x*scale)+m_cameraUp3d*(-pixels.y*scale);
}
float EngineSimApplication::pixelsToUnits(float pixels) const {
    const float f = m_displayHeight / m_engineView->m_bounds.height();
    return pixels * f;
}

float EngineSimApplication::unitsToPixels(float units) const {
    const float f = m_engineView->m_bounds.height() / m_displayHeight;
    return units * f;
}

void EngineSimApplication::run() {
    int checkFrame = 0;
    int capturedFrames = 0;
    bool interactionChecksPassed = true;
    double peakCheckRpm = 0;
    std::ofstream verification;
    if (m_visualCheck) verification.open("visual-check.log");
    while (true) {
        m_engine.StartFrame();

        if (!m_engine.IsOpen()) break;
        if (m_engine.ProcessKeyDown(ysKey::Code::F4)) performAction(SimulatorAction::Mode);
        if (m_engine.ProcessKeyDown(ysKey::Code::F5)) performAction(SimulatorAction::Cutaway);
        if (m_engine.ProcessKeyDown(ysKey::Code::F6)) performAction(SimulatorAction::Learn);
        if (m_engine.ProcessKeyDown(ysKey::Code::F7)) performAction(SimulatorAction::Labels);
        if (m_engine.ProcessKeyDown(ysKey::Code::F8)) performAction(SimulatorAction::Exploded);
        if (m_engine.ProcessKeyDown(ysKey::Code::F9)) performAction(SimulatorAction::Slow);
        if (m_engine.ProcessKeyDown(ysKey::Code::P)) performAction(SimulatorAction::Pause);
        if (m_engine.ProcessKeyDown(ysKey::Code::F3)) performAction(SimulatorAction::Reset);
        const float orbitStep = (std::min)(m_engine.GetFrameLength(), 0.05f);
        if (m_engine.IsKeyDown(ysKey::Code::F1)) {
            if (m_engineView->is3d()) m_engineView->m_yaw += orbitStep; else m_displayAngle += orbitStep;
        }
        if (m_engine.IsKeyDown(ysKey::Code::F2)) {
            if (m_engineView->is3d()) m_engineView->m_yaw -= orbitStep; else m_displayAngle -= orbitStep;
        }
        if (m_engine.ProcessKeyDown(ysKey::Code::Escape)) {
            break;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Return)) {
            m_audioSource->SetMode(ysAudioSource::Mode::Stop);
            loadScript();
            setPaused(m_paused);
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Tab)) {
            m_screen++;
            if (m_screen > 3) m_screen = 0;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F)) {
            if (m_engine.GetGameWindow()->GetWindowStyle() != ysWindow::WindowStyle::Fullscreen) {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
                m_infoCluster->setLogMessage("Entered fullscreen mode");
            }
            else {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Windowed);
                m_infoCluster->setLogMessage("Exited fullscreen mode");
            }
        }

        m_gameWindowHeight = m_engine.GetGameWindow()->GetGameHeight();
        m_screenHeight = m_engine.GetGameWindow()->GetScreenHeight();
        m_screenWidth = m_engine.GetGameWindow()->GetScreenWidth();

        updateScreenSizeStability();

        processEngineInput();
        if (m_visualCheck && m_iceEngine != nullptr) {
            ++checkFrame;
            m_iceEngine->getIgnitionModule()->m_enabled = true;
            m_simulator->m_starterMotor.m_enabled = checkFrame < 180 || (checkFrame > 200 && checkFrame < 270);
            if (checkFrame == 75) {
                // Exercise the same orbit and zoom handlers as real mouse input.
                const Point dragStart = m_engineView->m_bounds.getPosition(Bounds::center);
                m_engineView->onMouseDown(dragStart);
                m_engineView->onDrag({}, dragStart, dragStart + Point(-175, 10));
                m_engineView->onMouseScroll(120);
            }
            if (checkFrame == 120) m_engineView->m_cutaway = false;
            if (checkFrame == 160) m_engineView->set3d(false);
            if (checkFrame == 200) {
                m_audioSource->SetMode(ysAudioSource::Mode::Stop);
                loadScript();
                m_audioSource->SetMode(ysAudioSource::Mode::Loop);
            }
            interactionChecksPassed = verifyLearningFrame(checkFrame, verification) && interactionChecksPassed;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Insert) &&
            m_engine.GetGameWindow()->IsActive()) {
            if (!isRecording() && readyToRecord()) {
                startRecording();
            }
            else if (isRecording()) {
                stopRecording();
            }
        }

        if (isRecording() && !readyToRecord()) {
            stopRecording();
        }

        if (m_iceEngine && (!m_paused || m_engine.ProcessKeyDown(ysKey::Code::Right))) {
            process(m_engine.GetFrameLength());
        }

        m_uiManager.update(m_engine.GetFrameLength());

        renderScene();

        m_engine.EndFrame();
        if (m_visualCheck) {
            if (checkFrame == 60 || checkFrame == 110 || checkFrame == 150 || checkFrame == 190 || checkFrame == 240
                || checkFrame == 280 || checkFrame == 300 || checkFrame == 320 || checkFrame == 340 || checkFrame == 355
                || checkFrame == 375 || checkFrame == 390 || checkFrame == 410 || checkFrame == 430 || checkFrame == 450 || checkFrame == 480) {
                const std::string path="visual-check-"+std::to_string(checkFrame)+".bmp";
                if(captureFrame(path)) ++capturedFrames;
                peakCheckRpm = (std::max)(peakCheckRpm,std::abs(m_iceEngine->getRpm()));
                verification << "frame=" << checkFrame << " mode=" << (m_engineView->is3d()?"3D":"2D")
                    << " vertices=" << m_engineMesh.vertexCount() << " rpm=" << m_iceEngine->getRpm()
                    << " crank=" << m_iceEngine->getOutputCrankshaft()->m_body.theta << " image=" << path << std::endl;
            }
            if(checkFrame >= 480 || m_iceEngine == nullptr) {
                m_visualCheckPassed = capturedFrames == 16 && peakCheckRpm > 10 && m_iceEngine != nullptr && interactionChecksPassed;
                verification << (m_visualCheckPassed ? "PASS" : "FAIL") << ": captures=" << capturedFrames
                    << " peak_rpm=" << peakCheckRpm << std::endl;
                break;
            }
        }

        if (isRecording()) {
            recordFrame();
        }
    }

    if (isRecording()) {
        stopRecording();
    }

    m_simulator->endAudioRenderingThread();
}

void EngineSimApplication::destroy() {
    m_shaderSet.Destroy();

    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryVertexBuffer);
    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryIndexBuffer);

    m_assetManager.Destroy();
    m_engine.Destroy();

    m_simulator->destroy();
    m_audioBuffer.destroy();
}

void EngineSimApplication::loadEngine(
    Engine *engine,
    Vehicle *vehicle,
    Transmission *transmission)
{
    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) return;

    destroyObjects();

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
    }

    if (m_vehicle != nullptr) {
        delete m_vehicle;
        m_vehicle = nullptr;
    }

    if (m_transmission != nullptr) {
        delete m_transmission;
        m_transmission = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
    }

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;

    m_simulator = engine->createSimulator(vehicle, transmission);

    createObjects(engine);

    m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());

    Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
    audioParams.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.airNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    m_simulator->synthesizer().setAudioParameters(audioParams);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();

        ysWindowsAudioWaveFile waveFile;
        waveFile.OpenFile(response->getFilename().c_str());
        waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
        waveFile.FillBuffer(0);
        waveFile.CloseFile();

        m_simulator->synthesizer().initializeImpulseResponse(
            reinterpret_cast<const int16_t *>(waveFile.GetBuffer()),
            waveFile.GetSampleCount(),
            response->getVolume(),
            i
        );

        waveFile.DestroyInternalBuffer();
    }

    m_simulator->startAudioRenderingThread();
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}

void EngineSimApplication::drawGeneratedUi(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}

void EngineSimApplication::drawModel2d(dbasic::StageEnableFlags flags, dbasic::ModelAsset *model, int layer) {
    // The original 2D scene and HUD use painter's order, even with a 3D depth buffer available.
    m_engine.DrawGeneric(flags, model->GetIndexBuffer(), model->GetVertexBuffer(), model->GetVertexSize(),
        model->GetBaseIndex(), model->GetBaseVertex(), model->GetFaceCount(), false, layer);
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer,
    dbasic::StageEnableFlags flags)
{
    m_engine.DrawGeneric(
        flags,
        m_geometryIndexBuffer,
        m_geometryVertexBuffer,
        sizeof(dbasic::Vertex),
        indices.BaseIndex,
        indices.BaseVertex,
        indices.FaceCount,
        false,
        layer);
}

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;

    if (settings.startFullscreen) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }

    m_background = ysColor::srgbiToLinear(m_applicationSettings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(m_applicationSettings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(m_applicationSettings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(m_applicationSettings.colorPink);
    m_red = ysColor::srgbiToLinear(m_applicationSettings.colorRed);
    m_orange = ysColor::srgbiToLinear(m_applicationSettings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(m_applicationSettings.colorYellow);
    m_blue = ysColor::srgbiToLinear(m_applicationSettings.colorBlue);
    m_green = ysColor::srgbiToLinear(m_applicationSettings.colorGreen);
}

void EngineSimApplication::createObjects(Engine *engine) {
    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        ConnectingRodObject *rodObject = new ConnectingRodObject;
        rodObject->initialize(this);
        rodObject->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rodObject);

        PistonObject *pistonObject = new PistonObject;
        pistonObject->initialize(this);
        pistonObject->m_piston = engine->getPiston(i);
        m_objects.push_back(pistonObject);

        CombustionChamberObject *ccObject = new CombustionChamberObject;
        ccObject->initialize(this);
        ccObject->m_chamber = m_iceEngine->getChamber(i);
        m_objects.push_back(ccObject);
    }

    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        CrankshaftObject *crankshaftObject = new CrankshaftObject;
        crankshaftObject->initialize(this);
        crankshaftObject->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaftObject);
    }

    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        CylinderBankObject *cbObject = new CylinderBankObject;
        cbObject->initialize(this);
        cbObject->m_bank = engine->getCylinderBank(i);
        cbObject->m_head = engine->getHead(i);
        m_objects.push_back(cbObject);

        CylinderHeadObject *chObject = new CylinderHeadObject;
        chObject->initialize(this);
        chObject->m_head = engine->getHead(i);
        chObject->m_engine = engine;
        m_objects.push_back(chObject);
    }
}

void EngineSimApplication::destroyObjects() {
    for (SimulationObject *object : m_objects) {
        object->destroy();
        delete object;
    }

    m_objects.clear();
}

const SimulationObject::ViewParameters &
    EngineSimApplication::getViewParameters() const
{
    return m_viewParameters;
}

void EngineSimApplication::loadScript() {
    m_starterHeld = m_keyboardStarterHeld = false;
    performAction(SimulatorAction::Starter, false);
    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    es_script::Compiler compiler;
    compiler.initialize();
    const bool compiled = compiler.compile("../assets/main.mr");
    if (compiled) {
        const es_script::Compiler::Output output = compiler.execute();
        configure(output.applicationSettings);

        engine = output.engine;
        vehicle = output.vehicle;
        transmission = output.transmission;
    }
    else {
        engine = nullptr;
        vehicle = nullptr;
        transmission = nullptr;
    }

    compiler.destroy();
#endif /* ATG_ENGINE_SIM_PIRANHA_ENABLED */

    if (engine == nullptr) {
        m_scriptError = "Script could not load. Check build/error_log.log, fix assets/main.mr, then press Enter.";
        if (!m_simulator) {
            // UI getters only need an idle object; allocate physics resources once an engine loads.
            m_simulator = new PistonEngineSimulator;
            refreshUserInterface();
        }
        if (m_infoCluster) m_infoCluster->setLogMessage(m_scriptError);
        return; // A failed reload preserves the last working engine.
    }
    m_scriptError.clear();

    if (vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        vehicle = new Vehicle;
        vehicle->initialize(vehParams);
    }

    if (transmission == nullptr) {
        const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        transmission = new Transmission;
        transmission->initialize(tParams);
    }

    loadEngine(engine, vehicle, transmission);
    refreshUserInterface();
}

void EngineSimApplication::processEngineInput() {
    const bool active = m_engine.GetGameWindow()->IsActive();
    performAction(SimulatorAction::Starter, active && m_engine.IsKeyDown(ysKey::Code::S),
        ActionSource::Keyboard);
    if (m_iceEngine == nullptr) {
        return;
    }

    const float dt = m_engine.GetFrameLength();
    const bool fineControlMode = m_engine.IsKeyDown(ysKey::Code::Space);

    const int mouseWheel = m_engine.GetMouseWheel();
    const int mouseWheelDelta = mouseWheel - m_lastMouseWheel;
    m_lastMouseWheel = mouseWheel;

    bool fineControlInUse = false;
    if (m_engine.IsKeyDown(ysKey::Code::Z)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.volume = clamp(audioParams.volume + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[Z] - Set volume to " + std::to_string(audioParams.volume));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::X)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.convolution = clamp(audioParams.convolution + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[X] - Set convolution level to " + std::to_string(audioParams.convolution));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::C)) {
        const double rate = fineControlMode
            ? 0.00001
            : 0.001;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.dF_F_mix = clamp(audioParams.dF_F_mix + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[C] - Set high freq. gain to " + std::to_string(audioParams.dF_F_mix));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::V)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.airNoise = clamp(audioParams.airNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[V] - Set low freq. noise to " + std::to_string(audioParams.airNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::B)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.inputSampleNoise = clamp(audioParams.inputSampleNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[B] - Set high freq. noise to " + std::to_string(audioParams.inputSampleNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N)) {
        const double rate = fineControlMode
            ? 10.0
            : 100.0;

        const double newSimulationFrequency = clamp(
            m_simulator->getSimulationFrequency() + mouseWheelDelta * rate * dt,
            400.0, 400000.0);

        m_simulator->setSimulationFrequency(newSimulationFrequency);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[N] - Set simulation freq to " + std::to_string(m_simulator->getSimulationFrequency()));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::G) && m_simulator->m_dyno.m_hold) {
        if (mouseWheelDelta > 0) {
            m_dynoSpeed += m_iceEngine->getDynoHoldStep();
        }
        else if (mouseWheelDelta < 0) {
            m_dynoSpeed -= m_iceEngine->getDynoHoldStep();
        }

        m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());

        m_infoCluster->setLogMessage("[G] - Set dyno speed to " + std::to_string(units::toRpm(m_dynoSpeed)));
        fineControlInUse = true;
    }

    const double prevTargetThrottle = m_targetSpeedSetting;
    m_targetSpeedSetting = fineControlMode ? m_targetSpeedSetting : 0.0;
    if (m_engine.IsKeyDown(ysKey::Code::Q)) {
        m_targetSpeedSetting = 0.01;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::W)) {
        m_targetSpeedSetting = 0.1;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::E)) {
        m_targetSpeedSetting = 0.2;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::R)) {
        m_targetSpeedSetting = 1.0;
    }
    else if (fineControlMode && !fineControlInUse) {
        m_targetSpeedSetting = clamp(m_targetSpeedSetting + mouseWheelDelta * 0.0001);
    }

    if (prevTargetThrottle != m_targetSpeedSetting) {
        m_infoCluster->setLogMessage("Speed control set to " + std::to_string(m_targetSpeedSetting));
    }

    m_speedSetting = m_targetSpeedSetting * 0.5 + 0.5 * m_speedSetting;

    m_iceEngine->setSpeedControl(m_speedSetting);
    if (m_engine.ProcessKeyDown(ysKey::Code::M)) {
        const int currentLayer = getViewParameters().Layer0;
        if (currentLayer + 1 < m_iceEngine->getMaxDepth()) {
            setViewLayer(currentLayer + 1);
        }

        m_infoCluster->setLogMessage("[M] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::OEM_Comma)) {
        if (getViewParameters().Layer0 - 1 >= 0)
            setViewLayer(getViewParameters().Layer0 - 1);

        m_infoCluster->setLogMessage("[,] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::D)) {
        m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;

        const std::string msg = m_simulator->m_dyno.m_enabled
            ? "DYNOMOMETER ENABLED"
            : "DYNOMOMETER DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::H)) {
        m_simulator->m_dyno.m_hold = !m_simulator->m_dyno.m_hold;

        const std::string msg = m_simulator->m_dyno.m_hold
            ? m_simulator->m_dyno.m_enabled ? "HOLD ENABLED" : "HOLD ON STANDBY [ENABLE DYNO. FOR HOLD]"
            : "HOLD DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_simulator->m_dyno.m_enabled) {
        if (!m_simulator->m_dyno.m_hold) {
            if (m_simulator->getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)) {
                m_dynoSpeed += units::rpm(500) * dt;
            }
            else {
                m_dynoSpeed *= (1 / (1 + dt));
            }

            if (m_dynoSpeed > m_iceEngine->getRedline()) {
                m_simulator->m_dyno.m_enabled = false;
                m_dynoSpeed = units::rpm(0);
            }
        }
    }
    else {
        if (!m_simulator->m_dyno.m_hold) {
            m_dynoSpeed = units::rpm(0);
        }
    }

    m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
    m_simulator->m_dyno.m_rotationSpeed = m_dynoSpeed;

    if (m_engine.ProcessKeyDown(ysKey::Code::A)) {
        performAction(SimulatorAction::Ignition);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::Up)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() + 1);

        m_infoCluster->setLogMessage(
            "UPSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::Down)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() - 1);

        if (m_simulator->getTransmission()->getGear() != -1) {
            m_infoCluster->setLogMessage(
                "DOWNSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
        }
        else {
            m_infoCluster->setLogMessage("SHIFTED TO NEUTRAL");
        }
    }

    if (m_engine.IsKeyDown(ysKey::Code::T)) {
        m_targetClutchPressure -= 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::U)) {
        m_targetClutchPressure += 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::Shift)) {
        m_targetClutchPressure = 0.0;
        m_infoCluster->setLogMessage("CLUTCH DEPRESSED");
    }
    else if (!m_engine.IsKeyDown(ysKey::Code::Y)) {
        m_targetClutchPressure = 1.0;
    }

    m_targetClutchPressure = clamp(m_targetClutchPressure);

    double clutchRC = 0.001;
    if (m_engine.IsKeyDown(ysKey::Code::Space)) {
        clutchRC = 1.0;
    }

    const double clutch_s = dt / (dt + clutchRC);
    m_clutchPressure = m_clutchPressure * (1 - clutch_s) + m_targetClutchPressure * clutch_s;
    m_simulator->getTransmission()->setClutchPressure(m_clutchPressure);
}

void EngineSimApplication::renderScene() {
    getShaders()->ResetBaseColor();
    getShaders()->SetObjectTransform(ysMath::LoadIdentity());

    m_textRenderer.SetColor(ysColor::linearToSrgb(m_foreground));
    m_shaders.SetClearColor(ysColor::linearToSrgb(m_shadow));

    const int screenWidth = m_engine.GetGameWindow()->GetGameWidth();
    const int screenHeight = m_engine.GetGameWindow()->GetGameHeight();
    const float aspectRatio = screenWidth / (float)screenHeight;

    const Point cameraPos = m_engineView->getCameraPosition();
    m_shaders.m_cameraPosition = ysMath::LoadVector(cameraPos.x, cameraPos.y);

    m_shaders.CalculateUiCamera(screenWidth, screenHeight);

    m_learningPanel->setVisible(m_screen == 3);

    if (m_screen == 3) {
        const float side = (std::min)(screenWidth*0.48f, (std::max)(370.0f, screenWidth*0.31f));
        m_engineView->setDrawFrame(false);
        m_engineView->setBounds(Bounds(0, screenWidth-side, 0, static_cast<float>(screenHeight)));
        m_engineView->setLocalPosition({0,0});
        m_learningPanel->m_bounds = Bounds(screenWidth-side, static_cast<float>(screenWidth), 0, static_cast<float>(screenHeight));
        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(false); m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false); m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false); m_infoCluster->setVisible(false);
    }
    else if (m_screen == 0) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        Grid grid;
        grid.v_cells = 2;
        grid.h_cells = 3;
        Grid grid3x3;
        grid3x3.v_cells = 3;
        grid3x3.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 1, 0, 1, 1));
        m_engineView->setLocalPosition({ 0, 0 });

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 2);
        m_oscCluster->m_bounds = grid.get(windowBounds, 1, 1);
        m_performanceCluster->m_bounds = grid3x3.get(windowBounds, 0, 1);
        m_loadSimulationCluster->m_bounds = grid3x3.get(windowBounds, 0, 2);

        Grid grid1x3;
        grid1x3.v_cells = 3;
        grid1x3.h_cells = 1;
        m_mixerCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 2);
        m_infoCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 0, 1, 2);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(true);
        m_performanceCluster->setVisible(true);
        m_loadSimulationCluster->setVisible(true);
        m_mixerCluster->setVisible(true);
        m_infoCluster->setVisible(true);

        m_oscCluster->activate();
    }
    else if (m_screen == 1) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        m_engineView->setDrawFrame(false);
        m_engineView->setBounds(windowBounds);
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(false);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }
    else if (m_screen == 2) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        Grid grid;
        grid.v_cells = 1;
        grid.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 0, 0, 2, 1));
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 1);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }

    const float cameraAspectRatio =
        m_engineView->m_bounds.width() / m_engineView->m_bounds.height();
    m_engine.GetDevice()->ResizeRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height(),
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height()
    );
    m_engine.GetDevice()->RepositionRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.getPosition(Bounds::tl).x,
        screenHeight - m_engineView->m_bounds.getPosition(Bounds::tl).y
    );
    if (m_engineView->is3d()) {
        m_engineMesh.build(m_iceEngine, m_engineView->m_cutaway, m_learning.exploded);
        const auto center=m_engineMesh.center()+m_engineView->m_pan3d;
        const float radius=(std::max)(m_engineMesh.radius(),0.1f);
        const float fit=(std::max)(1.0f,1.0f/cameraAspectRatio);
        const float distance=radius*3.25f*fit/m_engineView->m_zoom3d;
        const float yaw=m_engineView->m_yaw, pitch=m_engineView->m_pitch;
        const EngineMesh::Vec3 outward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
        m_cameraEye3d=center+outward*distance;
        m_cameraForward3d=outward*-1;
        m_cameraRight3d=m_cameraForward3d.cross({0,1,0}).normalized();
        m_cameraUp3d=m_cameraRight3d.cross(m_cameraForward3d).normalized();
        m_cameraAspect3d=cameraAspectRatio;
        m_shaders.CalculateCamera3d(cameraAspectRatio,ysMath::LoadVector(center.x,center.y,center.z,1),
            distance,m_engineView->m_yaw,m_engineView->m_pitch);
    }
    else m_shaders.CalculateCamera(
        cameraAspectRatio * m_displayHeight / m_engineView->m_zoom,
        m_displayHeight / m_engineView->m_zoom,
        m_engineView->m_bounds,
        m_screenWidth,
        m_screenHeight,
        m_displayAngle);

    m_geometryGenerator.reset();

    render();

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryVertexBuffer,
        (char *)m_geometryGenerator.getVertexData(),
        sizeof(dbasic::Vertex) * m_geometryGenerator.getCurrentVertexCount(),
        0);

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryIndexBuffer,
        (char *)m_geometryGenerator.getIndexData(),
        sizeof(unsigned short) * m_geometryGenerator.getCurrentIndexCount(),
        0);
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();
    m_uiManager.initialize(this);

    m_engineView = m_uiManager.getRoot()->addElement<EngineView>();
    m_rightGaugeCluster = m_uiManager.getRoot()->addElement<RightGaugeCluster>();
    m_oscCluster = m_uiManager.getRoot()->addElement<OscilloscopeCluster>();
    m_performanceCluster = m_uiManager.getRoot()->addElement<PerformanceCluster>();
    m_loadSimulationCluster = m_uiManager.getRoot()->addElement<LoadSimulationCluster>();
    m_mixerCluster = m_uiManager.getRoot()->addElement<MixerCluster>();
    m_infoCluster = m_uiManager.getRoot()->addElement<InfoCluster>();
    m_learningPanel = m_uiManager.getRoot()->addElement<LearningPanel>();
    if (m_iceEngine) m_learning.cylinder = (std::min)(m_learning.cylinder, m_iceEngine->getCylinderCount()-1);

    m_infoCluster->setEngine(m_iceEngine);
    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);
    m_oscCluster->setSimulator(m_simulator);
    if (m_iceEngine != nullptr) {
        m_oscCluster->setDynoMaxRange(units::toRpm(m_iceEngine->getRedline()));
    }
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);
}

void EngineSimApplication::startRecording() {
    m_recording = true;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Encoder::VideoSettings settings{};

    // Output filename
    settings.fname = "../workspace/video_capture/engine_sim_video_capture.mp4";
    settings.inputWidth = m_engine.GetScreenWidth();
    settings.inputHeight = m_engine.GetScreenHeight();
    settings.width = settings.inputWidth;
    settings.height = settings.inputHeight;
    settings.hardwareEncoding = true;
    settings.inputAlpha = true;
    settings.bitRate = 40000000;

    m_encoder.run(settings, 2);
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::updateScreenSizeStability() {
    m_screenResolution[m_screenResolutionIndex][0] = m_engine.GetScreenWidth();
    m_screenResolution[m_screenResolutionIndex][1] = m_engine.GetScreenHeight();

    m_screenResolutionIndex = (m_screenResolutionIndex + 1) % ScreenResolutionHistoryLength;
}

bool EngineSimApplication::readyToRecord() {
    const int w = m_screenResolution[0][0];
    const int h = m_screenResolution[0][1];

    if (w <= 0 && h <= 0) return false;
    if ((w % 2) != 0 || (h % 2) != 0) return false;

    for (int i = 1; i < ScreenResolutionHistoryLength; ++i) {
        if (m_screenResolution[i][0] != w) return false;
        if (m_screenResolution[i][1] != h) return false;
    }

    return true;
}

void EngineSimApplication::stopRecording() {
    m_recording = false;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    m_encoder.commit();
    m_encoder.stop();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::recordFrame() {
#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Frame *frame = m_encoder.newFrame(false);
    if (frame != nullptr && m_encoder.getError() == atg_dtv::Encoder::Error::None) {
        m_engine.GetDevice()->ReadRenderTarget(m_engine.GetScreenRenderTarget(), frame->m_rgb);
    }

    m_encoder.submitFrame();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

bool EngineSimApplication::captureFrame(const std::string &path) {
    // Read this application's render target; no desktop or other windows are captured.
    const int w=m_engine.GetScreenWidth(), h=m_engine.GetScreenHeight();
    if(w<=0 || h<=0) return false;
    std::vector<uint8_t> rgba(static_cast<size_t>(w)*h*4);
    if(m_engine.GetDevice()->ReadRenderTarget(m_engine.GetScreenRenderTarget(),rgba.data()) != ysError::None) return false;
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    file.bfType=0x4D42; file.bfOffBits=sizeof(file)+sizeof(info);
    file.bfSize=file.bfOffBits+static_cast<DWORD>(rgba.size());
    info.biSize=sizeof(info); info.biWidth=w; info.biHeight=-h;
    info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
    for(size_t i=0;i<rgba.size();i+=4) std::swap(rgba[i],rgba[i+2]);
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(&file),sizeof(file));
    out.write(reinterpret_cast<const char*>(&info),sizeof(info));
    out.write(reinterpret_cast<const char*>(rgba.data()),rgba.size());
    return out.good();
}
