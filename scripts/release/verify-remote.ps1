<#
Release verification on the mac or linux build host (docs/release-process.md, "Build and
upload order"):
  1. both full native suites through scripts\remote\remote-build.ps1 (it pushes this working
     tree as a snapshot and builds it there; skip with -SkipSuites),
  2. scripts\release\verify-<target>.sh on the host over ssh: the MCP suites against the
     built and packaged connectors, and on linux the Flatpak and AppImage checks.

  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\release\verify-remote.ps1 -Target mac|linux [-SkipSuites]

Run it after that target's builder (release-all-automated.bat verify-mac / verify-linux do
this unattended); it refuses while build\release-logs\<target>.exit reports a failed build.
Logs: build\release-logs\verify-<target>-suites.log and verify-<target>-checks.log, each with
an .exit marker. A step passes only with exit 0, no [FAIL] line and, for step 2, the script's
closing "ALL ... RELEASE CHECKS PASSED" line. Never name a log verify-<target>.log here: that
is where release-worker.bat redirects this script's own output, so the checks redirect would
fail on the lock and the closing tail would echo the file back into itself without end.
#>
param(
  [Parameter(Mandatory = $true)][ValidateSet('mac', 'linux')][string]$Target,
  [switch]$SkipSuites
)
# 'Continue', not 'Stop': native stderr would become terminating errors under 5.1.
$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'release-process-run.ps1')
$root =(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root
if (-not (Test-Path 'CMakeLists.txt')) { throw "not the repository root: $root" }
. (Join-Path $root 'scripts\remote\remote-hosts.ps1')
$remoteHost = (Get-PatchyRemoteHost $Target).ssh
$logs = Join-Path $root 'build\release-logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null

$version = (Select-String -Path 'CMakeLists.txt' -Pattern '^\s*VERSION\s+([0-9.]+)' | Select-Object -First 1).Matches[0].Groups[1].Value
if (-not $version) { throw 'could not read the project version from CMakeLists.txt' }

$builderMarker = Join-Path $logs "$Target.exit"
if (Test-Path $builderMarker) {
  $builder = (Get-Content $builderMarker -Raw).Trim()
  if ($builder -ne 'exit=0') { Write-Output "$Target builder reported $builder; not verifying."; exit 1 }
}

function Write-Marker([string]$log, [int]$code) {
  [IO.File]::WriteAllText([IO.Path]::ChangeExtension($log, '.exit'), "exit=$code`r`n")
}
function Count([string]$log, [string]$text) {
  return @(Select-String -LiteralPath $log -SimpleMatch $text -ErrorAction SilentlyContinue).Count
}

if (-not $SkipSuites) {
  $suitesLog = Join-Path $logs "verify-$Target-suites.log"
  Write-Output "== $Target full native suites (remote-build.ps1) =="
  $code = Invoke-CmdLine ('powershell -NoProfile -ExecutionPolicy Bypass -File scripts\remote\remote-build.ps1 -Target ' +
                         $Target + ' > "' + $suitesLog + '" 2>&1')
  $fails = (Count $suitesLog '[FAIL]') + (Count $suitesLog '[CRASH]')
  if ($code -eq 0 -and $fails -gt 0) { $code = 1 }
  Write-Marker $suitesLog $code
  Write-Output ("suites: exit={0} pass={1} fail={2}" -f $code, (Count $suitesLog '[PASS]'), $fails)
  if ($code -ne 0) { exit $code }
}

# The host script goes over ssh's stdin with LF line ends: a CR would reach bash as part of
# each command.
$script = (Get-Content (Join-Path $PSScriptRoot "verify-$Target.sh") -Raw) -replace "`r", ''
$scriptCopy = Join-Path $logs "verify-$Target.sh.tmp"
[IO.File]::WriteAllText($scriptCopy, $script, (New-Object System.Text.UTF8Encoding($false)))
$log = Join-Path $logs "verify-$Target-checks.log"
Write-Output "== $Target release checks (verify-$Target.sh $version on $remoteHost) =="
$code = Invoke-CmdLine ('ssh -o ConnectTimeout=30 ' + $remoteHost + ' "bash -s -- ' + $version + '" < "' +
                       $scriptCopy + '" > "' + $log + '" 2>&1')
$fails = Count $log '[FAIL]'
$finished = (Count $log 'RELEASE CHECKS PASSED') -gt 0
if ($code -eq 0 -and ($fails -gt 0 -or -not $finished)) { $code = 1 }
Write-Marker $log $code
Write-Output ("checks: exit={0} pass={1} fail={2}" -f $code, (Count $log '[PASS]'), $fails)
Get-Content -LiteralPath $log -Tail 4
exit $code
