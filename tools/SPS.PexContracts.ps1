# Skyrim big-endian PEX layout/opcodes verified against the local Champollion
# Pex/FileReader.cpp, Function.cpp and Instruction.cpp. No decompiler dependency.
function Read-SPSPexContract {
    param([Parameter(Mandatory)][string]$Path)
    $pexReader = [IO.BinaryReader]::new([IO.File]::OpenRead($Path))
    function Read-PexU16 { ([int]$pexReader.ReadByte() * 256) + $pexReader.ReadByte() }
    function Read-PexU32 {
        ([long]$pexReader.ReadByte() * 16777216) +
        ([long]$pexReader.ReadByte() * 65536) + (Read-PexU16)
    }
    function Read-PexRawString {
        $length = Read-PexU16
        $bytes = $pexReader.ReadBytes($length)
        if ($bytes.Length -ne $length) { throw 'Truncated PEX string' }
        [Text.Encoding]::UTF8.GetString($bytes)
    }
    function Read-PexString {
        $index = Read-PexU16
        if ($index -ge $pexStrings.Count) { throw 'Invalid PEX string index' }
        $pexStrings[$index]
    }
    function Read-PexValue {
        $type = $pexReader.ReadByte()
        $value = switch ($type) {
            0 { $null }
            1 { Read-PexString }
            2 { Read-PexString }
            3 { $n = Read-PexU32; if ($n -gt 2147483647) { $n -= 4294967296 }; $n }
            4 { Read-PexU32 }
            5 { $pexReader.ReadByte() }
            default { throw "Invalid PEX value type $type" }
        }
        [pscustomobject]@{ Type = $type; Value = $value }
    }
    try {
        if ((Read-PexU32) -ne 0xFA57C0DEL) { throw 'Expected Skyrim big-endian PEX' }
        if ($pexReader.ReadByte() -ne 3 -or $pexReader.ReadByte() -ne 2 -or (Read-PexU16) -ne 1) {
            throw 'Unexpected Skyrim PEX format version/game'
        }
        [void]$pexReader.ReadUInt64()
        1..3 | ForEach-Object { $null = Read-PexRawString }
        $count = Read-PexU16
        $pexStrings = @(for ($i = 0; $i -lt $count; ++$i) { Read-PexRawString })
        if ($pexReader.ReadByte()) {
            [void]$pexReader.ReadUInt64()
            $count = Read-PexU16
            for ($i = 0; $i -lt $count; ++$i) {
                $null = Read-PexString; $null = Read-PexString; $null = Read-PexString
                [void]$pexReader.ReadByte()
                $lines = Read-PexU16
                for ($j = 0; $j -lt $lines; ++$j) { $null = Read-PexU16 }
            }
        }
        $count = Read-PexU16
        for ($i = 0; $i -lt $count; ++$i) { $null = Read-PexString; [void]$pexReader.ReadByte() }
        if ((Read-PexU16) -ne 1) { throw 'Expected one bridge object' }
        $objectName = Read-PexString
        $null = Read-PexU32
        $parent = Read-PexString
        $null = Read-PexString; $null = Read-PexU32
        $autoState = Read-PexString
        if ($parent -ne '' -or $autoState -ne '' -or (Read-PexU16) -ne 0 -or (Read-PexU16) -ne 0) {
            throw 'Bridge unexpectedly has inheritance, variables, properties or an auto state'
        }
        if ((Read-PexU16) -ne 1 -or (Read-PexString) -ne '') { throw 'Expected only the empty bridge state' }
        $count = Read-PexU16
        $functions = @{}
        $arity = @(0,3,3,3,3,3,3,3,3,3,2,2,2,2,2,3,3,3,3,3,1,2,2,3,2,3,1,3,3,3,2,2,3,3,4,4)
        for ($i = 0; $i -lt $count; ++$i) {
            $name = Read-PexString
            $returnType = Read-PexString
            $null = Read-PexString; $null = Read-PexU32
            $flags = $pexReader.ReadByte()
            $parameters = Read-PexU16
            $types = @(for ($j = 0; $j -lt $parameters; ++$j) { $null = Read-PexString; Read-PexString })
            $locals = Read-PexU16
            for ($j = 0; $j -lt $locals; ++$j) { $null = Read-PexString; $null = Read-PexString }
            $instructions = Read-PexU16
            $returns = [Collections.Generic.List[object]]::new()
            for ($j = 0; $j -lt $instructions; ++$j) {
                $opcode = $pexReader.ReadByte()
                if ($opcode -ge $arity.Count) { throw "Unexpected Skyrim opcode $opcode" }
                for ($k = 0; $k -lt $arity[$opcode]; ++$k) {
                    $value = Read-PexValue
                    if ($opcode -eq 26) { $returns.Add($value) }
                }
                if ($opcode -in 23,24,25) {
                    $args = Read-PexValue
                    if ($args.Type -ne 3 -or $args.Value -lt 0 -or $args.Value -gt 65535) {
                        throw 'Invalid PEX call argument count'
                    }
                    for ($k = 0; $k -lt $args.Value; ++$k) { $null = Read-PexValue }
                }
            }
            if ($functions.ContainsKey($name)) { throw "Duplicate PEX function $name" }
            $functions[$name] = [pscustomobject]@{
                ReturnType = $returnType; Parameters = $types; Flags = $flags; Returns = $returns
            }
        }
        if ($pexReader.BaseStream.Position -ne $pexReader.BaseStream.Length) { throw 'Unexpected trailing PEX data' }
        [pscustomobject]@{ Name = $objectName; Functions = $functions }
    }
    catch { throw "$Path : $($_.Exception.Message)" }
    finally { $pexReader.Dispose() }
}

