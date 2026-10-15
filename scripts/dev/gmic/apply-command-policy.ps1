# Reapply the interpreter command-registration restriction after an upstream
# refresh. Latin-1 preserves the vendored bytes. See docs/gmic.md.
param([Parameter(Mandatory)][string]$Dir)
$ErrorActionPreference = 'Stop'
$Encoding = [Text.Encoding]::GetEncoding(28591)
$Path = Join-Path $Dir 'gmic.cpp'
$Text = [IO.File]::ReadAllText($Path, $Encoding)
if ($Text.Contains('PATCHY: command registration policy')) {
  throw 'gmic.cpp already has the command policy'
}
function Replace-Once([string]$Before, [string]$After) {
  if ($script:Text.Split(@($Before), [StringSplitOptions]::None).Count -ne 2) {
    throw "Expected one command-policy anchor: $Before"
  }
  $script:Text = $script:Text.Replace($Before, $After)
}
Replace-Once '#include "gmic_stdlib_community.h"' @'
#include "gmic_stdlib_community.h"
#include "gmic_command_policy.hpp" // PATCHY: command registration policy
'@
Replace-Once '    bool is_last_slash = false, _is_last_slash = false, is_newline = false;' @'
    bool is_last_slash = false, _is_last_slash = false, is_newline = false;
    bool patchy_restricted_definition = false; // PATCHY: command registration policy
'@
Replace-Once '        if (is_main_ && !std::strcmp(s_name,"_main_")) *is_main_ = true;' @'
        // PATCHY: command registration policy. Replace the entire definition,
        // including continuation lines, before any interpreter can execute it
        // or expose it through $$name. This also covers nested run(), aliases,
        // parallel interpreters, and later command redefinitions.
        patchy_restricted_definition = patchy::gmic::restricted_command(s_name.data());
        if (patchy_restricted_definition)
          CImg<char>::string("error \"Not available in this build\"").move_to(body);
        if (is_main_ && !std::strcmp(s_name,"_main_")) *is_main_ = true;
'@
Replace-Once '      } else { // Continuation of a previous line' @'
      } else { // Continuation of a previous line
        if (patchy_restricted_definition) continue; // PATCHY: keep only the error stub
'@
Replace-Once '      // Check for verbosity command, prior to the first output of a log message.' @'
      // PATCHY: patch-based denoising is a built-in, not a script definition.
      if (id_builtin_command==id_denoise)
        error(false,"Command '%s': Not available in this build.",command);

      // Check for verbosity command, prior to the first output of a log message.
'@
[IO.File]::WriteAllText($Path, $Text, $Encoding)
Write-Output 'patched command registration policy'
