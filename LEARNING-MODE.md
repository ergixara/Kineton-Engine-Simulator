# Engine Lab / Learning Mode

Engine Lab is the default workspace in the native simulator. It combines the
live 3D engine with an inspector sidebar. The original simulation, engine-script
selection, gauges, scopes, dyno analysis, drivetrain controls, and audio mixer
remain available. Press **F6** or **Classic UI** to return to the dashboard;
**Tab** cycles all four layouts. **F4** switches between 3D and the original 2D
engine drawing.

The initial window fits the desktop work area, and the bundled Roboto font keeps
the lessons readable. The compact classic dashboard leaves out the extra lower
viewport buttons so they do not cover the mechanism; the original controls stay
available.

## Explore the engine

Click a visible part in the 3D model or one of its labels. The selected region
turns gold and the **Parts** tab explains what it does, what it connects to, and
what changes or damage can affect. The sidebar's component list provides access
to parts hidden by other geometry. Scroll the sidebar to reach the list, then
use the cylinder arrows to inspect another cylinder. Shared components, such as
the crankshaft or an intake throttle, are selected as shared parts.

The inspector covers pistons, connecting rods, crankshaft, flywheel, cylinders,
heads, cams, intake/exhaust valves, spark plugs, combustion chambers,
intake/exhaust runners, and throttles. Cyan identifies the intake path, orange
the exhaust, purple the cam geometry, and gold the selection and ignition areas.
Labels have leader lines to their component anchors and can be turned off.

| Control | Action |
| --- | --- |
| Click a part or label | Select it and open its lesson |
| Left drag over the model | Orbit around the engine target |
| Right or middle drag over the model | Pan the camera target |
| Wheel over the model | Zoom |
| F1 / F2 | Rotate the camera left / right |
| F3 / Reset button | Reset camera orbit, zoom, and pan |
| F4 | Switch 3D / 2D |
| F5 | Toggle cutaway / solid cylinder sleeves |
| F6 | Switch Engine Lab / classic dashboard |
| F7 | Toggle component labels |
| F8 | Toggle exploded spacing |
| F9 | Toggle persistent slow motion at 1/40 speed |
| P / Right | Pause or resume / advance one simulation frame while paused |

Viewport buttons provide the same view controls, plus ignition and a
**Hold to start** button. The original **A** ignition and held **S** starter keys
still work. Exploded view separates sleeves and head-related teaching geometry
to expose the internals; pistons, rods, and crank keep their simulated positions.
Slow motion affects the entire simulation and sound synthesis. The original
held number-key speed overrides remain available.

Camera gestures stay within the viewport: pressing or dragging on a button,
label, tab, or sidebar entry does not orbit the engine. A movement of more than
five pixels turns a left-button gesture into an orbit and suppresses component
selection until release, even if the pointer returns to where it started. Short
clicks still inspect components. Pitch limits prevent upside-down views, and
bounded zoom prevents unhelpfully close or distant views. Reset restores the
default centered view, including any pan offset.

Keyboard shortcuts and their buttons use the same `SimulatorAction` dispatch.
Existing active colors and captions stay synchronized when alternating between
mouse and keyboard. Holding **S** and **Hold to start** together keeps the starter
engaged until both are released; losing focus or reloading clears held starter
input. No interface styling or simulator feature was changed for this input fix.

## Understand the readings

The sidebar keeps engine status, RPM, and actual throttle plate opening visible.
**Live Data** groups readings into Engine Core, Air / Fuel, Performance / Dyno,
Temperature / Pressure, Drivetrain, and Simulation. Every reading has an explicit
unit or state. Click a row to show its current value and a short explanation at
the top of the tab; related components are highlighted in the model.

Explanations distinguish absolute manifold pressure from vacuum, instantaneous
cylinder pressure from BMEP, gas temperature from metal or coolant temperature,
and consumed fuel from remaining fuel. Torque and power indicate **DYNO OFF**
until the dynamometer is enabled. Undefined or unavailable measurements show
**N/A**. Power, torque, and road-speed units follow the existing app settings.

The **Controls** tab documents the original throttle, clutch, gear, dyno, sound,
simulation-frequency, script-reload, and window shortcuts. Modifier-plus-wheel
tuning gestures remain available over both the viewport and sidebar. Engine
selection remains in `assets/main.mr`; press **Enter** to reload after editing.

## What the drawing represents

The rendering reads live piston/rod poses, crank angle, valve lift, cam timing,
throttle angle, and combustion state. Piston bore, stroke, bank arrangement, and
deck dimensions come from the engine. The geometry is an educational schematic,
not a complete CAD model: axial spacing, head castings, valve arrangement, cam
placement, and pipe routing are inferred. Valves are representative rather than
a promise of the exact real-world valve count. The combustion glow illustrates
the simulation's burning state; it is not a resolved flame or temperature map.
Fuel is mixed by the existing intake model; no separate injector is drawn.

