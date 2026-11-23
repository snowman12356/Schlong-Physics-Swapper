[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackagePath, [string]$Version = '')
$ErrorActionPreference = 'Stop'
if (-not $Version) {
    $cmakeText = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\CMakeLists.txt') -Raw
    if ($cmakeText -notmatch 'project\(SchlongPhysicsSwapper VERSION (\d+(?:\.\d+){2,3})') { throw 'Cannot read the version from CMakeLists.txt; pass -Version.' }
    $Version = $Matches[1]
}
. (Join-Path $PSScriptRoot 'SPS.PexContracts.ps1')

function Assert-Rejected {
    param([scriptblock]$Action, [string]$ExpectedError)
    try { & $Action } catch {
        if ($_.Exception.Message -notlike "*$ExpectedError*") { throw }
        return
    }
    throw "Validation unexpectedly accepted invalid input: $ExpectedError"
}

$contracts = @(Get-SPSBridgeContracts)
$bridge = Join-Path $PackagePath 'Scripts/SPS_FSMPBridge.pex'
$wrongVersion = $contracts[0].Clone()
$wrongVersion.Version = 6
Assert-Rejected { Test-SPSPexContract $bridge $wrongVersion } 'incorrect GetSPSBridgeVersion return value'
$wrongABI = $contracts[0].Clone()
$wrongABI.Signatures = $wrongABI.Signatures.Clone()
# A bridge that still returns the old scalar acknowledgement must be rejected:
# the native side reads a four-element array for read-back.
$wrongABI.Signatures.SetPlayerOwnerV3 = 'Bool(String,Bool,Int,Int):1'
Assert-Rejected { Test-SPSPexContract $bridge $wrongABI } 'incompatible SetPlayerOwnerV3 signature'
$wrongABI.Signatures = $contracts[0].Signatures.Clone()
$wrongABI.Signatures.CountDynamicBones = 'Int(Int[]):1'
Assert-Rejected { Test-SPSPexContract $bridge $wrongABI } 'incompatible CountDynamicBones signature'
$wrongABI.Signatures = $contracts[0].Signatures.Clone()
$wrongABI.Signatures.EnterOperation = 'Bool(String):1'
Assert-Rejected { Test-SPSPexContract $bridge $wrongABI } 'incompatible EnterOperation signature'

foreach ($actorContract in ($contracts | Where-Object { $_.Name -in 'SPS_PositionBridge','SPS_ArousalBridge' })) {
    $actorBridge = Join-Path $PackagePath "Scripts/$($actorContract.Name).pex"
    $oldVersion = $actorContract.Clone()
    $oldVersion.Version = $actorContract.Version - 1
    Assert-Rejected { Test-SPSPexContract $actorBridge $oldVersion } 'incorrect GetSPSBridgeVersion return value'
    $wrongGuard = $actorContract.Clone()
    $wrongGuard.Signatures = $actorContract.Signatures.Clone()
    $wrongGuard.Signatures.ActorRequestCurrent = 'Bool(String,Int):3'
    Assert-Rejected { Test-SPSPexContract $actorBridge $wrongGuard } 'incompatible ActorRequestCurrent signature'
    $wrongGuard.Signatures = $actorContract.Signatures.Clone()
    $wrongGuard.Signatures.ResolveActorRequest = 'Actor(String):1'
    Assert-Rejected { Test-SPSPexContract $actorBridge $wrongGuard } 'incompatible ResolveActorRequest signature'
}

$arousalContract = $contracts | Where-Object { $_.Name -eq 'SPS_ArousalBridge' }
$wrongArousalWrite = $arousalContract.Clone()
$wrongArousalWrite.Signatures = $arousalContract.Signatures.Clone()
$wrongArousalWrite.Signatures.SetActorArousal = 'Float(String,Int):1'
Assert-Rejected { Test-SPSPexContract (Join-Path $PackagePath 'Scripts/SPS_ArousalBridge.pex') $wrongArousalWrite } 'incompatible SetActorArousal signature'

# Scene return values must preserve unknown/clear/active, not collapse to Bool.
$positionContract = $contracts | Where-Object { $_.Name -eq 'SPS_PositionBridge' }
$wrongScene = $positionContract.Clone()
$wrongScene.Signatures = $positionContract.Signatures.Clone()
$wrongScene.Signatures.GetActorSceneState = 'Bool(String,Bool,Bool,Bool):1'
Assert-Rejected { Test-SPSPexContract (Join-Path $PackagePath 'Scripts/SPS_PositionBridge.pex') $wrongScene } 'incompatible GetActorSceneState signature'
$wrongScene.Signatures = $positionContract.Signatures.Clone()
$wrongScene.Signatures.ResolveSceneRequest = 'Actor(String):1'
Assert-Rejected { Test-SPSPexContract (Join-Path $PackagePath 'Scripts/SPS_PositionBridge.pex') $wrongScene } 'incompatible ResolveSceneRequest signature'
$ostimContract = $contracts | Where-Object { $_.Name -eq 'SPS_OStimBridge' }
$oldOstim = $ostimContract.Clone()
$oldOstim.Version = 1
Assert-Rejected { Test-SPSPexContract (Join-Path $PackagePath 'Optional/OStim/Scripts/SPS_OStimBridge.pex') $oldOstim } 'incorrect GetSPSBridgeVersion return value'

# Mutate only an isolated copy. The tested package is never changed.
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('SPS-GuardTests-' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path $temporary | Out-Null
    Copy-Item -LiteralPath (Join-Path $PackagePath 'SPSBuildManifest.json') -Destination $temporary
    foreach ($path in (Get-SPSBuildInputs $PackagePath $PackagePath).Keys) {
        $target = Join-Path $temporary $path
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $PackagePath $path) -Destination $target
    }
    Test-SPSBuildReceipt $temporary $Version
    foreach ($relative in @('SKSE/Plugins/SchlongPhysicsSwapper.dll', 'Scripts/SPS_FSMPBridge.pex', 'Source/Scripts/SPS_FSMPBridge.psc')) {
        $path = Join-Path $temporary $relative
        $bytes = [IO.File]::ReadAllBytes($path)
        $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 1
        [IO.File]::WriteAllBytes($path, $bytes)
        Assert-Rejected { Test-SPSBuildReceipt $temporary $Version } 'does not match the paired build'
        Copy-Item -LiteralPath (Join-Path $PackagePath $relative) -Destination $path -Force
    }
} finally {
    if (Test-Path -LiteralPath $temporary) {
        $resolved = (Resolve-Path -LiteralPath $temporary).Path
        $tempParent = (Resolve-Path -LiteralPath ([IO.Path]::GetTempPath())).Path.TrimEnd('\')
        if ((Split-Path -Parent $resolved) -ne $tempParent -or
            (Split-Path -Leaf $resolved) -notmatch '^SPS-GuardTests-[0-9a-f-]{36}$') {
            throw "Refusing to clear unexpected guard-test path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
Write-Output 'Package guards reject wrong bridge versions, signatures/native flags and mismatched DLL/PEX/PSC files.'
