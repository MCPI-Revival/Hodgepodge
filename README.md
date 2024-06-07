# Hodgepodge

> A random collection of useful and fun features

This mod adds some useful things, such as QoL features, fixing bugs, adding redstone, and experimenting with the latest MCPI modding features. Aimed at making MCPI more enjoyable.

## Installing

Currently this mod works with both Reborn 2.5.3 and 3.0.0. **MCPI++ doesn't work with it.**

- Grab `libredstone.so` from releases tag and put it in `~/.minecraft-pi/mods/` (or `~/.var/app/com.thebrokenrail.MCPIReborn/.minecraft-pi/mods/` if you use the flatpak)
- Put `assets/terrain.png` in `~/.minecraft-pi/overrides/images/` (or `~/.var/app/com.thebrokenrail.MCPIReborn/.minecraft-pi/overrides/images/` if you use the flatpak)
- Put `assets/items.png` in `~/.minecraft-pi/overrides/images/gui/` (or `~/.var/app/com.thebrokenrail.MCPIReborn/.minecraft-pi/overrides/images/gui/` if you use the flatpak)
- Done! If you run Reborn it should work, if not save the crash log and upload it in github issues or on the discord.

## Building

Run `make`, after running latest (and I mean latest, not latest release, absolute [latest build](https://gitea.thebrokenrail.com/minecraft-pi-reborn/minecraft-pi-reborn/actions)), you don't actually have to run it using `--copy-sdk` works fine as well. Using the Flatpak's symbols isn't supported because it is too slow to update, but if you insist just change all `~` to `~/.var/app/com.thebrokenrail.MCPIReborn/` in the makefile.

## Contributing

### Assets

#### Images

If you think you can draw something better than me, then feel free to try. You probably can, however if you want it to be included it needs to:

- be at the same resolution (this means 16x16 for tiles and blocks)
- not look like it's in a different art style with vannila or another part of this mod
- not replace a vanilla or back ported texture
- be original

Please only add one texture per pull request (except for where it makes sense, such as changing a block with unique textures on each side)

#### Audio

There are plans to add both ambient music and music disks, however for legal reasons I don't want to take these from modern minecraft. Currently this system doesn't exist, but feel free to submit music (in wav format), into the assets directory and I'll sort them out later. These need to be orignal and fit the the game.

### Code

Go for it! Some good things to contribute are:

- Bug fixes (for MCPI, Reborn, or this mod itself)
- Resolving TODOs
- Redstone backports (from any version or edition)
- More commands
- More chat symbols (from CP437)
- Quality of life features

