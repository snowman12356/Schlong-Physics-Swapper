[CmdletBinding()]
param(
    [string]$CommonLib = $env:COMMONLIB_SSE_FOLDER,
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$CMake = $env:SPS_CMAKE
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SPS.BuildTools.ps1')

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$workspace = Split-Path -Path $repo -Parent
$failures = [System.Collections.Generic.List[string]]::new()

function Write-Check {
    param([string]$Name, [bool]$Passed, [string]$Details)
    $label = if ($Passed) { '[OK]' } else { '[FAIL]' }
    $colour = if ($Passed) { 'Green' } else { 'Red' }
    Write-Host ("{0} {1}: {2}" -f $label, $Name, $Details) -ForegroundColor $colour
    if (-not $Passed) { $failures.Add("$Name - $Details") }
}

Write-Host 'Schlong Physics Swapper development environment'
Write-Host "Repository: $repo"
Write-Host ''

Write-Check 'PowerShell' ($PSVersionTable.PSVersion -ge [version]'7.4.0') `
    $PSVersionTable.PSVersion.ToString()

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath | Select-Object -First 1)
}
else { $null }
Write-Check 'Visual Studio C++ tools' (-not [string]::IsNullOrWhiteSpace($vsPath)) `
    $(if ($vsPath) { $vsPath } else { 'not found' })

$resolvedCommonLib = Resolve-SPSFirstDirectory @(
    $CommonLib,
    (Join-Path $workspace '.research-commonlib-download\CommonLibSSE-NG-ng')
)
$commonLibReady = $resolvedCommonLib -and
    (Test-Path -LiteralPath (Join-Path $resolvedCommonLib 'CMakeLists.txt') -PathType Leaf)
Write-Check 'CommonLibSSE-NG' $commonLibReady `
    $(if ($commonLibReady) { $resolvedCommonLib } else { 'not found' })

try {
    $resolvedVcpkg = Resolve-SPSVcpkgRoot @(
        $VcpkgRoot,
        (Join-Path $workspace '.research-vcpkg-download\vcpkg-master'),
        'C:\vcpkg-master'
    )
    Write-Check 'vcpkg dependencies' $true $resolvedVcpkg
}
catch {
    $resolvedVcpkg = $null
    Write-Check 'vcpkg dependencies' $false $_.Exception.Message
}

if ($resolvedVcpkg) {
    try {
        $resolvedCMake = Resolve-SPSCMake -ExplicitPath $CMake -VcpkgRoot $resolvedVcpkg
        Write-Check 'Pinned CMake' $true "$($resolvedCMake.Version) at $($resolvedCMake.Path)"
    }
    catch {
        Write-Check 'Pinned CMake' $false $_.Exception.Message
    }
}
else {
    Write-Check 'Pinned CMake' $false 'vcpkg must be repaired first'
}

$mcpSource = Join-Path $workspace '.research-mcp-example-download\SKSE-Menu-Framework-3-Example-master'
Write-Check 'SKSE Menu Framework source' (Test-Path -LiteralPath $mcpSource -PathType Container) $mcpSource

$git = Get-Command git.exe -ErrorAction SilentlyContinue
$gh = Get-Command gh.exe -ErrorAction SilentlyContinue
Write-Check 'Git' ($null -ne $git) $(if ($git) { $git.Source } else { 'not found' })
Write-Check 'GitHub CLI' ($null -ne $gh) $(if ($gh) { $gh.Source } else { 'not found' })

$mappings = Get-SPSSubstMappings
$workspaceTarget = [IO.Path]::GetFullPath($workspace).TrimEnd('\')
$workspaceMapping = $mappings.GetEnumerator() |
    Where-Object { $_.Value.Equals($workspaceTarget, [StringComparison]::OrdinalIgnoreCase) } |
    Select-Object -First 1
if ($workspaceMapping) {
    Write-Host "[INFO] Temporary build mapping already available: $($workspaceMapping.Key): -> $workspaceTarget"
}
else {
    Write-Host '[INFO] Build-Local.ps1 will create and remove a temporary R:-W: mapping when needed.'
}

Write-Host ''
if ($failures.Count -gt 0) {
    throw "The SPS development environment has $($failures.Count) blocking problem(s)."
}
Write-Host 'Environment ready for SPS builds.' -ForegroundColor Green
