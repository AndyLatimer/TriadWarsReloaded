"""Persistent player/world state for the local Triad Wars server.

Field names and encodings follow what the client deserialises (see NOTES.md):
  * 32-bit integers, floats and bools are plain JSON values;
  * every 64-bit value (ids, UIDs, object handles, money) is a decimal *string*,
    because the client's JSON library would otherwise round it through a double;
  * symbols (e.g. LocationName) are strings that the client hashes itself.
"""
import copy
import json
import os
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
STATE_PATH = os.path.join(HERE, "state.json")

with open(os.path.join(HERE, "data", "gamedb_min.json"), encoding="utf-8") as f:
    DB = json.load(f)

# ---- game database lookups ---------------------------------------------------
RACKET_TYPES = {r["$uid"]: r for r in DB["UFG::RacketType"]}
_BY_CAPTION = {}
for _r in DB["UFG::RacketType"]:
    _BY_CAPTION.setdefault(_r["mCaption"], []).append(_r)

SAFEHOUSE_UID = _BY_CAPTION["RACKET_SAFEHOUSE"][0]["$uid"]
WAREHOUSE_UID = _BY_CAPTION["RACKET_WAREHOUSE"][0]["$uid"]
TEMPLE_UID = _BY_CAPTION["RACKET_TEMPLE"][0]["$uid"]
EMPTY_UID = {r["mSize"]: r["$uid"] for r in _BY_CAPTION["Empty"]}   # SMALL / MEDIUM / LARGE

TURF_TEMPLATES = DB["UFG::TurfTemplate"]
TRIADS = DB["UFG::SelectableTriad"]
COMMODITIES = DB["UFG::CommodityType"]
XP_LEVELS = DB["UFG::XPDefinitions"][0]["mAuthoredXPLevels"]    # XP needed to reach each level
METAGAME = DB["UFG::MetagameInfoWrapper"][0]                    # build/upgrade costs and times
INFORMANT_TYPES = {i["$uid"]: i for i in DB["UFG::InformantType"]}  # contacts and their favours

# Upgrade favours: favour definition UID -> {"upgrade": enforcer upgrade UID, "name", "ftpe"}.
with open(os.path.join(HERE, "data", "favour_upgrades.json"), encoding="utf-8") as f:
    FAVOUR_UPGRADES = json.load(f)
FTPE_RAID_FAVOURS = sorted(uid for uid, v in FAVOUR_UPGRADES.items() if v["ftpe"])

# Store catalogues: store UID -> {"name", "type", "items": [store item UIDs]}. Built by
# tools/store_items.py; stores that are not listed have nothing for sale.
with open(os.path.join(HERE, "data", "store_items.json"), encoding="utf-8") as f:
    STORE_ITEMS = json.load(f)

# The open world has seven turf sites. The client maps each site's symbol to a
# slot 1..7 and looks the owner up as opponents[slot - 1].
TURF_LOCATIONS = ["Turf-Turf%05d" % n for n in range(1, 8)]

# mRequestedBags bit for each data bag in a serialized-player-data response.
BAG_PLAYER = 0x0001
BAG_CUSTOMIZATION = 0x0002
BAG_INFORMANTS = 0x0004
BAG_FAVOURS = 0x0008
BAG_ACTIVE_FAVOURS = 0x0010
BAG_CURRENCY = 0x0040
BAG_TURF = 0x0080
BAG_INVENTORY = 0x0100
BAG_SHIELD = 0x0200
BAG_TRIALS = 0x0400
BAG_EXPORT_REWARDS = 0x0800
BAG_ENFORCER_UPGRADES = 0x1000

AI_NAMES = ["Big Smile Lee", "Uncle Po", "Sonny Wo", "Broken Nose Jiang",
            "Two Chin Tsao", "Pockmark Cheuk", "Dogeyes Lin"]


def racket_instance(type_uid, level=1, block=0, slot=0, state=0, staff=0):
    r = {
        "RacketTypeUID": str(type_uid),
        "RacketLevel": level,
        "Staff": staff,
        "BlockIndex": block,
        "RacketState": state,
        "SlotIndex": slot,
        "CurrentProduction": 0,
        "Stats": {
            "MaxStaff": 0,
            "OutputMultiplier": 1.0,
            "ProductionTimeDivision": 1.0,
            "UpgradeDiscountMultiplier": 1.0,
            "ProductionInputDiscountDivision": 1.0,
            "NoStaffSpawn": 0,
        },
    }
    for n in range(1, 11):
        r["Production%d" % n] = 0
    return r


