$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$testBuild = Join-Path $repoRoot 'build/game-state-tests'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'MSVC C++ tools are required for this Windows host test.' }
$msvc = (Get-ChildItem (Join-Path $vsRoot 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdkVersion = (Get-ChildItem (Join-Path $sdk 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$game = Join-Path $repoRoot 'firmware/smart_desk_esp32/src/game'
$compilerArgs = @('/nologo', '/std:c++17', '/utf-8', '/EHsc', '/W4', '/WX', '/MD',
  "/I$game", "/I$PSScriptRoot", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/game_state_test.exe",
  "$game/pokemon.cpp", "$game/game_state.cpp", "$game/save_data.cpp", "$game/save_storage.cpp",
  "$game/exploration.cpp", "$game/encounter.cpp",
  "$PSScriptRoot/game_state_test.cpp", '/link', "/LIBPATH:$msvc/lib/x64",
  "/LIBPATH:$sdk/Lib/$sdkVersion/ucrt/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/um/x64")
& "$msvc/bin/Hostx64/x64/cl.exe" @compilerArgs
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
& "$testBuild/game_state_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }

# Compile the real app against host display/time/NVS fakes, in an asset-free sketch.
# Only these boundary headers are copied; no secrets, fonts or local assets.
$hostSketch = Join-Path $testBuild 'app-source'
$sketch = Join-Path $repoRoot 'firmware/smart_desk_esp32'
foreach ($relative in @('src/game/game_app.cpp', 'src/game/game_app.h', 'src/game/game_state.h',
    'src/game/pokemon.h', 'src/game/save_data.h', 'src/game/save_storage.h', 'src/game/exploration.h', 'src/game/encounter.h', 'src/core/app_config.h',
    'src/core/app_types.h', 'src/hardware/displays.h', 'src/services/network_time.h', 'hangul_renderer.h')) {
  $targetFile = Join-Path $hostSketch $relative
  New-Item -ItemType Directory -Force -Path (Split-Path $targetFile) | Out-Null
  Copy-Item -LiteralPath (Join-Path $sketch $relative) -Destination $targetFile
}
$appArgs = @('/nologo', '/std:c++17', '/utf-8', '/EHsc', '/W4', '/WX', '/wd4100', '/MD',
  "/I$PSScriptRoot/host_platform", "/I$hostSketch/src/game", "/I$PSScriptRoot", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/game_app_test.exe",
  "$game/pokemon.cpp", "$game/game_state.cpp", "$game/save_data.cpp", "$game/save_storage.cpp",
  "$game/exploration.cpp", "$game/encounter.cpp", "$hostSketch/src/game/game_app.cpp", "$PSScriptRoot/game_app_test.cpp",
  '/link', "/LIBPATH:$msvc/lib/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/ucrt/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/um/x64")
& "$msvc/bin/Hostx64/x64/cl.exe" @appArgs
if ($LASTEXITCODE -ne 0) { throw 'Host app compilation failed.' }
& "$testBuild/game_app_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host app tests failed.' }
