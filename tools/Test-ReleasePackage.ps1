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
    'Source\Scripts\SPS_SexLabBridge.psc',
    'Scripts\SPS_PositionBridge.pex',
    'Source\Scripts\SPS_PositionBridge.psc',
    'Scripts\SPS_ArousalBridge.pex',
    'Source\Scripts\SPS_ArousalBridge.psc',
    'Scripts\SPS_FSMPBridge.pex',
    'Source\Scripts\SPS_FSMPBridge.psc',
    'Optional\OStim\Scripts\SPS_OStimBridge.pex',
    'Optional\OSL Legacy\Scripts\OSLAroused_Main.pex',
    'Optional\OSL Legacy\Source\OSL Aroused Compatibility\OSLAroused_Main.psc',
    'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml',
    'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsSoft.xml',
    'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitalsToAnus.xml',
    'Optional\SOFTBODY SPS\README.md',
    'Optional\Personal Physics\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml',
    'Optional\Personal Physics\README.md',
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
Test-PexSymbols 'Scripts\SPS_SexLabBridge.pex' @(
    'GetSPSBridgeVersion', 'IsPlayerActive', 'GetPlayerRole')
Test-PexSymbols 'Scripts\SPS_PositionBridge.pex' @(
    'GetSPSBridgeVersion', 'SendPlayerAnimationEvent', 'SetPlayerSchlongBend')
Test-PexSymbols 'Scripts\SPS_ArousalBridge.pex' @(
    'GetSPSBridgeVersion', 'GetPlayerArousal')
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

$physicsChoices = @($module.SelectNodes('//installStep[@name="Physics XML compatibility"]//plugin') |
    ForEach-Object { [string]$_.name })
$expectedPhysicsChoices = @(
    'I have my own compatible physics',
    'Use my personal physics',
    'Use my SOFTBODY physics'
)
if (($physicsChoices -join '|') -ne ($expectedPhysicsChoices -join '|')) {
    throw 'The FOMOD physics choices or their order do not match the supported first-person wording.'
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

$softbodySpsRoot = Join-Path $resolvedPackage 'Optional\SOFTBODY SPS\SKSE\Plugins\hdtSkinnedMeshConfigs'
foreach ($fileName in @('MaleGenitals.xml', 'MaleGenitalsSoft.xml', 'MaleGenitalsToAnus.xml')) {
    [xml]$combined = Get-Content -LiteralPath (Join-Path $softbodySpsRoot $fileName) -Raw
    $maleShapes = @($combined.system.'per-triangle-shape' | Where-Object { $_.name -eq 'MaleGenitals' })
    if ($maleShapes.Count -ne 1) {
        throw "$fileName must contain exactly one combined MaleGenitals collision shape."
    }
    $maleShape = $maleShapes[0]
    foreach ($tag in @('Genitals', 'MaleHands', 'MaleBody', 'Malehands', 'penis', 'VirtualFeet', 'VirtualLegs', 'VirtualPenis')) {
        if ($tag -notin @($maleShape.'no-collide-with-tag')) {
            throw "$fileName is missing the SPS collision exclusion: $tag"
        }
    }
    if (@($maleShape.'weight-threshold').Count -eq 0) {
        throw "$fileName is missing SOFTBODY's MaleGenitals weight thresholds."
    }
    if ((Get-Content -LiteralPath (Join-Path $softbodySpsRoot $fileName) -Raw).Contains('GenitalsLag')) {
        throw "$fileName still contains SOFTBODY's rigid genital lag chain."
    }
}

$personalPhysicsPath = Join-Path $resolvedPackage 'Optional\Personal Physics\SKSE\Plugins\hdtSkinnedMeshConfigs\MaleGenitals.xml'
[xml]$personalPhysics = Get-Content -LiteralPath $personalPhysicsPath -Raw
$personalMaleShapes = @($personalPhysics.system.'per-triangle-shape' | Where-Object { $_.name -eq 'MaleGenitals' })
if ($personalMaleShapes.Count -ne 1) {
    throw 'The personal SPS physics must contain exactly one MaleGenitals collision shape.'
}
foreach ($tag in @('Genitals', 'MaleHands', 'MaleBody', 'Malehands', 'penis', 'VirtualFeet', 'VirtualLegs', 'VirtualPenis')) {
    if ($tag -notin @($personalMaleShapes[0].'no-collide-with-tag')) {
        throw "The personal SPS physics is missing the collision exclusion: $tag"
    }
}

Write-Output "Release package passed validation: $resolvedPackage"
Write-Output "FOMOD source entries checked: $($sourceNodes.Count)"
Write-Output "Bundled SMP XML files checked: $($smpXmlFiles.Count)"
