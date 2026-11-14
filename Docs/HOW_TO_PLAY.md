# Kart Arena — how to play

Free-for-all kart battles for 2–8 players. Drive through **?** boxes to get a weapon, smash the other karts,
score the most smashes before the timer runs out.

## Start
1. Unzip the folder anywhere (not inside Program Files) and run **SmashKarts.exe**.
   *Play_Windowed.bat* starts it in a window instead.
2. Type your name.
3. **Host Online Game** runs the match on your PC and gives you a 5-letter **room code** (shown in the lobby).
   Friends type the code in the box next to **Join Game**. No IP addresses, VPN or router setup needed.
   **Host LAN / IP Game** is the old way: friends type your IP address instead (same house, VPN, port forwarding).
   If the bottom of the menu says online play is off or unavailable, only the IP way works: see **HOSTING.md**.
4. In the lobby, drive around while everyone joins. The host picks the match length and clicks **Start Match**.
   Playing alone or with a few friends? The host can add practice bots with **+ Bot** (up to 8 karts in total;
   a friend who joins takes a bot's place).

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

## The arena
- Open centre inside a ring of low walls; the gaps in the ring are great spots for mines and spike strips.
- Two raised platforms (ramps on two sides, a **?** box on top) and a kicker ramp for jumps.
- Two pits with red curbs. You can't drive in, but explosions can throw you in, and that costs you a life (the thrower gets the point).

## Weapons
| Weapon | What it does |
|---|---|
| Rockets | 3 rockets side by side, straight ahead |
| Bullets | one press fires 6 quick rounds of 3 bullets |
| Machine Gun | 6.5 s of auto-aim at the nearest kart you can see |
| Seeker Missile | flies straight, then curves into the nearest kart in front |
| Cannon Balls | hold Space to charge; 4 balls land in a line ahead, the first one farthest |
| Sticky Bomb | hold to charge; sticks where it lands, explodes when someone gets close (or after 5 s) |
| Mines | drops 3 mines behind you; yours look green to you, everyone else's red |
| Mace Orbit | 6 s of spiked maces circling your kart: anyone they touch is smashed. You're faster and tougher meanwhile |
| Tesla Coil | 5 s of lightning: zaps every kart within the blue ring that you can see, twice a second |
| Snowball | 3 throws; a hit freezes the kart for 3 s. Ram a frozen kart to smash it |
| Spike Strip | dropped behind you; pops the tires of everyone who drives over it (slow and slippery until they respawn). Stays for 30 s |
| Nuke | slow, but the blast is huge: everyone near it gets hurt and thrown |
| Fake Loot Box | drops a fake **?** box behind you. It looks real to everyone else (yours shows green to you); whoever grabs it blows up |
| Invincibility | 5 s golden shield: nothing hurts or freezes you, and anyone you touch is smashed |

The weapon panel (bottom right) shows uses left, or the time left for Machine Gun, Mace Orbit, Tesla Coil and Invincibility.

## Problems?
- **"No game found with code ..."**: check the code with the host (it's at the top of their lobby panel and on the Tab scoreboard).
- **Can't connect with a code**: the host may have left; they host again and get a new code. Still stuck: use Host LAN / IP Game and HOSTING.md.
- **Can't connect by IP**: check the address, make sure the host is in the lobby, see HOSTING.md (firewall / VPN).
- **"Your game version doesn't match"**: everyone needs the same zip.
- **Low frame rate**: Esc → Settings → Quality: Low or Medium, and a smaller resolution.
