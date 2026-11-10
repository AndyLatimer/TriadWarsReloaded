"""Safehouse Manager for Triad Wars Offline Revival.

Manages the 4 Sleeping Dogs safehouses (North Point, Central, Aberdeen, Kennedy Town),
their unlock flags in state.json, interior coordinates, and garage spawn points.

Usage:
    python safehouse_manager.py list
    python safehouse_manager.py status
    python safehouse_manager.py unlock <id | name>
    python safehouse_manager.py lock <id | name>
    python safehouse_manager.py coords <id | name>
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER_DIR = os.path.normpath(os.path.join(HERE, ".."))
STATE_FILE = os.path.join(SERVER_DIR, "state.json")
MISSIONS_FILE = os.path.join(SERVER_DIR, "data", "missions_data.json")


def load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def get_safehouses():
    missions = load_json(MISSIONS_FILE)
    return missions.get("safehouses", [])


def list_safehouses():
    safehouses = get_safehouses()
    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")

    print("=" * 80)
    print(f"{'ID':<3} {'Distrito':<15} {'Nombre':<25} {'Estado':<10} {'Interior Chunk':<20}")
    print("=" * 80)
    for sh in safehouses:
        sid = sh.get("id", 0)
        dist = sh.get("district", "")
        name = sh.get("name", "")
        unlocked = sh.get("is_unlocked", False)
        chunk = sh.get("internal_id", "")
        status = "[DESBLOQUEADO]" if unlocked else "[BLOQUEADO]"
        print(f"{sid:<3} {dist:<15} {name:<25} {status:<12} {chunk:<20}")
    print("=" * 80)
    print(f"Banderas activas en GameplayState: {gps or '(ninguna)'}\n")


def print_status():
    safehouses = get_safehouses()
    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    player = state.get("player", {})
    pdb = player.get("PlayerDataBag", {})

    print("\n--- ESTADO GENERAL DE PISOS FRANCOS ---")
    print(f"Jugador:                  {pdb.get('Name', 'Player')} (ID: {pdb.get('PlayerDataID')})")
    print(f"Piso Franco Activo:       North Point Apartment (Interior_SafehouseNP)")
    print(f"GameplayState Flags:      {gps}")

    unlocked = [s["name"] for s in safehouses if s.get("is_unlocked")]
    print(f"Pisos Desbloqueados:      {len(unlocked)}/{len(safehouses)} ({', '.join(unlocked)})")
    print(f"Catálogo de Vehículos:    247 vehículos listos para entrega en garaje exterior\n")


def print_coords(target):
    safehouses = get_safehouses()
    sh = _find_safehouse(safehouses, target)
    if not sh:
        print(f"Error: No se encontró ningún piso franco que coincida con '{target}'.")
        return

    print("=" * 70)
    print(f"COORDENADAS: {sh['name']} ({sh['district']})")
    print("=" * 70)
    print(f"Interior Chunk:         {sh['internal_id']}")
    print(f"Símbolo del Motor:      {sh['symbol']}")
    door = sh.get("door_coordinates", {})
    interior = sh.get("interior_coordinates", {})
    garage = sh.get("garage_coordinates", {})
    print(f"Puerta Exterior:        X: {door.get('x'):.2f}, Y: {door.get('y'):.2f}, Z: {door.get('z'):.2f}")
    print(f"Interior 3D:            X: {interior.get('x'):.2f}, Y: {interior.get('y'):.2f}, Z: {interior.get('z'):.2f}")
    print(f"Garaje / Valet:         X: {garage.get('x'):.2f}, Y: {garage.get('y'):.2f}, Z: {garage.get('z'):.2f}")
    print(f"Servicios Activos:      {', '.join(sh.get('features', []))}")
    print("=" * 70)


def _find_safehouse(safehouses, target):
    target_str = str(target).strip().lower()
    for sh in safehouses:
        if str(sh.get("id")) == target_str:
            return sh
        sym = sh.get("symbol", "").lower()
        if sym == target_str or sym.endswith(target_str):
            return sh
        if target_str in sh.get("name", "").lower():
            return sh
        if target_str in sh.get("district", "").lower():
            return sh
    return None


def set_safehouse_state(target, unlock=True):
    missions = load_json(MISSIONS_FILE)
    safehouses = missions.get("safehouses", [])

    if str(target).strip().lower() == "all":
        targets = safehouses
    else:
        sh = _find_safehouse(safehouses, target)
        if not sh:
            print(f"Error: No se encontró ningún piso franco que coincida con '{target}'.")
            return
        targets = [sh]

    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    flags = [f.strip() for f in gps.split(";") if f.strip()]

    if unlock:
        if "SafehouseUnlocked=1" not in flags:
            flags.insert(0, "SafehouseUnlocked=1")
        if "UndercoverSafehouse=1" not in flags:
            flags.insert(1, "UndercoverSafehouse=1")

    for sh in targets:
        sh["is_unlocked"] = unlock
        sym_flag = f"{sh['symbol']}=1"
        door_flag = f"Door_{sh['symbol']}=0"
        if unlock:
            if sym_flag not in flags:
                flags.append(sym_flag)
            if door_flag not in flags:
                flags.append(door_flag)
        else:
            flags = [f for f in flags if f != sym_flag and f != door_flag]

    save_json(MISSIONS_FILE, missions)

    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    state.setdefault("player", {}).setdefault("PlayerDataBag", {})["GameplayState"] = new_gps
    save_json(STATE_FILE, state)

    action = "desbloqueados" if unlock else "bloqueados"
    print(f"Pisos francos ({len(targets)}) {action} correctamente.")
    print(f"GameplayState actualizado: {new_gps}")


def sync_all_sleeping_dogs():
    """Restaura y sincroniza todas las características del singleplayer de Sleeping Dogs."""
    missions = load_json(MISSIONS_FILE)
    safehouses = missions.get("safehouses", [])
    for sh in safehouses:
        sh["is_unlocked"] = True
    save_json(MISSIONS_FILE, missions)

    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    flags = [f.strip() for f in gps.split(";") if f.strip()]

    sd_flags = [
        "SafehouseUnlocked=1", "UndercoverSafehouse=1",
        "SafehouseNP=1", "Door_SafehouseNP=0",
        "SafehouseCT=1", "Door_SafehouseCT=0",
        "SafehouseAB=1", "Door_SafehouseAB=0",
        "SafehouseKT=1", "Door_SafehouseKT=0",
        "FIGHTCLUB_INTRO_COMPLETE=1",
        "StatueUpgradePoint=11", "StatueDefensiveStage=5", "StatueOffensiveStage=5",
        "CopDrivingStage=5", "CopShootingStage=5",
        "TriadDefensiveStage=5", "TriadOffensiveStage=5",
        "GunplayUpgradeStage=5", "VehicleRamUpgrade=1",
        "CopXP=100000", "TriadXP=100000",
        "CopUpgradePoint=10", "TriadUpgradePoint=10",
        "MaxHealthUpgradePoint=50",
        "EnableJadeStatueIndicators=1", "EnableJadeStatueIndicatorsIcon=1", "EnableJadeStatueIndicatorsText=1",
        "EnableHealthShrineIndicators=1", "EnableHealthShrineIndicatorsIcon=1", "EnableHealthShrineIndicatorsText=1",
        "EnableLockboxIndicators=1", "EnableLockboxIndicatorsIcon=1", "EnableLockboxIndicatorsText=1",
        "EnableSpyCams=1", "EnableSpyCamsIcon=1", "EnableSpyCamsText=1",
        "EnableSpyCamIndicators=1", "EnableSpyCamIndicatorsIcon=1", "EnableSpyCamIndicatorsText=1",
        "EnableJumpIndicators=1",
        "EnableValet=1", "EnablePDA=1",
        "EnableKaraoke=1",
        "MassageParlourBuff_Activated=1", "StrikingBuff_Activated=1", "HealthRegen_Activated=1",
        "DLCRedeemed=1", "TriadWarsTutorialComplete=1"
    ]

    for f in sd_flags:
        key = f.split("=")[0]
        flags = [existing for existing in flags if not existing.startswith(f"{key}=")]
        flags.append(f)

    # Ensure spawn exists
    has_spawn = any(f.startswith("ActiveSafehouseSpawn") for f in flags)
    if not has_spawn:
        flags.append("PlayerCheckpointLocationX=840.50")
        flags.append("PlayerCheckpointLocationY=-320.20")
        flags.append("PlayerCheckpointLocationZ=5.30")
        flags.append("PlayerLastStableLocationX=840.50")
        flags.append("PlayerLastStableLocationY=-320.20")
        flags.append("PlayerLastStableLocationZ=5.30")
        flags.append("ActiveSafehouseSpawn=SafehouseNP")

    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    state.setdefault("player", {}).setdefault("PlayerDataBag", {})["GameplayState"] = new_gps
    save_json(STATE_FILE, state)

    print("=" * 70)
    print("¡SINCRONIZACIÓN SLEEPING DOGS COMPLETADA!")
    print(" - 4 Pisos Francos desbloqueados (North Point, Central, Aberdeen, Kennedy)")
    print(" - Dojo de Sifu y 4 Clubes de Pelea de Distrito activados")
    print(" - Árboles de Habilidades Kung Fu, Cop, Triad y Gunplay al MAXIMO")
    print(" - Indicadores en Minimapa de Estatuas de Jade, Santuarios, Cajas Fuertes y Cámaras")
    print(" - Buffs activos: Masaje (Doble Face), Comida callejera (Fuerza)")
    print("=" * 70)


def _find_spawn_target(missions, target):
    target_str = str(target).strip().lower()

    # 1. Search in safehouses
    for sh in missions.get("safehouses", []):
        if str(sh.get("id")) == target_str:
            return sh, sh.get("door_coordinates", {})
        sym = sh.get("symbol", "").lower()
        if sym == target_str or sym.endswith(target_str):
            return sh, sh.get("door_coordinates", {})
        if target_str in sh.get("name", "").lower() or target_str in sh.get("district", "").lower():
            return sh, sh.get("door_coordinates", {})

    # 2. Search in special_interiors (Landmarks)
    for lmk in missions.get("special_interiors", []):
        if str(lmk.get("id")) == target_str:
            return lmk, lmk.get("coordinates", {})
        sym = lmk.get("symbol", "").lower()
        if sym == target_str or target_str in sym:
            return lmk, lmk.get("coordinates", {})
        if target_str in lmk.get("name", "").lower() or target_str in lmk.get("internal_id", "").lower():
            return lmk, lmk.get("coordinates", {})

    # 3. Search Sifu dojo
    dojo = missions.get("dojo", {})
    if target_str in ("dojo", "sifu", "artes marciales"):
        return dojo, dojo.get("coordinates", {})

    return None, None


def get_spawn():
    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    for part in gps.split(";"):
        if part.startswith("ActiveSafehouseSpawn="):
            sym = part.split("=")[1]
            missions = load_json(MISSIONS_FILE)
            for sh in missions.get("safehouses", []):
                if sh.get("symbol") == sym:
                    print(f"{sym}:{sh.get('name')}:{sh.get('district')}")
                    return
            for lmk in missions.get("special_interiors", []):
                if lmk.get("symbol") == sym:
                    print(f"{sym}:{lmk.get('name')}:{lmk.get('district')}")
                    return
            if sym == "Interior_DojoNP":
                print(f"{sym}:Sifu Martial Arts Dojo:North Point")
                return
            print(f"{sym}:Safehouse")
            return
    print("SafehouseNP:North Point Apartment:North Point")


def set_spawn_location(target):
    missions = load_json(MISSIONS_FILE)
    target_obj, coords = _find_spawn_target(missions, target)
    if not target_obj:
        print(f"Error: No se encontró ningún piso franco o punto de interés que coincida con '{target}'.")
        return

    x = coords.get("x", 840.5)
    y = coords.get("y", -320.2)
    z = coords.get("z", 5.3)

    symbol = target_obj.get("symbol", target_obj.get("internal_id", "SafehouseNP"))

    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    flags = [f.strip() for f in gps.split(";") if f.strip()]

    # Filter out previous spawn flags
    flags = [f for f in flags if not (f.startswith("PlayerCheckpointLocation") or
                                      f.startswith("PlayerLastStableLocation") or
                                      f.startswith("ActiveSafehouseSpawn"))]

    flags.append(f"PlayerCheckpointLocationX={x:.2f}")
    flags.append(f"PlayerCheckpointLocationY={y:.2f}")
    flags.append(f"PlayerCheckpointLocationZ={z:.2f}")
    flags.append(f"PlayerLastStableLocationX={x:.2f}")
    flags.append(f"PlayerLastStableLocationY={y:.2f}")
    flags.append(f"PlayerLastStableLocationZ={z:.2f}")
    flags.append(f"ActiveSafehouseSpawn={symbol}")

    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    state.setdefault("player", {}).setdefault("PlayerDataBag", {})["GameplayState"] = new_gps
    save_json(STATE_FILE, state)

    print("=" * 70)
    print(f"PUNTO DE REAPARICIÓN FIJADO EN: {target_obj['name']} ({target_obj.get('district', 'Hong Kong')})")
    print(f"Coordenadas de Spawn:  X: {x:.2f}, Y: {y:.2f}, Z: {z:.2f}")
    print(f"Interior Chunk:        {target_obj.get('internal_id', symbol)}")
    print("=" * 70)


def list_landmarks():
    missions = load_json(MISSIONS_FILE)
    landmarks = missions.get("special_interiors", [])
    print("=" * 75)
    print(f"PUNTOS DE INTERÉS E INTERIORES ESPECIALES - TOTAL: {len(landmarks)}")
    print("=" * 75)
    for l in landmarks:
        c = l.get("coordinates", {})
        print(f"[{l.get('id')}] {l.get('name')} ({l.get('district')})")
        print(f"    Símbolo: {l.get('symbol')} | Chunk 3D: {l.get('internal_id')}")
        print(f"    Coords:  X: {c.get('x'):.2f}, Y: {c.get('y'):.2f}, Z: {c.get('z'):.2f}")
        print(f"    Info:    {l.get('description')}")
        print("-" * 75)


def list_vehicles():
    missions = load_json(MISSIONS_FILE)
    vehicles = missions.get("starter_vehicles", [])
    state = load_json(STATE_FILE)
    current_uid = state.get("player", {}).get("PlayerDataBag", {}).get("LastUsedVehicle", "")

    print("=" * 75)
    print(f"VEHÍCULOS DESTACADOS PARA GARAJE (VALET) - TOTAL: {len(vehicles)}")
    print("=" * 75)
    for idx, v in enumerate(vehicles, 1):
        uid = str(v.get("uid"))
        active_tag = " [ACTIVO EN GARAJE]" if uid == str(current_uid) else ""
        print(f"[{idx}] {v.get('name')} ({v.get('class')}){active_tag}")
        print(f"    UID:  {uid}")
        print(f"    Info: {v.get('description')}")
        print("-" * 75)


def set_favorite_vehicle(target):
    missions = load_json(MISSIONS_FILE)
    vehicles = missions.get("starter_vehicles", [])
    target_str = str(target).strip().lower()

    found = None
    for idx, v in enumerate(vehicles, 1):
        if str(idx) == target_str or str(v.get("uid")) == target_str:
            found = v
            break
        if target_str in v.get("name", "").lower() or target_str in v.get("class", "").lower():
            found = v
            break

    if not found:
        print(f"Error: No se encontró ningún vehículo que coincida con '{target}'.")
        return

    uid = str(found["uid"])
    state = load_json(STATE_FILE)
    # Store catalogue preference separately. LastUsedVehicle is the game's
    # 32-bit driving ID, not the 64-bit store UID of the selected car.
    state["preferred_starter_vehicle_uid"] = uid
    save_json(STATE_FILE, state)

    print("=" * 70)
    print(f"VEHÍCULO DE GARAJE ACTUALIZADO: {found['name']} ({found['class']})")
    print(f"UID del Vehículo:   {uid}")
    print(f"Preferencia de garaje guardada; no se modifica el ID activo del juego.")
    print("=" * 70)


def get_favorite_vehicle():
    state = load_json(STATE_FILE)
    uid = str(state.get("preferred_starter_vehicle_uid", "3116199975099848891"))
    missions = load_json(MISSIONS_FILE)
    for v in missions.get("starter_vehicles", []):
        if str(v.get("uid")) == uid:
            print(f"{uid}:{v.get('name')}:{v.get('class')}")
            return
    print(f"{uid}:Tuono: BRG:Muscle Car")


def main():
    parser = argparse.ArgumentParser(description="Administrador de Pisos Francos y Puntos de Interés de Triad Wars")
    subparsers = parser.add_subparsers(dest="cmd")

    subparsers.add_parser("list", help="Listar todos los pisos francos y su estado")
    subparsers.add_parser("status", help="Mostrar resumen de estado activo")
    subparsers.add_parser("get-spawn", help="Obtener el punto de spawn activo")
    subparsers.add_parser("get-vehicle", help="Obtener el vehículo favorito activo en garaje")
    subparsers.add_parser("list-landmarks", help="Listar interiores y puntos de interés especiales")
    subparsers.add_parser("list-vehicles", help="Listar vehículos destacados de garaje")
    subparsers.add_parser("sync-sd", help="Sincronizar y restaurar todas las características de Sleeping Dogs")

    spawn_parser = subparsers.add_parser("spawn", help="Fijar punto de reaparición (piso franco o landmark)")
    spawn_parser.add_argument("target", help="ID o nombre (ej: 1, np, ct, ab, kt, koi, hospital, mansion, bambam)")

    veh_parser = subparsers.add_parser("vehicle", help="Fijar vehículo aparcado en el garaje exterior")
    veh_parser.add_argument("target", help="ID o nombre (ej: tuono, drifter, panzer, 860, 1100, neo)")

    unlock_parser = subparsers.add_parser("unlock", help="Desbloquear un piso franco (o 'all')")
    unlock_parser.add_argument("target", help="ID o nombre del piso (ej: 1, np, north, SafehouseNP, all)")

    lock_parser = subparsers.add_parser("lock", help="Bloquear un piso franco")
    lock_parser.add_argument("target", help="ID o nombre del piso (ej: 2, central, SafehouseCT)")

    coords_parser = subparsers.add_parser("coords", help="Mostrar coordenadas de un piso")
    coords_parser.add_argument("target", help="ID o nombre del piso (ej: 1, np, SafehouseNP)")

    args = parser.parse_args()

    if args.cmd == "list":
        list_safehouses()
    elif args.cmd == "status":
        print_status()
    elif args.cmd == "get-spawn":
        get_spawn()
    elif args.cmd == "get-vehicle":
        get_favorite_vehicle()
    elif args.cmd == "list-landmarks":
        list_landmarks()
    elif args.cmd == "list-vehicles":
        list_vehicles()
    elif args.cmd == "sync-sd":
        sync_all_sleeping_dogs()
    elif args.cmd == "spawn":
        set_spawn_location(args.target)
    elif args.cmd == "vehicle":
        set_favorite_vehicle(args.target)
    elif args.cmd == "unlock":
        set_safehouse_state(args.target, unlock=True)
    elif args.cmd == "lock":
        set_safehouse_state(args.target, unlock=False)
    elif args.cmd == "coords":
        print_coords(args.target)
    else:
        list_safehouses()


if __name__ == "__main__":
    main()
