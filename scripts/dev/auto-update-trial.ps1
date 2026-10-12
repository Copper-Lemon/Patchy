<#
Builds a local trial of the Windows one-click update (docs/auto-update.md) without touching
build\package: an unsigned installer made from build\release plus the runtime files of the
last staged package (build\package\staging\Patchy), and a manifest that offers it as
version 99.0 from a loopback web server.

  scripts\dev\auto-update-trial.ps1 [-Port 8765]

Everything lands in build\test-output\auto-update-trial:
  site\PatchyWindowsInstaller.exe, site\latest_version.json   what the server serves
  install\                                                     where the trial installs

The script only builds. It prints the three commands of the trial (install, serve, run).
Installing writes the per-user uninstall entry, the Start Menu shortcut and the "Open with"
registration, exactly as the real installer does; install\UninstallPatchy.exe removes them.
#>
param([int]$Port = 8765)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not (Test-Path -LiteralPath (Join-Path $repo 'CMakeLists.txt'))) { throw "not the Patchy repository: $repo" }
$release = Join-Path $repo 'build\release'
$lastStage = Join-Path $repo 'build\package\staging\Patchy'
$packaging = Join-Path $repo 'packaging\windows'
$icon = Join-Path $repo 'src\app\patchy.ico'
foreach ($needed in @((Join-Path $release 'patchy.exe'), (Join-Path $lastStage 'PatchyInstallManifest.txt'))) {
  if (-not (Test-Path -LiteralPath $needed)) { throw "missing $needed (build the release preset; a package must have been staged once)" }
}

$trial = Join-Path $repo 'build\test-output\auto-update-trial'
$installDir = Join-Path $trial 'install'
# Rebuilds the installer and manifest; an installed trial copy in install\ is left alone,
# so a changed installer can be tried as the next update.
foreach ($generated in 'stage', 'payload', 'site') {
  $old = Join-Path $repo "build\test-output\auto-update-trial\$generated"
  if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }
}
$stage = Join-Path $trial 'stage\Patchy'
$payload = Join-Path $trial 'payload'
$site = Join-Path $trial 'site'
foreach ($folder in @((Split-Path -Parent $stage), $payload, $site)) { New-Item -ItemType Directory -Path $folder -Force | Out-Null }

# The last package's file set, with this build's executables and translations over it.
Copy-Item -LiteralPath $lastStage -Destination (Split-Path -Parent $stage) -Recurse
foreach ($exe in 'patchy.exe', 'patchy-mcp.exe', 'patchy-8bf-host32.exe', 'patchy-8bf-host64.exe') {
  Copy-Item -LiteralPath (Join-Path $release $exe) -Destination (Join-Path $stage $exe) -Force
  # A fresh time on every trial build: a connector left running by the previous trial sees
  # a different file at its path, as it would after a real update (connector_freshness.hpp).
  (Get-Item -LiteralPath (Join-Path $stage $exe)).LastWriteTime = Get-Date
}
Get-ChildItem -LiteralPath (Join-Path $stage 'translations') -Filter 'patchy_*.qm' | ForEach-Object {
  Copy-Item -LiteralPath (Join-Path $release "translations\$($_.Name)") -Destination $_.FullName -Force
}

$version = [regex]::Match((Get-Content -Raw -LiteralPath (Join-Path $repo 'CMakeLists.txt')),
  'project\s*\([\s\S]*?\bVERSION\s+([0-9]+(?:\.[0-9]+){1,3})').Groups[1].Value
Compress-Archive -Path $stage -DestinationPath (Join-Path $payload 'PatchyWindowsNoInstaller.zip')
Copy-Item -LiteralPath (Join-Path $packaging 'InstallPatchy.ps1') -Destination $payload
Copy-Item -LiteralPath $icon -Destination (Join-Path $payload 'Patchy.ico')
Set-Content -LiteralPath (Join-Path $payload 'PatchyVersion.txt') -Value $version -Encoding ASCII
$csc = Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:winexe /platform:x64 /optimize+ "/win32icon:$icon" /reference:System.Windows.Forms.dll `
  "/out:$(Join-Path $payload 'InstallPatchy.exe')" (Join-Path $packaging 'InstallPatchyLauncher.cs')
if ($LASTEXITCODE -ne 0) { throw 'the installer launcher did not compile' }
& $csc /nologo /target:winexe /platform:x64 /optimize+ "/win32icon:$icon" `
  "/out:$(Join-Path $payload 'UninstallPatchy.exe')" (Join-Path $packaging 'UninstallPatchy.cs')
if ($LASTEXITCODE -ne 0) { throw 'the uninstaller did not compile' }

$installer = Join-Path $site 'PatchyWindowsInstaller.exe'
$sedPath = Join-Path $trial 'PatchyWindowsInstaller.sed'
$sed = (Get-Content -Raw -LiteralPath (Join-Path $packaging 'PatchyWindowsInstaller.sed.in')).
  Replace('__INSTALLER_PATH__', $installer).Replace('__PAYLOAD_DIR__', "$payload\").Replace('__APP_ICON_PATH__', $icon)
Set-Content -LiteralPath $sedPath -Value $sed -Encoding ASCII
Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\iexpress.exe') -ArgumentList @('/N', '/Q', $sedPath) -Wait
$deadline = (Get-Date).AddSeconds(120)
$lastSize = -1
while ($true) {
  if (Test-Path -LiteralPath $installer) {
    $size = (Get-Item -LiteralPath $installer).Length
    if ($size -gt 0 -and $size -eq $lastSize) { break }
    $lastSize = $size
  }
  if ((Get-Date) -gt $deadline) { throw "IExpress did not create $installer" }
  Start-Sleep -Milliseconds 500
}

$sha = [System.Security.Cryptography.SHA256]::Create()
$stream = [IO.File]::OpenRead($installer)
try { $hash = ([BitConverter]::ToString($sha.ComputeHash($stream)) -replace '-', '').ToLowerInvariant() } finally { $stream.Dispose(); $sha.Dispose() }
$manifest = @"
{
  "platforms": {
    "windows": {
      "version": "99.0",
      "download_url": "http://127.0.0.1:$Port/PatchyWindowsInstaller.exe",
      "installer_url": "http://127.0.0.1:$Port/PatchyWindowsInstaller.exe",
      "installer_size": $((Get-Item -LiteralPath $installer).Length),
      "installer_sha256": "$hash"
    }
  }
}
"@
[IO.File]::WriteAllText((Join-Path $site 'latest_version.json'), $manifest)

Write-Host "Trial installer ($version, unsigned): $installer"
Write-Host ''
Write-Host '1. Install the trial copy (update mode into an empty folder; Patchy starts, close it):'
Write-Host "   `$env:PATCHY_UPDATE_INSTALL_DIR = '$installDir'; & '$installer'; Remove-Item Env:PATCHY_UPDATE_INSTALL_DIR"
Write-Host '2. Serve the manifest and installer (leave running):'
Write-Host "   python -m http.server $Port --bind 127.0.0.1 --directory '$site'"
Write-Host '3. Start the installed copy against that manifest and click Update Now:'
Write-Host "   `$env:PATCHY_UPDATE_MANIFEST_URL = 'http://127.0.0.1:$Port/latest_version.json'; & '$installDir\patchy.exe'; Remove-Item Env:PATCHY_UPDATE_MANIFEST_URL"
Write-Host "Afterwards: '$installDir\UninstallPatchy.exe' removes the trial install."
