echo/
echo     Linking file system task
link/m ..\lib\head+ <linklist >fs.lst
dos2out -d fs >>fs.lst
echo/
echo FS done. Check the .lst-files for errors.
pause
echo on
for %%f in (*.lst) do type %%f
