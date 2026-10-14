@echo off
rem Launcher for setup-windows-ssh.ps1 (see its header). Double-click it, or run it
rem from cmd or PowerShell: either way it starts 64-bit Windows PowerShell 5.1 by full
rem path, so the shell that launched it never matters. The script then asks UAC for
rem elevation itself. Extra arguments (-PublicKeyFile <path>) are passed through.
setlocal
set "PATCHY_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
rem A 32-bit cmd sees System32 redirected; Sysnative is the real 64-bit System32 there.
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "PATCHY_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%PATCHY_PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-windows-ssh.ps1" -Pause %*
exit /b %ERRORLEVEL%
