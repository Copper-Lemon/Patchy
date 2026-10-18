# Runs ps-artboard-audit.jsx in Photoshop over COM (always authorized, docs/photoshop-com.md)
# and prints its result lines. -Cases picks the cases (default: all the script defines).
param([string[]]$Cases = @())

$ErrorActionPreference = 'Stop'
$jsx = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'ps-artboard-audit.jsx')
if ($Cases.Count -gt 0) {
  $list = ($Cases | ForEach-Object { '"' + $_ + '"' }) -join ','
  $jsx = "var AUDIT_CASES = [$list];`n" + $jsx
}
$ps = New-Object -ComObject Photoshop.Application
$ps.DoJavaScript($jsx)
