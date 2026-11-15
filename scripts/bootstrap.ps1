param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$toolDir = Join-Path $projectDir 'tools'
New-Item -ItemType Directory -Force $toolDir | Out-Null
$vcpkgDir = Join-Path $toolDir 'vcpkg'
if (-not (Test-Path "$vcpkgDir/.git")) {
    & git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg clone failed' }
}
& git -C $vcpkgDir checkout 44ccfd88b3eca6929d6d16398c7b19945c29a5f6
if ($LASTEXITCODE -ne 0) { throw 'vcpkg checkout failed' }
& "$vcpkgDir/bootstrap-vcpkg.bat" -disableMetrics
if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed' }
& $Python -m pip install --target "$toolDir/python-packages" cmake==4.4.4
if ($LASTEXITCODE -ne 0) { throw 'CMake setup failed' }
if (-not (Test-Path "$toolDir/ninja/ninja.exe")) {
    Invoke-WebRequest 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip' -OutFile "$toolDir/ninja.zip"
    Expand-Archive -LiteralPath "$toolDir/ninja.zip" -DestinationPath "$toolDir/ninja"
}
if (-not (Test-Path "$toolDir/msvc/VC/Tools/MSVC")) {
    Invoke-WebRequest 'https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/portable-msvc.py' -OutFile "$toolDir/portable-msvc.py"
    Push-Location $toolDir
    try {
        & $Python -u portable-msvc.py --accept-license --vs 2022 --msvc-version 14.44.17.14 --sdk-version 26100
        if ($LASTEXITCODE -ne 0) { throw 'Portable compiler setup failed' }
    } finally { Pop-Location }
}
