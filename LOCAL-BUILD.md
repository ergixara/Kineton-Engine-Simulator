# Local Windows build

This is the requested `ange-yaghi/engine-sim` repository, commit
`85f7c3b959a908ed5232ede4f1a4ac7eafe6b630`, with all five pinned submodules.
No code has been imported from the successor project.

## Rebuild and launch

From PowerShell in this directory:

```powershell
.\build-engine-sim.ps1
.\run-engine-sim.ps1
```

For a source-only rebuild with dependencies already installed, add
`-SkipDependencies`. Add `-Test` to run the complete local suite, including geometry,
learning-content, and input-backend tests (44 tests). The executable is `build/Release/engine-sim-app.exe`.
The launcher sets the working directory to `build`, required by the original
`../assets/main.mr` script path. The generated `build/Release/delta.conf` uses
relative paths to the original assets, fonts, and shaders. Keep the `assets`,
`es`, and `dependencies` directories in place. Edit `assets/main.mr` to select
another engine, then press Enter in the simulator to reload it.
Close the simulator before rebuilding changed C++ code, since Windows locks the
running executable. Incremental builds skip copying identical runtime DLLs.

The app now opens in **Engine Lab**, the learning workspace. See
[Learning Mode](LEARNING-MODE.md) for component inspection, labels, exploded view,
slow motion, telemetry explanations, and the implementation map. F6 returns to
the original dashboard; Tab cycles all four layouts. Engine selection still uses
the original engine scripts.

## Configuration

- Visual Studio Build Tools 2022 17.14.41; MSVC 19.44.35229, v143, x64.
- Windows SDK 10.0.26100.0; CMake 3.25.1; Release; C++17.
- Scripting ON, optional video capture OFF, optional Discord integration OFF.
  The bundled Discord integration uses an obsolete prebuilt library and its
  application include points at a nonexistent `../discord/Discord.h` path.
- vcpkg ports tag `2023.04.15`, commit
  `501db0f17ef6df184fcdbfbe0f87cde2313b6ab1`.
- SDL2 2.26.5, SDL2_image 2.6.3, Boost.Filesystem 1.82.0,
  libpng 1.6.39, zlib 1.2.13. Boost libraries are static with the shared MSVC CRT;
  SDL/image dependencies are DLLs deployed alongside the executable.
- WinFlexBison 2.5.25: Flex 2.6.4, Bison 3.8.2.
- Legacy D3DX10/11 and D3DCompiler 43 DLLs extracted from Microsoft's signed
  June 2010 redistributable and deployed locally. Vulkan loader is provided
  by the existing graphics installation.

## Setup recipes and compatibility

The sibling `../tools/setup-dependencies.ps1` can provision the local dependency
tree after Visual Studio C++ tools are installed. `../tools/triplets/x64-engine.cmake`
controls library linkage. The tools stay outside the simulator repository.

The old vcpkg runner could not recognize MSVC 14.44. The setup script uses the
runner and tool catalog shipped with Visual Studio while retaining pinned ports.
The old Boost modular build helper also needed a single `vc144` to `vc140`
library-name normalization rule, matching its existing rules for vc141–vc143.
Finally, its pkg-config downloader referenced a deleted MSYS pthread package.
The local acquisition recipe uses checksum-verified pkgconf 2.4.3 from the
Visual Studio vcpkg catalog instead. These changes affect build tooling only.

The initial build compatibility fix changed one repository configuration file:
`dependencies/submodules/delta-studio/CMakeLists.txt`. It linked both debug and
release D3DX libraries unconditionally, causing the Release executable to import
the unavailable `d3dx10d_43.dll`. The six D3DX entries now use CMake's
`optimized`/`debug` qualifiers. `dumpbin /dependents` confirms Release imports
`d3dx10_43.dll` and `d3dx11_43.dll`. The change is saved as
`patches/delta-studio-release-d3dx.patch`; the build script applies it to a fresh
submodule checkout and checks for conflicts rather than overwriting local edits.
The initial build fix changed no production C++ source, engine scripts, or physics
behavior. The subsequent 3D renderer and input fixes are described below. Input
changes to Delta are saved in `patches/delta-studio-input.patch`; the build script
applies and checks both patches when preparing the submodule.

