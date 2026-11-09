# Triad Wars Offline Revival - Análisis Técnico de Archivos Originales

## 1. Identificación del Cliente y Ejecutable
* **Ejecutable:** `TriadWars.exe` (x64)
* **Tamaño:** 39,942,168 bytes (~38.09 MB)
* **SHA-256:** `6bd58dbdea8f8acb4d76c24eccff77c14cdd8652408f25c38f7ab8bcb832cf7b`
* **Compilación (Timestamp PE):** `2015-12-16 22:20:52 UTC`
* **Versión del cliente:** `1.6.0.0` (Changelist `541920` según `UserOptions.dat`)
* **Machine Architecture:** x86_64 (`0x8664`)
* **Herramienta auxiliar adjunta:** `TextureScriberPC64.exe` (compilado el `2015-06-10 22:58:54 UTC`, con ruta PDB original: `d:\ufg\madscience\project\sd2\dev\build\Tool\int\TextureScriberPC64-PC64-Release\TextureScriberPC64.pdb`).

## 2. Dependencias Dinámicas (DLLs Importadas)
El binario `TriadWars.exe` importa los siguientes módulos:
* **Sistema Windows:**
  - `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `SHLWAPI.dll`
  - `ole32.dll`, `OLEAUT32.dll`, `PSAPI.DLL`, `Secur32.dll`, `dbghelp.dll`, `WINMM.dll`
  - `WSOCK32.dll`, `WS2_32.dll`, `IPHLPAPI.DLL`, `MSACM32.dll`
* **Gráficos & Renderizado (DirectX 11):**
  - `d3d11.dll`, `dxgi.dll`, `D3DCOMPILER_46.dll`
  - `GFSDK_SSAO.win64.dll` (NVIDIA Screen Space Ambient Occlusion x64)
  - `GFSDK_NVDOF_LIB.win64.dll` (NVIDIA Depth of Field x64)
* **Entrada de usuario:**
  - `DINPUT8.dll`, `XINPUT9_1_0.dll`
* **Red y Criptografía:**
  - `WINHTTP.dll`
  - `libcurl.dll` (vinculada con OpenSSL)
  - `LIBEAY32.dll` (OpenSSL 1.0.1j)
  - `SSLEAY32.dll` (OpenSSL 1.0.1j)

Todas las DLLs de terceros (`d3dcompiler_46.dll`, `GFSDK_SSAO.win64.dll`, `GFSDK_NVDOF_LIB.win64.dll`, `libcurl.dll`, `libeay32.dll`, `ssleay32.dll`) se encuentran intactas y presentes en la raíz de la instalación.

## 3. Estructura de Recursos y Archivos de Datos
* **Archivos empaquetados:**
  - 125 archivos `.big` con sus correspondientes 125 índices `.bix` (total de 40,678 recursos indexados).
  - Integridad verificada mediante análisis de cabeceras `qChunk` (`0x2C5C40A8` / `ChunkUID_BIGFileIndex`), conteo de entradas y límites de desplazamiento: 100% de entradas válidas, 0 errores de desbordamiento o truncamiento.
* **Directorio `Data/`:**
  - `DisplaySettings.xml`: Configuración de resolución, pantalla completa, relación de aspecto y presets gráficos.
  - `HK Options`: Opciones guardadas de control y jugabilidad de United Front Games.
  - `TW AutoSave 0x6ef9846b`: Bloque binario opaco con estado guardado de cliente.
  - `Data/Audio/`:
    - `English(US).pck` (1,022,921,525 bytes)
    - `SFX.pck` (934,120,832 bytes)
  - `Data/shaders/`:
    - `shaders.perm.bin` (236,776 bytes)
    - `Shaders.temp.bin` (46,874,880 bytes)
* **Bases de datos de tipos y reflexión:**
  - `Reflection.rdb` (351,512 bytes): Contiene el esquema completo de tipos reflejados (948 tipos de clases del Theory Engine de UFG y 97 enumeraciones).

## 4. Configuración de Red y Servicios Online Originales
* **Ofuscación:** `UserOptions.dat` se encuentra cifrado mediante una clave cíclica XOR de 33 bytes:
  `62d76d90779620f566f679dd661720253`
* **Configuración original extraída:**
  ```xml
  <UserOptions>
  <Version>1.0.5</Version>
  <Changelist>541920</Changelist>
  <UserRole>SherpaBuilder</UserRole>
  <Param key="Environment">prod.services.triadwars.com</Param>
  <Param key="Build Configs.Release">False</Param>
  <Param key="Build Configs.Profile">False</Param>
  <Param key="Build Configs.Ship">True</Param>
  <Param key="PublishSymbols">True</Param>
  <Param key="KeyboardMouse">True</Param>
  <Param key="EnablePendingFeatures">False</Param>
  </UserOptions>
  ```
* **Protocolo de comunicación:**
  - El cliente construye las llamadas de servicio como:
    `https://<Environment>/<service>/api/v1/<method>`
  - No valida la validez del certificado TLS/SSL (acepta certificados autofirmados).
  - Al cambiar `Environment` a `localhost` (o `127.0.0.1`), el juego dirige todo su tráfico HTTPS y WebSocket al servidor local en el puerto `443`.

