[CmdletBinding()]
param(
    [string]$Version,
    [string]$Repository = 'snowman12356/Schlong-Physics-Swapper',
    [string]$NotesFile = '',
    [switch]$Publish
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
[xml]$fomodInfo = Get-Content -LiteralPath (Join-Path $repo 'fomod\info.xml') -Raw
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = [string]$fomodInfo.fomod.Version
}

$tag = "v$Version"
$zip = Join-Path $repo "dist\Schlong-Physics-Swapper-$Version.zip"
if ([string]::IsNullOrWhiteSpace($NotesFile)) {
    $NotesFile = Join-Path $repo "dist\release-notes-$Version.md"
}
elseif (-not [IO.Path]::IsPathRooted($NotesFile)) {
    $NotesFile = Join-Path $repo $NotesFile
}

foreach ($required in @($zip, $NotesFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required release file was not found: $required"
    }
}
if (-not (Get-Command gh.exe -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI was not found.'
}

$tagCommit = (& git rev-parse "$tag^{commit}" 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($tagCommit)) {
    throw "Create the local $tag tag before publishing."
}
$tagCommit = $tagCommit.Trim()
$headCommit = (& git rev-parse HEAD).Trim()
if ($tagCommit -ne $headCommit) {
    throw "$tag points to $tagCommit but HEAD is $headCommit. Publish from the tagged release commit."
}

$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash
Write-Host "Repository: $Repository"
Write-Host "Tag: $tag ($tagCommit)"
Write-Host "ZIP: $zip"
Write-Host "ZIP SHA-256: $zipHash"
Write-Host "Notes: $NotesFile"

if (-not $Publish) {
    Write-Host ''
    Write-Host 'Preview only. Add -Publish to perform the GitHub network checks and create the release.'
    return
}

& gh auth status
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub authentication failed. Run this command from a network-enabled terminal or approve network access.'
}

& git fetch origin main --tags
if ($LASTEXITCODE -ne 0) { throw 'Could not refresh origin/main and the release tags.' }
& git merge-base --is-ancestor $tagCommit origin/main
if ($LASTEXITCODE -ne 0) {
    throw "$tagCommit is not on origin/main. Push the release commit before publishing."
}
$remoteTag = & git ls-remote --tags origin "refs/tags/$tag"
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($remoteTag)) {
    throw "$tag has not been pushed to origin."
}

& gh release view $tag --repo $Repository *> $null
if ($LASTEXITCODE -eq 0) {
    throw "GitHub release $tag already exists. This script will not overwrite a published release."
}

& gh release create $tag $zip --repo $Repository `
    --title "Schlong Physics Swapper $Version" --notes-file $NotesFile --verify-tag
if ($LASTEXITCODE -ne 0) { throw "GitHub release $tag could not be created." }
