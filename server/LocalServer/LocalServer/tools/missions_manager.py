"""Administrador de Misiones Secundarias, Redadas de Droga y Encuentros Dinámicos.

Uso:
    python missions_manager.py drugbusts
    python missions_manager.py encounters
    python missions_manager.py errands
    python missions_manager.py reset-drugbusts
"""
import argparse
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(HERE, "..", "data")
STATE_FILE = os.path.join(HERE, "..", "state.json")
MISSIONS_FILE = os.path.join(DATA_DIR, "missions_data.json")


def load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def list_drug_busts():
    missions = load_json(MISSIONS_FILE)
    state = load_json(STATE_FILE)
    collected = set(state.get("collected_crime_scenes", []))
    busts = missions.get("crime_scenes", [])

    print("=" * 85)
    print(f"REDADAS DE DROGA Y ESCENAS DE CRIMEN (DRUG BUSTS) - TOTAL: {len(busts)} (Completadas: {len(collected)}/{len(busts)})")
    print("=" * 85)
    print(f"{'ID':<5} {'Distrito':<15} {'Nombre de la Redada':<30} {'Banda Rival':<18} {'Estado'}")
    print("-" * 85)

    for b in busts:
        b_id = b.get("id")
        dist = b.get("district", "Hong Kong")
        name = b.get("name", f"Redada #{b_id}")
        gang = b.get("gang", "Desconocida")
        status = "[COMPLETADA]" if b_id in collected else "[ACTIVA]"
        print(f"{b_id:<5} {dist:<15} {name:<30} {gang:<18} {status}")

    print("=" * 85)


def list_encounters():
    missions = load_json(MISSIONS_FILE)
    encs = missions.get("world_encounters", [])
    state = load_json(STATE_FILE)
    completed = state.get("completed_encounters", 0)

    print("=" * 80)
    print(f"ENCUENTROS CALLEJEROS DINÁMICOS (WORLD ENCOUNTERS) - Completados: {completed}")
    print("=" * 80)
    for e in encs:
        print(f"Tipo #{e.get('type_id')}: {e.get('name')}")
        print(f"  Descripción:   {e.get('desc')}")
        print(f"  Zonas activas: {', '.join(e.get('districts', []))}")
        print(f"  Recompensa:    HK$ {e.get('reward_money', 5000):,} | Face: +{e.get('reward_face', 750)}")
        print("-" * 80)


def list_errands():
    missions = load_json(MISSIONS_FILE)
    errands = missions.get("errands", [])
    state = load_json(STATE_FILE)
    active = state.get("active_errands", [])
    completed = state.get("completed_errands", [])

    print("=" * 80)
    print(f"RECADOS Y FAVORES (ERRANDS) - Total en Catálogo: {len(errands)} | Activos: {len(active)} | Resueltos: {len(completed)}")
    print("=" * 80)
    for a in active:
        uid = str(a.get("uid"))
        match = next((e for e in errands if str(e.get("uid")) == uid), None)
        title = match.get("name", "Recado") if match else f"Errand {uid}"
        desc = match.get("desc", "") if match else ""
        st = "Listo para cobrar" if a.get("state") == 1 else "En curso"
        print(f"[Slot {a.get('slot')}] {title} ({st})")
        if desc:
            print(f"        {desc}")
    print("=" * 80)


def reset_drug_busts():
    state = load_json(STATE_FILE)
    state["collected_crime_scenes"] = []
    gps = state.get("gameplaystate", "")
    flags = [f.strip() for f in gps.split(";") if f.strip()]
    flags = [f for f in flags if not f.startswith("CasesCompleted=")]
    flags.append("CasesCompleted=0")
    new_gps = ";".join(flags)
    state["gameplaystate"] = new_gps
    if "player" in state and "PlayerDataBag" in state["player"]:
        state["player"]["PlayerDataBag"]["GameplayState"] = new_gps
    save_json(STATE_FILE, state)
    print("Todas las 20 Redadas de Droga han sido reiniciadas. ¡Están listas para completarse de nuevo!")


def main():
    parser = argparse.ArgumentParser(description="Administrador de Misiones Secundarias de Triad Wars")
    sub = parser.add_subparsers(dest="cmd")

    sub.add_parser("drugbusts", help="Listar las 20 redadas de droga y su estado")
    sub.add_parser("encounters", help="Listar los encuentros callejeros dinámicos")
    sub.add_parser("errands", help="Listar recados y favores activos")
    sub.add_parser("reset-drugbusts", help="Reiniciar las redadas para rejugabilidad")

    args = parser.parse_args()
    if args.cmd == "drugbusts":
        list_drug_busts()
    elif args.cmd == "encounters":
        list_encounters()
    elif args.cmd == "errands":
        list_errands()
    elif args.cmd == "reset-drugbusts":
        reset_drug_busts()
    else:
        list_drug_busts()


if __name__ == "__main__":
    main()
