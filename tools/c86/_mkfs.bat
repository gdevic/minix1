if "%1"=="-l" goto link
cc1 -dDOS mkfs
if errorlevel 1 goto fout
cc2 mkfs
if errorlevel 1 goto fout
cc3 mkfs
if errorlevel 1 goto fout
cc4 mkfs
if errorlevel 1 goto fout
:link
link mkfs+diskio,mkfs,nul,\lib\c86\c86s2s.lib
goto ok
:fout
pause error in compilation
:ok
