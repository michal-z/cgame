@ECHO off
SETLOCAL enableextensions enabledelayedexpansion

SET NAME="cgame"

SET CONFIG=D
SET C_FLAGS=/MP /std:c17 /experimental:c11atomics /GR- /nologo /Gm- /WX /Wall /wd4191 /wd4820 /wd4255 /wd5045 /wd4505 /I"src" /I"src\pch" /I"src\deps" /I"src\deps\d3d12"

IF %CONFIG%==D set C_FLAGS=%C_FLAGS% /GS /Zi /Od /D"_DEBUG" /MTd /RTCs
IF %CONFIG%==R set C_FLAGS=%C_FLAGS% /O2 /Gy /MT /D"NDEBUG" /Oi /Ot /GS-

SET LINK_FLAGS=/INCREMENTAL:NO /NOLOGO
IF %CONFIG%==D set LINK_FLAGS=%LINK_FLAGS% /DEBUG:FULL
IF %CONFIG%==R set LINK_FLAGS=%LINK_FLAGS%

IF "%1"=="clean" (
 IF exist *.pch DEL *.pch
 IF EXIST *.obj DEL *.obj
 IF EXIST *.lib DEL *.lib
 IF EXIST *.pdb DEL *.pdb
 IF EXIST *.exe DEL *.exe
)

IF NOT EXIST pch.lib (
 cl %C_FLAGS% /Fo"pch.lib" /Fp"pch.pch" /c /Yc"pch.h" "src\pch\pch.c"
) & if ERRORLEVEL 1 GOTO error

IF NOT "%1"=="hlsl" (
 IF EXIST %NAME%.exe DEL %NAME%.exe
 cl %C_FLAGS% /Fp"pch.pch" /Yu"pch.h" "src\*.c" /link %LINK_FLAGS% /OUT:%NAME%.exe d3d12.lib dxgi.lib user32.lib pch.lib
 IF "%1"=="run" IF EXIST %NAME%.exe %NAME%.exe
)

GOTO end

:error
echo ERROR ---------------------------------------------------------------------

:end
IF EXIST *.obj DEL *.obj
IF EXIST %NAME%.lib DEL %NAME%.lib
IF EXIST *.exp DEL *.exp