SAFEHOUSE_BLOCK = -1      # BlockIndex of the safehouse / warehouse / temple (off the grid)


def new_turf(location, template_index=0, level=1):
    tpl = TURF_TEMPLATES[template_index % len(TURF_TEMPLATES)]
    return {
        "Turf": {
            "LocationName": location,
            # The safehouse, warehouse and temple are not on the block grid: the client
            # positions (and mouse-picks) them from the turf origin only when BlockIndex is
            # -1. Any grid index puts their pick boxes over that grid lot instead, so the
            # safehouse cannot be selected in the turf manager.
            "Safehouse": racket_instance(SAFEHOUSE_UID, level, SAFEHOUSE_BLOCK),
            "Warehouse": racket_instance(WAREHOUSE_UID, level, SAFEHOUSE_BLOCK),
            "Temple": racket_instance(TEMPLE_UID, level, SAFEHOUSE_BLOCK),
            "TurfTemplateUID": tpl["$uid"],
            "RacketsBuilt": "0",
        },
        # Only built rackets are listed. The client draws empty lots itself from the
        # template, and counts every entry here against the turf's build points
        # (safehouse level minus len(Rackets)), so placeholders leave none to spend.
        "Rackets": [],
        "Commodities": [],
    }


def new_player(player_id, name, triad_index, location, template_index=0, account_id=None):
    return {
        "PlayerDataBag": {
            "PlayerDataID": str(player_id),
            "Name": name,
            "Face": "0",
            "LaunderedMoney": 0,
            "LaundrySecondsRemaining": 0,
            "EnforcerXP": 0,
            "Triad": TRIADS[triad_index % len(TRIADS)]["$uid"],
            "AccountID": str(account_id if account_id is not None else player_id),
            "LastUsedVehicle": "0",
            "GameplayState": "SafehouseUnlocked=1;UndercoverSafehouse=1;SafehouseNP=1;Door_SafehouseNP=0",
            "AvatarUrl": "",
            "PreferMatchmakingWithFriends": False,
            "IsFavourListInitialized": True,
        },
        "CurrencyBag": {"CleanMoney": "25000", "DirtyMoney": "5000"},
        "TurfBag": new_turf(location, template_index),
        "FavoursBag": [],
        "EnforcerUpgradesBag": [],
        "InventoryBag": [],
    }


def favour_instance(favour_id, definition_uid, is_new=1):
    """A favour the player owns (UFG::FavourInstanceData)."""
    return {
        "FavourID": str(favour_id),
        "SecondsElapsed": 0,
        "TotalSeconds": 0,
        "FavourDefinitionUID": str(definition_uid),
        "IsNew": is_new,
    }


def grant_starting_favours(player):
    """Give the two upgrade favours the first-raid results screen awards.

    The client shows these as raid rewards without asking the server for them, then
    its upgrade tutorial expects them to be owned.
    """
    owned = {f["FavourDefinitionUID"] for f in player["FavoursBag"]}
    equipped = {u["Item1"] for u in player["EnforcerUpgradesBag"]}
    next_id = 5001 + len(player["FavoursBag"]) + len(player["EnforcerUpgradesBag"])
    for uid in FTPE_RAID_FAVOURS:
        if uid in owned or FAVOUR_UPGRADES[uid]["upgrade"] in equipped:
            continue
        player["FavoursBag"].append(favour_instance(next_id, uid))
        next_id += 1


# Items the first-time-experience contacts hand over. The client never asks the server
# for them: after speaking to a contact it just re-reads the InventoryBag and expects the
# item to be there (FTPE_01 "Ridge Sport" car, FTPE_02 "Cleaver").
FTPE_STORE_ITEMS = ["5666866140888112945", "4038481858749259483"]


def inventory_item(item_id, store_item_uid, account_id):
    """An owned store item (UFG::InventoryItemInstanceData)."""
    return {
        "SecondsElapsed": 0,
        "TotalSeconds": 0,
        "StoreItemID": str(store_item_uid),
        "InventoryItemID": str(item_id),
        "AccountID": str(account_id),
    }