The upstream README omits the legacy DirectX runtime requirements, fetched
GoogleTest dependency, working-directory requirement, and the need to select
Release explicitly with the Visual Studio multi-configuration generator.
Merely adding SDL or Boost to PATH is insufficient; the build script passes
the vcpkg toolchain, include/library directories, and Flex/Bison executables.

Full configure/compiler logs are retained under `build`. Dependency logs and
individual dependency compiler logs are under `../tools` and its vcpkg buildtrees.

## Test corrections and results (2026-09-18)

Release build and tests: **27 passed, 0 failed, 0 skipped** using
`build-engine-sim.ps1 -SkipDependencies -Test`. Each test ran in its own process
with a 20-second timeout. See `build/tests.xml` and
`build/tests.log`. The original six-failure result is retained
in `build/tests-before-2026-09-18.xml`.

Three test files were corrected after tracing their failures against the current
API contracts and production callers:

- `test/gas_system_tests.cpp`: negative equilibrium flow represents inflow, so
  incoming mole counts must be negated and use the donor gas's energy. Outflow
  must use the newly calculated `maxFlowOut` and the source gas's temperature.
  The previous tests mixed up signs, reused `maxFlowIn`, and used ambient energy
  for hot gas leaving a vessel. Pressure equality assertions retain their original
  tolerance; explicit flow-direction assertions were added.
- `test/function_test.cpp`: Gaussian smoothing blends adjacent samples rather
  than interpolating their exact values. The test now compares three positions
  against an independent analytical truncated-Gaussian weighted average within
  1e-5 (allowing cached-kernel interpolation error), retaining endpoint checks.
  The old noisy diagnostic print loop was removed.
- `test/synthesizer_tests.cpp`: the old 16/32 Hz fixtures were below the built-in
  filters' cutoffs and omitted mandatory convolution impulse responses. Their
  expected unfiltered ramp output no longer matched the audio pipeline. Fixtures
  now use 44.1 kHz, initialized identity impulse responses, disabled noise, and
  correctly sized output buffers with guard samples. Tests check silence, sample
  count, zero padding, nonzero positive/negative unclipped audio, and exact PCM
  agreement between synchronous and background rendering. The worker is joined
  before reading output because `waitProcessed()` reports input consumption.

No production C++ code, physics equations, or engine configuration was modified
to make these tests pass. These are local test corrections, not a claim that the
unmodified upstream test suite passes.

The background-rendering comparison additionally passed 20 consecutive runs
to check for timing-dependent failures (`../tools/audio-thread-repeat.log`).

## Runtime verification (2026-09-16)

- The locally compiled x64 Release executable launched successfully and remained
  responsive throughout several minutes of inspection.
- The v0.1.12a interface, gauges, fonts, shaders, and Subaru EJ25 2.5 L model rendered.
- `assets/main.mr` compiled with an empty `build/error_log.log`.
- Physical keyboard input operated the starter/dyno; live crankshaft motion,
  changing exhaust-flow traces, and approximately 506 RPM were observed during cranking.
- The user confirmed the engine started and subsequently confirmed audio is working.
- The simulator's Windows audio session was found with nonzero peak 0.415766090,
  session volume 100%, no mute, on JBL TUNE 310C USB-C headphones at 74% device
  volume. No output routing or system volume was changed.
- At that time, automated synthetic keyboard presses were ignored: the original raw-input
  handler discards events with a null device handle. Physical-keyboard validation
  was therefore performed by the user. Throttle, gear changes, and script reload
  were not individually verified. The later input update below addresses that
  backend limitation; this historical observation is not its current behavior.
- The simulator was left open for use. There were no observed application crashes;
  the initial suite's two access violations occurred in the separate test executable.

