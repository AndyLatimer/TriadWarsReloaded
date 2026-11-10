# Guía de Continuidad Técnica del Proyecto (Triad Wars Offline Revival)

Este documento detalla el estado exacto del proyecto, las estructuras de memoria del motor nativo descubiertas mediante ingeniería inversa, los sistemas restaurados, los problemas pendientes identificados y las instrucciones para retomar el desarrollo.

---

## 1. Disposición del Workspace
* **`x:\projects\TriadWars\`**: Raíz del proyecto.
  - `beta/`: Instalación operativa de Triad Wars Beta 1.6.0.0 (Changelist 541920, x64, TheoryEngine).
    - `dinput8.dll`: Proxy hook compilado para la integración de pisos francos e interacciones.
    - `XINPUT9_1_0.dll`: Wrapper nativo para mandos DualSense PS5.
    - `safehouse_hook.log`: Registro en tiempo real de posición del jugador, eventos de teletransporte y estados del hook.
  - `sleeping-dogs/`: Referencia de sólo lectura de Sleeping Dogs: Definitive Edition.
  - `server/`: Código fuente del servidor local emulado (`LocalServer/LocalServer/`).
    - `handlers.py`: Controladores de red para comercio, misiones, tiendas, vehículos y estado de juego.
    - `state.json`: Base de datos de estado del jugador (inventario, bolsas, favores, estado del mundo).
    - `tools/safehouse_manager.py`: Gestor de puntos de spawn y desbloqueo de pisos francos.
  - `tools/`:
    - `SafehouseHook/`: Código fuente C++ del proxy `dinput8.cpp` y archivo de definición `dinput8.def`.
    - `xinput_dualsense/`: Código fuente del wrapper XInput para mando PS5.
    - `TheoryEngine/`: Reconstrucción de símbolos del motor de UFG.
    - `BigFileSystem/`, `FileRedirector/`, `PermToFBX/`, `SD-UltimateTexTool/`.
  - `launcher/`: Lanzador gráfico de Windows (`TriadWarsLauncher.exe`).
  - `docs/`: Informes técnicos y guías de arquitectura.

---

## 2. Puntos Clave de Ejecución
1. **Servidor Local:** Escucha en `127.0.0.1:443` (HTTPS).
   ```powershell
   cd x:\projects\TriadWars\server\LocalServer\LocalServer
   python server.py --host 127.0.0.1 --port 443
   ```
2. **Configuración del Cliente:** `beta/UserOptions.dat` debe tener `<Param key="Environment">localhost</Param>`.
   ```powershell
   python server/LocalServer/LocalServer/useroptions.py set Environment localhost
   ```
3. **Parche de Estabilidad del Motor (Spot-Shadow Pass):**
   Corrige la condición de carrera en el pase de sombras cuando los LODs no han cargado (`TriadWars.exe+0x548a`):
   ```powershell
   python server/LocalServer/LocalServer/tools/patch_exe.py apply
   ```
4. **Idioma en Español:**
   ```powershell
   python server/LocalServer/LocalServer/useroptions.py set Language Spanish
   ```
5. **Compilación de `dinput8.dll`:**
   ```cmd
   call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
   cd /d X:\projects\TriadWars\tools\SafehouseHook
   cl.exe /LD /O2 /MT dinput8.cpp user32.lib winmm.lib /link /DEF:dinput8.def /OUT:..\..\beta\dinput8.dll
   ```

---

## 3. Avances Realizados en Esta Sesión

### 3.1. Corrección del Concesionario de Vehículos (Tienda de Coches)
* **Problema:** En el concesionario de coches de mundo abierto, al intentar adquirir vehículos aparecía el error: *"An unexpected error occurred"*.
* **Causa:** En `handlers.py`, el endpoint `@route("game", "purchase")` validaba si el `store_uid` y el `item_uid` figuraban en el catálogo estático `store_items.json`. Si el vehículo no estaba catalogado allí, devolvía `_ok_bags()` sin la propiedad `data.CortexUID`, provocando el fallo del cliente.
* **Solución aplicada:** Se modificó `handlers.py` (líneas 1373-1405) para incorporar un fallback que acepte compras de vehículos dinámicos, descuente el precio (o auto-recargue saldo en modo offline), asigne el vehículo a la bolsa `InventoryBag` del jugador y retorne el objeto estructurado con `data.CortexUID` y `data.mDataBags`.

### 3.2. Descubrimientos de Ingeniería Inversa en `TriadWars.exe` (Changelist 541920)
Mediante desensamblado directo del ejecutable x64, se han localizado las estructuras y funciones exactas del motor de UFG:
1. **Puntero del Jugador Local (`GetLocalPlayer`):**
   - RVA: `0x3978A0`.
   - Código: `mov rax, [rip + 0x205AD51]; ret` -> Lee `*(void**)(g_base + 0x23F25F8)`.
2. **Nodo de Transformación Gráfica:**
   - Offset en `SimObject`: `+0x58` (`TransformNodeComponent*`).
   - Matriz de mundo 4x4: `+0x80`.
   - Matriz local 4x4: `+0xC0`.
   - Bandera de actualización `mChanged`: `+0x70`.
   - Función de actualización: `UpdateWorldTransform` en RVA `0x1CA120`.
3. **Tabla Global del Actor Manager (Game Action System):**
   - Contador de actores activos: `*(uint32_t*)(g_base + 0x25E09B8)`.
   - Puntero al array de actores: `*(uint8_t**)(g_base + 0x25E09C0)`.
   - Tamaño de entrada (stride): `0xA8` bytes.
   - Offset `+0x00`: Puntero al objeto nativo `TSActor*`.
   - Offset `+0x08`: `uint32_t UID` del `SimObject` asociado.
   - Referencia inversa: `*(void**)((uintptr_t)TSActor + 0xD0) == pSimObj`.
4. **Funciones Nativas de Teletransporte del Motor:**
   - **`TSActor::TeleportToTransform(TSActor* actor, const float* matrix)` en RVA `0x5958B0`:**
     - Ejecuta `PlaceOnGround` (`0x3B3390`) sobre las coordenadas `(x, y, z)`.
     - Reserva memoria para el evento: `Malloc(0x90, "TeleportPosition", 0)` (`0x1BF580`).
     - Lee el canal del evento en `g_base + 0x229F128`.
     - Construye `TeleportPositionEvent` en RVA `0x445290`.
     - Encola el evento en el despachador oficial en `g_base + 0x23F5870` vía `QueueEvent` (`0x1F0260`).
     - Invoca `TSActor::OnTeleport` en `vtable[0x14]` / `+0xA0`.
   - **`TSActor::TeleportToTransformRaw` en RVA `0x595C70`:** Versión que omite `PlaceOnGround` y despacha el evento directo.
5. **Desprotección Crítica de Memoria:**
   - En RVA `0x185B000`, la página `.rdata` contiene los contadores de referencia que `QueueEvent` incrementa. Debe desprotegerse con `VirtualProtect(g_base + 0x185B000, 0x4000, PAGE_EXECUTE_READWRITE)` para evitar excepciones `0xC0000005`.

---

## 4. Coordenadas Verificadas de Pisos Francos e Interiores

### 4.1. Apartamento de North Point (Sleeping Dogs)
* **Rellano Exterior (frente a la puerta, planta alta):**
  - $X = 880.85$, $Y = -101.90$, $Z = 26.25$.
* **Punto de Llegada Interior (dentro del apartamento):**
  - $X = 875.50$, $Y = -101.00$, $Z = 26.25$.
* **Puerta de Salida Interior (hacia el rellano):**
  - $X = 877.50$, $Y = -101.90$, $Z = 26.25$.
* **Muebles e Instalaciones Interactivas Interiores:**
  - **Nevera (Dragon Kick):** $X = 873.80, Y = -105.00, Z = 26.25$ (Radio 2.0 m).
  - **Inodoro (Mear / Pee):** $X = 870.50, Y = -107.20, Z = 26.25$ (Radio 2.0 m).
  - **Lavabo (Lavarse la cara / Curación 100%):** $X = 872.80, Y = -107.20, Z = 26.25$ (Radio 2.0 m).
  - **Cama (Dormir / Curación 100% y ciclo horario):** $X = 867.50, Y = -101.00, Z = 26.25$ (Radio 2.5 m).
  - **Armario (Wardrobe Shop / Cambio de ropa):** $X = 868.50, Y = -98.00, Z = 26.25$ (Radio 2.2 m).

---

## 5. Análisis del Problema Pendiente con la Entrada al Apartamento

A pesar de haber localizado las funciones de teletransporte y compilado la DLL proxy, la entrada al interactuar en la puerta sigue sin responder con fiabilidad según las pruebas del usuario. Las razones técnicas identificadas son:

1. **Contexto de Ejecución del Hilo:**
   - La DLL ejecuta la monitorización en un hilo secundario de Windows (`MonitorThread`).
   - Las funciones internas del motor como `TSActor::TeleportToTransform` y `QueueEvent` esperan ejecutarse sincrónicamente en el hilo principal del motor (hilo de render / simulación física). Invocar eventos de Havok desde un hilo asíncrono puede provocar que el despachador descarte el evento o falle silenciosamente.
2. **Ausencia de Interfaz Gráfica / HUD OSD:**
   - Al no existir un prompt visual en pantalla (*"Pulsa E para interactuar"*), el usuario no sabe si el juego está registrando su posición exacta o si está fuera del cilindro de activación.
3. **Controlador de Personaje de Havok:**
   - El componente de física de Havok mantiene una cápsula de colisión rígida. Modificar únicamente la matriz visual sin congelar o sincronizar la cápsula física en el paso de simulación puede resultar en que el motor mantenga al personaje en el exterior.

---

## 6. Vías Recomendadas para Continuar

Para la persona o agente que retome este trabajo, se aconseja seguir una de las siguientes soluciones probadas:

### Opción A (La más directa y 100% funcional): Spawn Directo en el Interior
En lugar de depender de la interacción de la puerta en el mundo exterior, reaparecer al personaje directamente dentro del apartamento al cargar la partida:
1. En `server/LocalServer/LocalServer/state.json`:
   - Modificar las coordenadas de checkpoint en `gameplaystate`:
     ```
     PlayerCheckpointLocationX=875.50;PlayerCheckpointLocationY=-101.00;PlayerCheckpointLocationZ=26.25;
     PlayerLastStableLocationX=875.50;PlayerLastStableLocationY=-101.00;PlayerLastStableLocationZ=26.25;
     ```
2. Configurar la opción en `tools/safehouse_manager.py` para que al seleccionar "North Point Interior" asigne esas coordenadas exactas. El jugador arrancará directamente dentro del piso con todos los muebles listos.

### Opción B: Hook Síncrono en el Bucle Principal
En lugar de un hilo en segundo plano:
1. Interceptar una función que se ejecute una vez por fotograma en el hilo principal (por ejemplo, el final de `SimObject::Update` o `IDXGISwapChain::Present`).
2. Comprobar la tecla e invocar `TSActor::TeleportToTransform` dentro del hilo principal donde Havok procesa los eventos de forma segura.

### Opción C: Prompt Visual con DirectX / ImGui
1. Crear un hook a `IDXGISwapChain::Present` (DirectX 11) para dibujar un indicador en pantalla (*"Puerta de North Point - Pulsa [E] o [Y]"*).
2. De esta forma se confirma visualmente que el volumen de proximidad está activo.