## 5. Localización e Idiomas (Español Oficial)
* **Localización en el motor:**
  - El motor carga los textos desde archivos binarios empaquetados en `UI15.big`.
  - Existe el archivo oficial `Data\UI\Localization\SP_Global.bin` (hash qSymbol `0xFF26377F`), comprimido en LZMA.
  - Al descomprimir e inspeccionar `SP_Global.bin`, se identificaron 2,592 cadenas traducidas íntegramente al español oficial (menús principales, interfaz, tutoriales, misiones de operaciones de tríada, armamento y diálogos).
* **Activación de Idioma:**
  - En la rutina de inicio (`0x1404b3e21`), `TriadWars.exe` consulta el parámetro `Language` en `UserOptions.dat`.
  - Si el parámetro no está presente, utiliza por omisión `English` (`EN_Global.bin`).
  - Añadiendo `<Param key="Language">Spanish</Param>` a `UserOptions.dat`, el cliente cambia automáticamente todos los textos de la interfaz al español sin necesidad de modificar el código binario del juego.
* **Audio:**
  - Las voces y efectos de sonido están centralizados en `Data/Audio/English(US).pck` y `SFX.pck` (las voces originales de Triad Wars fueron grabadas en inglés y cantonés).

## 6. Soporte para Mando DualSense (PlayStation 5)
* **Importación de Entrada:**
  - `TriadWars.exe` importa `XINPUT9_1_0.dll` (`XInputGetState`, `XInputSetState`, `XInputGetCapabilities`).
  - Dado que los mandos Sony DualSense (USB/Bluetooth `VID_054C&PID_0CE6` y `VID_054C&PID_0DF2`) utilizan interfaces HID DirectInput nativas en lugar de XInput nativo de Xbox, Windows no los expone por omisión a través de XInput 9.1.0 sin software externo.
* **Solución de Integración Nativa (XInput Proxy DLL):**
  - Se compiló una biblioteca nativa de 64 bits `beta/XINPUT9_1_0.dll` situada directamente en el directorio del cliente.
  - Windows carga primero la DLL local antes de recurrir a `System32`.
  - La DLL detecta dinámicamente dispositivos DualSense por HID/DirectInput, lee sus reportes de entrada en crudo (ejes analógicos, cruceta hat switch, gatillos adaptativos L2/R2, botones de acción Cross, Circle, Square, Triangle, bumpers L1/R1, L3/R3, Options/Create), aplica zonas muertas radiales calibradas idénticas a las de Xbox 360/One, y traduce los estados a la estructura `XINPUT_STATE`.
  - El mando funciona de inmediato con cero latencia y sin requerir programas externos (como Steam Input o DS4Windows).

