[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = '',

    [ValidateRange(1, 32)]
    [int]$Jobs = 4,

    [switch]$Reconfigure,

    [string]$ArtifactDirectory = 'build-output',

    [string]$CommonLib = $env:COMMONLIB_SSE_FOLDER,

    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$workspace = Split-Path -Path $repo -Parent
$repoName = Split-Path -Path $repo -Leaf

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

function Resolve-FirstExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

$CommonLib = Resolve-FirstExistingPath @(
    $CommonLib,
    (Join-Path $workspace '.research-commonlib-download\CommonLibSSE-NG-ng')
)
if (-not $CommonLib) {
    throw 'CommonLibSSE-NG was not found. Set COMMONLIB_SSE_FOLDER or pass -CommonLib.'
}

$VcpkgRoot = Resolve-FirstExistingPath @(
    (Join-Path $workspace '.research-vcpkg-download\vcpkg-master'),
    $VcpkgRoot,
    'C:\vcpkg-master'
)
if (-not $VcpkgRoot) {
    throw 'vcpkg was not found. Set VCPKG_ROOT or pass -VcpkgRoot.'
}

$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    throw "The vcpkg CMake toolchain was not found: $toolchain"
}

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    throw 'CMake was not found on PATH.'
}

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

if ($cachedDriveLetter) {
    if (Test-Path -LiteralPath ("{0}:\" -f $cachedDriveLetter)) {
        throw "Cached build requires temporary drive $cachedDriveLetter`: but that drive is already in use."
    }
    $driveLetter = $cachedDriveLetter
}
else {
    $driveLetter = @('R', 'S', 'T', 'U', 'V', 'W') |
        Where-Object { -not (Test-Path -LiteralPath ("{0}:\" -f $_)) } |
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
$mcpSource = Resolve-FirstExistingPath @(
    (Join-Path $workspace '.research-mcp-example-download\SKSE-Menu-Framework-3-Example-master')
)

Write-Host "SPS source: $repo"
Write-Host "Build folder: $physicalBuild"
Write-Host "Parallel jobs: $Jobs"
Write-Host "Temporary path: $mappedRepo"

& subst.exe $drive $workspace
if ($LASTEXITCODE -ne 0) {
    throw "Could not create temporary build drive $drive"
}

try {
    $env:COMMONLIB_SSE_FOLDER = $mappedCommonLib

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

        & $cmakeCommand.Source @configureArguments
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE."
        }
    }
    else {
        Write-Host 'Reusing the existing CMake configuration.'
    }

    & $cmakeCommand.Source --build $mappedBuild --config $Configuration `
        --target SchlongPhysicsSwapper --parallel $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "SPS build failed with exit code $LASTEXITCODE."
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
    & subst.exe $drive /d | Out-Null
}
