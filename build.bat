@echo off
setlocal enableextensions enabledelayedexpansion

set CONFIG=D
set C_FLAGS=/std:c17 /I. /experimental:c11atomics /GR- /nologo /Gm- /WX /Wall /wd4191 /wd4820 /wd4255 /wd5045

if %CONFIG%==D set C_FLAGS=%C_FLAGS% /GS /Zi /Od /D"_DEBUG" /MTd /RTCs
if %CONFIG%==R set C_FLAGS=%C_FLAGS% /O2 /Gy /MT /D"NDEBUG" /Oi /Ot /GS-

set LINK_FLAGS=/INCREMENTAL:NO /NOLOGO
if %CONFIG%==D set LINK_FLAGS=%LINK_FLAGS% /DEBUG:FULL
if %CONFIG%==R set LINK_FLAGS=%LINK_FLAGS%

if exist *.obj del *.obj
if exist *.pdb del *.pdb
if exist *.exe del *.exe

cl %C_FLAGS% cgame.c /link %LINK_FLAGS%
if "%1"=="run" if exist cgame.exe cgame.exe

if exist *.obj del *.obj
if exist *.lib del *.lib
if exist *.exp del *.exp
if exist vc140.pdb del vc140.pdb
