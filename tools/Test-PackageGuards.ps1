[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackagePath, [string]$Version = '2.0.0')
$ErrorActionPreference = 'Stop'
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
$wrongVersion.Version = 2
Assert-Rejected { Test-SPSPexContract $bridge $wrongVersion } 'incorrect GetSPSBridgeVersion return value'
$wrongABI = $contracts[0].Clone()
$wrongABI.Signatures = $wrongABI.Signatures.Clone()
$wrongABI.Signatures.SetPlayerOwnerV3 = 'Bool(Bool):1'
Assert-Rejected { Test-SPSPexContract $bridge $wrongABI } 'incompatible SetPlayerOwnerV3 signature'
$wrongABI.Signatures = $contracts[0].Signatures.Clone()
$wrongABI.Signatures.EnterOperation = 'Bool(String):1'
Assert-Rejected { Test-SPSPexContract $bridge $wrongABI } 'incompatible EnterOperation signature'

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
