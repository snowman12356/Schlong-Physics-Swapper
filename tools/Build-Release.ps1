[CmdletBinding()]
param(
    [string]$Version,
    [ValidateRange(1, 32)][int]$Jobs = 4,
    [switch]$Reconfigure,
    [string]$BuildDirectory = '',
    [string]$CommonLib = $env:COMMONLIB_SSE_FOLDER,
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$MenuFramework = $env:SPS_MENU_FRAMEWORK_SOURCE,
    [string]$CMake = $env:SPS_CMAKE,
    [string]$GameRoot = $env:SPS_SKYRIM_GAME_ROOT
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

[xml]$fomodInfo = Get-Content -LiteralPath (Join-Path $repo 'fomod\info.xml') -Raw
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = [string]$fomodInfo.fomod.Version
}

$moduleText = Get-Content -LiteralPath (Join-Path $repo 'fomod\ModuleConfig.xml') -Raw
$cmakeText = Get-Content -LiteralPath (Join-Path $repo 'CMakeLists.txt') -Raw
$pluginText = Get-Content -LiteralPath (Join-Path $repo 'src\plugin.cpp') -Raw
$escapedVersion = [regex]::Escape($Version)
$pluginVersionPattern = 'kVersion\s*=\s*"' + $escapedVersion + '"'
if ($fomodInfo.fomod.Version -ne $Version -or
    $moduleText -notmatch "Schlong Physics Swapper $escapedVersion" -or
    $cmakeText -notmatch "project\(SchlongPhysicsSwapper VERSION $escapedVersion" -or
    $pluginText -notmatch $pluginVersionPattern) {
    throw "The CMake, DLL and FOMOD versions must all match $Version before building a release."
}

$buildArguments = @{
    Configuration = 'Release'
    Jobs = $Jobs
    ArtifactDirectory = 'out\build'
    CommonLib = $CommonLib
    VcpkgRoot = $VcpkgRoot
    MenuFramework = $MenuFramework
    CMake = $CMake
}
if (-not [string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $buildArguments.BuildDirectory = $BuildDirectory
}
if ($Reconfigure) { $buildArguments.Reconfigure = $true }

& (Join-Path $PSScriptRoot 'Test-Environment.ps1') `
    -CommonLib $CommonLib -VcpkgRoot $VcpkgRoot -MenuFramework $MenuFramework -CMake $CMake
$papyrusArguments = @{}
if (-not [string]::IsNullOrWhiteSpace($GameRoot)) {
    $papyrusArguments.GameRoot = $GameRoot
}
& (Join-Path $PSScriptRoot 'Build-PapyrusBridge.ps1') @papyrusArguments
& (Join-Path $PSScriptRoot 'Build-Local.ps1') @buildArguments
& (Join-Path $PSScriptRoot 'New-ReleasePackage.ps1') `
    -Version $Version -BuildDirectory 'out\build' -CreateZip

$zip = Join-Path $repo "out\release\Schlong-Physics-Swapper-$Version.zip"
if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) {
    throw "The release archive was not created: $zip"
}

$verifyRoot = Join-Path ([IO.Path]::GetTempPath()) ("SPS-Release-{0}" -f [guid]::NewGuid())
try {
    Expand-Archive -LiteralPath $zip -DestinationPath $verifyRoot
    & (Join-Path $PSScriptRoot 'Test-ReleasePackage.ps1') `
        -PackagePath $verifyRoot -Version $Version

    $builtHash = (Get-FileHash -Algorithm SHA256 -LiteralPath `
        (Join-Path $repo 'out\build\SchlongPhysicsSwapper.dll')).Hash
    $packedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath `
        (Join-Path $verifyRoot 'SKSE\Plugins\SchlongPhysicsSwapper.dll')).Hash
    if ($builtHash -ne $packedHash) {
        throw 'The DLL inside the ZIP does not match the DLL that was just built.'
    }
}
finally {
    if (Test-Path -LiteralPath $verifyRoot) {
        Remove-Item -LiteralPath $verifyRoot -Recurse -Force
    }
}

$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash
Write-Host ''
Write-Host 'Release build completed successfully.' -ForegroundColor Green
Write-Host "ZIP: $zip"
Write-Host "ZIP SHA-256: $zipHash"
Write-Host "DLL SHA-256: $builtHash"
