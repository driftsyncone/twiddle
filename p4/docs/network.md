# Network

## What the panel needs
- **Wi-Fi:** a 2.4 or 5 GHz network it can join with the SSID and password you set in `secrets.h` or the setup portal
  (Wi-Fi goes through the C6 coprocessor, [c6-firmware.md](c6-firmware.md)). It is a normal DHCP client; give it a
  fixed lease in your router if you need a stable address.
- **mDNS:** `twiddle-p4.local` (the S3 firmware defaults to `twiddle-s3`, the Waveshare build to `twiddle-ws`) (set in `wlan.cpp`; change it with `#define PANEL_HOSTNAME` in `secrets.h`). Web server on
  port 80 ([architecture.md](architecture.md)). mDNS does not cross VLANs or subnets unless your router relays it; use the IP then.
- **Setup portal** (no SSID stored): AP `Twiddle-Setup`, password `twiddle-setup`, page `/setup` at `192.168.4.1`.
- **Cloud:** outbound HTTPS to `latency.autoblowapi.com` and the cluster host it returns. TLS uses the IDF
  certificate bundle. mbedTLS buffers are in PSRAM.

## Reaching the panel from your computer
`ota.sh`, `/screenshot` and `/log` need your computer to reach the panel on port 80. If the panel is on a separate
VLAN or guest network, your router/firewall must allow TCP 80 from your computer to the panel's address (allow only
what you need, remove the rule when done). Symptoms of a missing route are timeouts, not refusals.

## Reaching an emulator on your computer
For testing against the Autoblow emulator ([testing.md](testing.md)) the panel must reach your computer on the
emulator's port (8792, plain http). If your computer runs a host firewall that drops inbound LAN connections, allow
that port from the panel's subnet only, as a temporary (runtime) rule. For example with firewalld:
```sh
firewall-cmd --add-rich-rule='rule family="ipv4" source address="<panel subnet>/24" port port="8792" protocol="tcp" accept'
```
This is a runtime rule (gone after a reload or reboot); do not open it to everyone. Local connections (curl from the
computer itself) bypass the firewall, so a passing local test says nothing about LAN reachability.
Stop the emulator when you are done: it listens on all interfaces.
