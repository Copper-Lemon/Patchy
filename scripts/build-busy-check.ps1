<#
Called by scripts\run-throttled.bat before it starts a `cmake --build`: exits 1 (and says
what is running) when another build is already compiling on this machine, 0 otherwise.

Two builds at once on the dev box (several AI sessions share it) take twice as long each
and can starve the machine. Rather than rely on every session remembering to look first,
the shared build wrapper refuses. The caller then waits, or builds on the Windows offload
host instead: scripts\remote\remote-build.ps1 -Target windows probes the configured hosts
and builds on an idle one. The local build\release stays the release gate.

A build counts when it is a `cmake --build`, a ninja, or an MSBuild process. Any of them,
for any checkout: two builds compete for the same cores whichever folder they write.
PATCHY_ALLOW_CONCURRENT_BUILD=1 skips the check (for when Seth says to build anyway).
#>
$ErrorActionPreference = 'Stop'
if ($env:PATCHY_ALLOW_CONCURRENT_BUILD -eq '1') { exit 0 }

$self = $PID
$builds = @(Get-CimInstance Win32_Process | Where-Object {
    $_.ProcessId -ne $self -and (
      $_.Name -ieq 'ninja.exe' -or $_.Name -ieq 'MSBuild.exe' -or
      ($_.Name -ieq 'cmake.exe' -and $_.CommandLine -match '(^|\s)--build(\s|$)'))
  })
if ($builds.Count -eq 0) { exit 0 }

$lines = $builds | Sort-Object CreationDate | ForEach-Object {
  $started = if ($_.CreationDate) { $_.CreationDate.ToString('HH:mm:ss') } else { '?' }
  "  PID $($_.ProcessId) $($_.Name), started $started"
}
[Console]::Error.WriteLine(@"
run-throttled: another build is already running on this machine:
$($lines -join [Environment]::NewLine)
Not starting a second one beside it. Wait for it to finish, or build and test on the
Windows offload host meanwhile:
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\remote\remote-build.ps1 -Target windows
(the local build\release stays the release gate, so build here once this box is free).
Set PATCHY_ALLOW_CONCURRENT_BUILD=1 only when told to build here anyway.
"@)
exit 1
