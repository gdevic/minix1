if exist dos2out.obj goto link
cc1 dos2out >> dos2out.lst
if errorlevel 1 goto fout
cc2 dos2out >> dos2out.lst
if errorlevel 1 goto fout
cc3 dos2out >> dos2out.lst
if errorlevel 1 goto fout
cc4 dos2out >> dos2out.lst
if errorlevel 1 goto fout
:link
link/m dos2out,,,\lib\c86\C86s2s.lib >> dos2out.lst
