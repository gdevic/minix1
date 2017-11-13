echo off
if "%1"=="-l" _link.bat
echo/
echo    Making File system task
echo/
echo    To re-make the file system task after you changed
echo    a module, just delete the .OBJ file for that module.
echo    Specifying a '-l' switch will only do the linking
echo    (e.g. after a manual compilation of a module).
echo/
cd ..\lib
if exist head.OBJ goto cc
echo   assembling head
masm head,,nul,nul >..\fs\head.lst
:cc
cd ..\fs
if exist write.OBJ goto fout
echo    compiling write
cc1 -di8088 write >write.lst
if errorlevel 1 goto fout
cc2 write >>write.lst
if errorlevel 1 goto fout
cc3 write>>write.lst
if errorlevel 1 goto fout
cc4 write>>write.lst
:fout
rem
if exist cache.OBJ goto fout2
echo    compiling cache
cc1 -di8088 cache>cache.lst
if errorlevel 1 goto fout2
cc2 cache>>cache.lst
if errorlevel 1 goto fout2
cc3 cache>>cache.lst
if errorlevel 1 goto fout2
cc4 cache>>cache.lst
:fout2
rem
if exist device.OBJ goto fout3
echo    compiling devices
cc1 -di8088  device>device.lst
if errorlevel 1 goto fout3
cc2 device >>device.lst
if errorlevel 1 goto fout3
cc3 device >>device.lst
if errorlevel 1 goto fout3
cc4 device >>device.lst
:fout3
rem
if exist filedes.OBJ goto fout4
echo    compiling filedes
cc1 -di8088  filedes >filedes.lst
if errorlevel 1 goto fout4
cc2 filedes >>filedes.lst
if errorlevel 1 goto fout4
cc3 filedes >>filedes.lst
if errorlevel 1 goto fout4
cc4 filedes >>filedes.lst
:fout4
rem
if exist inode.OBJ goto fout5
echo    compiling inode
cc1 -di8088  inode >inode.lst
if errorlevel 1 goto fout5
cc2 inode >>inode.lst
if errorlevel 1 goto fout5
cc3 inode >>inode.lst
if errorlevel 1 goto fout5
cc4 inode >>inode.lst
:fout5
rem
if exist main.OBJ goto fout6
echo    compiling main
cc1 -di8088  main >>main.lst
if errorlevel 1 goto fout6
cc2 main >>main.lst
if errorlevel 1 goto fout6
cc3 main >>main.lst
if errorlevel 1 goto fout6
cc4 main >>main.lst
:fout6
rem
if exist link.OBJ goto fout7
echo    compiling link
cc1 -di8088  link >link.lst
if errorlevel 1 goto fout7
cc2 link >>link.lst
if errorlevel 1 goto fout7
cc3 link >>link.lst
if errorlevel 1 goto fout7
cc4 link >>link.lst
:fout7
rem
if exist misc.OBJ goto fout8
echo    compiling misc
cc1 -di8088  misc >misc.lst
if errorlevel 1 goto fout8
cc2 misc >>misc.lst
if errorlevel 1 goto fout8
cc3 misc >>misc.lst
if errorlevel 1 goto fout8
cc4 misc >>misc.lst
:fout8
rem
if exist mount.OBJ goto fout9
echo    compiling mount
cc1 -di8088  mount >mount.lst
if errorlevel 1 goto fout9
cc2 mount >>mount.lst
if errorlevel 1 goto fout9
cc3 mount >>mount.lst
if errorlevel 1 goto fout9
cc4 mount >>mount.lst
:fout9
rem
if exist open.OBJ goto fout10
echo    compiling open
cc1 -di8088  open >open.lst
if errorlevel 1 goto fout10
cc2 open >>open.lst
if errorlevel 1 goto fout10
cc3 open >>open.lst
if errorlevel 1 goto fout10
cc4 open >>open.lst
:fout10
if exist path.OBJ goto fout11
echo    compiling path
cc1 -di8088  path >path.lst
if errorlevel 1 goto fout11
cc2 path >>path.lst
if errorlevel 1 goto fout11
cc3 path >>path.lst
if errorlevel 1 goto fout11
cc4 path >>path.lst
:fout11
if exist pipe.OBJ goto fout12
echo    compiling pipe
cc1 -di8088  pipe >pipe.lst
if errorlevel 1 goto fout12
cc2 pipe >>pipe.lst
if errorlevel 1 goto fout12
cc3 pipe >>pipe.lst
if errorlevel 1 goto fout12
cc4 pipe >>pipe.lst
:fout12
if exist protect.OBJ goto fout13
echo    compiling protect
cc1 -di8088  protect >protect.lst
if errorlevel 1 goto fout13
cc2 protect >>protect.lst
if errorlevel 1 goto fout13
cc3 protect >>protect.lst
if errorlevel 1 goto fout13
cc4 protect >>protect.lst
:fout13
if exist putc.OBJ goto fout14
echo    compiling putc
cc1 -di8088  putc >putc.lst
if errorlevel 1 goto fout14
cc2 putc >>putc.lst
if errorlevel 1 goto fout14
cc3 putc >>putc.lst
if errorlevel 1 goto fout14
cc4 putc >>putc.lst
:fout14
if exist read.OBJ goto fout15
echo    compiling read
cc1 -di8088  read >read.lst
if errorlevel 1 goto fout15
cc2 read >>read.lst
if errorlevel 1 goto fout15
cc3 read >>read.lst
if errorlevel 1 goto fout15
cc4 read >>read.lst
:fout15
if exist stadir.OBJ goto fout16
echo    compiling stadir
cc1 -di8088  stadir >stadir.lst
if errorlevel 1 goto fout16
cc2 stadir >>stadir.lst
if errorlevel 1 goto fout16
cc3 stadir >>stadir.lst
if errorlevel 1 goto fout16
cc4 stadir >>stadir.lst
:fout16
if exist super.OBJ goto fout17
echo    compiling super
cc1 -di8088  super >super.lst
if errorlevel 1 goto fout17
cc2 super >>super.lst
if errorlevel 1 goto fout17
cc3 super >>super.lst
if errorlevel 1 goto fout17
cc4 super >>super.lst
:fout17
if exist table.OBJ goto fout18
echo    compiling table
cc1 -di8088  table >table.lst
if errorlevel 1 goto fout18
cc2 table >>table.lst
if errorlevel 1 goto fout18
cc3 table >>table.lst
if errorlevel 1 goto fout18
cc4 table >>table.lst
:fout18
if exist time.OBJ goto fout19
echo   compiling time
cc1 -di8088  time >time.lst
if errorlevel 1 goto fout19
cc2 time >>time.lst
if errorlevel 1 goto fout19
cc3 time >>time.lst
if errorlevel 1 goto fout19
cc4 time >>time.lst
:fout19
if exist utility.OBJ goto fout20
echo    compiling utility
cc1 -di8088  utility >utility.lst
if errorlevel 1 goto fout20
cc2 utility >>utility.lst
if errorlevel 1 goto fout20
cc3 utility >>utility.lst
if errorlevel 1 goto fout20
cc4 utility >>utility.lst
:fout20
echo/
_link.bat
