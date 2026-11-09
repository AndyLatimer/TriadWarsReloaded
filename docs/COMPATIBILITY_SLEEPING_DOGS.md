# Análisis de Compatibilidad: Triad Wars vs Sleeping Dogs: Definitive Edition

## 1. Comparativa de Versiones de Motor (Theory Engine)

| Parámetro | Sleeping Dogs: Definitive Edition | Triad Wars Beta |
| :--- | :--- | :--- |
| **Ejecutable** | `sdhdship.exe` | `TriadWars.exe` |
| **Fecha de compilación** | 14 de octubre de 2014, 18:30:08 UTC | 16 de diciembre de 2015, 22:20:52 UTC |
| **Diferencia temporal** | Versión base | +14 meses de desarrollo continuo |
| **Tipos en Reflection.rdb**| 620 clases de motor | 948 clases de motor (+328 nuevos tipos) |
| **Enumeraciones** | 77 enums | 97 enums (+20 nuevos enums) |
| **Servicios de red** | Steamworks API, Twitch SDK | Servidor HTTP REST + WebSocket dedicado (`UFG::OnlineServices`) |
| **Arquitectura de archivos** | 11 archivos `.big` masivos (~13.6 GB) | 125 archivos `.big` segmentados (~9.75 GB) |
| **Desplazamiento en .bix** | Padding de alineación en 0x84 | Padding de alineación en 0x88 (shift de 4 bytes) |

## 2. Compatibilidad del Esquema de Reflexión (Reflection.rdb)
Se realizó una inspección a bajo nivel de las 545 clases coincidentes entre ambos títulos:
- **506 clases** conservan una disposición de memoria binaria idéntica (mismo tamaño, mismos campos de desplazamiento).
- **39 clases** sufrieron cambios estructurales en el motor para dar soporte a sistemas en línea, gestión asíncrona de turfs, sincronización de facciones y mutadores de combate.
- **328 nuevas clases** en Triad Wars corresponden a sistemas inexistentes en Sleeping Dogs:
  - `UFG::TurfTemplate`, `UFG::RacketType`, `UFG::RacketInstanceData` (gestión de negocios y territorios)
  - `UFG::StoreClothing`, `UFG::StoreOutfit`, `UFG::StoreBundle`, `UFG::StoreContactBundle` (tienda y comercio)
  - `UFG::FavourInstanceData`, `UFG::EnforcerUpgradeDefinition` (favores y mejoras)
  - `UFG::CommodityType` (mercado negro y contrabando)
  - `UFG::MetagameInfoWrapper` (metajuego y progresión económica)

## 3. Compatibilidad de Archivos de Recursos (.big / .bin)
* **Texturas y Modelos:**
  - El formato de contenedor `.perm.bin` y `.temp.bin` utiliza la misma arquitectura básica de trozos (`qChunk`), pero los activos que referencian tipos de datos alterados o nuevos no pueden cargarse directamente en Sleeping Dogs sin adaptación.
  - La herramienta `SD-UltimateTexTool` de `sneakyevil` utiliza internamente el convertidor `TextureScriberPC64.exe` proveniente de Triad Wars para compilar texturas compatibles con Sleeping Dogs DE.
* **Archivos de Sonido (.pck):**
  - Triad Wars incluye pistas de voz, emisoras de radio y efectos de sonido en `Data/Audio/English(US).pck` (1.02 GB) y `Data/Audio/SFX.pck` (934 MB), mientras que Sleeping Dogs DE utiliza una jerarquía `data/Audio/SD2/`. Los formatos Wwise son compatibles entre ambos.
* **Shaders:**
  - `Data/shaders/shaders.perm.bin` en Triad Wars creció a 236 KB (vs 82 KB en SDDE) y `Shaders.temp.bin` a 46.8 MB (vs 14.1 MB), reflejando técnicas de renderizado actualizadas, oclusión ambiental mejorada e iluminación optimizada.

## 4. Regla de Oro
**No mezclar archivos `.big` o binarios de Sleeping Dogs directamente en Triad Wars**. Triad Wars contiene una evolución de 14 meses con esquemas de memoria expandidos. Cualquier sustitución indiscriminada provoca fallos de acceso a punteros nulos o corrupción en el deserializador de recursos del motor.
