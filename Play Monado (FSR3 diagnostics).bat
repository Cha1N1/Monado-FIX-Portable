@echo off
rem =====================================================================
rem  FSR 3 diagnostics (temporal upscaler): bisection of the whole-image
rem  trembling / floor disappearing reported on GitHub (#14 #15 #12 #16).
rem  Each option runs the game with some built-in toggles OFF so the log
rem  tells WHICH component is at fault. Play 2-3 minutes each (walk, run,
rem  turn the camera fast), close the game, and the log is saved to
rem  build\fsr3_diag_<option>.log
rem
rem  Toggles used (runtime/nvn/vulkan/temporal.c + exec.c):
rem   XCDE_TMP_LOG            one line a second: camera, jitter, history resets
rem   XCDE_TMP_CHECK          FSR 3 validates its inputs, reports in the log
rem   XCDE_TMP_GAME_VEL       the game's own velocity buffer (T215/T228)
rem   XCDE_TMP_NO_REACTIVE    the reactive mask handed to FSR (img[5])
rem   XCDE_TMP_NO_HUD_MASK    the HUD history mask
rem   XCDE_TMP_LOD_BIAS       the negative texture LOD bias added for FSR
rem   XCDE_TMP_VEL_PRECISE    T228 precise camera motion mode
rem   XCDE_TMP_JIT_NODEPTH    jitter on draws without a depth test (T260)
rem   XCDE_TMP_OFF            FSR 3 completely off (reference run)
rem   XCDE_OCCLUSION          the occlusion-query culling (T139); 0 = always visible
rem =====================================================================

cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist "build\xcde.exe" (
    echo No encuentro build\xcde.exe - ejecuta primero "Build Monado.bat" o usa la version portable.
    pause
    exit /b 1
)

:menu
echo.
echo   1. Base                     (FSR 3 validacion + log, todo encendido [FSR3 tal como esta])
echo   2. Sin velocidad del juego  (velocidad propia del juego OFF [sospechoso: piso/vectores])
echo   3. Sin mascara reactiva     (reactive mask OFF [sospechoso: piso desaparece])
echo   4. Jitter solo depth-test   (regla vieja de T260 [sospechoso: temblor de todo])
echo   5. Sin sesgo de texturas    (LOD bias OFF [sospechoso: chispeo])
echo   6. Conservador              (2 + 3 + 4 + 5: todo lo anterior OFF de golpe)
echo   7. FSR 3 OFF                (referencia: sin el upscaler - TEST CLAVE: si el piso sigue desapareciendo aqui, NO es culpa del FSR)
echo   9. Sin oclusion             (FSR 3 OFF + la culling por oclusion siempre "visible" [sospechoso iGPU: piso invisible])
echo   10. FSR 3 ON + sin oclusion (si isolatea apuntando a lo del piso)
echo   11. Sin jitter              (FSR 3 ON, el sub-pixel jitter en 0 en TODO [sospechoso: parpadeo de toda la imagen; si aqui se va, es la convencion del jitter])
echo   12. Jitter Y invertido      (FSR 3 ON con XCDE_TMP_JIT_SIGN=1 [la convencion de AMD: TEST PRINCIPAL del parpadeo])
echo   13. Jitter X invertido      (XCDE_TMP_JIT_SIGN=2)
echo   14. Jitter X e Y invertidos (XCDE_TMP_JIT_SIGN=3)
echo   8. Salir
echo.
set CH=
set /p CH=Opcion y ENTER (1-11, 8=salir):
if not defined CH goto menu
for /f "delims=0123456789" %%x in ("%CH%") do goto menu
if %CH% GTR 14 goto menu
if %CH% EQU 8 exit /b 0
if %CH% EQU 0 goto menu

rem ---- clean leftovers of the previous option
set XCDE_TMP_CHECK=
set XCDE_TMP_LOG=
set XCDE_TMP_GAME_VEL=
set XCDE_TMP_NO_REACTIVE=
set XCDE_TMP_NO_HUD_MASK=
set XCDE_TMP_LOD_BIAS=
set XCDE_TMP_VEL_PRECISE=
set XCDE_TMP_JIT_NODEPTH=
set XCDE_TMP_OLD_RESET=
set XCDE_TMP_OFF=
set XCDE_OCCLUSION=
set XCDE_TMP_NOJITTER=
set XCDE_TMP_JIT_SIGN=

set OUT=build\fsr3_diag_%CH%.log

rem ---- base toggles every FSR 3 run gets (efeito: validacion FSR + log por segundo)
set XCDE_TMP_CHECK=1
set XCDE_TMP_LOG=1
set XCDE_TMP_OFF=1
for %%C in (2,3,4,5,6,10,11,12,13,14) do if %CH%==%%C set XCDE_TMP_OFF=
if %CH%==2 set XCDE_TMP_GAME_VEL=0
if %CH%==2 set XCDE_TMP_VEL_PRECISE=0
if %CH%==3 set XCDE_TMP_NO_REACTIVE=1
if %CH%==4 set XCDE_TMP_JIT_NODEPTH=0
if %CH%==5 set XCDE_TMP_LOD_BIAS=0
if %CH%==6 (
    set XCDE_TMP_GAME_VEL=0
    set XCDE_TMP_VEL_PRECISE=0
    set XCDE_TMP_NO_REACTIVE=1
    set XCDE_TMP_JIT_NODEPTH=0
    set XCDE_TMP_LOD_BIAS=0
)
if %CH% EQU 9 set XCDE_OCCLUSION=0
if %CH% EQU 10 set XCDE_OCCLUSION=0
if %CH% EQU 11 set XCDE_TMP_NOJITTER=1
if %CH% EQU 12 set XCDE_TMP_JIT_SIGN=1
if %CH% EQU 13 set XCDE_TMP_JIT_SIGN=2
if %CH% EQU 14 set XCDE_TMP_JIT_SIGN=3

echo.
echo   Opcion %CH% - variables activas:
set XCDE_
echo   Juega 2-3 minutos EN EL CAMPO: camina, corre, gira la camara, y hazte a la zona
echo   donde se desaparece el piso. Consola negra con el log en vivo.
echo   Al cerrar el juego queda en %OUT%
echo.
"%CD%\build\xcde.exe" targets > "%OUT%" 2>&1

echo   Listo: %OUT%
echo   Mandame ese fichero (o el contenido de las lineas [tmp] y [occl]) y que observaste.
pause
