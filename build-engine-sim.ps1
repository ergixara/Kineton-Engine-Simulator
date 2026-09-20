[CmdletBinding()]
param([switch]$SkipDependencies, [switch]$Test)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$tools = Join-Path (Split-Path $root -Parent) 'tools'
$vcpkg = Join-Path $tools 'vcpkg'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio 2022 Build Tools with the Desktop development with C++ workload.' }
$cmake = Join-Path $vcpkg 'downloads/tools/cmake-3.25.1-windows/cmake-3.25.1-windows-i386/bin/cmake.exe'
if (!(Test-Path $cmake) -or !(Test-Path "$tools/winflexbison/win_bison.exe") -or !(Test-Path "$tools/directx-runtime/d3dx11_43.dll")) {
    & "$tools/setup-dependencies.ps1"
}
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$delta = Join-Path $root 'dependencies/submodules/delta-studio'
foreach ($patchName in @('delta-studio-release-d3dx.patch', 'delta-studio-input.patch')) {
    $patch = Join-Path $root "patches/$patchName"
    & git -C $delta apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -ne 0) {
        & git -C $delta apply --check $patch
        if ($LASTEXITCODE) { throw "$patchName conflicts with local changes; inspect the delta-studio submodule." }
        & git -C $delta apply $patch
        if ($LASTEXITCODE) { throw "Unable to apply $patchName." }
    }
}
if (!$SkipDependencies) {
    & "$vcpkg/vcpkg.exe" install sdl2:x64-engine sdl2-image:x64-engine boost-filesystem:x64-engine "--overlay-triplets=$tools/triplets" --disable-metrics 2>&1 | Tee-Object "$build/dependencies.log"
    if ($LASTEXITCODE) { throw 'Dependency installation failed; see build/dependencies.log.' }
}
$prefix = "$vcpkg/installed/x64-engine".Replace('\','/')
$configure = @('-S', $root, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_GENERATOR_INSTANCE=$vs", "-DCMAKE_TOOLCHAIN_FILE=$vcpkg/scripts/buildsystems/vcpkg.cmake",
    '-DVCPKG_TARGET_TRIPLET=x64-engine',
    "-DFLEX_EXECUTABLE=$tools/winflexbison/win_flex.exe", "-DBISON_EXECUTABLE=$tools/winflexbison/win_bison.exe",
    "-DSDL2_INCLUDE_DIR=$prefix/include/SDL2", "-DSDL2_LIBRARY=$prefix/lib/SDL2.lib",
    "-DSDL2_IMAGE_INCLUDE_DIR=$prefix/include/SDL2", "-DSDL2_IMAGE_LIBRARY=$prefix/lib/SDL2_image.lib",
    '-DDISCORD_ENABLED=OFF', '-DDTV=OFF', '-DPIRANHA_ENABLED=ON')
& $cmake @configure 2>&1 | Tee-Object "$build/configure.log"
if ($LASTEXITCODE) { throw 'CMake configuration failed; see build/configure.log.' }
& $cmake --build $build --config Release --target engine-sim-app engine-sim-test --parallel 4 2>&1 | Tee-Object "$build/compile.log"
if ($LASTEXITCODE) { throw 'Build failed; see build/compile.log.' }
$bin = Join-Path $build 'Release'
Get-ChildItem "$prefix/bin/*.dll", "$tools/directx-runtime/*.dll" | ForEach-Object {
    # Leave identical loaded DLLs alone during an incremental rebuild.
    & $cmake -E copy_if_different $_.FullName $bin
    if ($LASTEXITCODE) { throw "Could not deploy $($_.Name). Close Engine Simulator and rebuild." }
}
@('../../dependencies/submodules/delta-studio/engines/basic', '../../assets') | Set-Content "$bin/delta.conf" -Encoding ascii
if ($Test) {
    $ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
    & $ctest --test-dir $build -C Release --timeout 20 --output-log "$build/tests.log" --output-junit "$build/tests.xml"
    if ($LASTEXITCODE) { throw 'Tests failed; see build/tests.log and build/tests.xml. The executable was built and deployed.' }
}
Write-Host "Built: $bin/engine-sim-app.exe"
Write-Host "Launch: & '$root/run-engine-sim.ps1'"
