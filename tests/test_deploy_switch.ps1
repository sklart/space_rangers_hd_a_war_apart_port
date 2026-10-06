$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent $PSScriptRoot
$script = Join-Path $repo 'tools\deploy-switch.ps1'
$baselineExe = Join-Path $repo 'windows\Space Rangers HD A War Apart\Rangers.exe'
$nro = Join-Path $repo 'port\switch\SpaceRangersHDAWarApart.nro'
if (-not (Test-Path -LiteralPath $baselineExe) -or -not (Test-Path -LiteralPath $nro)) { throw 'local deployment fixture is unavailable' }
$build = Join-Path $repo 'build'; [void](New-Item -ItemType Directory -Force -Path $build)
$root = Join-Path $build 'deploy-switch-test'
if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
try {
  $source = Join-Path $root 'source'; $sd = Join-Path $root 'sd'; [void](New-Item -ItemType Directory -Force -Path $source); [void](New-Item -ItemType Directory -Force -Path $sd)
  foreach ($relative in @('install.txt', 'cfg.txt', 'install_russian.txt', 'DATA\common.pkg', 'CFG\Main.dat', 'CFG\russian\Lang.dat', 'CFG\CacheData.dat')) {
    $file = Join-Path $source $relative; [void](New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file)); Set-Content -LiteralPath $file -Value 'fixture' -NoNewline
  }
  $badSource = Join-Path $root 'bad-source'; $badSd = Join-Path $root 'bad-sd'; [void](New-Item -ItemType Directory -Force -Path $badSource); [void](New-Item -ItemType Directory -Force -Path $badSd)
  foreach ($relative in @('install.txt', 'cfg.txt', 'install_russian.txt', 'DATA\common.pkg', 'CFG\Main.dat', 'CFG\russian\Lang.dat', 'CFG\CacheData.dat')) {
    $file = Join-Path $badSource $relative; [void](New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file)); Set-Content -LiteralPath $file -Value 'fixture' -NoNewline
  }
  Set-Content -LiteralPath (Join-Path $badSource 'Rangers.exe') -Value 'wrong baseline' -NoNewline
  $bad = & $script -SdRoot $badSd -NroPath $nro -GameSource $badSource -InitialGameCopy 2>&1
  if ($LASTEXITCODE -eq 0 -or (Test-Path -LiteralPath (Join-Path $badSd 'switch\space-rangers-hd-a-war-apart\game'))) { throw 'baseline rejection copied game/' }  New-Item -ItemType HardLink -Path (Join-Path $source 'Rangers.exe') -Target $baselineExe | Out-Null
  $initial = & $script -SdRoot $sd -NroPath $nro -GameSource $source -InitialGameCopy 2>&1
  if ($LASTEXITCODE -ne 0) { throw "initial deployment failed" }
  $app = Join-Path $sd 'switch\space-rangers-hd-a-war-apart'; $game = Join-Path $app 'game'
  $sourceHash = (Get-FileHash -LiteralPath $nro -Algorithm SHA256).Hash; $destinationNro = Join-Path $app 'SpaceRangersHDAWarApart.nro'
  if ((Get-FileHash -LiteralPath $destinationNro -Algorithm SHA256).Hash -ne $sourceHash) { throw 'NRO destination hash mismatch' }
  $repeat = & $script -SdRoot $sd -NroPath $nro -GameSource $source -InitialGameCopy 2>&1
  if ($LASTEXITCODE -ne 0) { throw "repeat-copy protection failed" }
  $gameSnapshot = Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { "$($_.FullName.Substring($game.Length))|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)" }
  $update = & $script -SdRoot $sd -NroPath $nro -UpdateOnly 2>&1
  if ($LASTEXITCODE -ne 0) { throw "update deployment failed" }
  $afterSnapshot = Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { "$($_.FullName.Substring($game.Length))|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)" }
  if (Compare-Object $gameSnapshot $afterSnapshot) { throw 'UpdateOnly changed game/' }
  $logs = Join-Path $app 'logs'; Set-Content -LiteralPath (Join-Path $logs 'port.log') -Value 'port'; Set-Content -LiteralPath (Join-Path $logs 'port-prev.log') -Value 'prev'; Set-Content -LiteralPath (Join-Path $logs 'gr-main.log') -Value 'gr'
  $collected = Join-Path $root 'hardware-logs'; $collect = & $script -SdRoot $sd -CollectLogs -HardwareLogsRoot $collected 2>&1
  if ($LASTEXITCODE -ne 0) { throw "log collection failed" }
  $logSet = Get-ChildItem -LiteralPath $collected -Recurse -File | Select-Object -ExpandProperty Name | Sort-Object
  if (@(Compare-Object @('gr-main.log', 'port-prev.log', 'port.log') $logSet).Count -ne 0) { throw 'collected log set mismatch' }
  Write-Host 'deploy-switch synthetic regression passed'
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