`../tools/verify-audio.cpp` and `../tools/audio-check/CMakeLists.txt` provide a
read-only process-specific audio-session meter. Its output is saved in
`build/audio-verification.txt`. `build/dll-imports.txt` records the executable's
actual DLL requirements. Upstream deprecation/conversion and CMake policy/version
warnings remain; the Release build completes successfully.

## Runtime recheck (2026-09-18)

The rebuilt executable was launched again using `run-engine-sim.ps1`. Windows
reported the process responsive with title `Engine Sim | AngeTheGreat | v0.1.12a`;
`build/error_log.log` was empty. Desktop automation was unavailable for this
recheck, so this is not a new visual inspection.

The read-only audio meter found the application's audio session and measured a
nonzero peak of 0.039885737. The current default endpoint was Speakers
(Realtek(R) Audio), at 40.1% volume and **muted**; the application session was
100% and unmuted. No audio settings were changed. This differs from the headphone
output on which the user confirmed audible sound on September 16. Measurement:
`build/audio-verification-2026-09-18.txt`.

Additional physical-keyboard checks for throttle, gear changes, and script reload
have been requested from the user and remain unconfirmed. The earlier confirmed
engine start and audible sound remain the basis for those runtime checks.

## Native 3D engine view (2026-09-18)

The first native 3D version added a larger engine viewport alongside the existing
gauges. It uses actual 3D triangle geometry, perspective projection, a depth
buffer, smooth surface normals, and three lights in the existing DirectX 11
renderer. No external viewer or additional dependency is needed.

The model shows pistons and rings, connecting rods, crankshaft journals and webs,
a flywheel with rotating bolts/timing mark, cutaway cylinder sleeves, and two
representative valves per cylinder. Piston/rod positions, crank rotation, bore,
bank angles, deck heights, and valve lifts come from the loaded engine and live
simulation. Axial journal spacing, exterior dimensions, valve arrangement, and
decorative parts are inferred for visualization: this is a schematic cutaway,
not a dimensionally complete CAD model. The original 2D physics and audio are
unchanged. All cylinders are shown in 3D; the original layer selection applies
to the 2D view.

Controls (the viewport buttons call the same actions as their keyboard shortcuts):

| Control | Action |
| --- | --- |
| Left drag over the engine | Orbit the 3D camera |
| Right or middle drag over the engine | Pan the camera target |
| Mouse wheel over the engine | Zoom |
| F1 / F2 | Rotate camera left/right |
| F3 / RESET | Reset camera orbit, zoom, and pan |
| F4 / 3D or 2D button | Switch 3D and original 2D views |
| F5 / CUTAWAY or SOLID button | Open/close cylinder sleeves |
| Tab | Cycle the dashboard layouts, now including Engine Lab |
| A, then hold S | Ignition, then starter (original controls) |
| Enter | Reload the engine script; return to the default 3D camera |

Implementation changes:

- `include/engine_mesh.h`, `src/engine_mesh.cpp`: procedural geometry and
  simulation-driven assembly; no physics state is modified by rendering.
- `include/geometry_generator.h`, `src/geometry_generator.cpp`: upload triangles
  with positions and normals, with capacity and 16-bit index checks.
- `include/shaders.h`, `src/shaders.cpp`: perspective orbit camera and lighting.
- `include/engine_view.h`, `src/engine_view.cpp`: orbit/zoom, mode/reset/cutaway
  buttons, and visible control hints.
- `include/engine_sim_application.h`, `src/engine_sim_application.cpp`: depth
  buffer, 3D rendering, larger vertex buffers, camera keys, default layout,
  and framebuffer-based runtime verification.
- `src/piston_object.cpp`, `src/connecting_rod_object.cpp`,
  `src/crankshaft_object.cpp`, `src/cylinder_head_object.cpp`, `src/ui_element.cpp`:
  explicitly disable depth tests for original 2D models and HUD elements so
  their existing painter's order is preserved.
- `src/main.cpp`, `run-engine-sim.ps1`: optional `--verify-3d` / `-Verify3d`
  verification mode.
- `CMakeLists.txt`, `test/engine_mesh_tests.cpp`, `build-engine-sim.ps1`: compile
  geometry and run three new tests covering outward winding/unit normals on
  multiple axes, cutaway bore/cut edges, and degenerate or missing input.

