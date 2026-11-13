# Changelog

## Unreleased (`finish-game`)

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
