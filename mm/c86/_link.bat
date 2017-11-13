echo    Linking memory manager
link/m ..\lib\head+ <linklist >link.lst
dos2out mm >dos2out.lst >dos2out.lst
echo/
echo MM done. Check the .lst-files for errors.
pause
echo on
for %%f in (*.lst) do type %%f
