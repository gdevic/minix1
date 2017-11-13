echo off
if "%1"=="-l" _link.bat
echo/
echo    Making Memory Manager
echo/
echo    To re-make the memory manager after you changed
echo    a module, just delete the .OBJ file for that module.
echo    Specifying a '-l' switch will only do the linking
echo    (e.g. after a manual compilation of a module).
echo/
cd ..\lib
if exist head.OBJ goto cc
masm head,,nul,nul >head.lst
:cc
cd ..\mm
if exist EXEC.OBJ goto fout
echo    compiling: exec
cc1 -di8088 exec >exec.lst
if errorlevel 1 goto fout
cc2 exec >>exec.lst
if errorlevel 1 goto fout
cc3 exec>>exec.lst
if errorlevel 1 goto fout
cc4 exec>>exec.lst
:fout
if exist ALLOC.OBJ goto fout2
echo    compiling: alloc
cc1 -di8088 alloc>alloc.lst
if errorlevel 1 goto fout2
cc2 alloc>>alloc.lst
if errorlevel 1 goto fout2
cc3 alloc>>alloc.lst
if errorlevel 1 goto fout2
cc4 alloc>>alloc.lst
:fout2
if exist BREAK.OBJ goto fout3
echo    compiling: break
cc1 -di8088  break>break.lst
if errorlevel 1 goto fout3
cc2 break >>break.lst
if errorlevel 1 goto fout3
cc3 break >>break.lst
if errorlevel 1 goto fout3
cc4 break >>break.lst
:fout3
rem
if exist FORKEXIT.OBJ goto fout4
echo    compiling: forkexit
cc1 -di8088  forkexit >forkexit.lst
if errorlevel 1 goto fout4
cc2 forkexit >>forkexit.lst
if errorlevel 1 goto fout4
cc3 forkexit >>forkexit.lst
if errorlevel 1 goto fout4
cc4 forkexit >>forkexit.lst
:fout4
if exist getset.OBJ goto fout5
echo    compiling: getset
cc1 -di8088  getset >getset.lst
if errorlevel 1 goto fout5
cc2 getset >>getset.lst
if errorlevel 1 goto fout5
cc3 getset >>getset.lst
if errorlevel 1 goto fout5
cc4 getset >>getset.lst
:fout5
if exist main.OBJ goto fout6
echo    compiling: main
cc1 -di8088  main >>main.lst
if errorlevel 1 goto fout6
cc2 main >>main.lst
if errorlevel 1 goto fout6
cc3 main >>main.lst
if errorlevel 1 goto fout6
cc4 main >>main.lst
:fout6
if exist putc.OBJ goto fout7
echo    compiling: putc
cc1 -di8088  putc >putc.lst
if errorlevel 1 goto fout7
cc2 putc >>putc.lst
if errorlevel 1 goto fout7
cc3 putc >>putc.lst
if errorlevel 1 goto fout7
cc4 putc >>putc.lst
:fout7
if exist signal.OBJ goto fout8
echo    compiling: signal
cc1 -di8088  signal >signal.lst
if errorlevel 1 goto fout8
cc2 signal >>signal.lst
if errorlevel 1 goto fout8
cc3 signal >>signal.lst
if errorlevel 1 goto fout8
cc4 signal >>signal.lst
:fout8
if exist table.OBJ goto fout9
echo    compiling: table
cc1 -di8088  table >table.lst
if errorlevel 1 goto fout9
cc2 table >>table.lst
if errorlevel 1 goto fout9
cc3 table >>table.lst
if errorlevel 1 goto fout9
cc4 table >>table.lst
:fout9
if exist utility.OBJ goto fout10
echo    compiling: utility
cc1 -di8088  utility >utility.lst
if errorlevel 1 goto fout10
cc2 utility >>utility.lst
if errorlevel 1 goto fout10
cc3 utility >>utility.lst
if errorlevel 1 goto fout10
cc4 utility >>utility.lst
:fout10
_link.bat
