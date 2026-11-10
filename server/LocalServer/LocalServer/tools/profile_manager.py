"""Administrador de Perfil, Economía y Reputación para Triad Wars Offline.

Permite consultar y modificar el dinero, prestigio (Face), oro y estadísticas
del jugador sin reiniciar la partida.

Uso:
    python profile_manager.py status
    python profile_manager.py add-cash 100000
    python profile_manager.py add-gold 5000
    python profile_manager.py set-face 9
    python profile_manager.py max-stats
    python profile_manager.py backup
"""
import argparse
import json
import os
import shutil
import time

HERE = os.path.dirname(os.path.abspath(__file__))
STATE_FILE = os.path.join(HERE, "..", "state.json")
BACKUPS_DIR = os.path.join(HERE, "..", "backups")

# Umbrales oficiales de nivel Face
FACE_THRESHOLDS = [0, 1000, 5000, 15000, 40000, 100000, 250000, 600000, 1500000, 4000000]


def load_state():
    if not os.path.exists(STATE_FILE):
        print(f"Error: No se encontró el archivo de estado en {STATE_FILE}")
        return None
    with open(STATE_FILE, "r", encoding="utf-8") as f:
        return json.load(f)


def save_state(data):
    temp = STATE_FILE + ".tmp"
    with open(temp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    os.replace(temp, STATE_FILE)


def show_status():
    state = load_state()
    if not state:
        return

    player = state.get("player", {})
    pdb = player.get("PlayerDataBag", {})
    cb = player.get("CurrencyBag", {})
    turf = player.get("TurfBag", {}).get("Turf", {})
    rackets = player.get("TurfBag", {}).get("Rackets", [])

    name = pdb.get("Name", "Desconocido")
    player_id = pdb.get("PlayerDataID", "1001")
    dirty_money = int(cb.get("DirtyMoney", 0))
    clean_money = int(cb.get("CleanMoney", 0))
    face = int(pdb.get("Face", 0))

    # Determinar nivel de Face
    level = 0
    for i, thresh in enumerate(FACE_THRESHOLDS):
        if face >= thresh:
            level = i

    print("=" * 65)
    print(f"ESTADO DEL PERFIL DE JUGADOR: {name} (ID: {player_id})")
    print("=" * 65)
    print(f"  Dinero Sucio (Dirty Money):   HK$ {dirty_money:,}")
    print(f"  Oro Limpio (Clean Money):     {clean_money:,}")
    print(f"  Reputación Triad Face:       {face:,} pts (Rango {level}/9)")
    print(f"  Territorio (Turf):           {turf.get('Name', 'Sin Turf')} (Nivel {turf.get('Level', 1)})")
    print(f"  Negocios Activos (Rackets):   {len(rackets)}")
    print(f"  Vehículo de Garaje:          {state.get('preferred_starter_vehicle_uid', 'Por Defecto')}")

    gps = state.get("gameplaystate", "")
    spawn = "North Point"
    if "ActiveSafehouseSpawn=SafehouseCT" in gps:
        spawn = "Central"
    elif "ActiveSafehouseSpawn=SafehouseAB" in gps:
        spawn = "Aberdeen"
    elif "ActiveSafehouseSpawn=SafehouseKT" in gps:
        spawn = "Kennedy Town"
    print(f"  Punto de Reaparición Actual:  {spawn}")
    print("=" * 65)


def add_cash(amount):
    state = load_state()
    if not state:
        return
    cb = state.setdefault("player", {}).setdefault("CurrencyBag", {})
    current = int(cb.get("DirtyMoney", 0))
    new_val = max(0, current + amount)
    cb["DirtyMoney"] = str(new_val)
    save_state(state)
    print(f"[OK] Dinero Sucio actualizado: HK$ {current:,} -> HK$ {new_val:,}")


def add_gold(amount):
    state = load_state()
    if not state:
        return
    cb = state.setdefault("player", {}).setdefault("CurrencyBag", {})
    current = int(cb.get("CleanMoney", 0))
    new_val = max(0, current + amount)
    cb["CleanMoney"] = str(new_val)
    save_state(state)
    print(f"[OK] Oro Limpio actualizado: {current:,} -> {new_val:,}")


def set_face(level):
    if not 0 <= level <= 9:
        print("Error: El nivel de Face debe estar entre 0 y 9.")
        return
    state = load_state()
    if not state:
        return
    pts = FACE_THRESHOLDS[level]
    pdb = state.setdefault("player", {}).setdefault("PlayerDataBag", {})
    current = int(pdb.get("Face", 0))
    pdb["Face"] = str(pts)
    save_state(state)
    print(f"[OK] Reputación Face ajustada a Rango {level}/9: {current:,} pts -> {pts:,} pts")


def max_stats():
    state = load_state()
    if not state:
        return
    cb = state.setdefault("player", {}).setdefault("CurrencyBag", {})
    pdb = state.setdefault("player", {}).setdefault("PlayerDataBag", {})
    cb["DirtyMoney"] = "10000000"
    cb["CleanMoney"] = "50000"
    pdb["Face"] = str(FACE_THRESHOLDS[9])
    save_state(state)
    print("[OK] ¡Perfil potenciado al MÁXIMO!")
    print("  Dinero Sucio: HK$ 10,000,000")
    print("  Oro Limpio:   50,000")
    print(f"  Face:         {FACE_THRESHOLDS[9]:,} pts (Rango 9/9)")


def backup_profile():
    if not os.path.exists(STATE_FILE):
        return
    os.makedirs(BACKUPS_DIR, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    dest = os.path.join(BACKUPS_DIR, f"state_{stamp}.json")
    shutil.copy2(STATE_FILE, dest)
    print(f"[OK] Copia de seguridad creada: {dest}")


def main():
    parser = argparse.ArgumentParser(description="Administrador de Perfil Triad Wars")
    subparsers = parser.add_subparsers(dest="cmd")

    subparsers.add_parser("status", help="Muestra el estado actual del perfil")

    cash_p = subparsers.add_parser("add-cash", help="Añade dinero sucio")
    cash_p.add_argument("amount", type=int, help="Cantidad en HK$")

    gold_p = subparsers.add_parser("add-gold", help="Añade oro limpio")
    gold_p.add_argument("amount", type=int, help="Cantidad de oro")

    face_p = subparsers.add_parser("set-face", help="Establece el rango de prestigio Face (0-9)")
    face_p.add_argument("level", type=int, help="Nivel (0..9)")

    subparsers.add_parser("max-stats", help="Establece dinero, oro y Face al máximo")
    subparsers.add_parser("backup", help="Crea una copia de seguridad del estado")

    args = parser.parse_args()

    if args.cmd == "status" or not args.cmd:
        show_status()
    elif args.cmd == "add-cash":
        add_cash(args.amount)
    elif args.cmd == "add-gold":
        add_gold(args.amount)
    elif args.cmd == "set-face":
        set_face(args.level)
    elif args.cmd == "max-stats":
        max_stats()
    elif args.cmd == "backup":
        backup_profile()


if __name__ == "__main__":
    main()
