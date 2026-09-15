#requires -Version 7.4

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = '',

    [ValidateRange(1, 32)]
    [int]$Jobs = 4,

    [switch]$Reconfigure,

    [string]$ArtifactDirectory = 'out\build',

    [string]$CommonLib = $env:COMMONLIB_SSE_FOLDER,

    [string]$VcpkgRoot = $env:VCPKG_ROOT,

    [string]$MenuFramework = $env:SPS_MENU_FRAMEWORK_SOURCE,

    [string]$CMake = $env:SPS_CMAKE
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SPS.BuildTools.ps1')

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$workspace = Split-Path -Path $repo -Parent
$repoName = Split-Path -Path $repo -Leaf
$dependencyRoot = Join-Path $workspace '.sps-deps'
$referenceRoot = if ([string]::IsNullOrWhiteSpace($env:SPS_REFERENCE_ROOT)) {
    Join-Path $workspace 'codex-references'
}
else {
    $env:SPS_REFERENCE_ROOT
}

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $buildRoot = $env:SPS_BUILD_ROOT
    if ([string]::IsNullOrWhiteSpace($buildRoot)) {
        if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
            $buildRoot = Join-Path $repo 'build-local-vs'
        }
        else {
            $buildRoot = Join-Path $env:LOCALAPPDATA 'SPSBuild'
        }
    }
    $physicalBuild = [IO.Path]::GetFullPath((Join-Path $buildRoot $repoName))
}
elseif ([IO.Path]::IsPathRooted($BuildDirectory)) {
    $physicalBuild = [IO.Path]::GetFullPath($BuildDirectory)
}
else {
    $physicalBuild = [IO.Path]::GetFullPath((Join-Path $repo $BuildDirectory))
}

if ([IO.Path]::IsPathRooted($ArtifactDirectory)) {
    $physicalArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
}
else {
    $physicalArtifactDirectory = [IO.Path]::GetFullPath((Join-Path $repo $ArtifactDirectory))
}

$CommonLib = Resolve-SPSFirstDirectory @(
    $CommonLib,
    (Join-Path $dependencyRoot 'CommonLibSSE-NG'),
    (Join-Path $referenceRoot 'native\CommonLibSSE-NG'),
    (Join-Path $workspace '.research-commonlib-download\CommonLibSSE-NG-ng')
)
if (-not $CommonLib) {
    throw 'CommonLibSSE-NG was not found. Set COMMONLIB_SSE_FOLDER or pass -CommonLib.'
}

$VcpkgRoot = Resolve-SPSVcpkgRoot @(
    $VcpkgRoot,
    (Join-Path $dependencyRoot 'vcpkg'),
    (Join-Path $workspace '.research-vcpkg-download\vcpkg-master'),
    'C:\vcpkg-master'
)
if (-not $VcpkgRoot) {
    throw 'vcpkg was not found. Set VCPKG_ROOT or pass -VcpkgRoot.'
}

$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    throw "The vcpkg CMake toolchain was not found: $toolchain"
}

$cmakeCommand = Resolve-SPSCMake -ExplicitPath $CMake -VcpkgRoot $VcpkgRoot

$cacheFile = Join-Path $physicalBuild 'CMakeCache.txt'
$cachedDriveLetter = $null
if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
    $cachedHome = Get-Content -LiteralPath $cacheFile |
        Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } |
        Select-Object -First 1
    if ($cachedHome -match '=([R-W]):[/\\]') {
        $cachedDriveLetter = $Matches[1]
    }
}

