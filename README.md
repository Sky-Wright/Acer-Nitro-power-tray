# Acer-Nitro-power-tray

Simple brute force power profiles for easy GUI control for Acer Nitro AN-515-42-RED.

## Why this exists
The Acer Nitro AN515-42 (Ryzen 2500U) has a notorious BIOS-level ACPI `_PPC` clamp that permanently throttles the CPU to 1.6GHz while on battery, completely ignoring standard Linux power-profiles-daemon or KDE power sliders. 

This Qt6 system tray app bypasses the BIOS limits entirely by:
1. Overriding the ACPI `_PPC` clamp in the kernel.
2. Restoring the maximum frequency boundaries across all CPU cores.
3. Using a `setuid` root helper to fire `ryzenadj` via `fork()+execv()`.
4. Automatically detecting AC vs. Battery states and dynamically applying custom Wattage (TDP) limits based on the physical capabilities of the AN515-42 battery vs wall power.

## Profiles
* **Performance ⚡**: Full boost enabled. (15W AC / 9W Battery)
* **Balanced ⚖️**: Dynamic scaling. (10W AC / 6W Battery)
* **Powersave 🌙**: Extreme battery saver, forces CPU below 1.6GHz. (6W AC / 3W Battery)

## Installation (Arch Linux)
Before installing, you **must** disable normal Linux power profiles entirely, otherwise they will fight this tray app for control. 

Disable `power-profiles-daemon` (or `tlp` if you use it):
```bash
sudo systemctl stop power-profiles-daemon
sudo systemctl mask power-profiles-daemon
```

Build and install the package using `makepkg` (this will automatically pull in the `ryzenadj` dependency):
```bash
git clone git@github.com:Sky-Wright/Acer-Nitro-power-tray.git
cd Acer-Nitro-power-tray
makepkg -sif
```

Once installed, launch it from your application menu, or run `linux-power-tray &` from the terminal. It will automatically add itself to your startup applications.

*(Other distros are on their own lol. You'll have to compile with CMake manually and figure out your own setuid permissions.)*