Run the repeatable rendered-frame check with the simulator closed:

```powershell
.\run-engine-sim.ps1 -Verify3d
```

The current check runs ignition/starter, exercises orbit and zoom handlers,
changes cutaway/solid/2D modes, and reloads the script. It then tests Learning
Mode selection, labels, exploded view, pause/resume, 1/40-speed physics,
telemetry explanations, sidebar scrolling, compact layouts, and dyno readings.
It captures **16 application framebuffers** to `build/visual-check-*.bmp`, then
exits. `build/visual-check.log` records modes, geometry counts, RPM, crank angles,
individual assertions, and PASS/FAIL. Capture, engine-load, motion, or learning
assertion failures produce a failure exit code. The check uses native hit routing
and UI handlers; it does not verify physical Windows input delivery.

Historical September 18 verification: the x64 Release build and all **30 tests passed**. The
rendered-frame check exited successfully with five captures, including 3D
cutaway from two camera positions, solid sleeves, the original 2D view, and a
fresh 3D view after script reload. The measured engine speed during the check
was 738–791 RPM, with changing crank angles. Captures were visually inspected;
the script error log was empty. `build/3d-preview.png` is a compact copy of the
default cutaway capture. The normal application was relaunched in 3D afterward.

## Learning workspace update (2026-09-19)

Engine Lab adds selectable 3D components, projected labels, short component
lessons, grouped live telemetry with explanations, an exploded teaching view,
pause, and persistent 1/40-speed playback. The model now distinguishes heads,
camshafts, intake/exhaust passages and valves, spark plugs, and combustion areas.
The crank journals and webs follow the live shaft rotation. Rendering and
exploded spacing do not alter production physics or engine scripts; slow motion
deliberately slows the whole simulation and its synthesized audio.

Roboto text now renders from its correct font atlas: the unused Delta console
no longer replaces the app's atlas at the end of a frame. The initial window
fits the Windows desktop work area so controls remain above the taskbar. Compact
classic dashboards omit the extra lower viewport buttons and use a shorter hint
line to keep the mechanism visible; original controls remain available.

The current Release build completed and **39 tests passed, 0 failed**. The suite
now also checks closest-hit component picking, label anchors, complete lesson
metadata, exploded-view state isolation, crank rotation, valve lift, throttle
rotation, and cam/combustion response. Logs: `build/tests.log`, `build/tests.xml`,
and `../tools/learning-build.log`.

The Subaru EJ25 runtime check also **passed with 16 rendered captures**. Its
native hit-routing and UI-handler assertions covered component labels and mesh
picking, exploded view, pause freezing physics, resume, persistent 1/40 speed,
labels disappearing from the click regions when hidden, Live Data and Controls
tabs, MAP explanation and linked highlighting, a lower drivetrain metric's
scroll-to-explanation behavior, component selection at 1280 x 720, all three
classic layouts, and dyno torque/power/BMEP availability. Runtime log:
`build/learning-ej25-runtime.log`.

The same **16-capture check passed with the GM LS V8**, covering another bank
layout and a larger mesh (about 110,000 vertices, versus 59,000 for the EJ25).
Its results are saved in `build/learning-v8-runtime.log`. A deliberately invalid
startup script also exited cleanly: the verifier returned the expected failure
exit code 1 with no engine and zero captures (`build/learning-empty-runtime.log`).
That check confirms graceful handling of the missing engine, not a successful
3D render; its no-engine screen was not visually verified. The original
`assets/main.mr` was restored byte-for-byte, confirmed
by SHA-256. The generic `visual-check.log` and current BMP captures correspond
to the V8 run; the named logs preserve each engine's result.

The normal application was then launched with the Subaru EJ25 in Engine Lab.
A Windows screenshot was inspected: the controls fit above the taskbar, and
`build/error_log.log` was empty after script compilation. The application is
ready with the engine stopped; this is not a claim that combustion is currently
running.

