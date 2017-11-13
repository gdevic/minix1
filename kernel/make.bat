echo off
echo/
if "%1"=="-l" _link.bat
echo    Making Kernel
echo/
echo    To re-make the kernel after you changed a module,
echo    just delete the .OBJ file for that module.
echo    Specifying a '-l' switch will only link the kernel
echo    (e.g. after a manual compilation of a module).
echo    NOTE: do not forget the "-di8088" on a manual compilation.
echo/
if exist MPX88.OBJ goto a1
echo    assembling: mpx88
masm mpx88,,nul,nul >mpx88.lst
:a1
if exist KLIB88.OBJ goto cc
echo    assembling: klib88
masm klib88,,nul,nul >klib88.lst
:cc
if exist CLOCK.OBJ goto fout
echo    compiling: clock
cc1 -di8088 clock >clock.lst
if errorlevel 1 goto fout
cc2 clock >>clock.lst
if errorlevel 1 goto fout
cc3 clock>>clock.lst
if errorlevel 1 goto fout
cc4 clock>>clock.lst
:fout
if exist DMP.OBJ goto fout2
echo    compiling: dmp
cc1 -di8088 dmp>dmp.lst
if errorlevel 1 goto fout2
cc2 dmp>>dmp.lst
if errorlevel 1 goto fout2
cc3 dmp>>dmp.lst
if errorlevel 1 goto fout2
cc4 dmp>>dmp.lst
:fout2
if exist FLOPPY.OBJ goto fout3
echo    compiling: floppy
cc1 -di8088  floppy>floppy.lst
if errorlevel 1 goto fout3
cc2 floppy >>floppy.lst
if errorlevel 1 goto fout3
cc3 floppy >>floppy.lst
if errorlevel 1 goto fout3
cc4 floppy >>floppy.lst
:fout3
if exist MAIN.OBJ goto fout4
echo    compiling: main
cc1 -di8088  main >main.lst
if errorlevel 1 goto fout4
cc2 main >>main.lst
if errorlevel 1 goto fout4
cc3 main >>main.lst
if errorlevel 1 goto fout4
cc4 main >>main.lst
:fout4
if exist MEMORY.OBJ goto fout5
echo    compiling: memory
cc1 -di8088  memory >memory.lst
if errorlevel 1 goto fout5
cc2 memory >>memory.lst
if errorlevel 1 goto fout5
cc3 memory >>memory.lst
if errorlevel 1 goto fout5
cc4 memory >>memory.lst
:fout5
if exist PRINTER.OBJ goto fout6
echo    compiling: printer
cc1 -di8088  printer >printer.lst
if errorlevel 1 goto fout6
cc2 printer >>printer.lst
if errorlevel 1 goto fout6
cc3 printer >>printer.lst
if errorlevel 1 goto fout6
cc4 printer >>printer.lst
:fout6
if exist PROC.OBJ goto fout7
echo    compiling: proc
cc1 -di8088  proc >proc.lst
if errorlevel 1 goto fout7
cc2 proc >>proc.lst
if errorlevel 1 goto fout7
cc3 proc >>proc.lst
if errorlevel 1 goto fout7
cc4 proc >>proc.lst
:fout7
if exist SYSTEM.OBJ goto fout8
echo    compiling: system
cc1 -di8088  system >system.lst
if errorlevel 1 goto fout8
cc2 system >>system.lst
if errorlevel 1 goto fout8
cc3 system >>system.lst
if errorlevel 1 goto fout8
cc4 system >>system.lst
:fout8
if exist TABLE.OBJ goto fout9
echo    compiling: table
cc1 -di8088  table >table.lst
if errorlevel 1 goto fout9
cc2 table >>table.lst
if errorlevel 1 goto fout9
cc3 table >>table.lst
if errorlevel 1 goto fout9
cc4 table >>table.lst
:fout9
if exist TTY.OBJ goto fout10
echo    compiling: tty
cc1 -di8088  tty >tty.lst
if errorlevel 1 goto fout10
cc2 tty >>tty.lst
if errorlevel 1 goto fout10
cc3 tty >>tty.lst
if errorlevel 1 goto fout10
cc4 tty >>tty.lst
:fout10
if exist WINI.OBJ goto fout11
echo    compiling: wini
cc1 -di8088  wini >wini.lst
if errorlevel 1 goto fout11
cc2 wini >>wini.lst
if errorlevel 1 goto fout11
cc3 wini >>wini.lst
if errorlevel 1 goto fout11
cc4 wini >>wini.lst
:fout11
_link.bat
