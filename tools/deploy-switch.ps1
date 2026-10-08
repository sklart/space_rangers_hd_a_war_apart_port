[CmdletBinding(DefaultParameterSetName = 'Deploy')]
param(
  [Parameter(Mandatory)] [ValidateNotNullOrEmpty()] [string]$SdRoot,
  [Parameter(ParameterSetName = 'Deploy')] [string]$NroPath,
  [Parameter(ParameterSetName = 'Deploy')] [string]$BuildGit = 'NOT_RECORDED',
  [Parameter(ParameterSetName = 'Deploy')] [string]$GameSource,
  [Parameter(ParameterSetName = 'Deploy')] [switch]$InitialGameCopy,
  [Parameter(ParameterSetName = 'Deploy')] [switch]$UpdateOnly,
  [Parameter(ParameterSetName = 'Deploy')] [switch]$ForceGameCopy,
  [Parameter(Mandatory, ParameterSetName = 'CollectLogs')] [switch]$CollectLogs,
  [Parameter(ParameterSetName = 'CollectLogs')] [string]$HardwareLogsRoot = (Join-Path $PSScriptRoot '..\hardware-logs')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Baseline = '83300344AF802BC51E64389C58F047E5AFDF195C133048098BE3881FAE29ED98'
$RequiredReleaseFiles = @('install.txt', 'cfg.txt', 'install_russian.txt', 'DATA\common.pkg', 'CFG\Main.dat', 'CFG\russian\Lang.dat', 'CFG\CacheData.dat')
$AppDirectoryName = 'space-rangers-hd-a-war-apart'
$NroName = 'Space Rangers HD - A War Apart.nro'

function Fail([string]$Message) { Write-Host "BLOCKED: $Message" -ForegroundColor Red; exit 1 }
function Get-PathHash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant() }
function Test-ReleaseTree([string]$Root, [string]$Label) {
  $missing = @()
  foreach ($relative in $RequiredReleaseFiles) { if (-not (Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf)) { $missing += $relative } }
  $exe = Join-Path $Root 'Rangers.exe'
  if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { $missing += 'Rangers.exe' }
  if ($missing.Count -ne 0) { throw "$Label is missing required release files: $($missing -join ', ')" }
  $hash = Get-PathHash $exe
  if ($hash -ne $Baseline) { throw "$Label Rangers.exe SHA-256 mismatch: $hash" }
  return $hash
}
function New-DeploymentLayout([string]$AppRoot) { foreach ($name in @('game', 'config', 'save', 'logs', 'runtime')) { [void](New-Item -ItemType Directory -Force -Path (Join-Path $AppRoot $name)) } }
function Test-DeploymentLayout([string]$AppRoot) {
  $missing = @()
  foreach ($name in @('game', 'config', 'save', 'logs', 'runtime')) {
    if (-not (Test-Path -LiteralPath (Join-Path $AppRoot $name) -PathType Container)) { $missing += $name }
  }
  if ($missing.Count -ne 0) { throw "SD writable directories are missing: $($missing -join ', ')" }
}
function Copy-GameTree([string]$Source, [string]$Destination) {
  [void](New-Item -ItemType Directory -Force -Path $Destination)
  & robocopy $Source $Destination /E /R:2 /W:1 | Out-Host
  if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }
}
function Write-DeploymentManifest([string]$AppRoot, [string]$Mode, [string]$RangersHash, [string]$NroHash, [long]$NroSize, [string]$SourcePath, [string]$BuildGit, [string]$TimestampUtc, [string]$DestinationHash) {
  @('baseline=2.1.2500', "rangers_sha256=$RangersHash", "nro_sha256=$NroHash", "nro_size=$NroSize", "deployment_mode=$Mode", "nro_source_path=$SourcePath", "nro_source_sha256=$NroHash", "nro_destination_sha256=$DestinationHash", "build_git=$BuildGit", "timestamp_utc=$TimestampUtc") |
    Set-Content -LiteralPath (Join-Path $AppRoot 'runtime\deployment.txt') -Encoding utf8
}
function Normalize-RootPath([string]$Path) { (Resolve-Path -LiteralPath $Path).ProviderPath.TrimEnd([char[]]'\/') }
function Test-SdRoot([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return $false }
  $resolved = Normalize-RootPath $Path
  $roots = @()
  foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
    try { $roots += Normalize-RootPath $drive.Root } catch { }
  }
  return $roots -contains $resolved
}
function Collect-HardwareLogs([string]$AppRoot, [string]$DestinationRoot) {
  if (-not (Test-Path -LiteralPath $AppRoot -PathType Container)) { throw 'SD application root is missing' }
  $logs = Join-Path $AppRoot 'logs'
  if (-not (Test-Path -LiteralPath $logs -PathType Container)) { throw 'NO LOGS FOUND' }
  $sources = @()
  foreach ($name in @('port.log', 'port-prev.log', 'gr-main.log')) {
    $source = Join-Path $logs $name
    if (Test-Path -LiteralPath $source -PathType Leaf) { $sources += $source }
  }
  if ($sources.Count -eq 0) { throw 'NO LOGS FOUND' }
  $destination = Join-Path $DestinationRoot (Get-Date -Format 'yyyyMMdd-HHmmss'); [void](New-Item -ItemType Directory -Force -Path $destination)
  foreach ($source in $sources) { Copy-Item -LiteralPath $source -Destination (Join-Path $destination (Split-Path -Leaf $source)) -Force }
  Write-Host "CollectLogs: PASS ($($sources.Count) files) -> $destination"
}

