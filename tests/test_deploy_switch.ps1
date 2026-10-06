$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$script = Join-Path $repo 'tools\deploy-switch.ps1'
$baselineExe = Join-Path $repo 'windows\Space Rangers HD A War Apart\Rangers.exe'
$nro = Join-Path $repo 'port\switch\SpaceRangersHDAWarApart.nro'
if (-not (Test-Path -LiteralPath $baselineExe) -or -not (Test-Path -LiteralPath $nro)) { throw 'local deployment fixture is unavailable' }
function Invoke-Deploy([string[]]$DeployArguments) {
  $quoted = $DeployArguments | ForEach-Object { if ($_.StartsWith('-')) { $_ } else { "'$_'" } }
  $lines = Invoke-Expression ("& '$script' " + ($quoted -join ' ')) 6>&1 2>&1
  [pscustomobject]@{ ExitCode = $LASTEXITCODE; Text = ($lines | Out-String) }
}
function Assert-Failed($Result, [string]$ExpectedText) {
  if ($Result.ExitCode -eq 0 -or $Result.Text -notmatch [regex]::Escape($ExpectedText)) { throw "expected failure '$ExpectedText': $($Result.Text)" }
}
$build = Join-Path $repo 'build'; [void](New-Item -ItemType Directory -Force -Path $build)
$root = Join-Path $build 'deploy-switch-test'
if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
try {
  $source = Join-Path $root 'source'; $sd = Join-Path $root 'sd'; $badSource = Join-Path $root 'bad-source'; $badSd = Join-Path $root 'bad-sd'
  foreach ($directory in @($source, $sd, $badSource, $badSd)) { [void](New-Item -ItemType Directory -Force -Path $directory) }
  New-PSDrive -Name SRHDSD -PSProvider FileSystem -Root $sd | Out-Null
  New-PSDrive -Name SRHDBAD -PSProvider FileSystem -Root $badSd | Out-Null
  $sdRoot = 'SRHDSD:\'; $badRoot = 'SRHDBAD:\'
  $noApp = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-CollectLogs', '-HardwareLogsRoot', (Join-Path $root 'hardware-logs'))
  Assert-Failed $noApp 'SD application root is missing'
  $app = Join-Path $sd 'switch\space-rangers-hd-a-war-apart'; $logs = Join-Path $app 'logs'; [void](New-Item -ItemType Directory -Force -Path $logs)
  $emptyLogs = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-CollectLogs', '-HardwareLogsRoot', (Join-Path $root 'hardware-logs'))
  Assert-Failed $emptyLogs 'NO LOGS FOUND'
  $nested = Join-Path $sd 'nested'; [void](New-Item -ItemType Directory -Force -Path $nested)
  $badRootResult = Invoke-Deploy -DeployArguments @('-SdRoot', $nested, '-CollectLogs')
  Assert-Failed $badRootResult 'SdRoot must be a drive/filesystem root'
  foreach ($relative in @('install.txt', 'cfg.txt', 'install_russian.txt', 'DATA\common.pkg', 'CFG\Main.dat', 'CFG\russian\Lang.dat', 'CFG\CacheData.dat')) {
    foreach ($tree in @($source, $badSource)) { $file = Join-Path $tree $relative; [void](New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file)); Set-Content -LiteralPath $file -Value 'fixture' -NoNewline }
  }
  Set-Content -LiteralPath (Join-Path $badSource 'Rangers.exe') -Value 'wrong baseline' -NoNewline
  $badBaseline = Invoke-Deploy -DeployArguments @('-SdRoot', $badRoot, '-NroPath', $nro, '-GameSource', $badSource, '-InitialGameCopy')
  Assert-Failed $badBaseline 'Rangers.exe SHA-256 mismatch'
  if (Test-Path -LiteralPath (Join-Path $badSd 'switch\space-rangers-hd-a-war-apart\game')) { throw 'baseline rejection copied game/' }
  New-Item -ItemType HardLink -Path (Join-Path $source 'Rangers.exe') -Target $baselineExe | Out-Null
  $initial = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-NroPath', $nro, '-GameSource', $source, '-InitialGameCopy')
  if ($initial.ExitCode -ne 0 -or $initial.Text -notmatch 'READY FOR SWITCH LAUNCH') { throw "initial deployment failed: $($initial.Text)" }
  $game = Join-Path $app 'game'; $destinationNro = Join-Path $app 'SpaceRangersHDAWarApart.nro'; $sourceHash = (Get-FileHash -LiteralPath $nro -Algorithm SHA256).Hash
  if ((Get-FileHash -LiteralPath $destinationNro -Algorithm SHA256).Hash -ne $sourceHash) { throw 'NRO destination hash mismatch' }
  $repeat = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-NroPath', $nro, '-GameSource', $source, '-InitialGameCopy')
  if ($repeat.ExitCode -ne 0 -or $repeat.Text -notmatch 'GAME COPY SKIPPED') { throw "repeat-copy protection failed: $($repeat.Text)" }
  $gameSnapshot = Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { "$($_.FullName.Substring($game.Length))|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)" }
  $update = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-NroPath', $nro, '-UpdateOnly')
  if ($update.ExitCode -ne 0 -or $update.Text -notmatch 'Mode: UPDATE') { throw "update deployment failed: $($update.Text)" }
  $afterSnapshot = Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { "$($_.FullName.Substring($game.Length))|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)" }
  if (Compare-Object $gameSnapshot $afterSnapshot) { throw 'UpdateOnly changed game/' }
  Set-Content -LiteralPath (Join-Path $logs 'port.log') -Value 'port'
  $collected = Join-Path $root 'hardware-logs'; $partial = Invoke-Deploy -DeployArguments @('-SdRoot', $sdRoot, '-CollectLogs', '-HardwareLogsRoot', $collected)
  if ($partial.ExitCode -ne 0 -or $partial.Text -notmatch 'CollectLogs: PASS \(1 files\)') { throw "partial log collection failed: $($partial.Text)" }
  $logSet = Get-ChildItem -LiteralPath $collected -Recurse -File | Select-Object -ExpandProperty Name
  if (@(Compare-Object @('port.log') $logSet).Count -ne 0) { throw 'partial collected log set mismatch' }
  Write-Host 'deploy-switch synthetic regression passed'
} finally {
  Remove-PSDrive -Name SRHDSD -ErrorAction SilentlyContinue; Remove-PSDrive -Name SRHDBAD -ErrorAction SilentlyContinue
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
