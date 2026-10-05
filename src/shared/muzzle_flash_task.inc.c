#include "main/random.h"

/* Part of the muzzle flash library; see muzzle_flash.h. */

/// Per-frame muzzle-flash task. Frame 0 enables transient point-light slot 0
/// as a white 0x1000 light at the weapon's world position, parks the task's own
/// coordinate on the muzzle offset under the hand frame, and rolls the flash
/// size (`scale`), its spin (`angle`) and the four streak angles; every
/// later frame just halves the size and the brightness. Each frame then draws
/// the core (`muzzleFlashDrawCore`), a full-screen fade at the current
/// brightness and the four streaks, decays the light's range by 0x190 and
/// releases the pool block after seven frames. Nothing runs at all once
/// effects are hidden (`gRoomEffectState->effectControl` >=
/// `ROOM_EFFECT_CONTROL_HIDDEN`).
static inline void muzzleFlashTask(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    u8                             rgb[3];
    s32                            i;

    work      = (EffectWork*)task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[0];
    slot      = &lightSlot->light;

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }

    work->age++;
    switch (task->state) {
        case 0:
            slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
            slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
            slot->head.transform.coord.coord.t[2]              = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            slot->head.color.r                                 = 0x1000;
            slot->head.color.g                                 = 0x1000;
            slot->head.color.b                                 = 0x1000;
            slot->inner                                        = 0xFA0;
            slot->outer                                        = 0x12C0;
            lightSlot->framesLeft                              = 4;

            coord->parent       = work->parent;
            coord->coord.t[0]   = _gMuzzleOffset.vx;
            coord->coord.t[1]   = _gMuzzleOffset.vy;
            coord->coord.t[2]   = _gMuzzleOffset.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);

            work->period    = 0xC0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & 0x3FF) + 0x600;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & 0xFFF;
            task->state     = 1;
            for (i = 0; i < 4; i++) {
                gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gMuzzleFlashAngles[i] = ((i & 3) << 10) + ((gRandomLcgState >> 16) & 0x3FF);
            }
            break;
        case 1:
            /* The `(u16)` casts are load-shape, not arithmetic: the ROM reads
               both fields with `lhu` and sign-extends in the shift pair
               (`sll 16` / `sra 17`). A plain `>>= 1` on the `s16` field emits
               `lh` / `sra 1` instead. */
            work->scale  = work->scale >> 1;
            work->period = work->period >> 1;
            break;
    }

    muzzleFlashDrawCore(coord, work->scale, work->angle);
    /* Chained on purpose: it is one `lbu` stored three times, in reverse index
       order. Three separate assignments reload the field each time, because the
       stores into `rgb` may alias it. */
    rgb[0] = rgb[1] = rgb[2] = work->period;
    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
    for (i = 0; i < 4; i++) {
        muzzleFlashDrawStreak(coord, gMuzzleFlashAngles[i], work->period);
    }
    if (slot->inner >= 0x191) {
        slot->inner -= 0x190;
    }
    if (work->age >= 7) {
        effectKillTask(work, task);
    }
}
