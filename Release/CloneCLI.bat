@echo off
REM
REM CloneCLI.bat - A batch file wrapper for CloneVDI.exe.
REM
REM Running CloneVDI inside this "wrapper" when using the Windows
REM Command Prompt (cmd.exe) ensures that the CMD window waits for
REM CloneVDI to finish before issuing the next prompt (the CMD
REM window does not normally wait for GUI applications to terminate).
REM
REM An alternative is to run CloneVDI using the "start /wait" command,
REM eg. from the prompt type "start /wait clonevdi "+[clonevdi arguments]
REM
clonevdi.exe %1 %2 %3 %4 %5 %6 %7 %8 %9