try {
  if (-not (Test-SdRoot $SdRoot)) { throw 'SdRoot must be a drive/filesystem root' }
  $sd = (Resolve-Path -LiteralPath $SdRoot).ProviderPath; $appRoot = Join-Path $sd "switch\$AppDirectoryName"
  if ($CollectLogs) { Collect-HardwareLogs $appRoot $HardwareLogsRoot; exit 0 }
  if (($InitialGameCopy -and $UpdateOnly) -or (-not $InitialGameCopy -and -not $UpdateOnly)) { throw 'Choose exactly one mode: -InitialGameCopy or -UpdateOnly' }
  if ([string]::IsNullOrWhiteSpace($NroPath) -or -not (Test-Path -LiteralPath $NroPath -PathType Leaf)) { throw 'NroPath must name an existing NRO file' }
  $nroSource = (Resolve-Path -LiteralPath $NroPath).Path; $nroHash = Get-PathHash $nroSource; $nroSize = (Get-Item -LiteralPath $nroSource).Length
  if ($BuildGit -ne 'NOT_RECORDED' -and $BuildGit -notmatch '^[0-9a-fA-F]{7,40}$') { throw 'BuildGit must be a Git commit ID' }
  $timestampUtc = [DateTime]::UtcNow.ToString('o')
  Write-Host "NRO source path: $nroSource"; Write-Host "NRO source size: $nroSize"; Write-Host "NRO source SHA-256: $nroHash"; Write-Host "NRO build_git: $BuildGit"; Write-Host "NRO timestamp UTC: $timestampUtc"
  $mode = if ($InitialGameCopy) { 'initial' } else { 'update' }; $gameRoot = Join-Path $appRoot 'game'; $rangersHash = $null
  if ($InitialGameCopy) {
    if ([string]::IsNullOrWhiteSpace($GameSource) -or -not (Test-Path -LiteralPath $GameSource -PathType Container)) { throw 'GameSource is required and must exist for -InitialGameCopy' }
    $sourceRoot = (Resolve-Path -LiteralPath $GameSource).Path; $sourceHash = Test-ReleaseTree $sourceRoot 'GameSource'; New-DeploymentLayout $appRoot; Test-DeploymentLayout $appRoot
    $existingExe = Join-Path $gameRoot 'Rangers.exe'
    if ((Test-Path -LiteralPath $existingExe -PathType Leaf) -and (Get-PathHash $existingExe) -eq $Baseline -and -not $ForceGameCopy) { Write-Host 'GAME COPY SKIPPED — existing baseline valid' }
    else { Copy-GameTree $sourceRoot $gameRoot }
    $rangersHash = Test-ReleaseTree $gameRoot 'SD game root'; if ($rangersHash -ne $sourceHash) { throw 'SD Rangers.exe hash differs from GameSource' }
  } else { Test-DeploymentLayout $appRoot; $rangersHash = Test-ReleaseTree $gameRoot 'SD game root' }
  $nroDestination = Join-Path $appRoot $NroName; Copy-Item -LiteralPath $nroSource -Destination $nroDestination -Force
  $destinationHash = Get-PathHash $nroDestination; if ($destinationHash -ne $nroHash) { throw 'NRO destination SHA-256 differs from source' }
  Write-DeploymentManifest $appRoot $mode $rangersHash $nroHash $nroSize $nroSource $BuildGit $timestampUtc $destinationHash
  Write-Host "`nSwitch deployment preflight"; Write-Host 'Game root: PASS'; Write-Host 'Rangers.exe baseline: PASS'; Write-Host 'Required release files: PASS'; Write-Host 'NRO copy: PASS'; Write-Host "NRO SHA-256: $nroHash"; Write-Host 'Writable directories: PASS'; Write-Host "`nMode: $($mode.ToUpperInvariant())"; Write-Host 'READY FOR SWITCH LAUNCH' -ForegroundColor Green
  exit 0
} catch { Fail $_.Exception.Message }
