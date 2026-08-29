[CmdletBinding()]
param(
    [string]$GameRoot = 'C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition'
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$compiler = Join-Path $GameRoot 'Papyrus Compiler\PapyrusCompiler.exe'
$baseSources = Join-Path $GameRoot 'Data\Source\Scripts'
$flags = Join-Path $baseSources 'TESV_Papyrus_Flags.flg'
$sourceDirectory = Join-Path $repo 'scripts\Source'
$stubDirectory = Join-Path $repo 'scripts\BuildStubs'
$outputDirectory = Join-Path $repo 'scripts'

foreach ($requiredPath in @($compiler, $baseSources, $flags, $sourceDirectory, $stubDirectory)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required Papyrus build path was not found: $requiredPath"
    }
}

$includes = "$sourceDirectory;$stubDirectory;$baseSources"
& $compiler 'SPS_FSMPBridge.psc' "-f=$flags" "-i=$includes" "-o=$outputDirectory" '-op'
if ($LASTEXITCODE -ne 0) {
    throw "Papyrus bridge compilation failed with exit code $LASTEXITCODE."
}

$output = Join-Path $outputDirectory 'SPS_FSMPBridge.pex'
if (-not (Test-Path -LiteralPath $output -PathType Leaf) -or (Get-Item -LiteralPath $output).Length -eq 0) {
    throw "Papyrus compiler did not create the expected bridge: $output"
}

Write-Host "Papyrus bridge built: $output"
