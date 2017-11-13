masm bootblok,,nul,nul > bootblok.lst
link bootblok,,nul,nul >>bootblok.lst
exe2bin bootblok.exe bootblok.bin >>bootblok.lst
del bootblok.exe >nul
del bootblok.obj >nul
if "%batch%" == "MINIX" make minix
