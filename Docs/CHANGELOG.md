# Changelog

## Unreleased (`finish-game`)

### Phase 8, part 2 — Nov 13 (game feel)
- **Camera shakes** (`KartFeedback`, perlin-noise shakes set up in C++, no assets): a jolt when your kart is hit (scaled by the damage), a rumble from explosions near you (reach and strength follow the blast size: bullets barely, rockets ~15 m, Sticky Bomb and death explosions harder, the Nuke ~45 m, capped), and a small kick when you fire (charge throws kick on release). A new hit restarts a shake instead of stacking.
- **Red screen flash** when your kart is hit (stronger for bigger hits, gone in about a quarter second, under the rest of the HUD).
- Settings: **Screen shake On / Off** (saved with your name and volume).
- Only the local player is affected; it runs from the existing health RepNotify and explosion multicasts, nothing new replicates.
- Tests: `SmashKarts.Rules.ExplosionShake` (11 rules tests pass); `SK.AutoTest` loop (arena), weapons, extras and tier3 (L_Test) pass. The weapons / status / tier3 / extras scenarios place karts at L_Test coordinates, so run them on `L_Test` (on the arena the bullet lane runs into the SW platform).

### Phase 7 — Nov 13 (online play with room codes, through EOS)
- **Host Online Game**: friends join with a 5-character **room code** instead of an IP address. No port forwarding, VPN, firewall rule or Epic account.
  New `UKartOnlineSubsystem` (game instance subsystem): logs in to Epic Online Services at startup with an EOS Connect **Device ID** (created once per Windows user, no login window); the host advertises an EOS session carrying the room code; joiners search for it and connect **peer-to-peer** through the engine's NetDriverEOS (NAT traversal, relays when needed).
  Codes use capitals and digits without the look-alikes 0 O 1 I L; a new code every time you host.
