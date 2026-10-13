$ErrorActionPreference = 'Stop'
# Builds the release: PHOQER-<version>-Windows-x64-Setup.exe and the VST3 / standalone zips in dist\release.
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$iscc = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw "Missing: $cmake" }

$version = [regex]::Match((Get-Content -Raw (Join-Path $projectRoot 'CMakeLists.txt')), 'project\(PHOQER VERSION ([0-9.]+)').Groups[1].Value
if (-not $version) { throw 'Could not read the version from CMakeLists.txt.' }

# JUCE's binary-data step breaks on build paths with parentheses, so build outside the project when needed.
$buildRoot = if ($projectRoot -match '[()]') { Join-Path $env:LOCALAPPDATA 'PHOQER-build' } else { Join-Path $projectRoot 'build' }
& $cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $buildRoot --config Release --target PHOQER_Standalone PHOQER_VST3 --parallel
if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }

$artefacts = Join-Path $buildRoot 'PHOQER_artefacts\Release'
$dist = Join-Path $projectRoot 'dist'
$release = Join-Path $dist 'release'
if (Test-Path -LiteralPath $dist) { Remove-Item -LiteralPath $dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $release | Out-Null
Copy-Item -LiteralPath (Join-Path $artefacts 'VST3\PHOQER.vst3') -Destination $dist -Recurse
Copy-Item -LiteralPath (Join-Path $artefacts 'Standalone') -Destination (Join-Path $dist 'Standalone') -Recurse

if (Test-Path -LiteralPath $iscc) {
    & $iscc "/DMyAppVersion=$version" (Join-Path $projectRoot 'installer\PHOQER.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }
} else { Write-Warning 'Inno Setup 6 not found: skipping the installer, building the zips only.' }
Compress-Archive -Path (Join-Path $dist 'PHOQER.vst3') -DestinationPath (Join-Path $release "PHOQER-$version-Windows-x64-VST3.zip") -Force
Compress-Archive -Path (Join-Path $dist 'Standalone\*') -DestinationPath (Join-Path $release "PHOQER-$version-Windows-x64-Standalone.zip") -Force
Get-ChildItem -LiteralPath $release | ForEach-Object { Write-Host "Built: $($_.FullName)" }
