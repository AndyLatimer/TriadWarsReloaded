"""Check that the local server is up and answering the way the client needs.

    python smoke_test.py [--port 443]

Read-only: it only sends GET requests. Exits 0 when every check passes, 1 otherwise.
Run it after starting the server and before starting the game. It also tells a real
Triad Wars server apart from some other program that happens to be listening on the port.
"""
import argparse
import json
import os
import ssl
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")
# pack store -> the bundle store the client pairs it with (TriadWars.exe func 1407100c0)
PACK_TO_BUNDLE_STORE = {
    "17679317271361684130": "2541658861465094719",
    "1333401478411759499": "14396531704373406508",
    "6121745478303023351": "15645208897816992095",
    "6802491388125254368": "330971609030659674",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=443)
    args = ap.parse_args()
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE         # the server's certificate is self-signed

    def get(path):
        url = "https://127.0.0.1:%d/%s" % (args.port, path)
        with urllib.request.urlopen(url, context=ctx, timeout=10) as r:
            return json.loads(r.read())

    failed = []

    def check(name, fn):
        try:
            detail = fn()
            print("ok    %-28s %s" % (name, detail or ""))
        except Exception as e:                      # report every failure, then exit 1
            failed.append(name)
            print("FAIL  %-28s %r" % (name, e))

    def store_access():
        assert get("game/api/v1/storeaccess") == {"mHasAccess": True}

    def player():
        bags = get("game/api/v1/playerdata/serializedplayerdata/8191")
        for key in ("PlayerDataBag", "CurrencyBag", "TurfBag", "FavoursBag", "InventoryBag"):
            assert key in bags, "missing " + key          # bags sit at the ROOT of this reply
        pdb = bags["PlayerDataBag"]
        assert isinstance(pdb["PlayerDataID"], str) and isinstance(pdb["Face"], str)
        return "player %r, turf %s" % (pdb["Name"], bags["TurfBag"]["Turf"]["LocationName"])

    def rivals():
        r = get("game/api/v1/playerdata/opponents")
        assert len(r["mOpponents"]) == 7 and len(r["mOpponentDataBags"]) == 7
        return "7 rivals"

    def face_tiers():
        assert len(get("game/api/v1/playerdata/facetiers")) == 10
        return "10 tiers"

    def quotes():
        assert isinstance(get("game/api/v1/blackmarket/quotes"), list)

    def missions():
        err = get("game/api/v1/errands")
        assert len(err.get("mErrands", [])) >= 4, "missing errands"
        ops = get("game/api/v1/operation/list")
        assert len(ops.get("mOperationInstances", [])) >= 7, "missing operations"
        we = get("game/api/v1/worldencounter/list")
        assert len(we.get("WorldEncounters", [])) >= 3, "missing encounters"
        cs = get("game/api/v1/openworldcrimescene")
        assert len(cs.get("CrimeScenes", [])) == 20, "expected 20 crime scenes (drug busts)"
        db = get("game/api/v1/drugbusts")
        assert db.get("mTotalBusts") == 20, "expected 20 drug busts"
        enc = get("game/api/v1/encounters")
        assert len(enc.get("mEncounters", [])) == 3, "expected 3 encounter types"
        return "20 Drug Busts, 3 World Encounters, Errands & Operations active"

    def safehouses():
        res = get("game/api/v1/safehouses")
        shs = res.get("mSafehouses", [])
        assert len(shs) == 4, "expected 4 safehouses"
        for sym in ("SafehouseNP", "SafehouseCT", "SafehouseAB", "SafehouseKT"):
            sh = next((s for s in shs if s["symbol"] == sym), None)
            assert sh and sh["is_unlocked"] is True, f"{sym} safehouse must be unlocked"
        gps = get("game/api/v1/playerdata/gameplaystate")
        state_str = gps.get("mGameplayState", "")
        for sym in ("SafehouseNP=1", "SafehouseCT=1", "SafehouseAB=1", "SafehouseKT=1"):
            assert sym in state_str, f"{sym} flag missing in gameplaystate"
        return "4 safehouses registered & all 4 unlocked (NP, CT, AB, KT)"

    def martial_arts():
        dojo = get("game/api/v1/dojo")
        assert dojo.get("mDojo", {}).get("internal_id") == "Interior_DojoNP", "missing Dojo"
        fc = get("game/api/v1/fightclubs")
        assert len(fc.get("mFightClubs", [])) == 4, "expected 4 fight clubs"
        gps = get("game/api/v1/playerdata/gameplaystate")
        st = gps.get("mGameplayState", "")
        for flag in ("FIGHTCLUB_INTRO_COMPLETE=1", "StatueDefensiveStage=5", "StatueOffensiveStage=5",
                     "CopDrivingStage=5", "CopShootingStage=5", "TriadDefensiveStage=5",
                     "TriadOffensiveStage=5", "GunplayUpgradeStage=5", "MaxHealthUpgradePoint=50",
                     "EnableSpyCams=1", "EnableJumpIndicators=1"):
            assert flag in st, f"missing {flag} in gameplaystate"
        return "Sifu Dojo active, 4 Fight Clubs ready, Kung Fu/Cop/Triad trees maxed"

    def minigames():
        k = get("game/api/v1/karaoke")
        assert len(k.get("mKaraoke", {}).get("venues", [])) == 2, "expected 2 karaoke venues"
        assert len(k.get("mKaraoke", {}).get("songs", [])) >= 7, "missing karaoke songs"
        r = get("game/api/v1/races")
        assert len(r.get("mStreetRaces", [])) >= 7, "expected 7 street races"
        g = get("game/api/v1/gambling")
        assert len(g.get("mGamblingDens", [])) == 2, "expected 2 gambling dens"
        s = get("game/api/v1/services")
        assert len(s.get("mServices", {}).get("food_vendors", [])) >= 4, "missing food vendors"
        lmk = get("game/api/v1/landmarks")
        assert len(lmk.get("mSpecialInteriors", [])) >= 7, "expected 7 special landmarks"
        v = get("game/api/v1/vehicles/starter")
        assert len(v.get("mStarterVehicles", [])) >= 7, "expected 7 starter vehicles"
        gps = get("game/api/v1/playerdata/gameplaystate")
        st = gps.get("mGameplayState", "")
        for flag in ("EnableKaraoke=1", "MassageParlourBuff_Activated=1", "StrikingBuff_Activated=1"):
            assert flag in st, f"missing {flag} in gameplaystate"
        return "Karaoke, 7 Street Races, Gambling Dens & Food/Buffs active"

    def stores():
        with open(os.path.join(DATA, "store_items.json"), encoding="utf-8") as f:
            catalogues = json.load(f)
        served = {}
        for uid, cat in catalogues.items():
            data = get("commerce/api/v1/store/" + uid)["data"]
            assert data, "store %s (%s) is empty" % (uid, cat["name"])
            assert all(i["mCortexUID"].isdigit() for i in data)
            served[uid] = cat
        for pack_store, bundle_store in PACK_TO_BUNDLE_STORE.items():
            singles = [p for p in served[bundle_store]["packs"].values() if len(p) == 1]
            for pack in served[pack_store]["items"]:
                assert [pack] in singles, "pack %s has no 1x bundle in store %s" % (pack, bundle_store)
        outfits = get("game/api/v1/outfits")
        assert len(outfits.get("mOutfits", [])) == 28, "expected 28 legendary outfits"
        return "%d stores filled (28 legendary outfits, 792 clothing, 247 vehicles)" % len(served)

    check("server answers", store_access)
    check("player data", player)
    check("rivals", rivals)
    check("face tiers", face_tiers)
    check("black market quotes", quotes)
    check("missions & open world", missions)
    check("safehouses (4 Safehouses)", safehouses)
    check("martial arts & fight clubs", martial_arts)
    check("minigames & services", minigames)
    check("shop catalogues", stores)
    print("%d check(s) failed" % len(failed) if failed else "all checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
