[CmdletBinding()]
param(
    [switch]$ForceLocal,
    [string]$MirrorRoot = 'D:\repos\mirrors'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$upstreams = @(
    @{ Name = 'space-rangers-hd-cpp'; Url = 'https://github.com/pakompom/SpaceRangersHD_CPP.git'; Mirror = 'SpaceRangersHD_CPP.git' },
    @{ Name = 'okgf'; Url = 'https://github.com/pakompom/okgf.git'; Mirror = 'okgf.git' }
)

function Test-UpstreamAvailable([string]$url) {
    & git ls-remote --exit-code $url HEAD *> $null
    return $LASTEXITCODE -eq 0
}

foreach ($upstream in $upstreams) {
    $mirrorPath = Join-Path $MirrorRoot $upstream.Mirror
    $useLocal = $ForceLocal -or -not (Test-UpstreamAvailable $upstream.Url)
    if ($useLocal) {
        if (-not (Test-Path (Join-Path $mirrorPath 'HEAD'))) {
            throw "Local mirror is unavailable: $mirrorPath"
        }
        $url = 'file:///' + ($mirrorPath -replace '\\', '/')
        Write-Host "[upstream] $($upstream.Name): local mirror $url"
    } else {
        $url = $upstream.Url
        Write-Host "[upstream] $($upstream.Name): GitHub $url"
    }
    & git -C $repoRoot config "submodule.$($upstream.Name).url" $url
    if ($LASTEXITCODE -ne 0) { throw "Cannot configure $($upstream.Name)" }
}

& git -C $repoRoot -c protocol.file.allow=always submodule update --init --recursive
if ($LASTEXITCODE -ne 0) { throw 'Submodule bootstrap failed' }
