[CmdletBinding()]
param(
    [string]$Version = '2.0.0',
    [string]$BuildDirectory = 'out\build',
    [string]$OutputDirectory = 'out\release',
    [switch]$CreateZip
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path -LiteralPath (Join-Path $repo $BuildDirectory)).Path
$manifest = Join-Path $build 'SPSBuildManifest.json'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw 'Run Build-Release.ps1 first to compile the DLL and bridges together and record their hashes.'
}
$dist = Join-Path $repo $OutputDirectory
$stageName = "Schlong-Physics-Swapper-{0}" -f $Version
$stage = Join-Path $dist $stageName

New-Item -ItemType Directory -Path $dist -Force | Out-Null

if (Test-Path -LiteralPath $stage) {
    $resolvedDist = (Resolve-Path -LiteralPath $dist).Path
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if ((Split-Path -Parent $resolvedStage) -ne $resolvedDist -or (Split-Path -Leaf $resolvedStage) -ne $stageName) {
        throw "Refusing to clear unexpected staging path: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $manifest -Destination (Join-Path $stage 'SPSBuildManifest.json')

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

function New-DeterministicZip {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDirectory,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $destinationParent = Split-Path -Parent $DestinationPath
    $temporaryZip = Join-Path $destinationParent (
        '.{0}.{1}.tmp' -f ([IO.Path]::GetFileName($DestinationPath)), [guid]::NewGuid())
    $fixedTimestamp = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)

    try {
        $output = [IO.File]::Open($temporaryZip, [IO.FileMode]::CreateNew,
            [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        try {
            $archive = [IO.Compression.ZipArchive]::new(
                $output, [IO.Compression.ZipArchiveMode]::Create, $false)
            try {
                $files = Get-ChildItem -LiteralPath $SourceDirectory -File -Recurse |
                    Sort-Object { [IO.Path]::GetRelativePath($SourceDirectory, $_.FullName) }
                foreach ($file in $files) {
                    $entryName = [IO.Path]::GetRelativePath(
                        $SourceDirectory, $file.FullName).Replace('\', '/')
                    $entry = $archive.CreateEntry(
                        $entryName, [IO.Compression.CompressionLevel]::Optimal)
                    $entry.LastWriteTime = $fixedTimestamp
                    $input = [IO.File]::OpenRead($file.FullName)
                    try {
                        $entryStream = $entry.Open()
                        try { $input.CopyTo($entryStream) }
                        finally { $entryStream.Dispose() }
                    }
                    finally { $input.Dispose() }
                }
            }
            finally { $archive.Dispose() }
        }
        finally { $output.Dispose() }

        Move-Item -LiteralPath $temporaryZip -Destination $DestinationPath -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporaryZip -PathType Leaf) {
            Remove-Item -LiteralPath $temporaryZip -Force
        }
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
Copy-ReleaseFile 'scripts\SPS_PositionBridge.pex' 'Scripts\SPS_PositionBridge.pex'
Copy-ReleaseFile 'scripts\Source\SPS_PositionBridge.psc' 'Source\Scripts\SPS_PositionBridge.psc'
Copy-ReleaseFile 'scripts\SPS_ArousalBridge.pex' 'Scripts\SPS_ArousalBridge.pex'
Copy-ReleaseFile 'scripts\Source\SPS_ArousalBridge.psc' 'Source\Scripts\SPS_ArousalBridge.psc'
Copy-ReleaseFile 'scripts\SPS_FSMPBridge.pex' 'Scripts\SPS_FSMPBridge.pex'
Copy-ReleaseFile 'scripts\Source\SPS_FSMPBridge.psc' 'Source\Scripts\SPS_FSMPBridge.psc'
Copy-ReleaseFile 'scripts\SPS_OStimBridge.pex' 'Optional\OStim\Scripts\SPS_OStimBridge.pex'
Copy-ReleaseFile 'scripts\Source\SPS_OStimBridge.psc' 'Optional\OStim\Source\Scripts\SPS_OStimBridge.psc'
Copy-ReleaseFile 'compat\OSL Aroused\Scripts\OSLAroused_Main.pex' 'Optional\OSL Legacy\Scripts\OSLAroused_Main.pex'
Copy-ReleaseFile 'compat\OSL Aroused\Scripts\Source\OSLAroused_Main.psc' 'Optional\OSL Legacy\Source\OSL Aroused Compatibility\OSLAroused_Main.psc'
Copy-ReleaseFile 'compat\OSL Aroused\README.md' 'Optional\OSL Legacy\Source\OSL Aroused Compatibility\README.md'
Copy-ReleaseFile 'compat\OSL Aroused\LICENSE.OSLAroused-Unlicense.txt' 'Licenses\OSL-Aroused-Unlicense.txt'
Copy-ReleaseFile 'compat\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml' 'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml'
Copy-ReleaseFile 'compat\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsSoft.xml' 'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsSoft.xml'
Copy-ReleaseFile 'compat\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsToAnus.xml' 'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsToAnus.xml'
Copy-ReleaseFile 'compat\SOFTBODY SPS\README.md' 'Optional\SOFTBODY SPS\README.md'
Copy-ReleaseFile 'compat\Personal Physics\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml' 'Optional\Personal Physics\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml'
Copy-ReleaseFile 'compat\Personal Physics\README.md' 'Optional\Personal Physics\README.md'
Copy-ReleaseFile 'LICENSE' 'LICENSE'
Copy-ReleaseFile 'README.md' 'README.md'
Copy-ReleaseFile 'THIRD_PARTY.md' 'THIRD_PARTY.md'
Copy-ReleaseFile 'THIRD_PARTY_LICENSES.txt' 'Licenses\Third-Party-Software-Licenses.txt'
Copy-ReleaseFile '.github\ISSUE_TEMPLATE\bug_report.yml' 'docs\bug-report-template.yml'
Copy-ReleaseFile 'docs\SUPPORT.md' 'docs\SUPPORT.md'
Copy-ReleaseFile 'docs\MOD_AUTHOR_API.md' 'Mod Author API\README.md'
Copy-ReleaseFile 'src\SPSAPI.h' 'Mod Author API\SPSAPI.h'
Copy-ReleaseDirectory 'fomod' 'fomod'

& (Join-Path $PSScriptRoot 'Test-ReleasePackage.ps1') -PackagePath $stage -Version $Version
if (-not $?) { throw 'Release validation failed.' }

if ($CreateZip) {
    $zip = Join-Path $dist ("Schlong-Physics-Swapper-{0}.zip" -f $Version)
    New-DeterministicZip -SourceDirectory $stage -DestinationPath $zip
    Write-Output "Created: $zip"
}
Write-Output "Staged: $stage"