$substMappings = Get-SPSSubstMappings
$workspaceTarget = [IO.Path]::GetFullPath($workspace).TrimEnd('\')
$existingWorkspaceDrive = $substMappings.GetEnumerator() |
    Where-Object { $_.Value.Equals($workspaceTarget, [StringComparison]::OrdinalIgnoreCase) } |
    Select-Object -First 1

if ($cachedDriveLetter) {
    if ($substMappings.ContainsKey($cachedDriveLetter)) {
        if (-not $substMappings[$cachedDriveLetter].Equals(
                $workspaceTarget, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Cached build requires $cachedDriveLetter`: but it maps to $($substMappings[$cachedDriveLetter])."
        }
        $driveLetter = $cachedDriveLetter
    }
    else {
        if (Test-Path -LiteralPath ("{0}:\" -f $cachedDriveLetter)) {
            throw "Cached build requires $cachedDriveLetter`: but that drive is already in use."
        }
        $driveLetter = $cachedDriveLetter
    }
}
elseif ($existingWorkspaceDrive) {
    $driveLetter = [string]$existingWorkspaceDrive.Key
}
else {
    $driveLetter = @('R', 'S', 'T', 'U', 'V', 'W') |
        Where-Object { -not $substMappings.ContainsKey($_) -and
            -not (Test-Path -LiteralPath ("{0}:\" -f $_)) } |
        Select-Object -First 1
}
if (-not $driveLetter) {
    throw 'No free temporary build drive was available from R: through W:.'
}

$drive = "${driveLetter}:"
$mappedWorkspace = "$drive\"
$mappedRepo = "$mappedWorkspace$repoName"

function Convert-ToMappedPath {
    param([string]$Path)

    $resolved = [IO.Path]::GetFullPath($Path)
    if ($resolved.Equals($workspace, [StringComparison]::OrdinalIgnoreCase)) {
        return $mappedWorkspace.TrimEnd('\')
    }
    if ($resolved.StartsWith($workspace + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        return $mappedWorkspace + $resolved.Substring($workspace.Length + 1)
    }
    return $resolved
}

$mappedCommonLib = Convert-ToMappedPath $CommonLib
$mappedToolchain = Convert-ToMappedPath $toolchain
$mappedBuild = Convert-ToMappedPath $physicalBuild
$mcpSource = Resolve-SPSFirstDirectory @(
    $MenuFramework,
    (Join-Path $dependencyRoot 'SKSE-Menu-Framework-3-Example'),
    (Join-Path $referenceRoot 'native\SKSE-Menu-Framework-3-Example'),
    (Join-Path $workspace '.research-mcp-example-download\SKSE-Menu-Framework-3-Example-master')
)

Write-Host "SPS source: $repo"
Write-Host "Build folder: $physicalBuild"
Write-Host "Parallel jobs: $Jobs"
Write-Host "CMake: $($cmakeCommand.Version) at $($cmakeCommand.Path)"
Write-Host "Temporary path: $mappedRepo"

$driveCreated = $false
if (-not $substMappings.ContainsKey($driveLetter)) {
    & subst.exe $drive $workspace
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create temporary build drive $drive"
    }
    $driveCreated = $true
}

try {
    $cleanEnvironment = Get-SPSCleanEnvironment -Overrides @{
        COMMONLIB_SSE_FOLDER = $mappedCommonLib
        VCPKG_ROOT = (Convert-ToMappedPath $VcpkgRoot)
    }

    $solutionFile = Join-Path $physicalBuild 'SchlongPhysicsSwapper.sln'
    $configurationReady = (Test-Path -LiteralPath $cacheFile -PathType Leaf) -and
        (Test-Path -LiteralPath $solutionFile -PathType Leaf)
    if ($Reconfigure -or -not $configurationReady) {
        $configureArguments = @(
            '--fresh',
            '-S', $mappedRepo,
            '-B', $mappedBuild,
            '-G', 'Visual Studio 17 2022',
            '-A', 'x64',
            "-DCMAKE_TOOLCHAIN_FILE=$mappedToolchain",
            '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
            '-DVCPKG_MANIFEST_MODE=OFF'
        )
        if ($mcpSource) {
            $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_MCP_SDK=$(Convert-ToMappedPath $mcpSource)"
        }

        Invoke-SPSProcess -FilePath $cmakeCommand.Path -ArgumentList $configureArguments `
            -Environment $cleanEnvironment -Description 'CMake configuration'
    }
    else {
        Write-Host 'Reusing the existing CMake configuration.'
    }

    $buildArguments = @(
        '--build', $mappedBuild,
        '--config', $Configuration,
        '--target', 'SchlongPhysicsSwapper',
        '--parallel', [string]$Jobs
    )
    Invoke-SPSProcess -FilePath $cmakeCommand.Path -ArgumentList $buildArguments `
        -Environment $cleanEnvironment -Description 'SPS build'

    foreach ($testName in @('SPSCoreTests', 'SPSDiagnosticsTests')) {
        $testExecutable = Join-Path $physicalBuild "$Configuration\$testName.exe"
        if (Test-Path -LiteralPath $testExecutable -PathType Leaf) {
            Invoke-SPSProcess -FilePath $testExecutable -ArgumentList @() `
                -Environment $cleanEnvironment -Description $testName
        }
    }

    $dll = Join-Path $physicalBuild "$Configuration\SchlongPhysicsSwapper.dll"
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
        throw "The build completed but the DLL was not found: $dll"
    }

    $item = Get-Item -LiteralPath $dll
    New-Item -ItemType Directory -Path $physicalArtifactDirectory -Force | Out-Null
    $artifactDll = Join-Path $physicalArtifactDirectory 'SchlongPhysicsSwapper.dll'
    Copy-Item -LiteralPath $dll -Destination $artifactDll -Force
    $artifact = Get-Item -LiteralPath $artifactDll
    $hash = (Get-FileHash -LiteralPath $artifactDll -Algorithm SHA256).Hash
    Write-Host ''
    Write-Host 'Build completed successfully.'
    Write-Host "Cached DLL: $($item.FullName)"
    Write-Host "Ready DLL: $($artifact.FullName)"
    Write-Host "SHA-256: $hash"
}
finally {
    if ($driveCreated) {
        & subst.exe $drive /d | Out-Null
    }
}
