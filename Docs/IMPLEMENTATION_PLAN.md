# Implementation Plan — kart arena FFA (UE 5.8.4, C++)

Branch: `finish-game` · Deadline: **Nov 30, 2026** · Started: Nov 12, 2026
Status key: ✅ done · 🔨 in progress · ⏳ next · 🛑 checkpoint (stop for a human test)

## Recon findings (Nov 12) — differences from the brief, section 3
| # | Finding | Action |
|---|---|---|
| 1 | Engine 5.8.4 confirmed at `C:\Program Files\Epic Games\UE_5.8` (CL 58902052). | — |
| 2 | Extra asset `Weapons/MachineGun/BP_TurretWeapon` (BP child of `ATurretWeapon`, holds the gun mesh). Not in the brief. | Keep. DA_MachineGun points at it. |
| 3 | Uncommitted `.uproject` change enabled **ModelContextProtocol** (Unreal MCP). It runs on `127.0.0.1:8000/mcp` but only exposes the Agent Skills toolset. It is `NoRedist` and has Runtime modules. | Kept, restricted to `Editor` targets so it never ships. Not used for automation; headless Python is used instead. |
| 4 | `[Health]` log is `Log` verbosity, not a Warning. `[Fire]` and `[LootBox]` are Warnings. | Warnings removed in phase 1 (now `Verbose`). |
| 5 | `bEnableLegacyInputScales=True`: `AddControllerYawInput` is multiplied by the PlayerController's legacy yaw scale (2.5), so `TurnRate 40` ≈ 100°/s. | **Do not change** the PC yaw scale or this flag; the tuned feel depends on it. The new `AKartPlayerController` keeps the engine defaults. |
| 6 | DefaultEngine.ini: Lumen GI/reflections, **hardware ray tracing on**, Substrate, VSM, DX12 SM6 only. Heavy for friends' PCs and slow to cook. | Phase 4 (before the first package): `r.RayTracing=False`, and a quality preset menu. Lumen stays. |
| 7 | No GameMode/State/PlayerState/PC/HUD/menu/session code exists. `BP_KartGameMode` is a `GameModeBase` child (DefaultPawn = BP_Kart). | Built in phase 1. BP_KartGameMode is reparented to `AKartGameMode`. |
| 8 | No Python/editor-scripting plugins enabled. | Enabled (editor-only) in phase 0. |
| 9 | No system Python. | Scripts run inside the editor (commandlet). The engine's bundled Python is at `Engine/Binaries/ThirdParty/Python3/Win64`. |

## Naming (player-facing)
Gameplay follows the brief. Player-facing names are original: the game title is a placeholder until you pick one,
**"Lob-Grenuke" → "Sticky Bomb"**, **"Spikey-Go-Round" → "Mace Orbit"**. All other weapon names are generic.
The internal module/repo name `SmashKarts` stays (renaming a UE module mid-project is costly).

## Workflow
- Build: `Build.bat SmashKartsEditor Win64 Development -Project=... -WaitMutex -FromMsBuild` (editor must be closed).
- Assets: idempotent Python scripts in `Tools/EditorScripts/`, run headless with
  `UnrealEditor-Cmd.exe SmashKarts.uproject -run=pythonscript -script=<file> -unattended -nop4 -nosplash`.
- Self-tests: automation tests (`SmashKarts.*`) + a scripted listen-server/client run (`SK.AutoTest`) read back from `Saved/Logs`.
- Commit after each working step on `finish-game`. Push/merge to `main` only after a passed checkpoint.

## Architecture (phase 1)
| Class | Authority | Replicates | Client predicts |
|---|---|---|---|
| `AKartGameMode : AGameMode` | Server only | — | — |
| `AKartGameState : AGameState` | Server writes | MatchState (engine), end timestamps (`MatchEndServerTime`, `CountdownEndServerTime`, `ResultsEndServerTime`), match length; reliable multicast for eliminations | Timer display from `GetServerWorldTimeSeconds()` |
| `AKartPlayerState : APlayerState` | Server writes | Score (engine), Eliminations, Deaths, JoinOrder, ColorIndex, bIsHost, RespawnServerTime | — |
| `AKartPlayerController` | Client sends `Server_*` RPCs (host actions, debug) | — | Local UI input (runtime mapping context: Tab/Enter/Backspace/Esc) |
| `AKartHUD` + C++ UMG widgets | Local only | — | Polls replicated state each frame |
| `UKartStatusComponent` | Server sets flags/timers | Flag bitmask + end times | Movement reads flags on both sides (phase 2) |
| `AKartCharacter` (additions) | Server: elimination, damage gating | `bEliminated` (RepNotify), death FX multicast | Input locked locally from replicated state |

