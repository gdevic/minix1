echo/
if "%1"=="-b" goto asm
if "%1"=="-m" goto asm
if "%1"=="-d" goto asm
echo   No switch specified for linkage.
echo   Switches are:
echo    -b   link for boot-diskette
echo    -m   link as a minix-program
echo    -d   link as a dos-program
echo   Note that the appropriate symbol must be
echo   defined in both fsck.c and fsck1.asm
echo   Optionally you can specify '-l', which
echo   will only link fsck.
goto eind
:asm
if "%2"=="-l" goto link
if exist fsck1.obj goto c1
masm fsck1,,nul,nul >fsck.lst
:c1
if exist fsck.obj goto link
cc1 fsck >fsck.lst
if errorlevel 1 goto fout
cc2 fsck >>fsck.lst
if errorlevel 1 goto fout
cc3 fsck >>fsck.lst
if errorlevel 1 goto fout
cc4 fsck >>fsck.lst
if errorlevel 1 goto fout
:link
if not "%1"=="-b" goto m
echo    Linking (boot version)
link/m fsck1+fsck,fsck,fsck,..\lib\mxc86 >>fsck.lst
dos2out -d fsck >>fsck.lst
goto eind
:m
if not "%1"=="-m" goto d
echo    Linking (minix version)
link/m ..\lib\crtso+fsck+fsck1,fsck,fsck,..\lib\mxc86 >>fsck.lst
dos2out -d fsck >>fsck.lst
goto eind
:d
if not "%1"=="-d" goto fout
echo    Linking (dos version)
link/m fsck+fsck1,,,\lib\c86\c86s2s.lib >>fsck.lst
goto eind
:fout
echo/
echo   error in compilation
:eind
