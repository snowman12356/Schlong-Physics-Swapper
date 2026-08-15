[CmdletBinding()]
param(
    [string]$Version = '1.8.1',
    [string]$BuildDirectory = 'build-static',
    [switch]$CreateZip
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path -LiteralPath (Join-Path $repo $BuildDirectory)).Path
$dist = Join-Path $repo 'dist'
$stageName = "Schlong-Physics-Swapper-{0}-ReleaseCandidate" -f $Version
$stage = Join-Path $dist $stageName

if (Test-Path -LiteralPath $stage) {
    $resolvedDist = (Resolve-Path -LiteralPath $dist).Path
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if ((Split-Path -Parent $resolvedStage) -ne $resolvedDist -or (Split-Path -Leaf $resolvedStage) -ne $stageName) {
        throw "Refusing to clear unexpected staging path: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage | Out-Null

function Copy-ReleaseFile {
    param([string]$Source, [string]$Destination)
    $sourcePath = Join-Path $repo $Source
    $destinationPath = Join-Path $stage $Destination
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Missing source file: $Source"
    }
    $parent = Split-Path -Parent $destinationPath
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

function Copy-ReleaseDirectory {
    param([string]$Source, [string]$Destination)
    $sourcePath = Join-Path $repo $Source
    $destinationPath = Join-Path $stage $Destination
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Container)) {
        throw "Missing source directory: $Source"
    }
    if (-not (Test-Path -LiteralPath $destinationPath)) {
        New-Item -ItemType Directory -Path $destinationPath | Out-Null
    }
    foreach ($item in Get-ChildItem -LiteralPath $sourcePath -Force) {
        Copy-Item -LiteralPath $item.FullName -Destination $destinationPath -Recurse -Force
    }
}

$dll = Join-Path $build 'SchlongPhysicsSwapper.dll'
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
    throw "Build the DLL first: $dll"
}
$pluginDir = Join-Path $stage 'SKSE\Plugins'
if (-not (Test-Path -LiteralPath $pluginDir)) {
    New-Item -ItemType Directory -Path $pluginDir | Out-Null
}
Copy-Item -LiteralPath $dll -Destination (Join-Path $pluginDir 'SchlongPhysicsSwapper.dll') -Force

Copy-ReleaseFile 'config\SchlongPhysicsSwapper.ini' 'SKSE\Plugins\SchlongPhysicsSwapper.ini'
Copy-ReleaseFile 'config\CBPCMasterConfig_ZZZ_SchlongPhysicsSwapper.txt' 'SKSE\Plugins\CBPCMasterConfig_ZZZ_SchlongPhysicsSwapper.txt'
Copy-ReleaseFile 'config\CBPConfig_ZZZ_SchlongPhysicsSwapper.txt' 'SKSE\Plugins\CBPConfig_ZZZ_SchlongPhysicsSwapper.txt'
Copy-ReleaseFile 'scripts\SPS_SexLabBridge.pex' 'Scripts\SPS_SexLabBridge.pex'
Copy-ReleaseFile 'scripts\Source\SPS_SexLabBridge.psc' 'Source\Scripts\SPS_SexLabBridge.psc'
Copy-ReleaseFile 'compat\OSL Aroused\Scripts\OSLAroused_Main.pex' 'Scripts\OSLAroused_Main.pex'
Copy-ReleaseFile 'compat\OSL Aroused\Scripts\Source\OSLAroused_Main.psc' 'Source\OSL Aroused Compatibility\OSLAroused_Main.psc'
Copy-ReleaseFile 'compat\OSL Aroused\README.md' 'Source\OSL Aroused Compatibility\README.md'
Copy-ReleaseFile 'compat\OSL Aroused\LICENSE.OSLAroused-Unlicense.txt' 'Licenses\OSL-Aroused-Unlicense.txt'
Copy-ReleaseFile 'LICENSE' 'LICENSE'
Copy-ReleaseFile 'README.md' 'README.md'
Copy-ReleaseFile 'THIRD_PARTY.md' 'THIRD_PARTY.md'
Copy-ReleaseFile 'THIRD_PARTY_LICENSES.txt' 'Licenses\Third-Party-Software-Licenses.txt'
Copy-ReleaseFile '.github\ISSUE_TEMPLATE\bug_report.yml' 'docs\bug-report-template.yml'
Copy-ReleaseFile 'docs\SUPPORT.md' 'docs\SUPPORT.md'
Copy-ReleaseFile 'docs\MOD_AUTHOR_API.md' 'Mod Author API\README.md'
Copy-ReleaseFile 'src\SPSAPI.h' 'Mod Author API\SPSAPI.h'
Copy-ReleaseDirectory 'fomod' 'fomod'

& (Join-Path $PSScriptRoot 'Test-ReleasePackage.ps1') -PackagePath $stage
if (-not $?) { throw 'Release validation failed.' }

if ($CreateZip) {
    $zip = Join-Path $dist ("Schlong-Physics-Swapper-{0}-ReleaseCandidate.zip" -f $Version)
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    Write-Output "Created: $zip"
}
Write-Output "Staged: $stage"
