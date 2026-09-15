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
$compilerArgs = @('/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/MD',
  "/I$game", "/I$PSScriptRoot", "/I$msvc/include",
  "/I$sdk/Include/$sdkVersion/ucrt", "/I$sdk/Include/$sdkVersion/shared", "/I$sdk/Include/$sdkVersion/um",
  "/Fo$testBuild/", "/Fe$testBuild/game_state_test.exe",
  "$game/pokemon.cpp", "$game/game_state.cpp", "$game/save_data.cpp", "$game/save_storage.cpp",
  "$PSScriptRoot/game_state_test.cpp", '/link', "/LIBPATH:$msvc/lib/x64",
  "/LIBPATH:$sdk/Lib/$sdkVersion/ucrt/x64", "/LIBPATH:$sdk/Lib/$sdkVersion/um/x64")
& "$msvc/bin/Hostx64/x64/cl.exe" @compilerArgs
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
& "$testBuild/game_state_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }
