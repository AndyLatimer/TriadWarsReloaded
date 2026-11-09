# Triad Wars local server

A stand-in for the Triad Wars online services so the 1.6.0.0 client
(`TriadWars.exe`) can run on one machine with no network backend.

## Running

Double-click `Play-TriadWars.bat` in the game folder. It starts this server
if it is not already running, then starts the game.

To run the pieces by hand:

    cd LocalServer
    python server.py            # listens on https://127.0.0.1:443
    ..\TriadWars.exe

Requirements: Python 3.9 or newer on `PATH` (standard library only), and
port 443 free.

## What was changed in the game folder

| File | Change |
| --- | --- |
| `UserOptions.dat` | `Environment` switched from `prod.services.triadwars.com` to `localhost`. The original is kept as `UserOptions.dat.orig`. |
| `Play-TriadWars.bat` | New launcher. |
| `LocalServer\` | New folder (this server). |

`TriadWars.exe` itself is not modified, unless the optional crash patch below
has been applied. To undo everything:
`python LocalServer\useroptions.py restore`, then delete `LocalServer` and
`Play-TriadWars.bat`.

### Optional: shadow-crash patch for `TriadWars.exe`

About one launch in seven the client crashes a few seconds after the world
loads (access violation at `TriadWars.exe+0x548a`; Windows reports it as
offset `0x1b81b7`). It is a race inside the client, not something a server
reply causes: the spot-shadow pass draws a character whose model has not
finished loading. `tools\patch_exe.py` adds the missing null check:

    python LocalServer\tools\patch_exe.py status
    python LocalServer\tools\patch_exe.py apply      # keeps TriadWars.exe.orig
    python LocalServer\tools\patch_exe.py restore    # game must be closed

It changes 30 bytes, refuses to touch any executable other than the original
1.6.0.0 build, and can be applied while the game is running (it takes effect
on the next launch).

## Files

| File | Purpose |
| --- | --- |
| `server.py` | HTTPS + WebSocket plumbing, request log, routing. |
| `handlers.py` | One function per API endpoint. Edits are picked up without restarting. |
| `state.py` | Player/world model and the JSON encodings the client expects. |
| `state.json` | Your saved profile (created on first run). Delete it to start over. |
| `data/gamedb_min.json` | Object UIDs and tuning tables extracted from the game's own data. |
| `data/store_items.json` | What the clothing, outfit, favour-pack and pack-bundle stores sell (see `tools/store_items.py`). |
| `useroptions.py` | Reads/edits the obfuscated `UserOptions.dat`. |
| `cert.pem`, `key.pem` | Self-signed certificate for `localhost` (the client does not verify it). |
| `logs/` | One request log per server run. Lines marked `UNHANDLED` are endpoints with no handler yet. |
| `tools/` | The scripts used to unpack the game data and decode its schema, crash-analysis helpers, `smoke_test.py`, `patch_exe.py` (see above) and `make_bundle.py`. |
| `devdata/` | Reference data for extending the server: the full object-database export and the client's endpoint and key lists. Not needed to play. |

## How it works

The client builds every service URL as
`https://<Environment>/<service>/api/v1/<method>`, where `<Environment>` comes
from `UserOptions.dat` (XOR-obfuscated with a fixed 33-byte key). It logs in by
POSTing `{ "username", "password" }` to `login/api/v1/acceptcredentials` and
keeps the `connect.sid` cookie; with no credentials on the command line it uses
`guest` / `nopassword`. It disables TLS certificate checks, so a self-signed
certificate is accepted.

Player data travels as "data bags", gated by the `mRequestedBags` bitmask.
Where the bag object sits depends on the endpoint:

* `playerdata/serializedplayerdata/...` replies with the bag object as the
  **root** of the response (no wrapper).
* Action endpoints (name change, turf selection, raids, turf management, ...)
  nest it under `mDataBags`, next to `mReturnCode` / `mMessage`.
* `playerdata/opponents` carries one bag object per rival in the root-level
  array `mOpponentDataBags`.

Getting this wrong fails silently: the client builds a blank player record
(no name, no turf) and carries on until something needs the player's turf.
The first-raid results screen is one such place; it stays empty and its
Continue prompt does nothing.

The bags:

| Bit | Bag | Served |
| --- | --- | --- |
| 0x0001 | PlayerDataBag | yes |
| 0x0002 | CustomizationBag | yes, once the character has been customised |
| 0x0004 | InformantsBag | no |
| 0x0008 | FavoursBag | yes (array of favour instances) |
| 0x0010 | ActiveFavoursBag | no |
| 0x0040 | CurrencyBag | yes |
| 0x0080 | TurfBag | yes |
| 0x0100 | InventoryBag | yes (array of owned store items) |
| 0x0200 | ShieldBag | no |
| 0x0400 | TrialsBag | no |
| 0x0800 | ExportRewardsBag | no |
| 0x1000 | EnforcerUpgradesBag | yes (array of `{Item1: upgrade uid, Item2: slot}`) |

The reply only sets the bits for bags it actually contains, which the client
accepts.

Encoding rules that matter (getting these wrong reads as zero, or crashes):

* JSON keys are the reflected field names from `Reflection.rdb`; unknown keys are ignored.
* 32-bit integers, floats and booleans are plain JSON values.
* Every 64-bit value (ids, object UIDs, handles, face, money) is a decimal **string**.
* Symbols such as `LocationName` are strings; the client hashes them itself.
* A top-level reply must be an object unless the endpoint is known to take an array.
* `mReturnCode` 0 means success.

### Turf replies

Every `turf/*` endpoint shares one client callback and one reply shape:
`mReturnCode`, `mMessage`, `mActionType`, `mRacket` (the racket's fields),
`mRacketTypeUID` at the **root** (read only when `mRacket` is present),
timers, and `mDataBags`. Two details matter:

* If the root `mRacketTypeUID` is missing, the result is tagged as racket 0
  and the panel that asked stays on its loading spinner.
* `mActionType` decides what the panel does next and whether a tutorial step
  advances. Values confirmed from the client's result listener: `1` move to
  warehouse (shows the "units moved/sold" message), `4` remove racket (shows
  "racket destroyed"), `8` racket info, `9` safehouse info (the only value
  that advances the "select your safehouse" tutorial step). Inferred from how
  the listener groups the remaining values, not confirmed: `2` start
  construction, `3` upgrade racket, `5`/`6`/`7` upgrade safehouse / warehouse
  / temple, `10` warehouse info, `11` temple info.

The safehouse, warehouse and temple must carry `BlockIndex` -1. They are not
on the block grid, and any grid index places their mouse-pick boxes over that
grid lot, so they cannot be selected.

### Turf sites

The world has seven turf sites, `Turf-Turf00001` to `Turf-Turf00007`. The
client looks up the owner of site *n* as `mOpponents[n-1]`, so the server
always supplies seven rivals, with their bags in the parallel root-level array
`mOpponentDataBags`.

## What is emulated

Working, and exercised in play (intro, tutorials, first open-world raids, turf
intro, shops). `..\HANDOFF.txt` has the setup steps and the current list of
what works and what does not.

* login, heartbeat, logout, the messaging WebSocket handshake
* player profile, currency, turf and customization bags; rivals
* save game and gameplay-state storage (persisted in `state.json`)
* name change, turf site selection, character customization
* XP gain, leaderboards, profiles, telemetry sinks
* raids (start, timer, end and rewards); using favours and equipping upgrades
* turf: building info, racket construction, safehouse upgrade
* the wardrobe and favour shops and their purchases (below)

Implemented from the client's response parsers but not yet exercised in play
(values are simple approximations of the original server's rules):

* death penalty, money laundering, racket removal, warehouse/temple upgrades
* export missions, world encounters, crime scenes

Details that matter (each one was a hang or a crash before it was right):

* contacts (informants): `informants/buyfrominformanttype/<uid>` grants the
  contact's first favour and names it in a root-level `FavourDefinitionUID`.
  The turf intro's "speak with your contact" step waits for the favour whose
  database object is named `FTPE_01` (given by contact `FTPE_Informant_01`).
  The two tutorial contacts make no server request at all: the client just
  re-reads the InventoryBag afterwards and expects the car (Ridge Sport) and
  the Cleaver to be in it, so new profiles start with both.
* `blackmarket/quotes` must reply with a bare array of quotes; an object
  crashes the client.
* `playerdata/gainxp` must return `mCurrentXP` as a string; a number is read
  as 0 and sends the level-up screen into a loop.
* `raid/endraid` must list at least one favour in `mFavoursAwarded` (favour
  definition UIDs as strings) for a successful raid. The first open-world raid
  shows its results in tutorial mode, which only offers Continue after the
  favour cards; with none the screen never unlocks.

* `commerce/store/<uid>` for the Wardrobe Store and the Outfit Store returns
  every finished clothing item / outfit in the game data (`data/store_items.json`,
  built by `tools/store_items.py`) at a flat approximate price in dirty money.
  The reply is `mReturnCode`, `mMessage` and a bare array `data` of item
  records; `mCortexUID` must be an unsigned decimal string. These two stores
  must not be empty: the wardrobe screen then shows its "empty" state without
  building its category tree and the next button press crashes the client
  (null read at `TriadWars.exe+0x6fcc48`).
* The favour-pack stores (the regular "Favor pack - Pack Store" and the three
  tutorial pack stores) list favour packs at a flat approximate price in clean
  money. They must not be empty either: the favour shop builds its large tile
  from the first pack without checking that one exists
  (`TriadWars.exe+0x72240f`). Which pack belongs in which tutorial store is
  hard-coded in the client (func `140654510`).
* Each pack store is paired with a bundle store (the regular "Favour Pack
  Bundle Store" and three tutorial ones; pairing hard-coded in func
  `1407100c0`). Selecting a pack opens a details screen that searches the
  bundle store for `UFG::StoreBundle` items made of that pack (1x, 3x, 5x).
  The bundle is what gets bought, and the 1x bundle supplies the price shown;
  if it is missing the client crashes (`TriadWars.exe+0x6a34ec`).
* `game/purchase/<store uid>/<item uid>` for the stores above takes the price.
  Clothing and outfits are added to the InventoryBag. Packs are opened on the
  spot and pay out six upgrade favours each (what the real packs held was
  decided by the original server). The reply nests the bags under
  `data.mDataBags`, next to `data.CortexUID`; for a bundle, `data.ItemContext`
  holds one `{"StoreItemIDStr": pack uid, "FavourResults":
  [{"FavourDefinitionUIDString": uid}, ...]}` record per pack.

Answered with an empty but valid result:

* the other stores (the hub's general "Bundle Store", vehicles, health, gold,
  two unused tutorial stores), coupons and their purchases; black market
  trades; favour sales and informants
* errands, operations, trials; notices and private messages
* player-run triads (clans) and friends, which need other players

Not emulated: the separate "bombd" game-server layer used for multiplayer
sessions. Everything here is single-player.

## Problems already solved

All fixed in the current server. Listed so the family is recognisable.

| Symptom | Cause |
| --- | --- |
| Blank player; first-raid results screen empty and stuck | Data bags wrapped wrongly (root vs `mDataBags`, see above). |
| Values read as zero | 64-bit values must be unsigned decimal strings; a leading minus reads as 0. The client itself sends some UIDs as negative numbers; convert with `int(x) % 2**64`. |
| Wrong or no rival at a turf site | Site *n*'s owner is rival *n-1*; seven rivals must always be sent, in order. |
| Safehouse could not be selected | Safehouse, warehouse and temple need `BlockIndex` -1. |
| No build points | Empty lots were listed as rackets and counted against the limit. |
| Turf panel stuck loading; "select your safehouse" never advanced | Turf replies need `mRacketTypeUID` at the root and the right `mActionType` (9 for safehouse info). |
| "Speak with your contact" never completed; no car or cleaver | The contact's favour must be granted; the two tutorial items are expected to already be in the inventory, with no request made. |
| Raid results crashed | Face-tier table must have exactly ten tiers. |
| Level-up screen looped | `mCurrentXP` must be a string. |
| First open-world raid results never offered Continue | A successful raid must award at least one favour. |
| Using a favour crashed the UI | The reply must name the favour's definition UID. |
| Black market crashed | `blackmarket/quotes` must be a bare array. |
| Outfits crashed (`+0x6fcc48`) | Clothing/outfit stores were empty. |
| Favour shop crashed (`+0x72240f`) | Pack store was empty. |
| Selecting a pack crashed (`+0x6a34ec`) | The pack's 1x bundle was missing from the paired bundle store. |
| Game vanished mid-play with no crash dump | A debugging script force-closed it on a timeout. Not a game fault; never `taskkill` the game. |

## Filling another store

1. `devdata\gamedb.json` has every `UFG::Store` (17) and every store item
   type. A store accepts one item type, named in `mAllowedStoreItemType`.
2. `python tools\findimm.py <store uid>` shows where the client hard-codes
   that store, which tells you which screen uses it. Read that screen's code
   for what it assumes about the items before choosing them.
3. Add the store to `tools\store_items.py` and run it (it rewrites
   `data\store_items.json`), add a price for the item type to `STORE_PRICES`
   in `handlers.py`, and make `purchase` do something sensible for it.
4. Extend `tools\smoke_test.py` if the client pairs the store with another
   one, as it does for packs and bundles.

To regenerate `devdata\gamedb.json` from the game's own files (not re-run
since it was first produced):

    cd devdata
    python ..\tools\qcmp.py unpack ..\..\Global.big     # writes unpack\Global_<offset>.bin
    python ..\tools\gamedb.py export gamedb.json        # reads unpack\Global_0279af88.bin

## Extending

Play with the server window visible and watch for `UNHANDLED` lines; each one
names an endpoint that needs a handler in `handlers.py`. Request and response
bodies are in `logs/`. `tools/keys.py` lists the JSON keys (and their types) a
given endpoint's client-side parser reads.
