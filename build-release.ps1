$ErrorActionPreference = 'Stop'
# Builds the release in dist\release: the Setup.exe and .msi installers, the VST3 zip, the standalone exe and the source zip.
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$iscc = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw "Missing: $cmake" }

$version = [regex]::Match((Get-Content -Raw (Join-Path $projectRoot 'CMakeLists.txt')), 'project\(PHOQER VERSION ([0-9.]+)').Groups[1].Value
if (-not $version) { throw 'Could not read the version from CMakeLists.txt.' }

# JUCE's binary-data step breaks on build paths with parentheses, so build outside the project when needed.
# Named after the project folder, so two checkouts never share one build tree.
$buildRoot = if ($projectRoot -match '[()]') { Join-Path $env:LOCALAPPDATA ((Split-Path -Leaf $projectRoot) + '-build') } else { Join-Path $projectRoot 'build' }
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
} else { Write-Warning 'Inno Setup 6 not found: skipping the Setup.exe.' }
Compress-Archive -Path (Join-Path $dist 'PHOQER.vst3') -DestinationPath (Join-Path $release "PHOQER-$version-Windows-x64-VST3.zip") -Force
# The standalone ships as the bare exe: its licence texts are built in (HELP > ABOUT > OPEN TEXTS).
Copy-Item -LiteralPath (Join-Path $dist 'Standalone\PHOQER.exe') -Destination (Join-Path $release "PHOQER-$version-Windows-x64-Standalone.exe")

$wix = Join-Path ${env:ProgramFiles(x86)} 'WiX Toolset v3.14\bin'
if (Test-Path -LiteralPath $wix) {
    $obj = Join-Path $buildRoot 'msi'
    New-Item -ItemType Directory -Force -Path $obj | Out-Null
    & (Join-Path $wix 'candle.exe') -nologo -arch x64 "-dVersion=$version" "-dSourceDir=$projectRoot" "-dDistDir=$dist" -out "$obj\" (Join-Path $projectRoot 'installer\PHOQER.wxs')
    if ($LASTEXITCODE -ne 0) { throw 'MSI compile failed.' }
    & (Join-Path $wix 'light.exe') -nologo -pdbout "$obj\PHOQER.wixpdb" -out (Join-Path $release "PHOQER-$version-Windows-x64.msi") "$obj\PHOQER.wixobj"
    if ($LASTEXITCODE -ne 0) { throw 'MSI link failed.' }
} else { Write-Warning 'WiX Toolset 3.14 not found: skipping the MSI.' }

# The source at HEAD with JUCE included, so it builds without git.
$name = "PHOQER-$version"
$src = Join-Path $buildRoot 'source'
if (Test-Path -LiteralPath $src) { Remove-Item -LiteralPath $src -Recurse -Force }
New-Item -ItemType Directory -Force -Path $src | Out-Null
git -C $projectRoot archive --format=tar --prefix="$name/" -o (Join-Path $src 'main.tar') HEAD
if ($LASTEXITCODE -ne 0) { throw 'git archive failed.' }
git -C (Join-Path $projectRoot 'vendor\JUCE') archive --format=tar --prefix="$name/vendor/JUCE/" -o (Join-Path $src 'juce.tar') HEAD
if ($LASTEXITCODE -ne 0) { throw 'git archive of JUCE failed.' }
foreach ($t in 'main.tar', 'juce.tar') { tar -xf (Join-Path $src $t) -C $src; if ($LASTEXITCODE -ne 0) { throw "Unpacking $t failed." } }
tar -a -cf (Join-Path $release "$name-Source.zip") -C $src $name
if ($LASTEXITCODE -ne 0) { throw 'Source zip failed.' }
Get-ChildItem -LiteralPath $release | ForEach-Object { Write-Host "Built: $($_.FullName)" }
