@echo off
REM
REM CloneCLI.bat - A batch file wrapper for SlimVDI.exe.
REM
REM Running SlimVDI inside this "wrapper" when using the Windows
REM Command Prompt (cmd.exe) ensures that the CMD window waits for
REM SlimVDI to finish before issuing the next prompt (the CMD
REM window does not normally wait for GUI applications to terminate).
REM
REM An alternative is to run SlimVDI using the "start /wait" command,
REM eg. from the prompt type "start /wait slimvdi "+[slimvdi arguments]
REM
slimvdi.exe %1 %2 %3 %4 %5 %6 %7 %8 %9
