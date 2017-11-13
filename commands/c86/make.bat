echo off
:begin
if "%1"=="" goto eind
echo/
echo   Compiling %1 for Minix
cc1 %1 > %1.lst
if errorlevel 1 goto fout
cc2 %1 >> %1.lst
if errorlevel 1 goto fout
cc3 %1 >> %1.lst
if errorlevel 1 goto fout
cc4 %1 >> %1.lst
if errorlevel 1 goto fout
link ..\lib\crtso+%1,%1,%1,..\lib\mxc86 >> %1.lst
dos2out -d %1 >>%1.lst
shift
goto begin
:fout
echo +++error in compilation+++
shift
goto begin
:eind