At the time of those checks, Delta's hardware-only Windows input backend ignored
synthetic mouse and keyboard events. Those checks therefore validated native hit
regions and handlers after rendering, not Windows input delivery. The input update
below fixes that backend limitation. The user's
earlier confirmation that engine audio works remains the auditory verification;
this run does not add a fresh listening check. The September 18 results above
are retained as history of the earlier 3D version.

## Input and interaction update (2026-09-19)

The current UI, model, lessons, telemetry, and simulator features are preserved.
This update changes input routing and camera interaction, without redesigning the
workspace.

The input audit found that the backend discarded raw-input events with a null
device handle, a case used by touchpads and accessibility-generated input. The
frame-polled button state could also lose a press followed by a release before
the next UI update, and hit testing used a cached mouse position. Delta now
accepts these events, queues mouse button edges and movement with their event
positions, preserves keyboard transitions, and reads the current Windows cursor
for hover and continued dragging. Pointer positions are converted to the same
client/render coordinates as the UI. Focus loss or capture loss clears held input
and cancels the active gesture so a drag or starter cannot remain stuck.

`SimulatorAction` is the common dispatch for F3-F9, ignition, pause, and the
corresponding viewport and sidebar buttons. Starter input also uses this dispatch,
with separate pointer and keyboard held states: releasing one source does not
stop the starter while the other is held. Both sources clear on focus loss and
script reload. Existing active-button colors and captions derive from the shared
simulator and learning state.

Left drag orbits around the engine target, right or middle drag pans that target,
and the wheel zooms. A left-button gesture becomes a drag after moving more than
five pixels; that decision stays latched until release, even if the pointer returns to its
starting point. A short click still selects a part or label. Gestures beginning
on buttons, labels, tabs, or sidebar entries cannot orbit the engine. F3 and the
Reset button both restore default orbit, zoom, and pan. Pitch and zoom remain
bounded; original modifier-plus-wheel tuning controls remain available.

Implementation: `include/simulator_action.h`, the application input/action
sections, `src/ui_manager.cpp`, `src/engine_view.cpp`, the sidebar action bindings,
and `patches/delta-studio-input.patch`. Backend regression coverage is in
`test/input_backend_tests.cpp`.

The input-update Release build completed and **44 tests passed, 0 failed**.
This includes backend regression tests for ordered input transitions. Build and
test output is in `../tools/input-build.log`. The expanded runtime verifier also
**passed with 16 captures**, including every input assertion, separate and combined
starter sources, camera gestures, and retained Learning Mode checks. Its result is
saved in `build/input-runtime.log`.

The launched program was also exercised through native Windows mouse and keyboard
events. These checks verified F4 view, F5 cutaway, F6 Learn, F7 labels, F8 exploded
view, F9 slow motion, A ignition, P pause, and synchronized button states when
alternating between clicks and keys. Left-button orbit, wheel zoom, the Reset
button, mesh and projected-label selection, button-drag isolation, Live Data/MAP
selection, and sidebar scrolling also worked through the Windows input path.
Further native checks verified F3 restoring the default angle, the Parts and
Controls tabs, the cylinder-next arrow changing cylinder 1 to 2, and the
Connecting Rod list entry selecting both its highlighted geometry and lesson.
Turning labels off in the sidebar and back on with F7 kept both button styles
synchronized. Enabling exploded view in the sidebar updated the viewport button
and geometry together. Sidebar Pause, Slow, and Classic UI actions were covered
by runtime regression rather than separate native clicks.

During the native session, user input also visibly engaged the starter: the UI
showed **STARTER ENGAGED**, approximately 234 RPM, and an active Hold to start
button, followed by ENGINE STOPPED and the released button. Ignition was off, so
this verifies starter engagement/release rather than successful combustion.

Native automation provides short left-button drags. Combined starter sources and
right/middle-button pan gestures were checked by the runtime regression path;
right/middle pan was not exercised as a full native held gesture.
The earlier audio confirmation remains the auditory verification; no new listening
check is claimed. The 39-test and earlier rendered-frame results above remain as
history of the preceding Learning Mode build.
