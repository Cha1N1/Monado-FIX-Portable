# Monado-FIX-Portable
---

## 🇪🇸 Español

### 📌 ¿Qué es Monado? (Proyecto Original - Resumen)

**Monado** es una **recompilación estática** de *Xenoblade Chronicles: Definitive Edition* (Nintendo Switch, 2020) diseñada para convertirse en una aplicación nativa de Windows x86-64 con un renderizador Vulkan.

* **No es un emulador:** El código ARM64 original del juego se traduce con antelación (Ahead-of-Time) a C y se compila directamente para tu PC. El sistema operativo de la consola y la API gráfica (NVN) se reimplementan sobre Windows y Vulkan.
* **Requisitos:** Requiere una copia propia y legal del juego (versión 2.0.0, con el juego base y su actualización aplicados) extraída en carpetas `exefs` y `romfs`. No se incluyen archivos del juego ni formatos empaquetados (`.nsp`, `.xci`, `.nca`).

### 🚀 Mejoras y Características de Monado-FIX-Portable

Este repositorio (`Monado-FIX-Portable`) toma como base el fork de `vinitaison/Monado` (y el proyecto original de `TheMonadoGuy`) para ofrecer una experiencia mucho más accesible, optimizada y fácil de usar en Windows:

1. **Versión Portable y Opciones de Compilación Facilitada:**
* Se incluyen scripts y distribuciones diseñadas para simplificar el proceso, permitiendo configuraciones más directas o versiones portables listas para usar sin lidiar con cadenas de dependencias complejas.


2. **Correcciones y Mejoras de Rendimiento (Fixes):**
* Parches y soluciones orientados a mitigar problemas de estabilidad, bajones de FPS y crashes (enfocados en la gestión de shaders pesados y compatibilidad mejorada con hardware AMD e Intel).


3. **Scripts Automatizados y Diagnóstico:**
* Archivos batch dedicados para facilitar la ejecución y análisis (`Play Monado.bat`, `Play Monado (performance metrics).bat`, `Play Monado (stutter diagnostics).bat`, `Crear Monado Portable.bat`, etc.).
* Gestión optimizada de precarga de shaders para minimizar el *stuttering*.



### ⚙️ Instrucciones Rápidas de Uso

1. Coloca o vincula tus carpetas `exefs` y `romfs` extraídas (versión 2.0.0) dentro de la estructura de carpetas en `/targets` (o `/targets/Exefs` y `/targets/Romfs`).
2. Si utilizas una versión portable o precompilada, solo asegúrate de ubicar tus archivos de juego correspondientes e inicia ejecutando `Play Monado.bat`.
3. Si prefieres compilar desde cero, ejecuta `Build Monado.bat` para instalar de forma automatizada las dependencias necesarias y compilar el proyecto.

### 🛠️ Problemas Corregidos (Fixes y Changelog)

*Consulta todas las actualizaciones detalladas en la sección de [Releases](https://github.com/Ivanprestef2/Monado-FIX-Portable/releases).*

* **Optimización de shaders pesados:** Mitigación progresiva de tirones de rendimiento y crashes críticos vinculados a ciertos shaders problemáticos.
* **Compatibilidad ampliada:** Ajustes orientados a mejorar la estabilidad general en configuraciones de hardware variadas (incluyendo gráficas integradas y componentes AMD/Intel).
* **Versión Portable sin dependencias obligatorias:** Corrección en la estructuración de directorios y scripts `.bat` para permitir una ejecución directa sin requerir instalaciones de desarrollo adicionales.
* **Mejoras en el diagnóstico de errores:** Inclusión de scripts dedicados al registro de métricas de rendimiento e identificación de bloqueos por *stuttering*.

---

## 🇬🇧 English

### 📌 What is Monado? (Original Project Summary)

**Monado** is a **static recompilation** of *Xenoblade Chronicles: Definitive Edition* (Nintendo Switch, 2020) into a native Windows x86-64 application powered by a Vulkan renderer.

* **Not an emulator:** The game's native ARM64 code is translated ahead-of-time into C and compiled specifically for your PC. The console's operating system and graphics API (NVN) are reimplemented on top of Windows and Vulkan.
* **Requirements:** Requires your own legally obtained copy of the game (version 2.0.0, base game with the update applied) extracted into plain `exefs` and `romfs` folders. No game files, `.nsp`, `.xci`, or `.nca` packages are included.

### 🚀 Improvements & Features in Monado-FIX-Portable

This repository (`Monado-FIX-Portable`) builds upon `vinitaison/Monado`'s fork (originally pioneered by `TheMonadoGuy`) to provide a much more accessible, streamlined, and optimized experience on Windows:

1. **Portable & Streamlined Setup Options:**
* Includes automated setup scripts or portable-ready configurations, allowing you to run the game with minimal hassle and zero complex dependency chains.


2. **Bug Fixes & Performance Improvements:**
* Specialized patches and fixes targeting stability issues, frame drops, and crash prevention (with specific refinements for AMD/Intel GPU compatibility and heavy shader handling).


3. **Automated Batch Scripts & Diagnostics:**
* Dedicated `.bat` scripts for seamless launching, performance metrics, and stutter diagnostics (`Play Monado.bat`, `Play Monado (stutter diagnostics).bat`, etc.).
* Optimized precompiled shader management to reduce hitching during gameplay.



### ⚙️ Quick Setup Instructions

1. Place your legally dumped game `exefs` and `romfs` folders (version 2.0.0) into the `/targets` directory (`targets/Exefs` and `targets/Romfs`).
2. If using a portable build, simply drop your game files in place and launch `Play Monado.bat`.
3. If building from source, run `Build Monado.bat` to automatically download dependencies and build the application.

### 🛠️ Fixed Issues & Changelog

*Check out all detailed update logs directly in the [Releases](https://github.com/Ivanprestef2/Monado-FIX-Portable/releases) section.*

* **Heavy Shader Optimization:** Progressive mitigation of performance hitches and critical crashes linked to specific heavy shaders.
* **Expanded Hardware Compatibility:** Adjustments aimed at improving overall stability across diverse hardware configurations (including integrated graphics and AMD/Intel GPUs).
* **Standalone Portable Build:** Directory structure and `.bat` script refinements allowing straightforward execution without needing extra development toolchains.
* **Enhanced Diagnostics:** Integration of dedicated scripts for tracking performance metrics and troubleshooting stutter issues.

---

### 💡 Créditos / Credits

* **Proyecto Original / Original Project:** `TheMonadoGuy` / `vinitaison` (Monado)
* **Desarrollo, Fixes y Portability / Development, Fixes & Portable Fork:**
* `Ivanprestef2` (Ivan Prestef2) — *Asistido por Claude AI / Assisted by Claude AI*
* [`Marval53`](https://github.com/Marval53) — *Colaborador / Contributor (Asistido por ChatGPT 6.1 Sol / Assisted by ChatGPT 6.1 Sol)*