def grant_starting_inventory(player):
    bag = player.setdefault("InventoryBag", [])
    owned = {i.get("StoreItemID") for i in bag}
    next_id = max([7000] + [int(i.get("InventoryItemID", 0)) for i in bag]) + 1
    account = player["PlayerDataBag"].get("AccountID", "1")
    for uid in FTPE_STORE_ITEMS:
        if uid not in owned:
            bag.append(inventory_item(next_id, uid, account))
            next_id += 1


def default_state():
    player = new_player(1001, "Player", 1, TURF_LOCATIONS[0], 0, account_id=1)
    grant_starting_favours(player)
    grant_starting_inventory(player)
    return {
        "version": 5,
        "created": int(time.time()),
        "player": player,
        "opponents": [
            {
                "type": 0,
                "remaining_seconds": 86400,
                "data": new_player(2001 + i, AI_NAMES[i], i, TURF_LOCATIONS[i], i),
            }
            for i in range(7)
        ],
        "gamestate": "",          # opaque save blob the client POSTs to playersavegame
        "gameplaystate": "",
    }


class State:
    def __init__(self, path=STATE_PATH):
        self.path = path
        self.lock = threading.RLock()
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                self.data = json.load(f)
            self._migrate()
        else:
            self.data = default_state()
            self.save()

    def _migrate(self):
        """Bring a profile saved by an older server version up to date."""
        version = self.data.get("version", 1)
        if version >= 5:
            return
        player = self.data["player"]
        if version < 2:
            for key in ("FavoursBag", "EnforcerUpgradesBag", "InventoryBag"):
                player.setdefault(key, [])
            grant_starting_favours(player)
        if version < 3:
            # Move the safehouse-family buildings off the block grid (see new_turf).
            for owner in [player] + [o["data"] for o in self.data.get("opponents", [])]:
                turf = owner.get("TurfBag", {}).get("Turf", {})
                for key in ("Safehouse", "Warehouse", "Temple"):
                    if key in turf:
                        turf[key]["BlockIndex"] = SAFEHOUSE_BLOCK
        if version < 4:
            # Empty-lot placeholders used to be stored as rackets; drop them (see new_turf).
            empties = set(EMPTY_UID.values())
            for owner in [player] + [o["data"] for o in self.data.get("opponents", [])]:
                bag = owner.get("TurfBag")
                if bag:
                    bag["Rackets"] = [r for r in bag.get("Rackets", [])
                                      if r.get("RacketTypeUID") not in empties]
        if version < 5:
            grant_starting_inventory(player)
        self.data["version"] = 5
        self.save()

    def save(self):
        with self.lock:
            tmp = self.path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=1)
            os.replace(tmp, self.path)

    # -- response builders ---------------------------------------------------
    @staticmethod
    def bags(player, mask):
        """Build an mDataBags object containing only the bags we actually hold."""
        out = {}
        have = 0
        # PlayerData/Customization/Currency/Turf are objects; Favours, Inventory and
        # EnforcerUpgrades are arrays of records (EnforcerUpgrades as {Item1: uid, Item2: slot}).
        for bit, key in ((BAG_PLAYER, "PlayerDataBag"), (BAG_CUSTOMIZATION, "CustomizationBag"),
                         (BAG_FAVOURS, "FavoursBag"), (BAG_CURRENCY, "CurrencyBag"),
                         (BAG_TURF, "TurfBag"), (BAG_INVENTORY, "InventoryBag"),
                         (BAG_ENFORCER_UPGRADES, "EnforcerUpgradesBag")):
            if mask & bit and key in player:
                out[key] = copy.deepcopy(player[key])
                have |= bit
        out["mRequestedBags"] = have
        return out

    def player_bags(self, mask):
        with self.lock:
            return self.bags(self.data["player"], mask)

    def opponents(self, mask=0x1FFF):
        """Opponent summaries plus their data bags.

        The client reads these as two parallel root-level arrays: mOpponents[i]
        describes the rival and mOpponentDataBags[i] holds that rival's bags.
        """
        with self.lock:
            infos, bags = [], []
            for o in self.data["opponents"]:
                p = o["data"]
                infos.append({
                    "mPlayerDataID": p["PlayerDataBag"]["PlayerDataID"],
                    "mOpponentType": o.get("type", 0),
                    "mTriad": {},
                    "mRemainingSeconds": o.get("remaining_seconds", 86400),
                })
                bags.append(self.bags(p, mask))
            return infos, bags
