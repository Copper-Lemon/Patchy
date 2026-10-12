using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

internal static class InstallPatchyLauncher
{
    [STAThread]
    private static int Main()
    {
        Application.EnableVisualStyles();

        string payloadDirectory = AppDomain.CurrentDomain.BaseDirectory;
        string scriptPath = Path.Combine(payloadDirectory, "InstallPatchy.ps1");
        string payloadZip = Path.Combine(payloadDirectory, "PatchyWindowsNoInstaller.zip");
        string uninstallerExe = Path.Combine(payloadDirectory, "UninstallPatchy.exe");
        string versionPath = Path.Combine(payloadDirectory, "PatchyVersion.txt");
        string version = File.Exists(versionPath) ? File.ReadAllText(versionPath).Trim() : "0.0.0";

        if (!File.Exists(scriptPath) || !File.Exists(payloadZip) || !File.Exists(uninstallerExe))
        {
            MessageBox.Show("The Patchy installer payload is incomplete.", "Patchy Setup",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }

        string powershell = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.System),
            @"WindowsPowerShell\v1.0\powershell.exe");
        if (!File.Exists(powershell))
        {
            powershell = "powershell.exe";
        }

        ProcessStartInfo startInfo = new ProcessStartInfo();
        startInfo.FileName = powershell;
        startInfo.Arguments =
            "-NoLogo -NoProfile -ExecutionPolicy Bypass -STA " +
            "-File " + Quote(scriptPath) + " " +
            "-PayloadZip " + Quote(payloadZip) + " " +
            "-Version " + Quote(version);

        // One-click update (docs/auto-update.md): a running Patchy starts this installer
        // with the request in the environment, so nothing has to pass through the
        // self-extractor's command line. No variable means the normal wizard.
        string updateDirectory = Environment.GetEnvironmentVariable("PATCHY_UPDATE_INSTALL_DIR");
        bool updating = !string.IsNullOrWhiteSpace(updateDirectory);
        if (updating)
        {
            char[] separators = new char[] { '\\', '/' };
            updateDirectory = updateDirectory.Trim().TrimEnd(separators);
            startInfo.Arguments += " -Update -InstallDir " + Quote(updateDirectory);
            int waitProcessId;
            if (int.TryParse(Environment.GetEnvironmentVariable("PATCHY_UPDATE_WAIT_PID"), out waitProcessId) &&
                waitProcessId > 0)
            {
                startInfo.Arguments += " -WaitForProcessId " + waitProcessId;
            }
        }
        startInfo.WorkingDirectory = payloadDirectory;
        startInfo.UseShellExecute = false;
        // The Patchy the update starts afterwards must not inherit the request.
        startInfo.EnvironmentVariables.Remove("PATCHY_UPDATE_INSTALL_DIR");
        startInfo.EnvironmentVariables.Remove("PATCHY_UPDATE_WAIT_PID");
        startInfo.CreateNoWindow = true;
        startInfo.WindowStyle = ProcessWindowStyle.Hidden;

        try
        {
            using (Process process = Process.Start(startInfo))
            {
                process.WaitForExit();
                int exitCode = process.ExitCode;
                if (exitCode != 0 && updating)
                {
                    RecoverFromFailedUpdate(updateDirectory);
                }
                return exitCode;
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Patchy Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
            if (updating)
            {
                RecoverFromFailedUpdate(updateDirectory);
            }
            return 1;
        }
    }

    // An update that did not go through must not leave the user with nothing on screen:
    // Patchy closed itself to make way for it. The script reports its own errors, but a
    // PowerShell that never ran the script (blocked by policy, stopped by antivirus) says
    // nothing. Start the installed Patchy again when it is still there; otherwise say what
    // to do.
    private static void RecoverFromFailedUpdate(string installDirectory)
    {
        try
        {
            string installedExe = Path.Combine(installDirectory, "patchy.exe");
            if (File.Exists(installedExe))
            {
                ProcessStartInfo relaunch = new ProcessStartInfo();
                relaunch.FileName = installedExe;
                relaunch.WorkingDirectory = installDirectory;
                relaunch.UseShellExecute = false;
                relaunch.EnvironmentVariables.Remove("PATCHY_UPDATE_INSTALL_DIR");
                relaunch.EnvironmentVariables.Remove("PATCHY_UPDATE_WAIT_PID");
                Process.Start(relaunch);
                return;
            }
        }
        catch (Exception)
        {
        }
        MessageBox.Show(
            "Patchy could not be updated. Download the installer from " +
            "https://github.com/SethRobinson/Patchy/releases/latest and run it.",
            "Patchy Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }
}
