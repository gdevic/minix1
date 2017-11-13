echo/
echo    Linking kernel
link/m mpx88+ < linklist >kernel.lst
dos2out -d kernel >> kernel.lst
echo/
echo Kernel done. Check the .LST-files for errors
pause 
echo on
for %%f in (*.lst) do type %%f

