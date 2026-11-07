# Monado V6 para compilar

Fuentes para Windows x64 con el hotfix del 5 de noviembre de 2026.
Incluye las correcciones de afinidad, DLSS, mouse y relación de aspecto.

## Building

1. Extrae todo el ZIP en una carpeta. Instala Git for Windows si no lo tienes.
2. Ejecuta `Build Monado.bat`. La primera preparación instala Python y las
   dependencias de MSYS2 y Zig. Necesita conexión a Internet y espacio libre.
3. Selecciona la carpeta que contiene `exefs` y `romfs` de tu propia copia de
   Xenoblade Chronicles: Definitive Edition 2.0.0. También puedes preparar esos
   archivos en `targets/exefs` y `targets/romfs` antes de ejecutar el script.
4. Al terminar, ejecuta `Play Monado.bat`. El primer arranque prepara los shaders.
   Si falla la compilación, revisa `build/build_all.log`.
5. Para DLSS en una RTX compatible, ejecuta desde la carpeta del proyecto
   `.venv\Scripts\python.exe tools\install_dlss.py` después de la compilación.
   El instalador descarga y verifica el runtime oficial. FSR 3.1 usa el DLL
   opcional oficial de AMD. La ruta `scripts/copy_upscaler_dlls.sh` indica su
   ubicación en el SDK. FSR 1 funciona con las dependencias normales.

El paquete incluye la semilla de variantes de renderizado que usa la
precompilación. Los archivos del juego los aporta cada usuario. El manifiesto
enumera los archivos de la entrega y su SHA-256. Conserva las licencias y NOTICE.
