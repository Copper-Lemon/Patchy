# One-time (idempotent) provisioning of the automatic-selection dependencies:
# the pinned ONNX Runtime CPU package and the pinned segmentation models.
# Everything lands in .deps (gitignored, same idea as .deps/Qt); nothing here is
# ever committed. Every download is checked against the SHA-256 pinned below, and
# a file that fails the check is deleted, never used. See docs/ai-selection.md.
#
# Run from anywhere (a git worktree provisions the main checkout's .deps, which is
# where CMake looks for it):
#   pwsh -File scripts\setup-ai-deps.ps1
# Rerunning is a fast no-op once everything is present and verified.
param(
  [string]$DepsDir = '',
  [ValidateSet('win-x64', 'win-arm64')]
  [string]$Platform = 'win-x64'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Keep in sync with scripts/setup-ai-deps.sh and cmake/patchy_ai.cmake.
$OrtVersion = '1.30.0'
$OrtPackages = @{
  'win-x64'   = 'c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949'
  'win-arm64' = 'e53db8a50b23ae35be901cc93428baf997dc8d420333b097b2eae53d3ea9f2d3'
}
$Models = @(
  @{ Name = 'mobile_sam_image_encoder.onnx'
     Url = 'https://huggingface.co/Acly/MobileSAM/resolve/main/mobile_sam_image_encoder.onnx'
     Sha256 = '580f5fb648ea1062c0aabc26217aed56921985f03f0cbbd852bba81d760cc749' },
  @{ Name = 'sam_mask_decoder_multi.onnx'
     Url = 'https://huggingface.co/Acly/MobileSAM/resolve/main/sam_mask_decoder_multi.onnx'
     Sha256 = '8976b90a87ba50a6a72217a5ff994f7d25ce16f2229fcc1ed259e1294c622ffe' },
  @{ Name = 'birefnet_lite_general_1024.onnx'
     Url = 'https://github.com/ZhengPeng7/BiRefNet/releases/download/v1/BiRefNet-general-bb_swin_v1_tiny-epoch_232.onnx'
     Sha256 = '5600024376f572a557870a5eb0afb1e5961636bef4e1e22132025467d0f03333' }
)

if (-not $DepsDir) {
  $CommonDir = (git -C $PSScriptRoot rev-parse --path-format=absolute --git-common-dir).Trim()
  if ($LASTEXITCODE -ne 0 -or -not $CommonDir) { throw 'git rev-parse failed; pass -DepsDir' }
  $MainCheckout = Split-Path -Parent $CommonDir
  if (-not (Test-Path (Join-Path $MainCheckout 'CMakeLists.txt'))) {
    throw "main checkout not found at $MainCheckout; pass -DepsDir"
  }
  $DepsDir = Join-Path $MainCheckout '.deps'
}
New-Item -ItemType Directory -Force $DepsDir | Out-Null

function Get-Sha256([string]$Path) {
  (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

# Downloads to a .part sibling, verifies, then renames, so an interrupted run
# never leaves a file that a later run would mistake for a verified one.
function Get-VerifiedFile([string]$Url, [string]$Destination, [string]$Sha256) {
  if (Test-Path -LiteralPath $Destination) {
    if ((Get-Sha256 $Destination) -eq $Sha256) { return }
    Write-Host "checksum mismatch, downloading again: $Destination"
    Remove-Item -LiteralPath $Destination
  }
  $Part = "$Destination.part"
  Write-Host "downloading $Url"
  Invoke-WebRequest -Uri $Url -OutFile $Part -UseBasicParsing
  $Actual = Get-Sha256 $Part
  if ($Actual -ne $Sha256) {
    Remove-Item -LiteralPath $Part
    throw "SHA-256 mismatch for $Url (expected $Sha256, got $Actual)"
  }
  Move-Item -LiteralPath $Part -Destination $Destination
}

Write-Host "== ONNX Runtime $OrtVersion ($Platform) =="
$OrtRoot = Join-Path $DepsDir "onnxruntime\$OrtVersion"
$OrtDir = Join-Path $OrtRoot $Platform
if (-not (Test-Path (Join-Path $OrtDir 'include\onnxruntime_cxx_api.h'))) {
  New-Item -ItemType Directory -Force $OrtRoot | Out-Null
  $Archive = Join-Path $OrtRoot "onnxruntime-$Platform-$OrtVersion.zip"
  $Url = "https://github.com/microsoft/onnxruntime/releases/download/v$OrtVersion/onnxruntime-$Platform-$OrtVersion.zip"
  Get-VerifiedFile $Url $Archive $OrtPackages[$Platform]
  $Staging = Join-Path $OrtRoot "staging-$Platform"
  if (Test-Path -LiteralPath $Staging) { Remove-Item -LiteralPath "$DepsDir\onnxruntime\$OrtVersion\staging-$Platform" -Recurse -Force }
  Expand-Archive -LiteralPath $Archive -DestinationPath $Staging
  $Inner = Join-Path $Staging "onnxruntime-$Platform-$OrtVersion"
  if (Test-Path -LiteralPath $OrtDir) { Remove-Item -LiteralPath "$DepsDir\onnxruntime\$OrtVersion\$Platform" -Recurse -Force }
  Move-Item -LiteralPath $Inner -Destination $OrtDir
  Remove-Item -LiteralPath "$DepsDir\onnxruntime\$OrtVersion\staging-$Platform" -Recurse -Force
  Remove-Item -LiteralPath $Archive
}
Write-Host "ONNX Runtime ready at $OrtDir"

Write-Host '== Segmentation models =='
$ModelDir = Join-Path $DepsDir 'ai-models'
New-Item -ItemType Directory -Force $ModelDir | Out-Null
foreach ($Model in $Models) {
  Get-VerifiedFile $Model.Url (Join-Path $ModelDir $Model.Name) $Model.Sha256
}
Write-Host "models ready at $ModelDir"
Write-Host '== automatic-selection dependencies ready =='
