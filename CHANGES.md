# Gameplay fixes + save system (latest revision)

* **Zombies arrive later** - the wait before each spawn (and before the first zombie of a wave) is now 5-7 s depending on
  the level: 7 s on 1-1, shrinking to 5 s on 5-10, +/-0.5 s jitter (`AdventureSpawnInterval` in `level.cpp`). Survival and
  mini-games keep their own timings; the 1-1 / 1-2 tutorials still hold the first zombie.
* **Seed selection alignment** - the click area of the seed packets started 66 px too far left (`input.cpp` used a 64 px sun
  panel, the drawing and tooltip use 130 px), so you had to click the packet before the one you wanted. Fixed.
* **Recharging packets can't be selected** - no hover cue, no selection, no pick-up, no planting (`Hud_IsRecharging` in
  `hud.cpp`, used by `game.cpp`, also for the D-pad/button-cycling fallback).
* **"Level Finish" screen removed** - after a win you get the *New Plant!* or *New Item!* screen (if the level has one),
  both with **[Next Level]** and **[Title Screen]** (top-right). A level without a reward just plays out 3 s and the next
  level starts. Retry is still in the pause menu ("Restart Level"). (`menu.cpp`, `menu.h`, `items.cpp`, `main.cpp`)
* **Coins** - no more automatic level bonus. Zombies drop coins (25 %: silver 10, gold 50) that you must **click**; the
  counter appears bottom-left only when a coin is picked up and disappears 3 s after the last pickup. (`hud.cpp`)
* **Adventure button** shows the real level as `<world>-<level>` (Day = 1 ... Roof = 5), e.g. "Level 2-3". The button now
  continues from the saved level.
* **Save system** (`save.cpp` / `save.h`, file `sd:/apps/pvz_wii/save.cfg`): saves the Adventure progress and the coins when
  a level is **completed**, **lost**, or **left / the game is quit**. Nothing is written during play.

# Sound effects (this revision)

* **168 sound effects / stings added** as `assets/sfx/<name>.wav` (converted from the supplied `.ogg` pack to 16-bit mono
  22.05 kHz PCM, ~12 MB; use `scripts/convert_sfx.py <folder>` to redo / add more). Every sound has an id in `SFX_LIST`
  (`source/sfx.h`), e.g. `Sfx_Play(SFX_CHERRYBOMB)`.
* New `source/sfx.cpp` (loader + mixer: lazy loading with a preloaded core set, ASND voices 1-15, same sound at most once per
  frame, `Sfx_PlayDelayed`, follows the options-menu volume) and `source/sfx_events.cpp`, which watches the game state once per
  frame (`Sfx_PollGame`, end of `Game_Update`) and plays: zombie hits (splat / cone / bucket / shield / snow-pea sparkle /
  butter), arm falling off, chewing, gulp, garlic "yuck", falling over, mind-control, pole-vault / dolphin / digger / pogo /
  zomboni / balloon / imp / gargantuar / jack-in-the-box / dancer / catapult sounds, ladder, idle groans, thrown cabbage /
  kernel / melon, spores, fumes, fire-pea, huge-wave siren + roar, win / final-level fanfare, game-over scream + jingle.
* Explicit calls where nothing shows in the state: planting (land / water), seed pick-up / put-back / can't-afford buzzer,
  sun pick-up, shovel, instant plants (cherry bomb, jalapeno, doom-shroom, ice-shroom, potato mine arming + blast, squash,
  chomper, magnet-shroom, coffee, blover, tangle kelp), lawn mower / pool cleaner, ready-set-plant, 1-1 sod roll, graves
  rumbling at night, pause, menu clicks / blips, plant-picker taps, Vasebreaker (vases, plants, zombies, win / lose),
  Zen Garden (water, grow, coins, shop, buzzer) and the present-opening award screen.
* Hooks live in: `audio.cpp`, `game.cpp`, `level.cpp`, `combat.cpp`, `zombie.cpp`, `mower.cpp`, `hud.cpp`, `menu.cpp`,
  `modes.cpp`, `zen.cpp`, `items.cpp`.
* Not wired yet (no matching game feature): Crazy Dave voices, bungee, bowling, Zomboss, rain / thunder, slot machine,
  portal, etc. - they are all available as `SFX_*` ids. Unused ids:
  rvthrow, achievement, boing, bonk, bossboulderattack, bossexplosion, bowling, bowlingimpact, bowlingimpact2, bugspray, bungee_scream, bungee_scream2, bungee_scream3, ceramic, chime, coblaunch, crazydavecrazy, crazydaveextralong1, crazydaveextralong2, crazydaveextralong3, crazydavelong1, crazydavelong2, crazydavelong3, crazydavescream, crazydavescream2, crazydaveshort1, crazydaveshort2, crazydaveshort3, evillaugh, fertilizer, floop, grassstep, gravebusterchomp, gravebutton, hatchback_close, hatchback_open, hydraulic, hydraulic_short, jack_surprise2, juicy, lightfill, loadingbar_flower, loadingbar_zombie, moneyfalls, newspaper_rip, paper, phonograph, portal, rain, reverse_explosion, shoop, siren, slotmachine, slurp, sukhbir, sukhbir2, sukhbir3, sukhbir4, sukhbir5, sukhbir6, swing, tapglass, thunder, wakeup, zombaquarium_die

# Changes in this revision

* **Background no longer stretched** – new `source/backdrop.{h,cpp}` ("lawn camera"). The art is drawn with one
  uniform scale chosen so a lawn lane is always `CELL_HEIGHT_DEFAULT` (64 px) tall; the lawn is centred on the screen and
  the rest is more yard (house, fence, pavement, edge of the road). Grid cell width now follows the painted cells.
  Per-background measurements live in `kArt[]` in `backdrop.cpp`. Used by adventure levels, the 1-1 sod roll-out,
  the final-fight roof and Vasebreaker. `pool.png` is cropped to its yard picture (it is a sprite sheet).
  The flat placeholder lawn / grid lines are skipped when the artwork is present; the pool basin is filled by the game.
* **Plant picker scrolls** – the seed-packet grid on the "Choose your plants" screen follows the cursor row (eased) and has a scroll bar.
* **Vasebreaker** – zombies now show a health bar; uses the same lawn camera.
* `ZOMBIE_SPAWN_COL_OFFSET` 1.4 → 2.4 so zombies still spawn just off-screen with the narrower lawn.
* **Sprites** (see the separate assets archive) – opaque white pockets between zombie legs / cherry stems made transparent.
