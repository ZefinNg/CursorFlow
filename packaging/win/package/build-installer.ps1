<#
.SYNOPSIS
    Builds CursorFlow (Release) and packages it into a Windows installer using
    the bundled Inno Setup compiler (packaging/win/package/Inno/ISCC.exe).

.DESCRIPTION
    Pipeline:
      1. Configure (only when <BuildDir>/CMakeCache.txt is missing)
      2. Build target CursorFlow
      3. cmake --install into <BuildDir>/installer-stage
         (the CMake install rule runs windeployqt and trims unused plugins)
      4. Compile packaging/win/CursorFlow.iss with ISCC

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\win\build-installer.ps1

.EXAMPLE
    # Reuse an already compiled build tree, skip the build step
    powershell -ExecutionPolicy Bypass -File packaging\win\build-installer.ps1 -SkipBuild
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$Configuration = "Release",
    [string]$QtDir = "",
    [string]$OutputDir = "",
    [string]$Version = "",
    [switch]$SkipBuild,
    [switch]$KeepStage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $ScriptDir))
$Iscc = Join-Path $ScriptDir "Inno\ISCC.exe"
$IssFile = Join-Path $ScriptDir "CursorFlow.iss"
$IconFile = Join-Path $RepoRoot "assets\image\CursorFlow_64x64.ico"
$LicenseFile = Join-Path $RepoRoot "packaging\LICENSE.txt"
$VCRedist = Join-Path $ScriptDir "VC_redist.x64.exe"

if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build" }
if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot "dist" }
$StageDir = Join-Path $BuildDir "installer-stage"

function Assert-File([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description not found: $Path"
    }
}

function Assert-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found in PATH: $Name"
    }
}

function Get-ProjectVersion([string]$CMakeFile) {
    $text = Get-Content -LiteralPath $CMakeFile -Raw
    $match = [regex]::Match($text, 'project\s*\(\s*CursorFlow[\s\S]*?VERSION\s+([0-9]+(?:\.[0-9]+){2,3})')
    if ($match.Success) {
        return $match.Groups[1].Value
    }
    throw "Unable to read project version from $CMakeFile"
}

function Get-QtDirFromCache([string]$CacheFile) {
    $text = Get-Content -LiteralPath $CacheFile -Raw
    $match = [regex]::Match($text, 'Qt5_DIR:PATH=(.+)')
    if (-not $match.Success) { return "" }
    # <qt>/lib/cmake/Qt5 -> <qt>
    $qtCmakeDir = $match.Groups[1].Value.Trim()
    return Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qtCmakeDir))
}

function New-InnoDefine([string]$Name, [string]$Value) {
    # ISCC /Dname=value is a preprocessor expression: quote it as a string literal.
    return '/D' + $Name + '="' + $Value + '"'
}

Assert-Command "cmake"
Assert-File $Iscc "Inno Setup compiler"
Assert-File $IssFile "Inno Setup script"
Assert-File $IconFile "Installer icon"
Assert-File $LicenseFile "License file"

Write-Host "== CursorFlow Windows packager =="
Write-Host "Repository : $RepoRoot"
Write-Host "Build dir  : $BuildDir"
Write-Host "Stage dir  : $StageDir"
Write-Host "Output dir : $OutputDir"

$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
    if (-not $QtDir) { $QtDir = "C:\Qt\5.15.2\msvc2019_64" }
    if (-not (Test-Path -LiteralPath $QtDir -PathType Container)) {
        throw "Qt directory not found: $QtDir (pass -QtDir or configure the build tree first)"
    }
    Write-Host "[1/4] Configuring CMake (Visual Studio 16 2019, x64) ..."
    & cmake -S $RepoRoot -B $BuildDir -G "Visual Studio 16 2019" -A x64 "-DCMAKE_PREFIX_PATH=$QtDir"
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed (exit code $LASTEXITCODE)." }
}
else {
    if (-not $QtDir) { $QtDir = Get-QtDirFromCache $cacheFile }
    Write-Host "[1/4] Reusing existing CMake cache (Qt: $QtDir)"
}

if (-not $SkipBuild) {
    Write-Host "[2/4] Building CursorFlow ($Configuration) ..."
    & cmake --build $BuildDir --config $Configuration --target CursorFlow
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)." }
}
else {
    Write-Host "[2/4] Skipping build (-SkipBuild)"
}

if ($Configuration -ne "Release") {
    Write-Warning "The CMake install rule runs windeployqt with --release; packaging a $Configuration build may deploy mismatched Qt DLLs."
}

Write-Host "[3/4] Staging deployment (install + windeployqt) ..."
if (Test-Path -LiteralPath $StageDir) {
    Remove-Item -LiteralPath $StageDir -Recurse -Force
}
& cmake --install $BuildDir --config $Configuration --prefix $StageDir
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed (exit code $LASTEXITCODE)." }

Assert-File (Join-Path $StageDir "CursorFlow.exe") "Staged CursorFlow.exe"
Assert-File (Join-Path $StageDir "Qt5Core.dll") "Qt runtime deployed by windeployqt"

if (-not $Version) { $Version = Get-ProjectVersion (Join-Path $RepoRoot "CMakeLists.txt") }
Write-Host "      Version   : $Version"

Write-Host "[4/4] Compiling installer with Inno Setup ..."
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$isccArgs = @(
    (New-InnoDefine "AppVersion" $Version),
    (New-InnoDefine "StageDir" $StageDir),
    (New-InnoDefine "OutputDir" $OutputDir),
    (New-InnoDefine "IconFile" $IconFile),
    (New-InnoDefine "LicenseFile" $LicenseFile)
)
if (Test-Path -LiteralPath $VCRedist -PathType Leaf) {
    $isccArgs += New-InnoDefine "VCRedist" $VCRedist
    Write-Host "      VC runtime: bundled"
}
$isccArgs += $IssFile

& $Iscc @isccArgs
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed (exit code $LASTEXITCODE)." }

$OutputFile = Join-Path $OutputDir "CursorFlow-$Version-win64-Setup.exe"
Assert-File $OutputFile "Installer output"

if (-not $KeepStage -and (Test-Path -LiteralPath $StageDir)) {
    Remove-Item -LiteralPath $StageDir -Recurse -Force
}

$sizeMb = [math]::Round((Get-Item -LiteralPath $OutputFile).Length / 1MB, 2)
Write-Host ""
Write-Host "Installer created successfully:"
Write-Host "  $OutputFile ($sizeMb MB)"
