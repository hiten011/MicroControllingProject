@echo off
rem Kobuki simulator build script - response-file edition.
rem Avoids Windows cmd.exe "The input line is too long" errors.
setlocal EnableExtensions EnableDelayedExpansion

set "SIM=%~dp0"
set "SIM=%SIM:~0,-1%"
for %%I in ("%SIM%\..") do set "ROOT=%%~fI"
set "SRC=%ROOT%\Src"
set "MODE=%~1"

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc not found. Add C:\msys64\ucrt64\bin to PATH.
    exit /b 1
)
if not exist "%SRC%" (
    echo ERROR: Source folder not found: "%SRC%"
    exit /b 1
)

if /i "%MODE%"=="shim" (
    set "OBJ=%SIM%\obj_dbg"
    set "EXE=%SIM%\kobuki_sim_dbg.exe"
    set "FWOPT=-O0 -g3"
    set "SIMOPT=-O0 -g3 -DSIM_SHIM"
    set "EXTRA=%SIM%\src\sim_shim.c"
) else (
    set "OBJ=%SIM%\obj"
    set "EXE=%SIM%\kobuki_sim.exe"
    set "FWOPT=-O1 -g"
    set "SIMOPT=-O2 -g"
    set "EXTRA="
)

if not exist "%OBJ%" mkdir "%OBJ%"
if errorlevel 1 exit /b 1
del /q "%OBJ%\*.o" 2>nul

rem GCC @response files can hold arbitrarily many include directories and sources.
rem Use forward slashes in response files so GCC's argument parser does
rem not interpret Windows backslashes as escape characters.
set "INCS=%OBJ%\includes.rsp"
set "FWS=%OBJ%\firmware.rsp"
set "OBS=%OBJ%\objects.rsp"
set "P=%SIM%\include"
set "P=!P:\=/!"
> "%INCS%" echo -I"!P!"
set "P=%SRC%"
set "P=!P:\=/!"
>> "%INCS%" echo -I"!P!"
if exist "%ROOT%\Inc" (
    set "P=%ROOT%\Inc"
    set "P=!P:\=/!"
    >> "%INCS%" echo -I"!P!"
)
for /d /r "%SRC%" %%D in (*) do (
    set "P=%%~fD"
    set "P=!P:\=/!"
    >> "%INCS%" echo -I"!P!"
)
if exist "%ROOT%\Inc" (
    for /d /r "%ROOT%\Inc" %%D in (*) do (
        set "P=%%~fD"
        set "P=!P:\=/!"
        >> "%INCS%" echo -I"!P!"
    )
)
if exist "%ROOT%\Drivers" (
    for /d /r "%ROOT%\Drivers" %%D in (*) do (
        set "P=%%~fD"
        set "P=!P:\=/!"
        >> "%INCS%" echo -I"!P!"
    )
)

> "%FWS%" type nul
set /a COUNT=0
for /r "%SRC%" %%F in (*.c) do (
    set "SKIP="
    if /i "%%~nxF"=="syscall.c" set "SKIP=1"
    if /i "%%~nxF"=="sysmem.c" set "SKIP=1"
    if /i "%MODE%"=="shim" (
        if /i "%%~nxF"=="uart.c" set "SKIP=1"
        if /i "%%~nxF"=="adc.c" set "SKIP=1"
    )
    if not defined SKIP (
        set "P=%%~fF"
        set "P=!P:\=/!"
        >> "%FWS%" echo "!P!"
        set /a COUNT+=1 >nul
    )
)
if !COUNT! EQU 0 (
    echo ERROR: no firmware .c files found in "%SRC%".
    exit /b 1
)

rem Compiling within OBJ keeps .o files out of the firmware Src folder.
echo Compiling !COUNT! firmware files from Src\ ...
pushd "%OBJ%"
gcc -std=gnu11 %FWOPT% -DSTM32G474xx -c -Dmain=firmware_main -Wno-int-to-pointer-cast -include "%SIM%\include\sim_nvic.h" @"%INCS%" @"%FWS%"
if errorlevel 1 (
    popd
    echo.
    echo Firmware failed to compile for the PC - see the errors above.
    exit /b 1
)
popd

> "%OBS%" type nul
for %%O in ("%OBJ%\*.o") do (
    set "P=%%~fO"
    set "P=!P:\=/!"
    >> "%OBS%" echo "!P!"
)

rem Linking also uses a response file to avoid command-line limits.
echo Linking simulator ...
if /i "%MODE%"=="shim" (
    gcc -std=gnu11 %SIMOPT% -DSTM32G474xx -Wall -Wno-int-to-pointer-cast -Wno-format-truncation @"%INCS%" -I"%SIM%\src" "%SIM%\src\sim_main.c" "%SIM%\src\sim_mmio.c" "%SIM%\src\sim_periph.c" "%SIM%\src\sim_robot.c" "%EXTRA%" @"%OBS%" -o "%EXE%" -lm
) else (
    gcc -std=gnu11 %SIMOPT% -DSTM32G474xx -Wall -Wno-int-to-pointer-cast -Wno-format-truncation @"%INCS%" -I"%SIM%\src" "%SIM%\src\sim_main.c" "%SIM%\src\sim_mmio.c" "%SIM%\src\sim_periph.c" "%SIM%\src\sim_robot.c" @"%OBS%" -o "%EXE%" -lm
)
if errorlevel 1 (
    echo Link failed.
    exit /b 1
)
echo Built "%EXE%"
endlocal
