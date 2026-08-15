[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath
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
    'Scripts\OSLAroused_Main.pex',
    'Mod Author API\README.md',
    'Mod Author API\SPSAPI.h'
)

foreach ($relativePath in $requiredFiles) {
    $fullPath = Join-Path $resolvedPackage $relativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Missing required package file: $relativePath"
    }
}

try {
    [xml]$info = Get-Content -LiteralPath (Join-Path $resolvedPackage 'fomod\info.xml') -Raw
    [xml]$module = Get-Content -LiteralPath (Join-Path $resolvedPackage 'fomod\ModuleConfig.xml') -Raw
} catch {
    throw "FOMOD XML is not well formed: $($_.Exception.Message)"
}

if ($info.fomod.Version -ne '1.8.1' -or $module.config.moduleName -notmatch '1\.8\.1') {
    throw 'FOMOD version does not match the 1.8.1 release.'
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
