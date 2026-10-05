/* Continue room_visual_effects.inc.c after the preceding overlay wrappers. */

/// Runs a pink flash that charges, tints the screen at its peak, then fades as a star.
///
/// `spawnArg2` owns an `EffectWork` released through `effectKillTask`, and the
/// task's extra body supplies its coordinate. `spawnArg1.value` must initially
/// be a positive charge duration in ticks; it is consumed as a countdown. The
/// first tick initializes the ramp. Later charge ticks draw two discs and an
/// inward ring; the fade reduces brightness by 16 and its base radius by eight
/// per tick (the star drawer receives three times that base radius).
/// Room effect control pauses the task when nonzero and cancels it at four or
/// above. State 3 also releases the work block.
static inline void _roomVisualEffectsFlashTask(Task* task)
{
    enum { FLASH_INITIALIZE,
           FLASH_CHARGE,
           FLASH_FADE,
           FLASH_RELEASE,
           FLASH_RAMP_SPAN  = 0x100,
           FLASH_PEAK_LEVEL = 0xFF,
           FLASH_FADE_STEP  = 0x10 };

    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        actorRenderComposeCoord(coord);
        work->age++;
        switch (task->state) {
            case FLASH_INITIALIZE:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = FLASH_RAMP_SPAN / task->spawnArg1.value;
                task->state = FLASH_CHARGE;
                break;
            case FLASH_CHARGE:
                // Consume the charge countdown and emit the peak screen tint once.
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                _roomVisualEffectsDrawFlashDisc(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                _roomVisualEffectsDrawFlashDisc(coord, (s16)((u16)work->angle * 2), rgb);
                _roomVisualEffectsDrawFlashRing(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = FLASH_PEAK_LEVEL;
                    task->state = FLASH_FADE;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                }
                break;
            case FLASH_FADE:
                // Fade the star until another 16-level decrement would exhaust it.
                if (work->scale > FLASH_FADE_STEP) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    _roomVisualEffectsDrawBurstStar(coord, work->angle * 3, rgb);
                    work->scale -= FLASH_FADE_STEP;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case FLASH_RELEASE:
                effectKillTask(work, task);
                break;
        }
    }
}
