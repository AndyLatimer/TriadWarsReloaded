# Arquitectura del Servidor Local de Triad Wars

## 1. Visión General
El servidor local (`server/LocalServer/LocalServer/`) emula los servicios centrales en la nube de Triad Wars (`prod.services.triadwars.com`), permitiendo la ejecución íntegra del cliente de forma desconectada en modo individual.

El servidor está implementado en Python estándar (sin librerías externas obligatorias) y atiende peticiones HTTPS y WebSocket en `https://127.0.0.1:443`.

## 2. Componentes del Servidor
* `server.py`:
  - Servidor multi-hilo HTTPS (`ThreadingHTTPServer`) con recarga automática en caliente de endpoints (`maybe_reload()`).
  - Negociación SSL/TLS (OpenSSL compatible con TLS 1.2).
  - Canales de WebSocket (`Sec-WebSocket-Key` / `Sec-WebSocket-Accept`) para la mensajería asíncrona del juego.
  - Registro pormenorizado en `logs/server-YYYYMMDD-HHMMSS.log`.
* `handlers.py`:
  - Enrutador modular por servicio (`game`, `commerce`, `social`, `logging`, `accounts`, `messaging`).
  - Procesamiento de respuestas JSON con las convenciones exactas del deserializador del juego:
    - `mReturnCode: 0` para éxito.
    - Valores de 64 bits devueltos estrictamente como **cadenas de texto numéricas** (para evitar truncamientos de coma flotante de doble precisión en el motor C++).
* `state.py`:
  - Modelo de estado y persistencia del perfil de jugador (`state.json`).
  - Gestión de bolsas de datos (`Data Bags`) solicitadas por máscara de bits `mRequestedBags`.
  - Simulación de rivales (7 oponentes asignados a los 7 distritos de turf).
  - Construcción de negocios (`Rackets`), mejoras de refugio (`Safehouse`), almacén (`Warehouse`) y templo (`Temple`).
  - Control de moneda sucia y lavandería de dinero en tiempo real.
* `useroptions.py`:
  - Codificador/decodificador XOR de `UserOptions.dat` mediante la clave cíclica de 33 bytes.
  - Permite conmutar transparentemente entre `prod.services.triadwars.com` y `localhost`.
* `tools/patch_exe.py`:
  - Parche opcional en caliente de 30 bytes para prevenir el bloqueo de puntero nulo en el pase de sombras (`spot-shadow pass`, offset `TriadWars.exe+0x548a`).

## 3. Máscara de Bolsas de Datos (Data Bags)

| Bit | Nombre de la Bolsa | Estado Emulado |
| :--- | :--- | :--- |
| `0x0001` | `PlayerDataBag` | Sí (Nombre, Face, XP, Tríada, Estado) |
| `0x0002` | `CustomizationBag` | Sí (Armas, Vestimenta, Presets) |
| `0x0004` | `InformantsBag` | No requerido en inicio |
| `0x0008` | `FavoursBag` | Sí (Cartas de favores y mutadores) |
| `0x0010` | `ActiveFavoursBag`| No requerido |
| `0x0040` | `CurrencyBag` | Sí (CleanMoney, DirtyMoney) |
| `0x0080` | `TurfBag` | Sí (Estructuras, Nivel, Ubicación) |
| `0x0100` | `InventoryBag` | Sí (Objetos de inventario del jugador) |
| `0x0200` | `ShieldBag` | No requerido |
| `0x0400` | `TrialsBag` | No requerido |
| `0x0800` | `ExportRewardsBag` | No requerido |
| `0x1000` | `EnforcerUpgradesBag` | Sí (Mejoras pasivas y ranuras) |

## 4. Persistencia y Reinicio
* Los datos del jugador se guardan automáticamente en `server/LocalServer/LocalServer/state.json`.
* Para reiniciar el juego como un jugador nuevo, basta con eliminar o renombrar `state.json`.

## 4. Reconstrucción de Contenido y Misiones Secundarias
Originalmente, los endpoints de misiones devolvían colecciones vacías (`[]`). Se completó la implementación para activar el 100% de los sistemas durmientes en el cliente:

1. **Encargos Callejeros (Errands - `/game/api/v1/errands`):**
   - Extraídos 87 encargos oficiales activos desde `devdata/gamedb.json` (`UFG::ErrandTypeMetadata`).
   - El servidor mantiene 4 ranuras activas que rotan al completarse (combate de artes marciales, asaltos a negocios, sabotajes industriales, conducción extrema y habilidades de Enforcer).
   - Endpoints implementados: `errands/end/<slot>`, `errands/claimreward/<slot>` y `completederrands` con persistencia en `state.json`.
2. **Operaciones de Tríada (Operations - `/game/api/v1/operation/list`):**
   - Activadas las 7 operaciones de tríada (`Catch`, `Defeat`, `Photo`, `Deliver`, `Collect`, `Destroy`, `Hijack`) con cronómetro dinámico (`mTotalSeconds: 7200`).
   - Endpoint `operation/endoperation` calcula recompensas de dinero sucio, experiencia de Enforcer y Face.
3. **Encuentros Dinámicos Callejeros (World Encounters - `/game/api/v1/worldencounter/list`):**
   - Activados los 3 tipos de eventos callejeros (`WorldEncounter_Ambush`, `WorldEncounter_PolicePursuit`, `WorldEncounter_ArmouredCar`).
   - Endpoint `worldencounter/endencounter` premia el éxito en persecuciones y asaltos a blindados.
4. **Escenas del Crimen (Crime Scenes - `/game/api/v1/openworldcrimescene`):**
   - Desplegadas escenas del crimen en coordenadas clave de Hong Kong (North Point Night Market, Golden Koi, Waterfront Docks, Central Plaza, Aberdeen, Kennedy Town y Victoria Peak).
   - El jugador puede localizar la silueta de tiza del cadáver, inspeccionar la escena y cobrar la recompensa de prestigio con `openworldcrimescenecollection/<id>`.
5. **Tienda de Vehículos (Vehicle Dealership - `/commerce/api/v1/store/14286134624752669825`):**
   - Desbloqueados los 247 vehículos completos de la base de datos `UFG::StoreVehicle` en el catálogo de compra, permitiendo guardarlos en el garaje del jugador (`InventoryBag`) y conducirlos por el mapa.

