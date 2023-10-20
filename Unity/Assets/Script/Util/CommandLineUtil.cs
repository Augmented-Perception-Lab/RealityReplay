using System;
using System.Diagnostics;
using System.Text;
using System.Threading;

namespace Assets.Script.Util
{
    public static class CommandLineUtil
    {
        //Adapted from https://answers.unity.com/questions/1127023/running-command-line-action-through-c-script.html
        public static Process ExecuteCommand(string workingDirectory, string fileName, string arguments)
        {
            var processInfo = new ProcessStartInfo(fileName, arguments)
            {
                CreateNoWindow = true,
                UseShellExecute = true,
                RedirectStandardError = false,
                RedirectStandardOutput = false,
                WorkingDirectory = workingDirectory
            };

            var process = Process.Start(processInfo);
            return process;
        }
    }
}