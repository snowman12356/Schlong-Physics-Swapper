[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$resolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
if (-not (Test-Path -LiteralPath $resolvedPackage -PathType Container)) {
    throw "Package path is not a directory: $resolvedPackage"
}

$requiredFiles = @(
    'fomod\info.xml',
    'fomod\ModuleConfig.xml',
    'SKSE\Plugins\SchlongPhysicsSwapper.dll',
    'SKSE\Plugins\SchlongPhysicsSwapper.ini',
    'SKSE\Plugins\CBPCMasterConfig_ZZZ_SchlongPhysicsSwapper.txt',
    'SKSE\Plugins\CBPConfig_ZZZ_SchlongPhysicsSwapper.txt',
    'Scripts\SPS_SexLabBridge.pex',
    'Scripts\SPS_FSMPBridge.pex',
    'Source\Scripts\SPS_FSMPBridge.psc',
    'Optional\OStim\Scripts\SPS_OStimBridge.pex',
    'Optional\OSL Legacy\Scripts\OSLAroused_Main.pex',
    'Optional\OSL Legacy\Source\OSL Aroused Compatibility\OSLAroused_Main.psc',
    'Licenses\OSL-Aroused-Unlicense.txt',
    'Mod Author API\README.md',
    'Mod Author API\SPSAPI.h'
)

foreach ($relativePath in $requiredFiles) {
    $fullPath = Join-Path $resolvedPackage $relativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Missing required package file: $relativePath"
    }
}

function Test-PexSymbols {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string[]]$Symbols
    )

    $pexPath = Join-Path $resolvedPackage $RelativePath
    $bytes = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($pexPath))
    foreach ($symbol in $Symbols) {
        if (-not $bytes.Contains($symbol)) {
            throw "$RelativePath is stale or incompatible; missing Papyrus symbol: $symbol"
        }
    }
}

Test-PexSymbols 'Scripts\SPS_FSMPBridge.pex' @(
    'GetSPSBridgeVersion', 'SetPlayerOwner', 'SetPlayerOwnerV2', 'ReleasePlayerPhysics', 'ResetPlayerPhysics')
Test-PexSymbols 'Scripts\SPS_SexLabBridge.pex' @('GetSPSBridgeVersion', 'GetPlayerRole')
Test-PexSymbols 'Optional\OStim\Scripts\SPS_OStimBridge.pex' @('GetSPSBridgeVersion', 'GetPlayerRole')

try {
    [xml]$info = Get-Content -LiteralPath (Join-Path $resolvedPackage 'fomod\info.xml') -Raw
    [xml]$module = Get-Content -LiteralPath (Join-Path $resolvedPackage 'fomod\ModuleConfig.xml') -Raw
} catch {
    throw "FOMOD XML is not well formed: $($_.Exception.Message)"
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = [string]$info.fomod.Version
}

$forbiddenPaths = @(
    'Scripts\OSLAroused_Main.pex',
    'Source\OSL Aroused Compatibility'
)

foreach ($relativePath in $forbiddenPaths) {
    $fullPath = Join-Path $resolvedPackage $relativePath
    if (Test-Path -LiteralPath $fullPath) {
        throw "Obsolete OSL override must not be included in the package: $relativePath"
    }
}
$escapedVersion = [regex]::Escape($Version)
if ($info.fomod.Version -ne $Version -or $module.config.moduleName -notmatch $escapedVersion) {
    throw "FOMOD version does not match the requested $Version release."
}

$sourceNodes = $module.SelectNodes('//*[@source]')
foreach ($node in $sourceNodes) {
    $relativeSource = [string]$node.source
    $sourcePath = Join-Path $resolvedPackage $relativeSource
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "FOMOD refers to a missing source: $relativeSource"
    }
}

$cbpcMap = (Get-Content -LiteralPath (Join-Path $resolvedPackage 'SKSE\Plugins\CBPCMasterConfig_ZZZ_SchlongPhysicsSwapper.txt') -Raw).ToLowerInvariant()
$cbpcValues = (Get-Content -LiteralPath (Join-Path $resolvedPackage 'SKSE\Plugins\CBPConfig_ZZZ_SchlongPhysicsSwapper.txt') -Raw).ToLowerInvariant()
foreach ($index in 1..6) {
    $bone = ('npc genitals0{0} [gen0{0}]' -f $index)
    $value = ('ubeps0{0}' -f $index)
    if (-not $cbpcMap.Contains($bone)) { throw "CBPC map is missing $bone" }
    if (-not $cbpcValues.Contains($value)) { throw "CBPC values are missing $value" }
}

$smpXmlFiles = Get-ChildItem -LiteralPath $resolvedPackage -Recurse -Filter '*.xml' |
    Where-Object { $_.FullName -match '[\\/]hdtSkinnedMeshConfigs[\\/]' }
foreach ($xmlFile in $smpXmlFiles) {
    try {
        [xml]$null = Get-Content -LiteralPath $xmlFile.FullName -Raw
    } catch {
        throw "Bundled SMP XML is not well formed: $($xmlFile.Name)"
    }
    $xmlText = (Get-Content -LiteralPath $xmlFile.FullName -Raw).ToLowerInvariant()
    foreach ($index in 1..6) {
        $bone = ('npc genitals0{0} [gen0{0}]' -f $index)
        if (-not $xmlText.Contains($bone)) { throw "$($xmlFile.Name) is missing $bone" }
    }
}

Write-Output "Release package passed validation: $resolvedPackage"
Write-Output "FOMOD source entries checked: $($sourceNodes.Count)"
if ($smpXmlFiles.Count -eq 0) {
    Write-Output 'No SMP XML is bundled, as expected; the compatible schlong addon supplies it.'
} else {
    Write-Output "Bundled SMP XML files checked: $($smpXmlFiles.Count)"
}
