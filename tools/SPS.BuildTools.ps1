Set-StrictMode -Version Latest

function Resolve-SPSFirstDirectory {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Resolve-SPSVcpkgRoot {
    param(
        [string[]]$Candidates,
        [string]$Triplet = 'x64-windows-static-md'
    )

    $requiredShares = @('directxmath', 'directxtk', 'fmt', 'nlohmann_json',
        'rapidcsv', 'SimpleIni', 'spdlog', 'toml11', 'xbyak')
    $checked = [System.Collections.Generic.List[string]]::new()
    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate) -or
            -not (Test-Path -LiteralPath $candidate -PathType Container)) {
            continue
        }
        $root = (Resolve-Path -LiteralPath $candidate).Path
        if ($checked.Contains($root)) { continue }
        $checked.Add($root)

        $toolchain = Join-Path $root 'scripts\buildsystems\vcpkg.cmake'
        $shareRoot = Join-Path $root "installed\$Triplet\share"
        $ready = (Test-Path -LiteralPath $toolchain -PathType Leaf)
        foreach ($share in $requiredShares) {
            $ready = $ready -and (Test-Path -LiteralPath (Join-Path $shareRoot $share) -PathType Container)
        }
        if ($ready) { return $root }
    }

    $details = if ($checked.Count -gt 0) { $checked -join '; ' } else { 'none' }
    throw "No ready vcpkg installation contains the $Triplet SPS dependencies. Checked: $details"
}

function Get-SPSCMakeVersion {
    param([Parameter(Mandatory = $true)][string]$Path)

    try {
        $firstLine = (& $Path --version 2>$null | Select-Object -First 1)
        if ($firstLine -match '(\d+\.\d+\.\d+)') {
            return [version]$Matches[1]
        }
    }
    catch {
        return $null
    }
    return $null
}

function Resolve-SPSCMake {
    param(
        [string]$ExplicitPath,
        [Parameter(Mandatory = $true)][string]$VcpkgRoot,
        [version]$MinimumVersion = [version]'4.0.0',
        [version]$PreferredVersion = [version]'4.4.0'
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "The requested CMake executable was not found: $ExplicitPath"
        }
        $resolvedExplicit = (Resolve-Path -LiteralPath $ExplicitPath).Path
        $explicitVersion = Get-SPSCMakeVersion -Path $resolvedExplicit
        if (-not $explicitVersion -or $explicitVersion -lt $MinimumVersion) {
            throw "The requested CMake must be $MinimumVersion or newer: $resolvedExplicit"
        }
        return [pscustomobject]@{ Path = $resolvedExplicit; Version = $explicitVersion }
    }

    $downloadedCandidates = [System.Collections.Generic.List[string]]::new()
    $downloadedTools = Join-Path $VcpkgRoot 'downloads\tools'
    if (Test-Path -LiteralPath $downloadedTools -PathType Container) {
        Get-ChildItem -LiteralPath $downloadedTools -Filter 'cmake.exe' -File -Recurse -ErrorAction SilentlyContinue |
            ForEach-Object { $downloadedCandidates.Add($_.FullName) }
    }
    $downloaded = foreach ($candidate in ($downloadedCandidates | Select-Object -Unique)) {
        $version = Get-SPSCMakeVersion -Path $candidate
        if ($version) {
            [pscustomobject]@{ Path = $candidate; Version = $version }
        }
    }
    $selected = $downloaded |
        Where-Object { $_.Version -eq $PreferredVersion } |
        Select-Object -First 1
    if (-not $selected) {
        $selected = $downloaded |
        Where-Object { $_.Version -ge $MinimumVersion } |
        Sort-Object Version -Descending |
        Select-Object -First 1
    }
    if ($selected) { return $selected }

    # A system CMake is only a fallback. Prefer the copy downloaded alongside
    # the pinned vcpkg setup so a global CMake update cannot silently change the
    # project generator between SPS releases. Prefer the tested 4.4.0 copy even
    # if vcpkg later downloads another CMake release alongside it.
    $pathCMake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($pathCMake) {
        $pathVersion = Get-SPSCMakeVersion -Path $pathCMake.Source
        if ($pathVersion -and $pathVersion -ge $MinimumVersion) {
            return [pscustomobject]@{ Path = $pathCMake.Source; Version = $pathVersion }
        }
    }
    $found = ($downloaded | Sort-Object Version -Descending |
        ForEach-Object { "$($_.Version) at $($_.Path)" }) -join '; '
    if ([string]::IsNullOrWhiteSpace($found)) { $found = 'none' }
    throw "CMake $MinimumVersion or newer is required. Found: $found"
}

function Get-SPSCleanEnvironment {
    param([hashtable]$Overrides = @{})

    # The Codex restricted process can contain both PATH and Path. MSBuild uses
    # a case-insensitive dictionary and refuses to start CL.exe when both names
    # are present. Preserve the current process environment while replacing all
    # case variants with exactly one canonical Path entry for child processes.
    $clean = @{}
    $processVariables = [Environment]::GetEnvironmentVariables(
        [EnvironmentVariableTarget]::Process)
    foreach ($entry in $processVariables.GetEnumerator()) {
        if ($entry.Key -ine 'Path') {
            $clean[[string]$entry.Key] = [string]$entry.Value
        }
    }
    $clean['Path'] = [Environment]::GetEnvironmentVariable(
        'Path', [EnvironmentVariableTarget]::Process)
    foreach ($entry in $Overrides.GetEnumerator()) {
        if ($null -eq $entry.Value) {
            $clean.Remove([string]$entry.Key)
        }
        else {
            $clean[[string]$entry.Key] = [string]$entry.Value
        }
    }
    return $clean
}

function Invoke-SPSProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$ArgumentList,
        [Parameter(Mandatory = $true)][hashtable]$Environment,
        [string]$Description = 'process'
    )

    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $FilePath
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.Environment.Clear()
    foreach ($entry in $Environment.GetEnumerator()) {
        $start.Environment[[string]$entry.Key] = [string]$entry.Value
    }
    foreach ($argument in $ArgumentList) {
        [void]$start.ArgumentList.Add($argument)
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) {
        throw "Could not start $Description."
    }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdoutText = $stdout.GetAwaiter().GetResult()
    $stderrText = $stderr.GetAwaiter().GetResult()
    if (-not [string]::IsNullOrWhiteSpace($stdoutText)) {
        Write-Host $stdoutText.TrimEnd()
    }
    if (-not [string]::IsNullOrWhiteSpace($stderrText)) {
        Write-Host $stderrText.TrimEnd()
    }
    if ($process.ExitCode -ne 0) {
        throw "$Description failed with exit code $($process.ExitCode)."
    }
}

function Get-SPSSubstMappings {
    $mappings = @{}
    foreach ($line in (& subst.exe 2>$null)) {
        if ($line -match '^([A-Za-z]):\\:\s*=>\s*(.+)$') {
            $target = [IO.Path]::GetFullPath($Matches[2].Trim())
            $mappings[$Matches[1].ToUpperInvariant()] = $target.TrimEnd('\')
        }
    }
    return $mappings
}