Lessons explain possible real-world effects of component changes or failure;
they do not add a component-damage simulation. Labels and exploded spacing only
change presentation. Production physics equations and engine scripts are
unchanged. No externally hosted viewer or new rendering dependency is required.

## Implementation map

| Files | Responsibility |
| --- | --- |
| `include/learning_content.h`, `src/learning_content.cpp` | Part identifiers, lesson metadata, selected cylinder, learning display state |
| `include/engine_mesh.h`, `src/engine_mesh.cpp` | Procedural 3D geometry, semantic part/cylinder tags, closest-triangle ray picking, label anchors, exploded spacing |
| `include/engine_view.h`, `src/engine_view.cpp` | Camera interaction, viewport buttons, clickable projected labels |
| `include/simulator_action.h`, application action/input sections | Shared keyboard and button actions, separate pointer/keyboard starter sources |
| `include/ui_manager.h`, `src/ui_manager.cpp` | Ordered pointer dispatch, click/drag threshold, viewport gesture ownership, focus/capture cancellation |
| `patches/delta-studio-input.patch`, `test/input_backend_tests.cpp` | Queued mouse edges and moves, keyboard transitions, current OS cursor coordinates, touchpad/accessibility input support, backend regression checks |
| `include/learning_panel.h`, `src/learning_panel.cpp` | Parts / Live Data / Controls tabs, readable lessons, grouped metrics and explanations |
| `include/engine_sim_application.h`, `src/engine_sim_application.cpp` | Workspace layout, camera projection, selection highlighting, simulation speed and pause integration |
| `src/geometry_generator.cpp`, `src/shaders.cpp` | Triangle upload, perspective rendering, lighting and original 2D compatibility |
| `src/learning_verification.cpp` | Repeatable native hit-routing and UI-handler checks within the rendered-frame verifier |
| `test/engine_mesh_tests.cpp`, `test/learning_content_tests.cpp` | Geometry, selection, live-motion, presentation isolation, and lesson coverage checks |

Build and test instructions and dated verification results are in
[LOCAL-BUILD.md](LOCAL-BUILD.md). Before the input update, all **39 tests passed**, and
the Subaru EJ25 and GM LS V8 rendered-frame checks each passed with **16 captures**.
They exercise part/label selection, exploded view, pause/resume, slow motion, metric
explanations, compact sidebar interaction, original layouts, and dyno readings.
Run it with `run-engine-sim.ps1 -Verify3d` while the simulator is closed.

A deliberately invalid startup script also exited cleanly with the expected
no-engine verification failure (exit code 1, zero captures). This is an error-path
check, not a successful render; the no-engine screen was not visually verified.
The original engine selection script was restored byte-for-byte afterward. The
normal Subaru Engine Lab was then launched and its Windows screenshot inspected:
controls fit above the taskbar and the script error log was empty. It was left
ready with the engine stopped.

Those earlier checks sent actions through native hit routing and UI handlers and
did not cover Windows input delivery. The current backend update accepts
null-device events used by touchpads and accessibility input, queues mouse edges
and movement, and cancels held input after focus/capture loss. This replaces the
previous behavior that discarded synthetic Windows input.

The input-update Release build now passes **44 tests**, and the expanded runtime
check passes **16 captures** with all input assertions, including combined mouse
and keyboard starter holds. Results are in `../tools/input-build.log` and
`build/input-runtime.log`.

Native Windows clicks and keys also verified the F4-F9 controls, ignition, pause,
matching active states, left-drag orbit, wheel zoom, the Reset button, component
and projected-label selection, button-drag isolation, Live Data/MAP selection, and
sidebar scrolling. Additional native checks verified F3 reset, Parts and Controls
tabs, cylinder selection changing from 1 to 2, and the Connecting Rod list entry
opening its lesson and highlighting its geometry. Sidebar labels-off followed by
F7 labels-on stayed synchronized, as did sidebar exploded-view activation and the
viewport button/model. Sidebar Pause, Slow, and Classic UI were verified through
runtime regression rather than separate native clicks.

User input during the native session also visibly engaged and
released the starter at approximately 234 RPM with matching button feedback.
Ignition was off, so this does not claim a combustion start. Native automation
provides short left drags; combined starter sources and right/middle-button panning
were covered by runtime regression, and right/middle pan was not exercised as a
full native held gesture. The user's earlier confirmation of working audio remains
valid; no new auditory check is claimed.

Possible next additions include a guided four-stroke walkthrough, timing diagrams
linked to crank angle, and animated flow indicators derived from measured gas
flow. These are future ideas, not features in this build.
