echo off
:begin
if "%1"=="" goto eind
echo/
echo   Compiling %1
cc1 %1 > %1.lst
if errorlevel 1 goto err
cc2 %1 >> %1.lst
if errorlevel 1 goto err
cc3 %1 >> %1.lst
if errorlevel 1 goto err
cc4 %1 >> %1.lst
if errorlevel 1 goto err
shift
goto begin
:err
echo +++error in compilation+++
shift
goto begin
:eind
if '%BATCH%'=='LIB' makelib
