@ECHO off
SETLOCAL enableextensions enabledelayedexpansion

SET NAME="cgame"

SET CONFIG=D
SET C_FLAGS=/std:c17 /I. /experimental:c11atomics /GR- /nologo /Gm-^
 /WX /Wall /wd4191 /wd4820 /wd4255 /wd5045^
 /I.^
 /I"deps"^
 /I"deps/d3d12"

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

IF NOT "%1"=="hlsl" (
 IF EXIST %NAME%.exe DEL %NAME%.exe
 cl %C_FLAGS% %NAME%.c /link %LINK_FLAGS%
 IF "%1"=="run" IF EXIST %NAME%.exe %NAME%.exe
)

IF EXIST *.obj DEL *.obj
IF EXIST %NAME%.lib DEL %NAME%.lib
IF EXIST *.exp DEL *.exp
IF EXIST vc140.pdb DEL vc140.pdb
