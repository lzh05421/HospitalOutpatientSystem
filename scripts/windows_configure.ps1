$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$QtRoot = "D:\Qt\6.11.0\mingw_64"
$QtTools = "D:\Qt\Tools"

$CMakeArgs = @(
    "-S", $ProjectRoot,
    "-B", "$ProjectRoot\build-codex",
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$QtTools\Ninja\ninja.exe",
    "-DCMAKE_CXX_COMPILER=$QtTools\mingw1310_64\bin\g++.exe",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

& "$QtTools\CMake_64\bin\cmake.exe" @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
