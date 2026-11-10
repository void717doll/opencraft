# sound assets

block, equipment-break, and pickup sounds come from `~/mineclonia/mods/CORE/mcl_sounds` and `mods/ENTITIES/mcl_item_entity/sounds`; remaining legacy clips retain their recorded minetest-game source. the supplied refi pack contains textures and no audio files, so no sounds are attributed to refi. the original minecraft sound archive is not used.

the playback cues follow luanti's `SoundMaker` and mineclonia's material sound groups: footsteps and landing, water movement, digging, node breaking and placement, player damage, and equipment breaking. footstep timing uses distance traveled and is independent of view bobbing. sneaking suppresses footsteps. glass, ice, gravel, grass, dirt, sand, wood, snow and stone use their corresponding groups; wool uses mineclonia's cloth clips. other engine blocks use the nearest material category rather than a full luanti node-sound api.

all clips are decoded/resampled from their supplied ogg files into mono 22,050 hz signed 16-bit pcm wav. the complete source/destination mapping, authors, licenses, original links, and conversion notes are in [sound-assets.json](sound-assets.json). attribution and upstream notices are also shipped with the audio in `resources/sounds/MINETEST-GAME-CREDITS.txt` and `MINETEST-GAME-LICENSE.txt`. individual clips retain their listed cc by 3.0, cc by-sa 3.0, or cc0 1.0 terms.

the existing sdl library handles audio; no extra runtime library is required. `resources/config/sounds.xml` lists groups and their variants. clips are preloaded and converted to the device format at startup. the audio callback mixes at most 24 voices without allocation, decoding, file access, or logging. local effects use stereo playback; positional block sounds use distance attenuation and left/right placement. variants and small pitch changes avoid repeating one identical sample. missing audio hardware disables playback without stopping the game.

sound volume is available on the main settings page, from zero (mute) to 100 percent, and persists as `soundVolume` in `config/client.lua`. the pause menu and loss of focus clear/pause active effects. inventory and death overlays keep effects audible. returning from pause resumes playback without replaying paused effects.

verification includes the native executable with sdl's dummy audio driver, every configured sound group, overlapping effects, pause/resume, mute, missing groups, and distance culling. dummy-device playback does not verify headphone/speaker output or subjective volume. mob calls, music, weather, and continuous ambient loops are not included in this batch.

native address and undefined-behavior checks passed. the native shutdown leak check reports allocations from an unloaded shared library; the same allocation pattern also occurs with bare sdl initialization and shutdown, without game or audio code. the native leak check is therefore not clean.

pickups play only after the server transfers at least one item, at gain 0.3 and pitch 0.7–1.1. inventory rearrangement and failed pickups stay silent. block breaking and digging use mineclonia material groups and gains. source notices accompany the clips in `MINECLONIA-CREDITS.txt` and `MINECLONIA-ITEM-CREDITS.txt`.

note blocks use five instrument samples from mineclonia’s `mcl_noteblock`: piano, bass guitar, snare, percussion hit, and bass drum. pitch changes span 25 semitones; supporting block materials select the instrument. source notices are in `resources/sounds/MINECLONIA-NOTEBLOCK-CREDITS.txt`, with conversions listed in `sound-assets.json`.
