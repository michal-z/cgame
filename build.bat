@ECHO off
SETLOCAL enableextensions enabledelayedexpansion

SET NAME="cgame"

SET CONFIG=D
SET C_FLAGS=/MP /std:c17 /experimental:c11atomics /GR- /nologo /Gm- /WX /Wall ^
/wd4191 /wd4820 /wd4255 /wd5045 /wd4505 /I"src" /I"src\pch" /I"src\deps" ^
/wd4710 /wd4711 /I"src\deps\d3d12"

IF %CONFIG%==D SET C_FLAGS=%C_FLAGS% /GS /Zi /Od /D"_DEBUG" /MTd /RTCs
IF %CONFIG%==R SET C_FLAGS=%C_FLAGS% /O2 /Gy /MT /D"NDEBUG" /Oi /Ot /GS-

SET LINK_FLAGS=/INCREMENTAL:NO /NOLOGO
IF %CONFIG%==D SET LINK_FLAGS=%LINK_FLAGS% /DEBUG:FULL
IF %CONFIG%==R SET LINK_FLAGS=%LINK_FLAGS%

SET DXC=bin\dxc.exe
SET HLSL_OUT_DIR=src\shaders\cso
SET HLSL_SM=6_6
SET HLSL_FLAGS=/WX /Ges /HV 2021 /nologo
if %CONFIG%==D SET HLSL_FLAGS=%HLSL_FLAGS% /Od /Zi /Qembed_debug
if %CONFIG%==R SET HLSL_FLAGS=%HLSL_FLAGS% /O3

IF "%1"=="clean" (
 IF EXIST *.pch DEL *.pch
 IF EXIST *.obj DEL *.obj
 IF EXIST *.lib DEL *.lib
 IF EXIST *.pdb DEL *.pdb
 IF EXIST *.exe DEL *.exe
)

SET COMPILE_HLSL=0
IF "%1"=="hlsl" SET COMPILE_HLSL=1
IF "%1"=="clean" SET COMPILE_HLSL=1

IF %COMPILE_HLSL%==1 (
IF EXIST %HLSL_OUT_DIR%\*.h DEL %HLSL_OUT_DIR%\*.h

%DXC% %HLSL_FLAGS% /T vs_%HLSL_SM% /E s00_vs /D_S00 src\shaders\shaders.c ^
/Fh %HLSL_OUT_DIR%\s00_vs.h

%DXC% %HLSL_FLAGS% /T ps_%HLSL_SM% /E s00_ps /D_S00 src\shaders\shaders.c ^
/Fh %HLSL_OUT_DIR%\s00_ps.h
)

IF NOT EXIST pch.lib (
 cl %C_FLAGS% /Fo"pch.lib" /Fp"pch.pch" /c /Yc"pch.h" "src\pch\pch.c"
) & if ERRORLEVEL 1 GOTO error

IF NOT "%1"=="hlsl" (
IF EXIST %NAME%.exe DEL %NAME%.exe
cl %C_FLAGS% /Fp"pch.pch" /Yu"pch.h" "src\*.c" /link %LINK_FLAGS% ^
/OUT:%NAME%.exe d3d12.lib dxgi.lib user32.lib pch.lib
IF "%1"=="run" IF EXIST %NAME%.exe %NAME%.exe
)

GOTO end

:error

ECHO ---------------
ECHO ERROR
ECHO ---------------

:end
IF EXIST *.obj DEL *.obj
IF EXIST %NAME%.lib DEL %NAME%.lib
IF EXIST *.exp DEL *.exp
