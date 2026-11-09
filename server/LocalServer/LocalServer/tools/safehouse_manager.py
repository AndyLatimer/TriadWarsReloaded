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
    sh = _find_safehouse(safehouses, target)
    if not sh:
        print(f"Error: No se encontró ningún piso franco que coincida con '{target}'.")
        return

    sh["is_unlocked"] = unlock
    save_json(MISSIONS_FILE, missions)

    state = load_json(STATE_FILE)
    gps = state.get("gameplaystate", "")
    flags = [f.strip() for f in gps.split(";") if f.strip()]
    sym_flag = f"{sh['symbol']}=1"

    if unlock:
        if "SafehouseUnlocked=1" not in flags:
            flags.insert(0, "SafehouseUnlocked=1")
        if "UndercoverSafehouse=1" not in flags:
            flags.insert(1, "UndercoverSafehouse=1")
        if sym_flag not in flags:
            flags.append(sym_flag)
        door_flag = f"Door_{sh['symbol']}=0"
        if door_flag not in flags:
            flags.append(door_flag)
    else:
        flags = [f for f in flags if f != sym_flag and f != f"Door_{sh['symbol']}=0"]

    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    state.setdefault("player", {}).setdefault("PlayerDataBag", {})["GameplayState"] = new_gps
    save_json(STATE_FILE, state)

    action = "desbloqueado" if unlock else "bloqueado"
    print(f"Piso franco '{sh['name']}' ({sh['symbol']}) {action} correctamente.")
    print(f"GameplayState actualizado: {new_gps}")


def set_spawn_location(target):
    missions = load_json(MISSIONS_FILE)
    safehouses = missions.get("safehouses", [])
    sh = _find_safehouse(safehouses, target)
    if not sh:
        print(f"Error: No se encontró ningún piso franco que coincida con '{target}'.")
        return

    door = sh.get("door_coordinates", {})
    x = door.get("x", 840.5)
    y = door.get("y", -320.2)
    z = door.get("z", 5.3)

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
    flags.append(f"ActiveSafehouseSpawn={sh['symbol']}")

    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    state.setdefault("player", {}).setdefault("PlayerDataBag", {})["GameplayState"] = new_gps
    save_json(STATE_FILE, state)

    print("=" * 70)
    print(f"PUNTO DE REAPARICIÓN FIJADO EN: {sh['name']} ({sh['district']})")
    print(f"Coordenadas de Spawn:  X: {x:.2f}, Y: {y:.2f}, Z: {z:.2f}")
    print(f"Interior Chunk:        {sh['internal_id']}")
    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(description="Administrador de Pisos Francos de Triad Wars")
    subparsers = parser.add_subparsers(dest="cmd")

    subparsers.add_parser("list", help="Listar todos los pisos francos y su estado")
    subparsers.add_parser("status", help="Mostrar resumen de estado activo")

    spawn_parser = subparsers.add_parser("spawn", help="Fijar punto de reaparición inicial del jugador")
    spawn_parser.add_argument("target", help="ID o nombre del piso (ej: 1, np, ct, ab, kt)")

    unlock_parser = subparsers.add_parser("unlock", help="Desbloquear un piso franco")
    unlock_parser.add_argument("target", help="ID o nombre del piso (ej: 1, np, north, SafehouseNP)")

    lock_parser = subparsers.add_parser("lock", help="Bloquear un piso franco")
    lock_parser.add_argument("target", help="ID o nombre del piso (ej: 2, central, SafehouseCT)")

    coords_parser = subparsers.add_parser("coords", help="Mostrar coordenadas de un piso")
    coords_parser.add_argument("target", help="ID o nombre del piso (ej: 1, np, SafehouseNP)")

    args = parser.parse_args()

    if args.cmd == "list":
        list_safehouses()
    elif args.cmd == "status":
        print_status()
    elif args.cmd == "spawn":
        set_spawn_location(args.target)
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
