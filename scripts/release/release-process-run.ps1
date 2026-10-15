<#
Dot-sourced by the release verification scripts. Invoke-CmdLine runs one cmd command line
(redirections included) and returns its exit code. It starts cmd.exe through
System.Diagnostics.Process, whose Arguments string reaches cmd exactly as written: calling
cmd from PowerShell instead quotes the line differently in Windows PowerShell 5.1 and in
pwsh 7, and a line with quoted paths that works in one breaks in the other. Let cmd do the
redirecting (> "log" 2>&1, < "input"): the log is then written live and is not re-encoded
as UTF-16 the way a PowerShell redirect would be.
#>
function Invoke-CmdLine {
  param(
    [Parameter(Mandatory = $true)][string]$CommandLine,
    [string]$WorkingDirectory = (Get-Location).Path
  )
  $info = New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName = Join-Path $env:SystemRoot 'System32\cmd.exe'
  # /s: cmd strips just the outer quotes and runs the rest verbatim.
  $info.Arguments = '/d /s /c "' + $CommandLine + '"'
  $info.WorkingDirectory = $WorkingDirectory
  $info.UseShellExecute = $false
  $process = [System.Diagnostics.Process]::Start($info)
  try {
    $process.WaitForExit()
    return $process.ExitCode
  } finally {
    $process.Dispose()
  }
}
