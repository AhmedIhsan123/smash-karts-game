# Changelog

## Unreleased (`finish-game`)

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
