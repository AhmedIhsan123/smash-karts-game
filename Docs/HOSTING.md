# Hosting and joining a game

One player **hosts** (their PC runs the match) and everyone else **joins**. Up to 8 players.

## 1. Online with a room code (easiest)
1. **Host:** start the game, type your name, click **Host Online Game**. In the lobby, the panel shows
   **ROOM CODE  XXXXX** (5 letters/digits). Read it out to your friends.
2. **Friends:** type the code in the box next to **Join Game** and click it. Capital letters don't matter.
3. When everyone is in, the host clicks **Start Match** (or presses Enter).

This goes through Epic Online Services: no IP addresses, VPN, port forwarding or firewall rules, and no Epic account.
The line at the bottom of the main menu shows whether online play is ready:
- *Online: ready* → room codes work.
- *Online: connecting...* → wait a few seconds.
- *Online: unavailable (...)* → no internet, or Epic's servers are down. It retries by itself; meanwhile use section 2.
- *Online play is off* → this copy of the game was built without online play. Use section 2.

Every time you host you get a new code. The code is also on the Tab scoreboard during a match, so anyone can invite a friend mid-game.

## 2. By IP address (LAN, VPN, port forwarding)
Click **Host LAN / IP Game** (just **Host Game** if online play is off). Friends type your IP address next to **Join Game**.
The game uses **UDP port 7777**.

### Same house / same Wi-Fi (LAN)
1. **Host:** host the game. The panel on the right lists your addresses; read out the one that starts with `192.168.` (or `10.`).
2. **Friends:** type that address next to **Join Game** and click it.

The first time you host, Windows asks whether to allow the game on networks: tick **Private networks** (and **Public** if your Wi-Fi is set to Public) and click **Allow**. See section 4 if you clicked Cancel.

### Over the internet with a virtual LAN (no router setup)
Everyone joins the same virtual network; then it works exactly like a LAN.

**Radmin VPN** (Windows, free, easiest)
1. Everyone installs **Radmin VPN** from radmin-vpn.com and opens it.
2. **Host:** *Network → Create Network*, choose a name and password, *Create*.
3. **Friends:** *Network → Join Network*, enter the same name and password.
4. **Host:** host by IP. In the lobby panel, the address starting with **26.** is the one friends type in.

**ZeroTier** (free)
1. **Host:** create an account at my.zerotier.com, click **Create A Network**, copy the 16-character **Network ID**.
2. Everyone installs **ZeroTier One**, then: tray icon → **Join New Network** → paste the ID.
3. **Host:** on my.zerotier.com, open the network and tick **Auth** next to every friend's device.
4. **Host:** host by IP; friends join the ZeroTier address shown in the lobby panel (often `10.x.x.x` or `172.x.x.x`).

If friends still can't connect over a VPN, add the firewall rule from section 4 on the host's PC (virtual adapters are often marked *Public*).

## 3. Over the internet without a VPN: port forwarding (needs router access)
1. **Host:** note your PC's local address from the lobby panel (`192.168.x.x`).
2. Open your router's admin page (usually `192.168.1.1` or `192.168.0.1`, login on the router sticker) → **Port Forwarding** (sometimes *Virtual Server* or *NAT*).
3. Add a rule: protocol **UDP**, external port **7777**, internal port **7777**, internal IP = your local address. Save.
4. Find your **public IP**: search "what is my ip" in a browser.
5. Friends join `your-public-ip` (port 7777 is the default, no need to type it).

If it still fails, your internet provider may use **CGNAT** (your router's WAN address differs from "what is my ip", or starts with `100.64`–`100.127`). Port forwarding can't work then; use a room code (section 1) or a VPN.

## 4. Windows Firewall rule (IP hosting only, host, once)
Either:
- **Windows Security → Firewall & network protection → Allow an app through firewall → Change settings → Allow another app… → Browse** → pick the game's `.exe` (in the game folder: `KartArena.exe` or `SmashKarts.exe`, and also the one in `<GameFolder>\Binaries\Win64\`) → tick **Private** and **Public** → **OK**.
- Or open **PowerShell as administrator** and run:
  `New-NetFirewallRule -DisplayName "Kart Arena UDP 7777" -Direction Inbound -Protocol UDP -LocalPort 7777 -Action Allow`

## Troubleshooting
| Message / problem | Fix |
|---|---|
| "No game found with code ..." | Check the code with the host (top of their lobby panel). Codes have no 0, O, 1, I or L. |
| "Couldn't connect to the host's online game" | The host left or lost internet. They host again (new code). |
| "Still connecting to online play" | Wait a few seconds and click again. |
| "Online play is unavailable" | No internet or Epic's servers are down. Use section 2 meanwhile. |
| "Couldn't connect…" after ~15 s (IP) | Wrong address, or the host isn't in the lobby yet, or the host's firewall blocks the game (section 4), or you're not on the same VPN network. |
| "Your game version doesn't match the host's" | Everyone must use the same zip. |
| "That game is full" | 8 players max. |
| "The host left the game" | The host quit or crashed. Someone hosts again. |
| Testing alone with two copies | One copy hosts by IP; the other joins `127.0.0.1`. (Two copies on one PC share one online identity, so room codes need two PCs.) |
| Lag | The host matters most: wired connection, close downloads/streams. |
