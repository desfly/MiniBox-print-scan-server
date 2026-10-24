# MiniBox firmware flashing invariant

For an already configured MiniBox, use normal sysupgrade so OpenWrt preserves
`/etc/config/network`, `/etc/config/wireless`, and the rest of the retained
configuration:

```sh
sysupgrade /tmp/minibox.bin
```

Do **not** use `sysupgrade -n` for routine firmware updates. `-n` explicitly
discards configuration, including the Wi-Fi STA credentials and the MiniBox
static addresses.

Recovery invariant in firmware package r26 and later:

- existing non-default LAN configuration is preserved;
- after an intentional configuration wipe / first boot, Ethernet is forced to
  the known maintenance address `192.168.55.251/24`;
- the recovery script does not write Wi-Fi SSID/password;
- Wi-Fi client `192.168.55.250` is retained only by normal sysupgrade until
  the separate provisioning-AP work is implemented and hardware-verified.

This keeps a deterministic Ethernet recovery path even after `sysupgrade -n`.
