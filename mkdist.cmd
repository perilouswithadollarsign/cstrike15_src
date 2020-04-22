if exist c:\rtest rd /s /q c:\rtest
md c:\rtest
md c:\rtest\bin
md c:\rtest\source2
xcopy /s ..\..\..\game\source2 c:\rtest\source2
xcopy ..\..\..\game\bin\rendersystemtest.exe c:\rtest

copy runbenchmarks.cmd c:\rtest
copy macrun.sh c:\rtest
copy readme.txt c:\rtest

for %%f in ( inputsystem worldrenderer filesystem_stdio ) do copy ..\..\..\game\bin\%%f.dll c:\rtest\bin

xcopy ..\..\..\game\bin\rendersystem*.dll c:\rtest\bin
xcopy ..\..\..\game\bin\*.dylib c:\rtest\bin

md c:\rtest\bin\rendersystemtest.app
xcopy /s ..\..\..\game\bin\rendersystemtest.app\*.* c:\rtest\bin\rendersystemtest.app

for %%f in ( tier0 vstdlib) do copy ..\..\..\game\bin\%%f.dll c:\rtest\bin

..\..\pkzip25 -add -dir=relative c:\rtest\rtest.zip c:\rtest\*.*

