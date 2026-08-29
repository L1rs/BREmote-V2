# BREmote V2 — L1rs fork

A fork of **[Luddi96/BREmote-V2](https://github.com/Luddi96/BREmote-V2)**, the open source eFoil and Esk8 remote.

This page only describes what is **different here**. Everything else — the build video, mechanics, electronics, wiring examples, config tools and the original documentation — is unchanged and lives upstream:

> ### → [Luddi96/BREmote-V2](https://github.com/Luddi96/BREmote-V2)
> Start there if you want to build a BREmote. Come back here for the additions below.

The hardware is untouched. Both boards still run an ESP32-C3 with an SX1262 (Heltec HT-CT62) and the same 4 MB flash layout, so you can move between this firmware and Luddi96's at any time.

---

## What this fork adds

**Maintenance over WiFi.** The receiver is screwed into the hull, and getting a USB cable to it for every setting change is a nuisance. Both boards now open a small web page on your home network that gives you the same serial console you get over USB — every `?` command, including reading and writing the configuration — plus a firmware upload. See [WiFi maintenance](#wifi-maintenance) below.

**KISS ESC telemetry.** The receiver can read battery voltage and temperature from a BLHeli_32 or AM32 ESC, so you get a battery gauge without a VESC. Same connector, no hardware change. See [KISS ESC telemetry](#kiss-esc-telemetry).

**Info pages on the remote display.** With steering switched off, the toggle cycles the display through speed, board battery and ESC temperature. See [Info pages](#info-pages-on-the-remote-display).

There is also a **fix to the radio initialisation** that everyone building on this code should know about, described under [Radio init](#radio-init-fix).

---

## Firmware

| File | Board |
|---|---|
| `Source/V2.2.7.1-L1rs1_Integration_Rx.ino.bin` | Receiver, app only |
| `Source/V2.2.7.1-L1rs1_Integration_Rx.ino.merged.bin` | Receiver, full image |
| `Source/V2.2.7.2-L1rs1_Integration_Tx.ino.bin` | Remote, app only |
| `Source/V2.2.7.2-L1rs1_Integration_Tx.ino.merged.bin` | Remote, full image |

Built with the Arduino IDE, esp32 core 3.3.11, board *ESP32C3 Dev Module*, partition scheme *Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)*. That is the same layout Luddi96 uses, so your stored configuration and pairing survive an update.

For a first flash over USB use the `merged.bin` with the [Espressif Flash Download Tool](https://dl.espressif.com/public/flash_download_tool.zip) at address `0x0`. **This erases the configuration**, so you will have to calibrate and pair again. To keep your settings, flash the plain `.ino.bin` to `0x10000` instead, or use the Arduino IDE.

Once WiFi is set up, updates go through the browser and you never need the cable again.

Upstream binaries are in [Luddi96's repository](https://github.com/Luddi96/BREmote-V2/tree/main/Source) if you want to go back.

---

## WiFi maintenance

### What you get

Open `http://<address of the board>/` in a browser and you get a terminal that behaves exactly like the USB console. Every command works there, including:

- `?conf` to read the configuration as a base64 string
- `?setConf:<string>` to write it back, so [Luddi96's config tool](https://lbre.de/BREmote/struct.html) still does the editing — you just copy the string in and out through the browser instead of a cable
- `?setBC:` for the battery curve, `?printGPS`, `?printRSSI`, `?printKiss` and everything else
- `/update` takes a firmware `.bin`

### When WiFi is on

WiFi never runs while you are riding. That is deliberate, and there is no setting to change it.

**Receiver:** joins the network at boot and switches WiFi off the moment the remote sends its first packet. It stays off until the next power cycle. So to reach it, **power the receiver up with the remote switched off** — on the foil battery or on a USB cable, whichever is easier.

**Remote:** joins the network while it sits on the charger, and in USB mode (hold throttle and toggle left while switching on). Both mean it is on a cable and not in use. Leaving the charge screen switches WiFi off again.

### One-time setup over USB

The credentials have to get onto the board once, and that has to happen over the cable. Connect the board over USB and open a serial terminal at 115200 baud — the Serial Monitor in the Arduino IDE does fine, as does [Luddi96's serial terminal](https://lbre.de/BREmote/sertest.html) in Chrome or Edge. Then send:

```
?setWifi:MyNetwork,mywifipassword,mymaintenancepassword
```

and afterwards:

```
?reboot
```

Three things about that command:

**The third field is a password you invent.** There is no default. It is not your WiFi password and it is not anything that already exists — you make it up here, and from then on it protects the web page and the firmware upload. Since the receiver drives a motor, do not reuse a password you use elsewhere. It is stored in plain text on the board.

**No commas in the SSID or in the maintenance password.** The command is split at the first and the last comma. Your WiFi password may contain commas, because it sits between them.

**2.4 GHz only.** The ESP32-C3 has no 5 GHz radio. If your router uses separate names per band, use the 2.4 GHz one.

The credentials live in their own file (`/wifi.txt`) and not in the configuration struct, so adding them does not disturb your calibration or pairing, and the config tool keeps working unchanged.

`?clearWifi` removes them again, `?printWifi` shows what is stored and whether the board is connected.

### Finding the board

**There is no mDNS**, so `bremote-rx-….local` will not resolve and neither will a ping to that name. It was left out on purpose: the mDNS library costs 40 KB of flash and the board is already at 90 %.

What the board does do is register its hostname with your router, as `bremote-rx-<MAC>` or `bremote-tx-<MAC>`. Look the address up in the router's device list once and give it a fixed one; then you can bookmark it.

The browser will ask for a user name and password: the user name is **`bremote`**, the password is the maintenance password from `?setWifi`.

### If it does not come up

`?printWifi` over USB tells you where it stopped. `Wifi: off` with an SSID stored usually means the receiver already saw the remote — switch the remote off and reboot. `connecting` that never finishes is almost always a wrong password or a 5 GHz-only network.

One trap worth knowing: the `?print…` commands run in a loop that only accepts `quit`. If a board seems to ignore everything you type, send `quit` first.

---

## KISS ESC telemetry

Set `data_src` to **3** and the receiver reads a BLHeli_32 or AM32 ESC instead of a VESC.

Wiring is the same as for a VESC: the ESC telemetry wire goes to **JP5**, the connector Luddi96 already documents for VESC UART. Nothing changes on the board, both run at 115200 baud through the same mux channel.

On the ESC side, **auto telemetry has to be enabled** in BLHeli Suite or the AM32 configurator. Without it the ESC only answers a DShot request and stays silent here.

Battery voltage and temperature go over the radio link and drive the usual battery and temperature bars. The frame also carries current, consumption and eRPM; those are read and shown by `?printKiss` but not transmitted, because adding fields to the telemetry packet would slow every other value down and break compatibility with boards running other firmware.

The battery percentage uses the same lookup table and load compensation as the analog and VESC paths, so `?setBC:` and the [LUT tools](https://lbre.de/BREmote/bat_conf.html) apply unchanged.

---

## Info pages on the remote display

`steer_enabled` gains a third value:

| Value | Meaning |
|---|---|
| 0 | Steering off (as before) |
| 1 | Steering on (as before) |
| 2 | Steering off, toggle switches display pages |

With `steer_enabled = 2`, **hold the toggle right** (throttle released) to step through:

| Page | Shows | Marker |
|---|---|---|
| 1 | Speed | `S` |
| 2 | Board battery in percent | `b` |
| 3 | ESC or VESC temperature in °C | `C` |

The marker flashes up briefly when you switch, then the value appears. Gear changes with a short toggle press and locking with a long left press are unchanged.

The battery page is worth having on its own: until now that value was only visible as the ten-step bar along the bottom edge, which is hard to read precisely.

Note that `steer_enabled = 2` also releases the toggle when `no_gear` and `no_lock` are both set. In Luddi96's code that combination keeps the toggle permanently in steering, which would have made the pages unreachable.

---

## Radio init fix

Worth knowing if you build on this code.

`startupRadio()` sets `radio.standbyXOSC = true` before `radio.begin()`. That is the wrong order: entering XOSC standby needs a running oscillator, but the TCXO is only powered by `setTCXO()`, which RadioLib calls afterwards. Whether the rejected command surfaces is a matter of code layout — the original happens to get away with it, but **almost any addition to the sketch makes `begin()` fail with `-707`** and the board sits in the error loop with no radio.

Running `begin()` in RC standby fixes that, but `begin()` also latches the **Rx/Tx fallback mode** from the same flag, and RadioLib exposes no setter for it. The chip then drops to RC standby after every packet, the oscillator stops, and the remote's receive window opens too late to catch the reply — the link goes one-way.

So this fork runs `begin()` with the flag off and writes the fallback mode back afterwards, through a small subclass of `SX1262`. Measured on the bench: 100 % of packets acknowledged, RSSI −19 dBm.

---

## Serial commands added by this fork

| Command | Does |
|---|---|
| `?setWifi:<ssid>,<wifi pw>,<maintenance pw>` | Store WiFi credentials, reboot to apply |
| `?clearWifi` | Remove them again |
| `?printWifi` | SSID, hostname, connection state and IP |
| `?printKiss` | ESC telemetry until you send `quit` (receiver only) |

`?setConf:` also gained a safety check: a string of the wrong length or the wrong config version is now rejected instead of written. A garbled paste used to cost the pairing, which is expensive on a receiver you cannot reach.

Everything else works as upstream. Send `?` for the full list.

---

## Status codes and everything else

Unchanged from upstream. The [original README](https://github.com/Luddi96/BREmote-V2#statuserror-codes) has the status and error codes, the startup button combinations, the connection examples for VESC and ESC wiring, and the links to the config tools.

---

## Changelog

### V2.2.7.x-L1rs1
* WiFi maintenance for receiver and remote: web terminal with all serial commands, plus firmware upload over the browser
* Remote joins the network while charging and in USB mode, receiver at boot until the remote connects
* KISS telemetry from BLHeli_32 and AM32 ESCs (`data_src` 3)
* Info pages on the remote display (`steer_enabled` 2)
* Fix radio init failing with `-707`, and the one-way link that came with the first attempt at fixing it
* `?setConf:` rejects a configuration of the wrong size or version instead of writing it

---

## Credits

All of this builds on **[Luddi96](https://github.com/Luddi96)**'s work — the hardware, the mechanics and the firmware this fork changes a small part of. The web terminal served by the boards is a fork of his [serial terminal](https://lbre.de/BREmote/sertest.html), with the Web Serial API swapped for `fetch()` so it can talk over the network; the markup and styling are his.

Licence: GPL-3.0, as upstream. Logo uses "watersport" and "Skate" by Adrien Coquet from https://thenounproject.com/. CC BY 3.0.