Match states: `WaitingToStart` (lobby: drive, no weapons) → `Countdown` (custom AGameMode state, karts frozen) → `InProgress` → `WaitingPostMatch` (results) → Play Again (countdown) or Back to Lobby (`WaitingToStart`). Results auto-return to the lobby after 12 s.
Respawn: the dead kart stays possessed and hidden (camera stays put), then is destroyed and `RestartPlayer` spawns a fresh one at the start farthest from living enemies (random among the best 3).

## Phases
| # | Phase | Main files | Status |
|---|---|---|---|
| 0 | Recon, plan, Python scripting, CLI build | `.uproject`, `Build.cs`, `Docs/*`, `Tools/EditorScripts/sk_common.py`, `sk_inspect.py` | ✅ Nov 12 |
| 1 | Core loop | `Game/KartGameMode`, `Game/KartGameState`, `Game/KartPlayerState`, `Game/KartPlayerController`, `UI/KartHUD`, `UI/KartHUDWidget`, `UI/KartScoreboardWidget`, `UI/KartWidgetBase`, `KartStatusComponent`, `KartDamageType`, `KartRules`, `Debug/KartDebugCommands`, `Tests/KartRulesTests`, `Debug/KartAutoTest`; edits to kart, health, weapon component, loot box, turret, projectile, trap; `sk_phase1_setup.py` | ✅ CP1 passed Nov 12 |
| 2 | Status + knockback | `KartStatusComponent` (all flags), `KartMovementComponent` (freeze, popped tires, spiky speed, knockback via `LaunchCharacter`/pending impulse, wall bounce), contact smash (game mode + ram via `MoveBlockedBy`), wall bounce via `HandleImpact`/`PhysWalking`, reverse cap, player colours | ✅ Nov 12 (self-tested) |
| 3 | Tier 2 weapons | `KartProjectile` (homing, sticky → trap, charge speed), `ProjectileWeapon` (charge), kart Fire Completed binding + charge meter, DA/BP for Bullets, Seeker, Cannon Balls, Sticky Bomb, HUD icons | 🛑 CP2 (code done Nov 12) |
| 4 | Menus + direct IP | `Game/KartGameInstance` (net errors, name save), `UI/KartMainMenuWidget`, lobby overlay, pause, settings, colours, name tags, damage numbers, `L_MainMenu`, `Docs/HOSTING.md`, renderer settings | ⏳ → 🛑 CP3 |
| 5 | Tier 3 weapons | `BuffWeapon`, trap effects (popped tires), Snowball freeze hit, Tesla zap, Mace Orbit | ⏳ |
| 6 | Arena | `sk_build_arena.py` → `L_Arena_01` blockout, spawns, loot spots, KillZ, lighting | ⏳ → 🛑 CP4 |
| 7 | EOS | OSS EOS + `NetDriverEOS`, Device ID login, create/find/join by room code, secrets in gitignored `Config/Windows/WindowsEngine.ini` | ⏳ → 🛑 CP5 |
| 8 | Extras + polish | Nuke, Fake Loot Box, Invincibility, audio, firing feel, balance (reverse cap done in phase 2) | ⏳ |
| 9 | Ship | packaging, log cleanup, `HOW_TO_PLAY.md`, zip | ⏳ → 🛑 Final |

Target dates: 0–3 by Nov 17 · 4–6 by Nov 22 · 7 by Nov 24 (EOS go/no-go) · 8–9 by Nov 29.

## Defaults chosen (change any time; all are editable properties)
- Match 180 s, respawn 3 s, spawn protection 2 s, results 12 s, countdown 3 s, max players 8, fall-credit window 5 s.
- Spawn protection is shown as a **blink** (mesh visibility toggles) instead of translucency, so no translucent material is needed.
- Lobby = the arena in `WaitingToStart`; loot boxes are hidden there.
- Disconnected players are not restored on rejoin (late joiners always start at 0).
- Phase 1 host controls are keys (Enter = start / play again, Backspace = back to lobby); phase 4 turns them into buttons.
- Player name: our own `?SKName=` URL option (phase 4 menu); without it players are "Player N".
- Explosion/death FX: copies of Niagara engine templates (`SimpleExplosion`, `RadialBurst`) in `Content/SmashKarts/FX`, referenced from BP defaults (no asset paths in C++).

## Risks
| Risk | Mitigation |
|---|---|
| EOS setup/config churn in 5.8 | Direct IP + virtual LAN is fully playable first; EOS behind a toggle; hard stop Nov 24. |
| Editor must be closed for CLI builds | Batch code work; one build per step. Live Coding not used for structural changes. |
| Headless Python level editing quirks | Scripts are idempotent and log every change; fallback is a manual checklist. |
| Knockback vs CMC prediction | Use the CMC's own launch path (`LaunchCharacter` → `PendingLaunchVelocity`, applied in the predicted move) and test at 60 ms. |
| Friends' hardware | Ray tracing off, quality preset menu, Shipping build. |
| Time | Cut order from the brief, section 8. |
