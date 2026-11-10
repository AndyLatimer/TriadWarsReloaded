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

## 3. Estado de la Integración de Pisos Francos (`SafehouseHook` / `dinput8.dll`)

### 3.1. Arquitectura del Hook
* **Ubicación:** Código fuente en `tools/SafehouseHook/dinput8.cpp`, binario inyectado como proxy en `beta/dinput8.dll`.
* **Mecanismo:** Proxy directo sobre `dinput8.dll` del sistema. Lanza un hilo monitor (`MonitorThread`) que:
  - Localiza el puntero del jugador local mediante `GetLocalPlayer` (`TriadWars.exe+0x3978A0`).
  - Obtiene el nodo de transformación (`TransformNodeComponent` a `+0x58`) y la posición en el mundo (`+0x80`).
  - Monitorea continuamente las teclas de interacción (`[E]`, `[Enter]`, `[Espacio]`, o botones del mando).
  - Al detectar interacción dentro del radio de la puerta, ejecuta `TeleportPlayer` escribiendo la matriz de transformación en el motor gráfico, ejecutando `UpdateTransform` (`+0x1CA120`), reteniendo la posición durante 500 ms contra el snap de físicas Havok, e invocando la función nativa `TeleportToTransformRaw` (`+0x595C70`).

### 3.2. Teclas Rápidas de Diagnóstico y Teletransporte (Hotkeys)
* **[F9]: Inspector de Posición en Vivo:** Imprime en `beta/safehouse_hook.log` las coordenadas actuales del jugador $(X, Y, Z)$ y la distancia calculada a todas las puertas configuradas.
* **[F7]: Teletransporte Forzado al Interior:** Fuerza la entrada inmediata al interior del piso franco de North Point (`X: 875.50, Y: -101.00, Z: 26.25`).
* **[F8]: Teletransporte Forzado al Rellano Exterior:** Fuerza la salida al rellano exterior de North Point (`X: 880.85, Y: -101.90, Z: 26.25`).

### 3.3. Dificultad Actual y Causa Técnica de la Entrada al Piso
1. **Ausencia de Triggers Nativos en el Mapa de Triad Wars:**
   - En Sleeping Dogs, las puertas de los pisos francos poseen triggers de script de misión vinculados a cinemáticas y árboles de acción. En Triad Wars, dichos scripts fueron eliminados al orientar el juego a cuarteles generales de turf en línea.
   - Como resultado, la puerta física no reacciona de forma nativa al interactuar con el juego base.
2. **Dependencia de Coordenadas Exactas en el Hook:**
   - El piso franco de North Point está elevado en un edificio. La puerta de entrada real se encuentra en el rellano superior en $Z \approx 26.25$, mientras que la entrada a nivel de calle/callejón está en $Z \approx 5.30$.
   - Si el jugador se encuentra a pie de calle o en una coordenada fuera del radio calibrado, el hook no detecta la pulsación y no se inicia la transición.
3. **Falta de Indicadores en Pantalla (HUD/OSD):**
   - El hook actual escribe diagnósticos en `beta/safehouse_hook.log` y emite beeps acústicos, pero no dibuja texto en la interfaz gráfica del juego, lo que dificulta al jugador conocer con precisión cuándo está dentro del volumen activo de interacción.

### 3.4. Vías Técnicas Recomendadas para Resolver la Entrada
1. **Spawn Directo en el Interior:** Añadir una opción en el Launcher o en `tools/safehouse_manager.py` para reaparecer directamente dentro del apartamento (`X: 875.50, Y: -101.00, Z: 26.25`), permitiendo explorar el interior y sus muebles sin depender de la puerta física.
2. **Implementación de OSD / Texto en Pantalla:** Interceptar `IDXGISwapChain::Present` con ImGui o inyectar avisos en el Scaleform nativo (`UFG::HUD`) para mostrar un prompt visible (*"Pulsa E para entrar"*).
3. **Trigger de Proximidad Cilíndrico Ampliado:** Extender la detección de la puerta en la entrada exterior a nivel de suelo para que el jugador pueda entrar desde el callejón sin tener que subir al rellano alto.