function Get-SPSBridgeContracts {
    @(
        @{ Name = 'SPS_FSMPBridge'; Version = 3; Prefix = ''; Signatures = @{
            SetPlayerOwnerV3 = 'Bool(String,Bool,Int,Int):1'
            EnterOperation = 'Bool(String):3'; OperationCurrent = 'Bool(String):3'
            QueueResetBarrier = 'Bool(String):3'; ResetBarrierPassed = 'Bool(String):3'
            SetPlayerOwnerV2 = 'Bool(Bool):1'; SetPlayerOwner = 'None(Bool):1'
            ReleasePlayerPhysics = 'None():1'; ResetPlayerPhysics = 'None(Bool):1'
        }}
        @{ Name = 'SPS_SexLabBridge'; Version = 3; Prefix = ''; Signatures = @{
            GetPlayerThreadID = 'Int():1'; IsPlayerActive = 'Bool():1'; GetPlayerRole = 'Int():1'
        }}
        @{ Name = 'SPS_PositionBridge'; Version = 1; Prefix = ''; Signatures = @{
            SendPlayerAnimationEvent = 'Bool(String):1'; SetPlayerSchlongBend = 'Bool(Int):1'
        }}
        @{ Name = 'SPS_ArousalBridge'; Version = 1; Prefix = ''; Signatures = @{ GetPlayerArousal = 'Float():1' }}
        @{ Name = 'SPS_OStimBridge'; Version = 1; Prefix = 'Optional/OStim/'; Signatures = @{ GetPlayerRole = 'Int():1' }}
    )
}

function Test-SPSPexContract {
    param([string]$Path, [hashtable]$Contract)
    $pex = Read-SPSPexContract $Path
    if ($pex.Name -ne $Contract.Name) { throw "Wrong bridge object in $Path" }
    $expected = @{ GetSPSBridgeVersion = 'Int():1' } + $Contract.Signatures
    foreach ($name in $expected.Keys) {
        $function = $pex.Functions[$name]
        $signature = '{0}({1}):{2}' -f $function.ReturnType, ($function.Parameters -join ','), $function.Flags
        if ($signature -ne $expected[$name]) { throw "$Path : incompatible $name signature: $signature" }
    }
    $returns = $pex.Functions.GetSPSBridgeVersion.Returns
    if ($returns.Count -ne 1 -or $returns[0].Type -ne 3 -or $returns[0].Value -ne $Contract.Version) {
        throw "$Path : incorrect GetSPSBridgeVersion return value (expected $($Contract.Version))"
    }
}

function Get-SPSBuildInputs {
    param([string]$Repo, [string]$Build)
    $inputs = @{ 'SKSE/Plugins/SchlongPhysicsSwapper.dll' = (Join-Path $Build 'SchlongPhysicsSwapper.dll') }
    foreach ($contract in Get-SPSBridgeContracts) {
        $inputs["$($contract.Prefix)Scripts/$($contract.Name).pex"] = Join-Path $Repo "scripts/$($contract.Name).pex"
        $inputs["$($contract.Prefix)Source/Scripts/$($contract.Name).psc"] = Join-Path $Repo "scripts/Source/$($contract.Name).psc"
    }
    $inputs
}

function New-SPSBuildReceipt {
    param([string]$Repo, [string]$Build, [string]$Version)
    $hashes = [ordered]@{}
    $inputs = Get-SPSBuildInputs $Repo $Build
    foreach ($path in ($inputs.Keys | Sort-Object)) {
        $hashes[$path] = (Get-FileHash -LiteralPath $inputs[$path] -Algorithm SHA256).Hash
    }
    @{ Schema = 1; Version = $Version; Files = $hashes } | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $Build 'SPSBuildManifest.json') -Encoding utf8
}

function Test-SPSBuildReceipt {
    param([string]$PackagePath, [string]$Version)
    $receipt = Get-Content -LiteralPath (Join-Path $PackagePath 'SPSBuildManifest.json') -Raw | ConvertFrom-Json -AsHashtable
    $paths = Get-SPSBuildInputs $PackagePath $PackagePath
    if ($receipt.Schema -ne 1 -or $receipt.Version -ne $Version -or $receipt.Files.Count -ne $paths.Count) {
        throw 'Invalid paired-build manifest'
    }
    foreach ($path in $paths.Keys) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $PackagePath $path) -Algorithm SHA256).Hash
        if ($hash -ne $receipt.Files[$path]) { throw "Package file does not match the paired build: $path" }
    }
}
