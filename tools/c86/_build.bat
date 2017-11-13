if "%1" == "-l" goto link
if exist diskio.obj goto cc
masm diskio,,nul,nul >build.lst
:cc
if exist build.obj goto link
cc1 -dMSDOS build >> build.lst
if errorlevel 1 goto einde
cc2 build >> build.lst
if errorlevel 1 goto einde
cc3 build >> build.lst
if errorlevel 1 goto einde
cc4 build >> build.lst
if errorlevel 1 goto einde
:link
link/m build+diskio,,,\lib\c86\C86s2s.lib >> build.lst
:einde
