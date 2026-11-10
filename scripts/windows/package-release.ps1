# Build the TX2000W standalone app and VST3 plug-in for one architecture and
# wrap them in an Inno Setup installer. Run inside Windows; from macOS use
# scripts/windows/package-release.sh, which also regenerates the solution.
[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64')][string] $Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string] $Configuration = 'Release',
    [string] $Version,
    [switch] $SkipBuild,
    [switch] $Clean
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$product = 'TX2000W'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$projectDir = Join-Path $repoRoot 'juce\Builds\VisualStudio2026'
$solution = Join-Path $projectDir "$product.sln"
$jucer = Join-Path $repoRoot 'juce\S-MU2000.jucer'
$distDir = Join-Path $repoRoot 'dist'
$buildRoot = Join-Path $projectDir "$Architecture\$Configuration"
$issPath = Join-Path $scriptDir "$product.iss"

if ([string]::IsNullOrWhiteSpace($Version)) {
    [xml]$jucerXml = Get-Content -LiteralPath $jucer -Raw
    $Version = [string]$jucerXml.JUCERPROJECT.version
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid version: $Version" }
if (-not (Test-Path -LiteralPath $solution)) { throw "Missing solution: $solution (regenerate it with the Projucer)" }

function Find-MSBuild {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Path }
    foreach ($vswhere in @(
            (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
            (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'))) {
        if (Test-Path -LiteralPath $vswhere) {
            $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
            if ($found -and (Test-Path -LiteralPath $found)) { return $found }
        }
    }
    throw 'Visual Studio MSBuild could not be found.'
}

function Find-InnoSetup {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Path }
    foreach ($candidate in @(
            (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'))) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'Inno Setup ISCC.exe could not be found. Install Inno Setup to build the installer.'
}

function Invoke-Native([string]$FilePath, [string[]]$Arguments) {
    Write-Host "==> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $FilePath" }
}

if ($Clean -and (Test-Path -LiteralPath $buildRoot)) { Remove-Item -LiteralPath $buildRoot -Recurse -Force }
if (-not $SkipBuild) {
    Invoke-Native (Find-MSBuild) @($solution, '/m', '/t:Rebuild', "/p:Configuration=$Configuration", "/p:Platform=$Architecture", '/v:minimal')
}

$vst3Platform = if ($Architecture -eq 'ARM64') { 'arm64-win' } else { 'x86_64-win' }
$standalone = Join-Path $buildRoot "Standalone Plugin\$product.exe"
$vst3Bundle = Join-Path $buildRoot "VST3\$product.vst3"
$vst3Binary = Join-Path $vst3Bundle "Contents\$vst3Platform\$product.vst3"
$vst3ModuleInfo = Join-Path $vst3Bundle 'Contents\Resources\moduleinfo.json'
foreach ($artifact in @($standalone, $vst3Bundle, $vst3Binary, $vst3ModuleInfo)) {
    if (-not (Test-Path -LiteralPath $artifact)) { throw "Required build output not found: $artifact" }
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
$outputName = "$product Windows $Architecture $Version Setup.exe"
$outputPath = Join-Path $distDir $outputName
if (Test-Path -LiteralPath $outputPath) { Remove-Item -LiteralPath $outputPath -Force }

$allowed = if ($Architecture -eq 'ARM64') { 'arm64' } else { 'x64compatible' }
$isccArgs = @(
    '/Qp',
    "/DAppVersion=$Version",
    "/DArchitecture=$Architecture",
    "/DAllowedArchitectures=$allowed",
    "/DInstall64Architectures=$allowed",
    "/DOutputBaseFilename=$([IO.Path]::GetFileNameWithoutExtension($outputName))",
    "/DOutputDir=$distDir",
    "/DBuildRoot=$buildRoot",
    "/DRepoRoot=$repoRoot",
    $issPath)
Invoke-Native (Find-InnoSetup) $isccArgs
if (-not (Test-Path -LiteralPath $outputPath)) { throw "Installer was not created: $outputPath" }
Write-Host "Installer ready: $outputPath" -ForegroundColor Green
