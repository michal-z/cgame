@ECHO off
SETLOCAL enableextensions enabledelayedexpansion

SET "NAME=cgame"

:: (D)ebug, (R)elease
SET CONFIG=D
SET CC=cl.exe
SET C_FLAGS=/std:c17 /experimental:c11atomics /GR- /nologo /Gm- /WX /Wall ^
  /fp:precise ^
  /wd4820 /wd4255 /wd5045 /wd4710 /wd4711 /wd4505 ^
  /I"src" ^
  /I"src\pch" ^
  /I"src\deps" ^
  /I"src\deps\d3d12" ^
  /I"src\deps\nuklear" ^
  /I"src\deps\box2d\include"

IF %CONFIG%==D SET C_FLAGS=%C_FLAGS% /GS /Zi /Od /D"_DEBUG" /MTd /RTCs
IF %CONFIG%==R SET C_FLAGS=%C_FLAGS% /O2 /Gy /MT /D"NDEBUG" /Oi /Ot /GS-

SET LINK_FLAGS=/INCREMENTAL:NO /NOLOGO /NOIMPLIB /NOEXP
IF %CONFIG%==D SET LINK_FLAGS=%LINK_FLAGS% /DEBUG:FULL
IF %CONFIG%==R SET LINK_FLAGS=%LINK_FLAGS%

set LIB_FLAGS=/NOLOGO
if %CONFIG%==D set LIB_FLAGS=%LIB_FLAGS%
if %CONFIG%==R set LIB_FLAGS=%LIB_FLAGS%

SET DXC=bin\dxc.exe
SET HLSL_OUT_DIR=src\shaders\cso
SET HLSL_SM=6_6
SET HLSL_FLAGS=/WX /Ges /HV 2021 /nologo
IF %CONFIG%==D SET HLSL_FLAGS=%HLSL_FLAGS% /Od /Zi /Qembed_debug
IF %CONFIG%==R SET HLSL_FLAGS=%HLSL_FLAGS% /O3

IF "%1"=="clean" (
  IF EXIST "*.pch" DEL "*.pch"
  IF EXIST "*.obj" DEL "*.obj"
  IF EXIST "*.lib" DEL "*.lib"
  IF EXIST "*.pdb" DEL "*.pdb"
  IF EXIST "*.exe" DEL "*.exe"
)

::
:: Shaders
::
:: Compiles all shaders from %FIRST_SHADER% to %LAST_SHADER%.
:: Shader is selected by defining a preprocessor symbol: _s00, _s01, ...
:: All shaders are kept in a single source file: "src\shaders\shaders.c".
::
SET FIRST_SHADER=0
SET LAST_SHADER=1

SET COMPILE_HLSL=1
IF "%1"=="hlsl" SET COMPILE_HLSL=1
IF "%1"=="clean" SET COMPILE_HLSL=1

IF %COMPILE_HLSL%==1 (
  IF EXIST "%HLSL_OUT_DIR%\*.h" DEL "%HLSL_OUT_DIR%\*.h"

  FOR /L %%i IN (%FIRST_SHADER%, 1, %LAST_SHADER%) DO (
    SET "SH=000000%%i"
    SET SH=s!SH:~-2!

    %DXC% %HLSL_FLAGS% /T vs_%HLSL_SM% /E !SH!_vs /D_!SH! src\shaders\shaders.c ^
      /Fh %HLSL_OUT_DIR%\!SH!_vs.h

    %DXC% %HLSL_FLAGS% /T ps_%HLSL_SM% /E !SH!_ps /D_!SH! src\shaders\shaders.c ^
      /Fh %HLSL_OUT_DIR%\!SH!_ps.h
  )
)

::
:: Nuklear
::
IF NOT EXIST nuklear.lib (
  %CC% %C_FLAGS% /Fd:"nuklear.pdb" /c "src\deps\nuklear\*.c" ^
    /wd4127 /wd4116 /wd4061 /wd4701 ^
    /D_CRT_SECURE_NO_WARNINGS

  lib %LIB_FLAGS% "*.obj" /OUT:"nuklear.lib"

  IF EXIST "*.obj" DEL "*.obj"
) & if ERRORLEVEL 1 GOTO error

::
:: Box2D
::
IF NOT EXIST box2d.lib (
  %CC% %C_FLAGS% /Fd:"box2d.pdb" /c "src\deps\box2d\src\*.c" ^
  /W3 /wd4242 /wd4244 /wd4018

  lib %LIB_FLAGS% "*.obj" /OUT:"box2d.lib"

  IF EXIST "*.obj" DEL "*.obj"
) & if ERRORLEVEL 1 GOTO error

::
:: Precompiled header
::
IF NOT EXIST pch.lib (
  %CC% %C_FLAGS% /Fo:"pch.lib" /Fp:"pch.pch" /Fd:"pch.pdb" /Yc"pch.h" ^
    /c "src\pch\pch.c"
) & if ERRORLEVEL 1 GOTO error

::
:: Game
::
IF NOT "%1"=="hlsl" (
  IF EXIST "%NAME%.exe" DEL "%NAME%.exe"

  %CC% %C_FLAGS% /MP /Fp:"pch.pch" /Fd:"pch.pdb" /Fe:"%NAME%.exe" ^
    /Yu"pch.h" "src\*.c" ^
    /link %LINK_FLAGS% d3d12.lib dxgi.lib user32.lib ^
    pch.lib nuklear.lib box2d.lib

  IF EXIST "*.obj" DEL "*.obj"

  IF "%1"=="run" IF EXIST "%NAME%.exe" "%NAME%.exe"
)

GOTO end

:error

ECHO ---------------
ECHO ERROR
ECHO ---------------

:end
