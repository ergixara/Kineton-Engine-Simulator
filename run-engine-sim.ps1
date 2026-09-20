param([switch]$Verify3d)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'build/Release/engine-sim-app.exe'
if (!(Test-Path $exe)) { throw 'Build first using build-engine-sim.ps1.' }
# The original script loader resolves ../assets/main.mr from its working directory.
if ($Verify3d) {
    $checkProcess = Start-Process -FilePath $exe -WorkingDirectory (Join-Path $PSScriptRoot 'build') -ArgumentList '--verify-3d' -PassThru
    $checkProcess.WaitForExit()
    if ($checkProcess.ExitCode) { throw 'Visual check failed; see build/visual-check.log.' }
    Write-Host '3D runtime check passed. See build/visual-check.log and visual-check-*.bmp.'
} else {
    Start-Process -FilePath $exe -WorkingDirectory (Join-Path $PSScriptRoot 'build')
}
