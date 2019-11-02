@echo off
setlocal enabledelayedexpansion

REM -------------------------------------------------------------------------
REM READ CONFIGURATION
REM -------------------------------------------------------------------------

set "MSVC_PATH="
set "GIT_PATH="
set "PANDOC_PATH="
set "JAVA_HOME="

if not exist "%~dpn0.ini" (
	echo Configuration file "%~dpn0.ini" not found^^!
	pause
	goto:eof
)

set IN_SECTION=

for /f "usebackq delims== tokens=1,2" %%a in ("%~dpn0.ini") do (
	if not "%%~a"=="" (
		if not "%%~b"=="" (
			if "!IN_SECTION!"=="[PATHS]" (
				set "%%~a=%%~b"
			)
		) else (
			set "IN_SECTION=%%~a"
		)
	)
)

REM -------------------------------------------------------------------------
REM CHECK CONFIGURATION
REM -------------------------------------------------------------------------

if "%MSVC_PATH%"=="" (
	echo Configuration incomplete: Required variable MSVC_PATH is not set^^!
	pause
	goto:eof
)

if "%GIT_PATH%"=="" (
	echo Configuration incomplete: Required variable GIT_PATH is not set^^!
	pause
	goto:eof
)

if "%PANDOC_PATH%"=="" (
	echo Configuration incomplete: Required variable PANDOC_PATH is not set^^!
	pause
	goto:eof
)

if "%JAVA_HOME%"=="" (
	echo Configuration incomplete: Required variable JAVA_HOME is not set^^!
	pause
	goto:eof
)

REM -------------------------------------------------------------------------
REM CHECK PATHS
REM -------------------------------------------------------------------------

if not exist "%MSVC_PATH%\vcvarsall.bat" (
	echo MSVC not found. Please check MSVC_PATH and try again^^!
	pause
	goto:eof
)

if not exist "%GIT_PATH%\bin\git.exe" (
	echo Git not found. Please check GIT_PATH and try again^^!
	pause
	goto:eof
)

if not exist "%PANDOC_PATH%\pandoc.exe" (
	echo Pandoc not found. Please check PANDOC_PATH and try again^^!
	pause
	goto:eof
)


if not exist "%JAVA_HOME%\bin\java.exe" (
	echo Java not found. Please check JAVA_HOME and try again^^!
	pause
	goto:eof
)

REM -------------------------------------------------------------------------
REM SET UP ENVIRONMENT
REM -------------------------------------------------------------------------

set "VCINSTALLDIR="
set "PATH=%GIT_PATH%\bin;%GIT_PATH%\usr\bin;%GIT_PATH%\mingw64\bin;%PATH%"

call "%MSVC_PATH%\vcvarsall.bat" x86

if "%VCINSTALLDIR%"=="" (
	echo Error: Failed to set up MSVC environment^^!
	pause
	goto:eof
)

REM -------------------------------------------------------------------------
REM GET BUILD DATE
REM -------------------------------------------------------------------------

set "ISO_DATE="

for /F "tokens=1,2 delims=:" %%a in ('"%~dp0\etc\utils\date.exe" +ISODATE:%%Y-%%m-%%d') do (
	if "%%a"=="ISODATE" set "ISO_DATE=%%b"
)

if "%ISO_DATE%"=="" (
	echo Error: Failed to determine build date^^!
	pause
	goto:eof
)

REM -------------------------------------------------------------------------
REM REMOVE EXISTING BINARIES
REM -------------------------------------------------------------------------

if exist "%~dp0\bin\" (
	rmdir /S /Q "%~dp0\bin"
)

if exist "%~dp0\obj\" (
	rmdir /S /Q "%~dp0\obj"
)

REM -------------------------------------------------------------------------
REM BUILD BINARIES
REM -------------------------------------------------------------------------

for %%p in (Win32,x64) do (
	for %%t in (Clean,Rebuild) do (
		MSBuild.exe /property:Configuration=Release /property:Platform=%%p /target:%%t /verbosity:normal "%~dp0\workspace.sln"
		if not "!ERRORLEVEL!"=="0" (
			echo Error: Build has failed^^!
			pause
			goto:eof
		)
	)
)

"%~dp0\etc\utils\rchhdrrsr.exe" "%~dp0\bin\Win32\Release\*.exe"
"%~dp0\etc\utils\rchhdrrsr.exe" "%~dp0\bin\.\x64\Release\*.exe"

