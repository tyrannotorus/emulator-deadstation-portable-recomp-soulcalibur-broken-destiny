# Soulcalibur: Broken Destiny recomp module

Soulcalibur: Broken Destiny (PSP) for the TrimUI Brick, as a recomp module for Deadstation Portable: the game's code translated
ahead of time to C and compiled for the Brick, with no emulation at run time.

- 60 fps fights at the PSP's speed
- 1024x768 in 4:3: more of each stage, with the HUD and menus laid out for it
- Fight GUI toggle and Unlock Everything, in Deadstation Portable's Options
- Remappable controls

You need your own copy of the game (USA, ULUS10457) as a CHD.

---

## AI Slop Section

Built with the Deadstation Portable recomp SDK (`sdk/`, a submodule).

    git submodule update --init --recursive
    sdk/tools/prepare-game.sh "Soulcalibur - Broken Destiny.chd" WORK   # the game's code to C (WORK outside the repository)
    TG5040_ROOT=... tools/build.sh WORK                                 # the module for the Brick
    TG5040_ROOT=... tools/package.sh                                    # the release archive and its store entry

- `module.cfg`: the manifest (disc, game image hash, settings, controls). `store.json`: the Recomp Store's page.
- `src/module.c`: what the module changes in the game (logic at 60, the 4:3 layout of HUD and menus, OPTIONS without INSTALL,
  Unlock Everything) and its settings.
- `tools/patch-gen.py`: puts the module's calls into the translated C, each at an instruction named by its exact text.

GPL-3.0-or-later.
