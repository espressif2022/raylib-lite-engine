# Pocket Lighthouse: Pirate Defense (`pirate_island`)

A hand-drawn storybook 3-in-1 puzzle & arcade defense game with between-round Roguelite upgrades for ESP-Mosaico (`480x480`) and ESP32-S3-BOX-3 (`320x240`):

1. **Act 1 — Swipe Gates + Switch Bridge**: Slide two vertical wooden X-gates across three bridge lanes to bounce pirates into the sea (`SPLASH!`) while letting friendly straw-hat villagers pass. Tap the center switch bridge (`变道`) to divert runners across lanes and counter lane-leaping jumper pirates.
2. **Act 2 — Clue Deduction Pirate Hunt**: Read the round's randomly rolled intelligence clue (`黑帽+持弯刀`, `紫色头巾+抱炸药桶`, or `偷戴草帽+拿金币`) and tap only the matching suspects hiding in cottage windows, diamond-leaf trees, berry bushes, and the lighthouse.
3. **Act 3 — 7×7 Hex-Grid Trap + Golden Beacons**: Place hand-drawn cottages and trees to surround the pirate leader before its BFS shortest-path AI reaches the shore. Watch the red prediction ring, block its 3-turn 2-step dash charge, and build on Golden Beacons to sprout a bonus adjacent tree.
4. **Island Workshop (Upgrades)**: After each 3-act round, pick 1 of 3 randomized upgrades (`加宽木闸`, `鹰眼探照灯`, `预筑路障`, `加固宝库`) that scale into higher rounds.

## Host Simulation

```bash
python3 host/run_game.py --project examples/pirate_island --port 8462
```