REM -------------------------------------------------------------------------
REM COPY DISTRIBUTION FILES
REM -------------------------------------------------------------------------

set "PACK_PATH=%TMP%\~%RANDOM%%RANDOM%.tmp"

mkdir "%PACK_PATH%"
mkdir "%PACK_PATH%\x64"

copy /Y "%~dp0\bin\Win32\Release\*.exe" "%PACK_PATH%"
copy /Y "%~dp0\bin\.\x64\Release\*.exe" "%PACK_PATH%\x64"

REM -------------------------------------------------------------------------
REM CREATE DOCUMENTS
REM -------------------------------------------------------------------------

"%PANDOC_PATH%\pandoc.exe" --from markdown_github+pandoc_title_block+header_attributes+implicit_figures+inline_notes --to html5 --toc -N --standalone -H "%~dp0\etc\css\github-pandoc.inc" "%~dp0\README.md" | "%JAVA_HOME%\bin\java.exe" -jar "%~dp0\..\Prerequisites\HTMLCompressor\bin\htmlcompressor-1.5.3.jar" --compress-css -o "%PACK_PATH%\README.html"
if not "%ERRORLEVEL%"=="0" (
	echo Error: Failed to generate documents^^!
	pause
	goto:eof
)

mkdir "%PACK_PATH%\img"
mkdir "%PACK_PATH%\img\msleep"

copy /Y "%~dp0\LICENSE.txt"      "%PACK_PATH%"
copy /Y "%~dp0\img\msleep\*.jpg" "%PACK_PATH%\img\msleep"

REM -------------------------------------------------------------------------
REM WRITE BUILD TAG
REM -------------------------------------------------------------------------

echo msleep for Win32>                                             "%PACK_PATH%\BUILD_TAG.txt"
echo Created by LoRd_MuldeR ^<mulder2@gmx.de^>.>>                  "%PACK_PATH%\BUILD_TAG.txt"
echo.>>                                                            "%PACK_PATH%\BUILD_TAG.txt"
echo This work is licensed under the CC0 1.0 Universal License.>>  "%PACK_PATH%\BUILD_TAG.txt"
echo To view a copy of the license, visit:>>                       "%PACK_PATH%\BUILD_TAG.txt"
echo https://creativecommons.org/publicdomain/zero/1.0/legalcode>> "%PACK_PATH%\BUILD_TAG.txt"

REM -------------------------------------------------------------------------
REM GENERATE OUTNAME
REM -------------------------------------------------------------------------

if not exist "%~dp0\out\" (
	mkdir "%~dp0\out"
)

set "OUTNAME=MSleep.%ISO_DATE%"

:validate_outname
if exist "%~dp0\out\%OUTNAME%.zip"        goto:next_outname
if exist "%~dp0\out\%OUTNAME%.source.tgz" goto:next_outname
goto:outname_generated

:next_outname
set "OUTNAME=%OUTNAME%.new"
goto:validate_outname

:outname_generated

REM -------------------------------------------------------------------------
REM CREATE PACKAGE
REM -------------------------------------------------------------------------

pushd "%PACK_PATH%"
if not "%ERRORLEVEL%"=="0" (
	echo Error: Failed to change directory to PACK_PATH^^!
	pause
	goto:eof
)

"%~dp0\etc\utils\zip.exe" -r -9 -z "%~dp0\out\%OUTNAME%.zip" "*.*" < "%PACK_PATH%\BUILD_TAG.txt"
if not "%ERRORLEVEL%"=="0" (
	echo Error: Failed to create ZIP package^^!
	pause
	goto:eof
)

popd

"%GIT_PATH%\bin\git.exe" archive --verbose --format=tar.gz --output "%~dp0\out\%OUTNAME%.source.tgz" HEAD
if not "%ERRORLEVEL%"=="0" (
	echo Error: Failed to export source codes^^!
	pause
	goto:eof
)

attrib +r "%~dp0\out\%OUTNAME%.zip"
attrib +r "%~dp0\out\%OUTNAME%.source.tgz"

REM -------------------------------------------------------------------------
REM FINAL CLEAN-UP
REM -------------------------------------------------------------------------

rmdir /Q /S "%PACK_PATH%"

REM -------------------------------------------------------------------------
REM COMPLETED
REM -------------------------------------------------------------------------

echo.
echo COMPLETED
echo.

pause
