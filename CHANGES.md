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
