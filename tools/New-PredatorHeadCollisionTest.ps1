[CmdletBinding()]
param(
    [string]$ReleaseZip = (Join-Path $PSScriptRoot '..\out\release\Schlong-Physics-Swapper-2.0.0.zip'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\out\compatibility\predator-head-test-1')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$releaseHash = 'E8A23CF7E2DB6C16FE6A5BA14E2D758540FA3838ECA12F56C11610A94C758F34'
$resolvedRelease = (Resolve-Path -LiteralPath $ReleaseZip).Path
if ((Get-FileHash -LiteralPath $resolvedRelease -Algorithm SHA256).Hash -ne $releaseHash) {
    throw 'This isolated test requires the validated SPS 2.0.0 release ZIP.'
}
$output = [IO.Path]::GetFullPath($OutputDirectory)
$zipPath = Join-Path $output 'SPS-SOFTBODY-Predator-Head-Collision-Test-1.zip'
$receiptPath = Join-Path $output 'validation.json'
foreach ($path in @($zipPath, $receiptPath)) {
    if (Test-Path -LiteralPath $path) { throw "Preserving existing output: $path" }
}

function Read-XmlBytes([byte[]]$Bytes) {
    $stream = [IO.MemoryStream]::new($Bytes, $false)
    try {
        $doc = [Xml.XmlDocument]::new()
        $doc.XmlResolver = $null
        $doc.PreserveWhitespace = $true
        $doc.Load($stream)
        return ,$doc
    } finally { $stream.Dispose() }
}

function Get-BytesHash([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '') }
    finally { $sha.Dispose() }
}

$files = [ordered]@{}
$checks = @()
$source = [IO.Compression.ZipFile]::OpenRead($resolvedRelease)
try {
    foreach ($relative in @('README.md', 'SKSE/Plugins/hdtSkinnedMeshConfigs/MaleGenitals.xml',
        'SKSE/Plugins/hdtSkinnedMeshConfigs/MaleGenitalsSoft.xml',
        'SKSE/Plugins/hdtSkinnedMeshConfigs/MaleGenitalsToAnus.xml')) {
        $matches = @($source.Entries | Where-Object {
            $_.FullName.Replace('\', '/') -ceq ('Optional/SOFTBODY SPS/' + $relative)
        })
        if ($matches.Count -ne 1) { throw "Missing or duplicate source entry: $relative" }
        $stream = $matches[0].Open()
        $buffer = [IO.MemoryStream]::new()
        try { $stream.CopyTo($buffer); $originalBytes = $buffer.ToArray() }
        finally { $stream.Dispose(); $buffer.Dispose() }
        $bytes = $originalBytes
        if ($relative.EndsWith('.xml')) {
            $originalDoc = Read-XmlBytes $originalBytes
            $text = [Text.Encoding]::UTF8.GetString($originalBytes)
            foreach ($index in 1..6) {
                $name = 'NPC Genitals0' + $index + ' [Gen0' + $index + ']'
                $nodes = $originalDoc.SelectNodes("/system/bone[@name='$name']/margin-multiplier")
                if ($nodes.Count -ne 1 -or $nodes[0].InnerText -cne '0') {
                    throw "Expected one explicit zero multiplier: $relative / $name"
                }
                $pattern = '(?s)(<bone\s+name="' + [regex]::Escape($name) +
                    '"[^>]*>.*?<margin-multiplier>)0(</margin-multiplier>.*?</bone>)'
                $found = [regex]::Matches($text, $pattern)
                if ($found.Count -ne 1) { throw "Ambiguous bone block: $relative / $name" }
                $match = $found[0]
                $text = $text.Remove($match.Index, $match.Length).Insert($match.Index,
                    $match.Groups[1].Value + '1' + $match.Groups[2].Value)
            }
            $bytes = [Text.Encoding]::UTF8.GetBytes($text)
            $testDoc = Read-XmlBytes $bytes
            if ($bytes.Length -ne $originalBytes.Length) { throw 'Unexpected file length change' }
            $changedBytes = 0
            for ($i = 0; $i -lt $bytes.Length; $i++) {
                if ($bytes[$i] -ne $originalBytes[$i]) {
                    if ($originalBytes[$i] -ne 48 -or $bytes[$i] -ne 49) { throw 'Unexpected byte edit' }
                    $changedBytes++
                }
            }
            if ($changedBytes -ne 6) { throw 'Expected exactly six zero-to-one edits per XML' }
            foreach ($index in 1..6) {
                $name = 'NPC Genitals0' + $index + ' [Gen0' + $index + ']'
                $nodes = $testDoc.SelectNodes("/system/bone[@name='$name']/margin-multiplier")
                if ($nodes.Count -ne 1 -or $nodes[0].InnerText -cne '1') { throw 'Multiplier validation failed' }
                $nodes[0].InnerText = '0'
            }
            if ($testDoc.OuterXml -cne $originalDoc.OuterXml) { throw 'Unrelated XML structure changed' }
            $shapes = @($originalDoc.SelectNodes('/system/per-triangle-shape|/system/per-vertex-shape'))
            if (@($shapes | Group-Object -Property name -CaseSensitive | Where-Object Count -gt 1).Count) {
                throw 'Duplicate collision shapes'
            }
            # The personal/private profiles contain different, much larger proxy margins.
            # This experiment is deliberately restricted to the published SOFTBODY set.
            foreach ($shape in $shapes) {
                if ([double]$shape.margin -gt 0.1 -or [double]$shape.penetration -gt 0.5) {
                    throw 'Unexpected proxy collision envelope; review before testing'
                }
            }
            $checks += [pscustomobject]@{ File = $relative; ChangedBytes = 6;
                OriginalSHA256 = (Get-BytesHash $originalBytes); TestSHA256 = (Get-BytesHash $bytes) }
        }
        $files[$relative] = $bytes
    }
} finally { $source.Dispose() }

$instructions = @'
SPS + SOFTBODY + Predator SMP Head - COLLISION TEST 1

This is an experimental XML-only test for SPS 2.0.0 with the
"Use my SOFTBODY physics" option and GT SOFTBODY 3.37.2.
It is not a confirmed fix or a replacement for the main SPS download.

With Skyrim closed, install this ZIP as a separate temporary SPS test
override in MO2. Let it win the three MaleGenitals XML conflicts over SPS,
SOFTBODY and any other genital XML provider. Keep Predator's matching head
and its dependencies installed. Restart Skyrim fully before testing.

Try the same scene where lip/throat collisions failed. Record whether the
head reacts in soft and erect states, including Test soft and Test erect
outside scene control. Also check scene cleanup, resting length, body
collisions, equipment changes and Repair current physics. Stop using this
test if it causes unwanted collision motion or stretching.

The test changes only six shaft collision-margin multipliers from 0 to 1
in each of the three released SOFTBODY XMLs. All masses, constraints,
gravity, damping, shape names, filters and weight thresholds are unchanged.
Other contacts on the same shaft mesh may still change. These XML paths
can affect NPCs using the same profiles as well as the player; SPS's
automatic state switching remains player-only.

Send the result (soft versus erect), SPS diagnostic report, FSMP/SMP Fixes
versions, and the winning genital XML if the problem remains. Compare
with this override disabled after another full restart.

To undo: close Skyrim, disable this test override and restart. No save
cleaning, DLL changes, INI changes or edits to other mods are needed.
Do not combine this experiment with a private large-mesh profile.

Original SOFTBODY credits and permissions are in README.md. This free
test package contains no Predator assets, DLLs, scripts or settings.
'@
$files['TEST-INSTRUCTIONS.txt'] = [Text.UTF8Encoding]::new($false).GetBytes($instructions)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$archiveStream = [IO.File]::Open($zipPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $zip = [IO.Compression.ZipArchive]::new($archiveStream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($name in $files.Keys) {
            $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(2026, 9, 14, 0, 0, 0, [TimeSpan]::Zero)
            $stream = $entry.Open()
            try { $stream.Write($files[$name], 0, $files[$name].Length) }
            finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $archiveStream.Dispose() }
$verify = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    if ($verify.Entries.Count -ne $files.Count) { throw 'Unexpected package entry count' }
    foreach ($name in $files.Keys) {
        $entry = $verify.GetEntry($name)
        if ($null -eq $entry) { throw "Missing package entry: $name" }
        $stream = $entry.Open()
        $buffer = [IO.MemoryStream]::new()
        try { $stream.CopyTo($buffer); $actual = $buffer.ToArray() }
        finally { $stream.Dispose(); $buffer.Dispose() }
        if ((Get-BytesHash $actual) -ne (Get-BytesHash $files[$name])) { throw "Package mismatch: $name" }
    }
} finally { $verify.Dispose() }
$receipt = [pscustomobject]@{
    SourceReleaseSHA256 = $releaseHash
    Package = $zipPath
    PackageSHA256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
    Entries = @($files.Keys)
    XmlChecks = $checks
    InGameTested = $false
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptPath -Encoding UTF8
$receipt | ConvertTo-Json -Depth 5
