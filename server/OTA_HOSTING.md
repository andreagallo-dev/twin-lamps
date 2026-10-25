# Hosting the firmware with Tailscale Funnel

For a remote update the lamp downloads its firmware over HTTPS from a fixed address, compiled into it as `SECRET_FW_BASE`. Here that address is a folder on an always-on home server, published to the internet with [Tailscale Funnel](https://tailscale.com/kb/1223/funnel): no router ports to open, a valid HTTPS certificate, and only that one folder exposed, under a secret path.

Any other HTTPS host works too, as long as the lamp can download `<FW_BASE>lampada_A.bin` and `<FW_BASE>lampada_B.bin`.

## Setup

1. **Install Tailscale** on the server and log in:

   ```bash
   sudo tailscale up
   ```

   In the Tailscale admin console, enable MagicDNS and HTTPS certificates, and allow Funnel for this machine in the access controls (the first `tailscale funnel` command prints a link that does it for you).

2. **Create the folder** for the firmware, owned by the user you log in with over SSH:

   ```bash
   sudo mkdir -p /srv/lamps-fw
   ```

   ```bash
   sudo chown "$USER" /srv/lamps-fw
   ```

3. **Pick a secret path**: random characters that nobody can guess.

   ```bash
   echo "fw-$(openssl rand -hex 8)"
   ```

4. **Publish the folder** on port 10000 (Funnel allows 443, 8443 and 10000) under that path, in the background so it survives reboots:

   ```bash
   sudo tailscale funnel --bg --https=10000 --set-path=/fw-xxxxxxxxxxxxxxxx /srv/lamps-fw
   ```

5. **Check** it:

   ```bash
   tailscale funnel status
   ```

   The firmware base address is then:

   ```
   https://your-server.your-tailnet.ts.net:10000/fw-xxxxxxxxxxxxxxxx/
   ```

   Put it, with the final `/`, in `SECRET_FW_BASE` in `secrets.h`. Put the SSH login and the folder in `tools/.env` (`SERVER=user@your-server`, `FW_DIR=/srv/lamps-fw`).

6. **Test** from a phone with Wi-Fi and Tailscale both off (so you really go through the internet): copy a test file into the folder and open its URL.

## Good to know

- The secret path is the only thing protecting the folder: keep it out of anything you publish.
- The `.bin` contains the broker password and the firmware address. Keep the folder empty between updates and delete the files right after each update.
- Your laptop reaches the server by name over Tailscale, so `update_lamp.sh` needs Tailscale running on the laptop too (`sudo tailscale up`).
- If an update fails while Funnel is not answering, check `tailscale funnel status` on the server and try the URL again from the phone.
