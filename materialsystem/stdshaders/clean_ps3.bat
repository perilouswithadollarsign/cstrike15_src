if exist fxctmp9_ps3_tmp rd /s /q fxctmp9_ps3_tmp
if exist vshtmp9_ps3_tmp rd /s /q vshtmp9_ps3_tmp
if exist pshtmp9_ps3_tmp rd /s /q pshtmp9_ps3_tmp

if exist shaders del /s shaders\*.ps3.*

p4 edit ..\..\..\game\platform\shaders\fxc\*.ps3.*
del ..\..\..\game\platform\shaders\fxc\*.ps3.*

