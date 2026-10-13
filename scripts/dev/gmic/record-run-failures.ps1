# Appends the filters a run of the gmic_allowlisted_filters_run_with_defaults core test
# reported as failing to src\filters\gmic\patchy_filters_run_failures.txt, each with a reason
# derived from the interpreter's message, then regenerates the allowlist. Review the appended
# lines: the reasons are a first reading of the error text. See docs/gmic.md.
#
#   powershell -NoProfile -File scripts\dev\gmic\record-run-failures.ps1 -Log <test output> [-Note "on a layer without alpha"]
param(
  [Parameter(Mandatory)][string]$Log,
  [string]$Note = '',
  [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$file = Join-Path $repo 'src\filters\gmic\patchy_filters_run_failures.txt'
$utf8 = New-Object Text.UTF8Encoding($false)
$text = [IO.File]::ReadAllText($file, $utf8)
$known = @{}
foreach ($line in $text -split "`n") {
  if ($line.Trim() -and -not $line.StartsWith('#')) { $known[($line -split '\s+')[0]] = $true }
}
$added = 0
foreach ($match in Select-String -Path $Log -Pattern 'GMIC-FAIL (\S+) \| (.*)$') {
  $id = $match.Matches[0].Groups[1].Value
  $message = $match.Matches[0].Groups[2].Value
  if ($known.ContainsKey($id)) { continue }
  $why = if ($message -match 'Not available in this build') { 'uses a patch-based algorithm, which this build leaves out' }
    elseif ($message -match 'FFT\(\)') { 'needs an FFT of any size (this build has no FFTW, only power-of-two sizes)' }
    elseif ($message -match 'input_cached|Unreachable network') { "needs a data file G'MIC downloads" }
    elseif ($message -match "Command 'font'") { "needs a font G'MIC downloads" }
    elseif ($message -match 'No display available|X11 or GDI32|display|window') { 'needs a window (interactive)' }
    elseif ($message -match 'did not finish') { 'does not finish in 10 s at default settings on the test image' }
    elseif ($message -match 'input layers|layers are needed|least two') { 'needs more input layers than it declares' }
    elseif ($message -match 'returned no image') { 'returns no image' }
    else { 'fails with its default parameters: ' + ($message -replace '\s+', ' ').Substring(0, [Math]::Min(110, $message.Length)).Trim() }
  if ($Note) { $why = "$why ($Note)" }
  $text = $text.TrimEnd() + "`n$id  # $why`n"
  $known[$id] = $true
  $added++
}
[IO.File]::WriteAllText($file, $text, $utf8)
"recorded $added new failure(s)"
& $Python (Join-Path $repo 'scripts\dev\gmic\build_allowlist.py') | Select-Object -First 1