- Main menu: **Host Online Game** + **Host LAN / IP Game**; the join box takes a room code or an IP address; a status line at the bottom (connecting / ready / unavailable with a retry countdown / off). Clicking Host Online or joining with a code while it's still connecting waits and carries on by itself. Without online play the menu looks like before (one **Host Game** button).
- Lobby: **ROOM CODE  XXXXX** for everyone (new replicated `AKartGameState::RoomCode`, set by the host), "Creating your online room..." or the error for the host. The Tab scoreboard shows the room code too, so anyone can invite a friend mid-match.
- Messages: no game with that code, different game version (checked before connecting), can't reach the host's online game, online play unavailable. Login retries after 10 s, 30 s, then every 60 s; the login is refreshed before it expires.
- **Direct IP keeps working** with or without online play: IP hosts listen with `?bUseIPSockets`, and IP addresses pass through NetDriverEOS as plain UDP. NetDriverEOS only replaces IpNetDriver after a successful EOS login.
- Online play is **off** in the editor (PIE), with `-NoEOS`, and when `Config/Windows/WindowsEngine.ini` (gitignored) has no credentials. New format: `[EOSSDK.Platform.KartArena]` with the five portal values (`WindowsEngine.ini.example`, `Docs/EOS_SETUP.md`). Not using the OnlineSubsystemEOS plugin: on Windows it can't log in with a Device ID, so the subsystem talks to the EOS SDK through EOSShared.
- Version 0.6.0 (players on 0.5 get the version-mismatch message; rooms also refuse a different version before connecting).
- Plugins: EOSShared, SocketSubsystemEOS (Win64). `[EOSSDK] ProductName=KartArena` (the name the SDK reports to Epic; the editor's Fab plugin already started an EOS platform under the module name).
- Dev builds: `SK.HostOnline`, `SK.Join <code | ip>`, `SK.Online` (status), `SK.JoinLastRoom` + `-EOSNewDevice` (a second copy on the same PC gets its own EOS identity and joins the room this PC hosted last, so room codes can be tested on one machine).
- Docs: HOSTING.md (room codes first), HOW_TO_PLAY.md, EOS_SETUP.md (new file format, how to check it works).
- Tests: `SmashKarts.Rules.RoomCode` (all 10 rules tests pass). Without credentials: full match loop (`SK.AutoTest 2 quit`) passes, host and join by IP from the menu work with player names, Host Online / a room code explain that online play isn't set up. The online path itself needs the portal credentials (checkpoint 5).

### Practice bots — Nov 12
- **`AKartBotController`** (server-only AI): unarmed it drives to the nearest loot box it can reach; armed it hunts the nearest kart and uses the weapon properly: aims projectiles (wider cone for the Seeker), sets the throw charge from the distance, drops traps when chased, starts the Machine Gun, Tesla Coil, Mace Orbit and Invincibility up close, and never sits on a weapon for more than 8 s. It obeys countdown, freeze and results like a player and idles in the lobby.
- Steering: looks in 9 directions with a kart-sized sweep (open space first, then toward the target), slides along walls to the gaps, 3-point-turns out of corners, backs up when stuck and gives up on a target it keeps getting stuck on. In a 45 s test on `L_Arena_01` six bots drove 180–230 m each, picked up 20 weapons, fired 26 times and got stuck once.
- Bots get a PlayerState (name "Bot Blaze" etc., colour, score), show in the scoreboard ("bot" instead of a ping) and respawn like players. Bots + players never exceed 8; a joining player takes a bot's seat.
- Host: **+ Bot / − Bot** buttons in the lobby (works in every build). Dev builds: `SK.SpawnBots <n>`.
- Test: `SK.AutoTest 2 quit bots` (six bots, 45 s of play: every bot keeps driving, bots score, then they're removed cleanly).
- Fix found by the test: `AAIController` resets its control rotation to the pawn's every tick by default (`bSetControlRotationFromPawnOrientation`), which silently undid all bot steering.

### Phase 8, part 1 — Nov 12 (extras)
- **Nuke**: slow (1500 cm/s) big yellow missile; 100 on a direct hit, 8 m splash 50 → 10, huge explosion, big death explosion, strong knockback.
- **Fake Loot Box** (trap): dropped behind you. To everyone else it looks exactly like a loot box (same mesh, size, spin and "?" signs; the look is copied from BP_LootBox); you see yours green. 100 to whoever touches it, 5 to anyone within 1 m; lasts 60 s.
- **Invincibility** (buff, 5 s): golden shield bubble; no damage, no freezing, no popped tires; touching another kart smashes it (kill feed "Invincibility"); two invincible karts just bounce.
- Loot boxes: a "?" on every side (spins with the box) and a burst when picked up.
- Traps: optional splash damage, disguise (only the owner sees through it), cosmetic spin.
- Drop weights are 10 like every other weapon. The brief suggested making Fake Loot Box and Invincibility rare (1): lower `DropWeight` on their data assets in the editor if they show up too often.
- Assets: `sk_phase8_setup.py` (14 weapons in the loot pool and catalog). Test: `SK.AutoTest 2 quit extras` (Nuke kill, Invincibility blocks damage and freezing, smashes on touch and runs out, Fake Loot Box uses the loot box mesh and kills).

### Checkpoint 4 — Nov 12
- Passed: first internet playtest with friends on `L_Arena_01` (v0.5 packaged build): joining, all tier 3 weapons, pit knock-ins, ramps and kicker, two full matches with Play Again.

### Phase 6 — Nov 12 (arena blockout)
- **`L_Arena_01`** (built by `Tools/EditorScripts/sk_build_arena.py`, re-runnable): 56 × 56 m inside 6 m walls. Open centre inside a broken ring of 2.5 m cover walls (5 m gaps = choke points for mines and Spike Strips); the ring route around it.
  NE and SW: 8 × 8 m platforms, 1.5 m high, two 18° ramps each (loot box on top). NW and SE: 6 × 6 m pits with 50 cm red curbs: you can't drive in (step height 30 cm), but knockback can throw you in (KillZ −500, falls credit the last attacker).
  Cover blocks north, south and west; a kicker ramp on the east lane that jumps toward the NE platform. 10 PlayerStarts around the edge facing the centre, 9 loot boxes (centre, the four ring gaps, both platforms, next to both pits).
  Lighting like L_Test (sun, sky atmosphere, real-time sky light, height fog) + an unbound post-process volume. Engine grid floor and seven flat colours (`Arena/Materials`); dressing with human-made packs comes when they're in the project.
- **`SK.AutoTest 2 quit arena`**: every PlayerStart is clear of geometry and on the floor, every loot box is on the floor, and the host kart is driven up all four ramps (climbs 141 cm) and over the kicker. The full match loop also passes on the arena.
- Hosting, the server default map and the editor startup map now use `L_Arena_01`. Version 0.5.0 (players on 0.4 get the version-mismatch message).
- Packaged Development build (zip 458 MB): the arena self-test and the menu flow (join by name from the main menu, full match, host leaves, wrong address) pass with the packaged exe; Host Game opens `L_Arena_01`.

### Phase 5 — Nov 12 (tier 3 weapons)
- **Mace Orbit** (buff, 6 s): 5 spiked maces orbit the kart at 150 cm; anyone they reach is smashed (100, kill feed "Mace Orbit"); +25 % speed, −10 % damage taken (from phase 2).
- **Tesla Coil** (buff, 5 s): zaps every enemy kart within 500 cm that it can see, right away and then every 0.5 s (10 zaps max, 10 damage each); flickering blue aura, a faint ring on the ground showing the range, jagged beams to each target.
- **Snowball** (3 throws): straight, 2500 cm/s, 20 m range, 7 damage, freezes for 3 s (ice block on the kart); ram a frozen kart to smash it.
- **Spike Strip** (trap): dropped behind you, 300 cm wide; pops the tires of each enemy that crosses it (until they respawn, sparks under the kart) for 10 damage, and stays for 30 s.
- New `ABuffWeapon` (weapon archetype D): one press applies a status for a duration, optional aura zaps; the weapon stays held until the buff ends. Machine Gun and buffs show **"Active: 4.2 s"** on the HUD (replicated end time on the weapon).
- New `UKartStatusFXComponent` on the kart: ice block, maces, Tesla aura + range ring, shield bubble (ready for Invincibility), popped-tire sparks, zap beams. Follows the replicated status flags, so every machine shows the same; uses engine meshes/materials set on BP_Kart.
- Traps: optional box trigger, "stays after catching" mode, status on trigger, decoration row (spikes). Default owner colour: **your own traps are green, everyone else's red** (Mines now too: BP_Mine got a material with a colour parameter).
- Trap drop now finds floors of any collision type (L_Test's floor is WorldDynamic).
- HUD: "Picked up: <weapon>" for 2 s.
- Assets (`sk_phase5_setup.py`): DA/DT/BP for the four weapons (drop weight 10 each, like the rest), loot box pool and weapon catalog = 11 weapons, mace contact smashes use `DT_MaceOrbit`.
- Tests: `SmashKarts.Rules.AuraZapCount`; `SK.AutoTest 2 quit tier3` (Snowball freeze + ram smash, Mace Orbit smash and timeout, Tesla zaps in range / none out of range, Spike Strip pops tires and stays) passes with 60 ms lag; the client log shows the statuses and zap beams arriving. Loop, status and weapons scenarios still pass.

### Checkpoint 3 — Nov 12
- Passed: packaged build, two windows on one PC and two PCs on a LAN (menus, lobby, match, pause/settings, leave/rejoin, host-left message, saved name).

### Phase 4 — Nov 12 (menus + direct IP)
- **Main menu** (`L_MainMenu`, `AKartMenuGameMode`): name (saved), Host Game, Join Game by IP (last address remembered), Settings, Quit, and the last connection message.
- **Settings**: master volume, resolution, window mode, quality preset (GameUserSettings), controls list. Same panel in the pause menu.
- **Game instance**: host = listen server on the arena map; join = `ClientTravel` with our `?SKName=` option; leave; errors become menu messages: "The host left the game", "Couldn't connect…" (15–20 s instead of 60), "That game is full", version mismatch. Tested headless (join from the menu with a name, host quits, dead port).
- **Lobby overlay**: players with colours + host badge, Start Match and 2/3/5-minute buttons for the host, the host's addresses with hints (LAN / Radmin / ZeroTier).
- **Pause menu** (Esc or P): Resume, Settings, Leave Match, End Match (host); kart ignores keys while it's open. **Results** buttons: Play Again, Back to Lobby (host), Leave. Cursor shows in lobby / results / menu only.
- **Name tags** (name + health bar, hidden on your own kart) and **floating damage numbers** for the attacker (machine-gun hits merge into one number).
- Config: main menu is the game's default map, `ArenaMapOverride` (L_Test until the arena exists), `KartGameInstance`, ray tracing off, window title, maps to cook + always cook `/Game/SmashKarts`.
- `Docs/HOSTING.md` (LAN, Radmin VPN, ZeroTier, port forwarding, firewall, troubleshooting).
- **First packaged build** (Development, Win64, ~1 GB, zip 504 MB): tested headless with the packaged exe: host + client joining from the main menu by name, full scripted match, host quits (client shows "The host left the game"), dead address ("Couldn't connect"). Fixed a unity-build name clash and two log errors found in the packaged run. `Docs/HOW_TO_PLAY.md`, `Play_Windowed.bat`.

### Checkpoint 2 feedback — Nov 12
- **Cannon Balls**: no sideways fan; the 4 balls fly straight ahead and land in a line 350 cm apart, the first ball farthest (new `VolleyLineSpacing`, speed per ball from flat-ground ballistics, unit-tested).
- **Bullets**: one press fires everything: 6 rounds, 0.12 s apart, of 3 bullets side by side (Uses 1, BurstCount 6, ProjectilesPerShot 3).
- **Drop weights**: every weapon 10 (equal chance). Setup scripts no longer push the old design weights.

### Phase 3 — Nov 12 (tier 2 weapons)
- **Bullets**: 9 presses × burst of 3, slight spread, 6000 cm/s, direct damage 7 → 4 over 4000 cm (new range falloff), small splash.
- **Seeker Missile**: flies straight 0.5 s, then homes on the nearest enemy in a 90° cone within 3500 (replicated target so clients home too; retargets if the target dies).
- **Cannon Balls**: charge throw (hold Space, 800–3000 cm/s), volley of 4 fanned over 30°, lobbed; 100 direct, 425 splash 45→5.
- **Sticky Bomb** (original name for the lob-and-stick bomb): charge throw; sticks to floor, walls or karts (100 to a kart it lands on, then rides along); explodes 0.5 s after an enemy comes within 400, or after a 5 s fuse; 800 splash 80→20; big death explosion.
- Charge throw: `Started`/`Completed` on IA_Fire, charge measured locally, sent as 0–1 and clamped on the server; HUD charge meter.
- Weapon data: `HudColor`, launch pitch, volley fan, range falloff, charge, homing, on-hit status (for Snowball), sticky fields. HUD: generated weapon icon (colour + initials).
- Assets (`sk_phase23_setup.py`): BP_Bullet / BP_SeekerMissile / BP_CannonBall / BP_StickyBomb, DT_* for each, DA_* with the design numbers, loot box pool and weapon catalog = all 7 weapons.
- **Fix: projectiles passed through floors/props of object type WorldDynamic** (L_Test's floor uses BlockAllDynamic), because they ignored WorldDynamic to avoid hitting each other. New `KartProjectile` object channel (`ECC_GameTraceChannel1`, DefaultEngine.ini): projectiles ignore each other but hit all geometry.
- Seeker tuning from a simulation of the engine's homing math: acceleration 16000 (8000 orbited the target) plus a 150 cm proximity fuse (counts as a direct hit); hits 36/36 cases across the 90° cone at 5–35 m.
- `SK.AutoTest <n> quit weapons`: fires Bullets, an off-axis Seeker, a charge-thrown Sticky Bomb and a Cannon Ball volley at a client and checks the damage.

### Phase 2 — Nov 12 (status effects + knockback)
- Movement reads replicated status flags in the CMC simulation: **Frozen** (no input, speed bleeds to 0, icy body tint), **TiresPopped** (grip 0.3, speed ×0.6), **Spiky** (speed ×1.25). Reverse speed capped at 50 % of forward (pulled in from phase 8).
- Damage: Spiky takes ×0.9 (contact smashes ignore it); Invincible / spawn-protected take nothing; Invincible karts can't be frozen.
- **Knockback** from damage types (`KnockbackStrength`/`KnockbackUpward`, scaled by splash falloff): the server launches the kart and the owning client predicts the same launch.
- **Wall bounce**: head-on wall hits bounce back 30 % of the impact speed (inside the CMC move, so it's predicted). Kart-to-kart bumps unchanged.
- **Contact smash** (Invincible > Spiky > normal): Spiky reach 200 cm, Invincible on touch, two Spiky karts smash each other, ramming a frozen kart (≥300 cm/s) smashes it; 0.5 s cooldown per pair; `DT_Smash`.
- Player colours on the kart body (BasicShapeMaterial `Color` parameter), pulled in from phase 4.
- HUD status line (FROZEN 2.1s, TIRES POPPED, …). Dev tools: `SK.AutoDrive`, `SK.AutoTest <n> quit status`, client correction counter in the log (`[Net] Correction #`).
- Build/script helpers refuse to run while the editor is open (UBT had silently built a hot-reload DLL into an open editor).

### Phase 1 — Nov 12 (core loop)
- `AKartGameMode` (AGameMode): lobby (`WaitingToStart`, drive around, no loot) → `Countdown` (custom state, karts frozen) → `InProgress` (timer) → `WaitingPostMatch` (results, auto lobby after 12 s) → play again / back to lobby. Host-only actions are checked on the server.
- Eliminations: 0 health or falling below KillZ → kart hidden/disabled, weapon and statuses cleared, death FX on every machine; killer +1 score/elims, victim +1 deaths; falls credit the last attacker within 5 s; no scoring outside a match.
- Respawn after 3 s at the PlayerStart farthest from living enemies (random among the best 3, occupied spots avoided); fresh kart with full health and 2 s spawn protection (blinks; firing ends it). Late joiners spawn straight in at 0.
- `AKartGameState` (timestamps from server time, sorted standings, palette, elimination multicast), `AKartPlayerState` (eliminations, deaths, join order, unique colour, host flag, respawn time), `AKartPlayerController` (host RPCs, runtime UI input: Tab / Enter / Backspace, debug RPC).
- `UKartStatusComponent`: replicated status bitmask + end times (Frozen, TiresPopped, Spiky, Invincible, Tesla, SpawnProtected); damage gate in `TakeDamage`.
- `UKartMovementComponent::ConstrainInputAcceleration` zeroes throttle while input-locked (same rule on server and client).
- HUD built in C++ (UMG tree): timer/phase, score + rank, health bar, weapon + uses, kill feed, banners, countdown/GO, respawn + spawn-protection text, controls hint; scoreboard on Tab; results screen.
- `UKartDamageType` + `DT_Rockets/Mines/MachineGun` for kill-feed names; projectile/trap/kart FX hooks now have Niagara defaults (`NS_Explosion`).
- Loot boxes only give weapons during a match; hidden in the lobby; reset at match start. Turret ignores eliminated karts. Debug Warnings removed (now Verbose).
- Debug commands (dev builds): `SK.GiveWeapon`, `SK.Damage`, `SK.Kill`, `SK.SetStatus`, `SK.EndMatch`, `SK.MatchTime`, `SK.StartMatch`, `SK.Lobby`, `SK.State`, `SK.ShowNet`, `SK.AutoTest`.
- Tests: `SmashKarts.Rules.*` automation tests (weighted roll, standings sort, time format, damage falloff, spawn pick, name sanitising) and the scripted listen-server match test `SK.AutoTest`.
- Assets (`sk_phase1_setup.py`): BP_KartGameState/PlayerState/PlayerController/HUD, BP_KartGameMode reparented and wired, weapon catalog, drop weights 14/6/20, display names, L_Test KillZ −1408 and 6 PlayerStarts.

### Phase 0 — Nov 12
- Enabled Python Editor Script Plugin, Editor Scripting Utilities and Niagara in `SmashKarts.uproject`; restricted Unreal MCP (ModelContextProtocol) and Python/scripting plugins to Editor targets.
- Added UMG, Slate, SlateCore, Niagara module dependencies.
- Added `Docs/IMPLEMENTATION_PLAN.md`, this changelog and `Tools/EditorScripts/`.
