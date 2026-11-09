# Guía de Continuidad Técnica del Proyecto

Este documento está diseñado para que cualquier desarrollador o agente de IA retome el proyecto sin necesidad de repetir la fase de investigación.

## 1. Disposición del Workspace
* **`x:\projects\TriadWars\`**: Raíz del proyecto.
  - `beta/`: Instalación completamente aislada y operativa de Triad Wars Beta 1.6.0.0.
  - `sleeping-dogs/`: Referencia de sólo lectura a Sleeping Dogs: Definitive Edition (enlaces aislados).
  - `server/`: Código fuente del servidor local emulado (`LocalServer/LocalServer/`).
  - `tools/`: Repositorios de ingeniería inversa clonados:
    - `TheoryEngine`: Reconstrucción en C++ del motor de UFG con símbolos.
    - `SDK`: SDK de desarrollo de plugins y ganchos de memoria (MinHook, etc.).
    - `BigFileSystem`: Desempaquetador y visor de archivos `.big` y `.bix`.
    - `FileRedirector`: Inyector DLL para cargar archivos sueltos desde carpetas de desarrollo sin reempaquetar `.big`.
    - `PermToFBX`: Extractor de modelos a FBX.
    - `SD-UltimateTexTool`: Utilidad de texturas.
  - `launcher/`: Lanzador integrado de Windows (inicia servidor, comprueba puertos, lanza el cliente).
  - `docs/`: Informes técnicos completos.
  - `logs/`: Registros de ejecución y depuración.

## 2. Puntos Clave de Ejecución
1. **Puerto del Servidor:** El cliente conecta por HTTPS a `127.0.0.1:443`. Comprobar con `Test-NetConnection -Port 443 -ComputerName 127.0.0.1` antes de arrancar.
2. **Configuración del Cliente:** `beta/UserOptions.dat` debe tener `<Param key="Environment">localhost</Param>`. Se puede modificar con:
   `python server/LocalServer/LocalServer/useroptions.py set Environment localhost`
3. **Inicio del Servidor:**
   ```powershell
   cd x:\projects\TriadWars\server\LocalServer\LocalServer
   python server.py --host 127.0.0.1 --port 443
   ```
4. **Verificación Automática:**
   ```powershell
   python server/LocalServer/LocalServer/tools/smoke_test.py --port 443
   ```
5. **Parche de Estabilidad del Motor (Spot-Shadow Pass):**
   El cliente original 1.6.0.0 tiene una condición de carrera nativa en el pase de sombras cuando un modelo de personaje no ha completado la carga de su LOD 0 (`TriadWars.exe+0x548a`).
   Se comprueba y aplica con:
   ```powershell
   python server/LocalServer/LocalServer/tools/patch_exe.py status
   python server/LocalServer/LocalServer/tools/patch_exe.py apply
   ```

6. **Localización Oficial en Español:**
   Los archivos originales incluyen `SP_Global.bin` en `UI15.big`. Para forzar el juego en español:
   ```powershell
   python server/LocalServer/LocalServer/useroptions.py set Language Spanish
   ```
   El Launcher incluye un botón para alternar entre Español e Inglés con un solo clic.

7. **Soporte Nativo de Mando DualSense (PS5):**
   Se implementó un wrapper XInput 9.1.0 nativo de 64 bits en `beta/XINPUT9_1_0.dll` (código fuente en `tools/xinput_dualsense/`).
   Intercepta automáticamente cualquier mando DualSense por DirectInput/HID (`VID_054C&PID_0CE6`), mapea todos sus botones, cruceta, sticks y gatillos L2/R2, aplicando zonas muertas. No se requiere ningún software adicional ni Steam Input/DS4Windows.

8. **Restauración de Misiones y Contenido Abierto:**
   Se completaron los controladores de red para activar los sistemas secundarios originales:
   - 87 encargos (*Errands*) en ranuras rotativas.
   - 7 operaciones de tríada (*Operations*).
   - Encuentros dinámicos (*World Encounters*: emboscadas, persecuciones policiales y furgones blindados).
   - 7 escenas del crimen en el mundo abierto (*Crime Scenes*).
   - 247 vehículos desbloqueados en el concesionario.


