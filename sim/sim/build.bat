@echo off
rem ------------------------------------------------------------------------
rem  Build the Kobuki simulator from the firmware in ..\Src (needs a PC gcc,
rem  e.g. MSYS2 UCRT64: C:\msys64\ucrt64\bin on PATH).
rem
rem    build.bat        kobuki_sim.exe      - your uart.c / adc.c run as-is,
rem                                          register accesses are trapped
rem    build.bat shim   kobuki_sim_dbg.exe  - uart.c / adc.c swapped for
rem                                          direct calls, -O0, for gdb
rem
rem  Every .c under Src\ is compiled (except the ARM-only syscall.c /
rem  sysmem.c), and every folder under Src\ is an include path, so new
rem  files are picked up automatically. main() becomes firmware_main().
rem ------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "SIM=%~dp0"
set "SIM=%SIM:~0,-1%"
for %%I in ("%SIM%\..") do set "ROOT=%%~fI"
set "SRC=%ROOT%\Src"
set "DEV=%ROOT%\Inc\drivers\STM32G4xx\Include"
set "MODE=%~1"

where gcc >nul 2>nul
if errorlevel 1 (
    echo gcc not found. Install MSYS2 and add C:\msys64\ucrt64\bin to PATH.
    exit /b 1
)

if /i "%MODE%"=="shim" (
    set "OBJ=%SIM%\obj_dbg"
    set "EXE=%SIM%\kobuki_sim_dbg.exe"
    set "FWOPT=-O0 -g3"
    set "SIMOPT=-O0 -g3 -DSIM_SHIM"
    set "EXTRA="%SIM%\src\sim_shim.c""
) else (
    set "OBJ=%SIM%\obj"
    set "EXE=%SIM%\kobuki_sim.exe"
    set "FWOPT=-O1 -g"
    set "SIMOPT=-O2 -g"
    set "EXTRA="
)

rem ---- collect firmware sources and include folders -----------------------
set "FW="
set "COUNT=0"
for /r "%SRC%" %%F in (*.c) do (
    set "N=%%~nxF"
    set "SKIP="
    if /i "!N!"=="syscall.c" set SKIP=1
    if /i "!N!"=="sysmem.c"  set SKIP=1
    if /i "%MODE%"=="shim" (
        if /i "!N!"=="uart.c" set SKIP=1
        if /i "!N!"=="adc.c"  set SKIP=1
    )
    if not defined SKIP (
        set FW=!FW! "%%F"
        set /a COUNT+=1
    )
)
set "INC=-I"%SRC%""
for /d /r "%SRC%" %%D in (*) do set INC=!INC! -I"%%D"

if not exist "%OBJ%" mkdir "%OBJ%"
del /q "%OBJ%\*.o" 2>nul

echo Compiling %COUNT% firmware files from Src\ ...
pushd "%OBJ%"
gcc -std=gnu11 %FWOPT% -c -Dmain=firmware_main -I"%SIM%\include" %INC% -I"%DEV%" -Wno-int-to-pointer-cast %FW%
if errorlevel 1 (
    popd
    echo.
    echo Firmware failed to compile for the PC - see the errors above.
    exit /b 1
)
popd

set "OBJS="
for %%O in ("%OBJ%\*.o") do set OBJS=!OBJS! "%%~fO"

echo Linking simulator ...
gcc -std=gnu11 %SIMOPT% -Wall -Wno-int-to-pointer-cast -Wno-format-truncation -I"%SIM%\src" -I"%SIM%\include" -I"%DEV%" ^
    "%SIM%\src\sim_main.c" "%SIM%\src\sim_mmio.c" "%SIM%\src\sim_periph.c" "%SIM%\src\sim_robot.c" %EXTRA% ^
    %OBJS% -o "%EXE%" -lm
if errorlevel 1 (
    echo Link failed.
    exit /b 1
)
echo Built %EXE%
endlocal
