if "%1"=="-l" goto link
if exist init.obj goto link
cc1 init >init.lst
if errorlevel 1 goto fout
cc2 init >>init.lst
if errorlevel 1 goto fout
cc3 init >>init.lst
if errorlevel 1 goto fout
cc4 init >>init.lst
if errorlevel 1 goto fout
:link
link/m ..\lib\head+init,init,init,..\lib\mxc86 >>init.lst
dos2out -d init >>init.lst
echo/
echo   Init done. Hit reurn to see list file.
pause
echo on
type init.lst
