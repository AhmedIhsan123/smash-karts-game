# EOS (Epic Online Services): Developer Portal checklist

Goal: **Host Online Game** with a room code. Friends join over the internet without port forwarding or a VPN
(EOS sessions find the host, EOS P2P handles NAT traversal and relays).
Login is **EOS Connect with a Device ID**: no Epic account for you or your friends, no login window.
This takes about 15–20 minutes. Only you (the developer) do this; friends just play the zip you send them.

## 1. Organisation and product
1. Open <https://dev.epicgames.com/portal> and sign in with your Epic account.
2. If asked, create an **Organization** (any name) and accept the developer agreement.
3. Click **Create Product**. Name it after the game. Wait until it shows up in the left sidebar.

## 2. Note the IDs
In the product, open **Product Settings** (left sidebar). On the **SDK Download & Credentials** page (it may be labelled "Product Settings → Details/Credentials" in newer portal layouts), copy:
- **Product ID**
- **Sandbox ID** for the **Live** sandbox
- **Deployment ID** for that sandbox (each sandbox has a default deployment; if the list is empty, click *Add deployment* and give it any name)

## 3. Client policy and client
Still under **Product Settings → Clients**:
1. **Add new client policy** → name `P2P Client` → policy type **Peer2Peer** → Save.
   (Peer2Peer is the preset for player-hosted games: it covers Connect login, Sessions and P2P.)
2. **Add new client** → name `Game Client` → client policy `P2P Client` → Save.
3. Copy the **Client ID** and **Client Secret**.

You do **not** need to set up Epic Account Services, identity providers, brand review or the store.

## 4. Where the values go (never in Git)
1. In the project folder, copy `Config/Windows/WindowsEngine.ini.example` to `Config/Windows/WindowsEngine.ini`
   (same folder; that file is gitignored).
2. Open it in Notepad and replace the five `PASTE_...` values. Keep the section name `[EOSSDK.Platform.KartArena]` as it is. Example shape:
   ```ini
   [EOSSDK.Platform.KartArena]
   ProductId=1a2b3c...
   SandboxId=4d5e6f...
   DeploymentId=7a8b9c...
   ClientId=xyz123...
   ClientSecret=abc456...
   ```
3. Tell me when it's filled in. You don't need to send me the values in chat.

A Peer2Peer client secret is not truly secret: it ships inside the game for every player (that's how EOS client
credentials work). It just stays out of the GitHub repo.

## 5. Check it works (2 minutes)
1. Start the game outside the editor (a packaged build, or right-click `SmashKarts.uproject` → *Launch game*).
   Online play is always off inside the editor (PIE), so the editor can't test room codes.
2. The bottom line of the main menu should change from *Online: connecting...* to **Online: ready** within a few seconds.
3. If it says *Online: unavailable (the EOS credentials were rejected ...)*: re-check the five values (no spaces, the Live sandbox's Deployment ID, the client uses the Peer2Peer policy).

## Values checklist
| Value | Where in the portal |
|---|---|
| Product ID | Product Settings → SDK credentials |
| Sandbox ID (Live) | Product Settings → SDK credentials → Sandboxes |
| Deployment ID (Live) | Product Settings → SDK credentials → Deployments |
| Client ID | Product Settings → Clients |
| Client Secret | Product Settings → Clients |
