# EOS (Epic Online Services) — Developer Portal checklist

Goal: hosting and joining over the internet without port forwarding (EOS sessions + P2P relay/NAT traversal).
Login is **EOS Connect with a Device ID**: no Epic account needed for your friends, no login window.
This takes about 15–20 minutes. Do it any time before phase 7.

## 1. Organisation and product
1. Open <https://dev.epicgames.com/portal> and sign in with your Epic account.
2. If asked, create an **Organization** (any name) and accept the developer agreement.
3. Click **Create Product**. Name it after the game. Wait until it shows up in the left sidebar.

## 2. Note the IDs
In the product, open **Product Settings** (left sidebar). On the **SDK Download & Credentials** page (it may be labelled "Product Settings → Details/Credentials" in newer portal layouts), copy:
- **Product ID**
- **Sandbox ID** for the **Live** sandbox (also copy the **Dev** sandbox ID if it is shown)
- **Deployment ID** for that sandbox (each sandbox has a default deployment; if the list is empty, click *Add deployment* and give it any name)

## 3. Client policy and client
Still under **Product Settings → Clients**:
1. **Add new client policy** → name `P2P Client` → policy type **Peer2Peer** → Save.
   (Peer2Peer is the preset for player-hosted games: it covers Connect login, Sessions, Lobbies and P2P.)
2. **Add new client** → name `Game Client` → client policy `P2P Client` → Save.
3. Copy the **Client ID** and **Client Secret**.

You do **not** need to set up Epic Account Services, brand review or the store.

## 4. Where the values go (never in Git)
1. Copy `Config/Windows/WindowsEngine.ini.example` to `Config/Windows/WindowsEngine.ini` (that file is gitignored).
2. Paste the five values into it. Leave the encryption key line as it is (64 hex characters; any value works for us).
3. Tell me when it's filled in. You don't need to send me the secret in chat.

Note: a Peer2Peer client secret is not truly secret. It ships inside the game for every player (that is how EOS client credentials work). It just stays out of the GitHub repo.

## Values checklist
| Value | Where in the portal |
|---|---|
| Product ID | Product Settings → SDK credentials |
| Sandbox ID (Live) | Product Settings → SDK credentials → Sandboxes |
| Deployment ID (Live) | Product Settings → SDK credentials → Deployments |
| Client ID | Product Settings → Clients |
| Client Secret | Product Settings → Clients |
