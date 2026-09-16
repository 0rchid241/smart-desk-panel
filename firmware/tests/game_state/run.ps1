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
  "/I$game", "/I$PSScriptRoot", "/I$PSScriptRoot/host_platform", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/game_state_test.exe",
  "$game/pokemon.cpp", "$game/pokemon_record_codec.cpp", "$game/game_state.cpp", "$game/save_data.cpp", "$game/save_storage.cpp", "$game/box_data.cpp", "$game/box_storage.cpp",
  "$game/exploration.cpp", "$game/encounter.cpp", "$game/type.cpp", "$game/move.cpp", "$game/battle.cpp",
  "$PSScriptRoot/game_state_test.cpp", "$PSScriptRoot/save_wire_test.cpp", "$PSScriptRoot/save_pair_test.cpp", "$PSScriptRoot/battle_test.cpp", '/link', "/LIBPATH:$msvc/lib/x64",
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
    'src/game/box_storage.h', 'src/game/box_data.h', 'src/game/pokemon_record_codec.h', 'src/game/type.h', 'src/game/move.h', 'src/game/battle.h', 'src/game/pokemon.h', 'src/game/save_data.h', 'src/game/save_storage.h', 'src/game/exploration.h', 'src/game/encounter.h', 'src/core/app_config.h',
    'src/core/app_types.h', 'src/hardware/displays.h', 'src/services/network_time.h', 'hangul_renderer.h')) {
  $targetFile = Join-Path $hostSketch $relative
  New-Item -ItemType Directory -Force -Path (Split-Path $targetFile) | Out-Null
  Copy-Item -LiteralPath (Join-Path $sketch $relative) -Destination $targetFile
}
$appArgs = @('/nologo', '/std:c++17', '/utf-8', '/EHsc', '/W4', '/WX', '/wd4100', '/MD',
  "/I$PSScriptRoot/host_platform", "/I$hostSketch/src/game", "/I$PSScriptRoot", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/game_app_test.exe",
  "$game/pokemon.cpp", "$game/pokemon_record_codec.cpp", "$game/game_state.cpp", "$game/save_data.cpp", "$game/save_storage.cpp", "$game/box_data.cpp", "$game/box_storage.cpp",
  "$game/exploration.cpp", "$game/encounter.cpp", "$game/type.cpp", "$game/move.cpp", "$game/battle.cpp", "$hostSketch/src/game/game_app.cpp", "$PSScriptRoot/game_app_test.cpp",
  '/link', "/LIBPATH:$msvc/lib/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/ucrt/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/um/x64")
& "$msvc/bin/Hostx64/x64/cl.exe" @appArgs
if ($LASTEXITCODE -ne 0) { throw 'Host app compilation failed.' }
& "$testBuild/game_app_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host app tests failed.' }

# 원본 bitmap 대신 테스트 전용 도형으로 애셋 경로를 실행한다.
$assetSketch = Join-Path $testBuild 'asset-app-source'
New-Item -ItemType Directory -Force -Path $assetSketch | Out-Null
Copy-Item -Path "$hostSketch/*" -Destination $assetSketch -Recurse -Force
$assetDir = Join-Path $assetSketch 'local_game_assets/pokemon'
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
@'
#pragma once
#include <cstdint>
constexpr int PIKACHU_IDLE_FRAME_COUNT = 6;
constexpr int PIKACHU_IDLE_FRAME_WIDTH = 40;
constexpr int PIKACHU_IDLE_FRAME_HEIGHT = 56;
const uint8_t hostIdle[280] = {0x80};
const uint8_t* const pikachu_idle_frames[6] = {hostIdle,hostIdle,hostIdle,hostIdle,hostIdle,hostIdle};
'@ | Set-Content -LiteralPath (Join-Path $assetDir 'pikachu_idle_1bit.h') -Encoding utf8
@'
#pragma once
#include <cstdint>
constexpr int WILD_SPRITE_WIDTH = 48;
constexpr int WILD_SPRITE_HEIGHT = 48;
// 4 blocks: 3/1/2/4 white pixels. Strict majority emits exactly 2 pixels.
const uint8_t pidgey_wild_1bit[288] = {0xeb,0,0,0,0,0,0x87};
const uint8_t rattata_wild_1bit[288] = {0xeb,0,0,0,0,0,0x87};
const uint8_t pikachu_wild_1bit[288] = {0xeb,0,0,0,0,0,0x87};
'@ | Set-Content -LiteralPath (Join-Path $assetDir 'wild_encounter_1bit.h') -Encoding utf8
$assetArgs = @($appArgs | ForEach-Object {
  $_.Replace($hostSketch, $assetSketch).Replace('game_app_test', 'game_app_asset_test')
})
& "$msvc/bin/Hostx64/x64/cl.exe" @assetArgs
if ($LASTEXITCODE -ne 0) { throw 'Host asset app compilation failed.' }
& "$testBuild/game_app_asset_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host asset app tests failed.' }


# Real Box storage adapter against a memory-backed Arduino LittleFS boundary.
$boxArgs = @('/nologo', '/std:c++17', '/utf-8', '/EHsc', '/W4', '/WX', '/MD',
  "/I$PSScriptRoot/host_platform", "/I$game", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/box_storage_test.exe",
  "$game/pokemon.cpp", "$game/pokemon_record_codec.cpp", "$game/game_state.cpp",
  "$game/exploration.cpp", "$game/encounter.cpp", "$game/type.cpp", "$game/move.cpp", "$game/battle.cpp",
  "$game/box_data.cpp", "$game/box_storage.cpp", "$PSScriptRoot/box_storage_test.cpp",
  '/link', "/LIBPATH:$msvc/lib/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/ucrt/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/um/x64")
& "$msvc/bin/Hostx64/x64/cl.exe" @boxArgs
if ($LASTEXITCODE -ne 0) { throw 'Host Box compilation failed.' }
& "$testBuild/box_storage_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host Box tests failed.' }
