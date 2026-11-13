# Kart Arena — how to play

Free-for-all kart battles for 2–8 players. Drive through **?** boxes to get a weapon, smash the other karts,
score the most smashes before the timer runs out.

## Start
1. Unzip the folder anywhere (not inside Program Files) and run **SmashKarts.exe**.
   *Play_Windowed.bat* starts it in a window instead.
2. Type your name.
3. **Host Game** to run the match on your PC, or type the host's address and **Join Game**.
   How to connect over the internet: see **HOSTING.md**.
4. In the lobby, drive around while everyone joins. The host picks the match length and clicks **Start Match**.

## Controls
| Key | Action |
|---|---|
| W / S | drive / reverse |
| A / D | steer |
| Space | fire (hold to charge throws, release to throw) |
| Tab | scoreboard |
| Esc or P | menu (settings, leave) |

## Rules
- 100 health. You respawn 3 seconds after being smashed, with 2 seconds of spawn protection (you blink; firing ends it).
- One weapon at a time: use it up before the next **?** box gives you another.
- Your own explosions and traps never hurt you.
- Falling off the arena costs you a life; if someone hit you in the last 5 seconds, they get the point.
- Most smashes wins. Ties: fewer deaths, then who joined first.

## Weapons
| Weapon | What it does |
|---|---|
| Rockets | 3 rockets side by side, straight ahead |
| Bullets | one press fires 6 quick rounds of 3 bullets |
| Machine Gun | 6.5 s of auto-aim at the nearest kart you can see |
| Seeker Missile | flies straight, then curves into the nearest kart in front |
| Cannon Balls | hold Space to charge; 4 balls land in a line ahead, the first one farthest |
| Sticky Bomb | hold to charge; sticks where it lands, explodes when someone gets close (or after 5 s) |
| Mines | drops 3 mines behind you; yours look green to you |

More weapons arrive in later builds.

## Problems?
- **Can't connect**: check the address, make sure the host is in the lobby, see HOSTING.md (firewall / VPN).
- **"Your game version doesn't match"**: everyone needs the same zip.
- **Low frame rate**: Esc → Settings → Quality: Low or Medium, and a smaller resolution.
