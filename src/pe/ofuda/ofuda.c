#include "pe/ofuda.h"

#include "types.h"

#include "gameplay/display.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// This overlay's id, the `u16` every package opens with.

/// PROVISIONAL: written before `Task` was processed, so the statements
/// about `Task` fields rest on unverified names. Rewrite once `Task` is done.
/// Draws the expanding flash the ofuda produces, over three states.
///
/// Spawned with a `EffectWork` block in `Task::spawnArg2` holding the effect's
/// brightness, ring radius and per-frame step, and a coordinate reached through
/// `Task::extra`. `Task::spawnArg1` counts the frames of the growing phase down
/// to zero.
///
/// State 0 arms a 30-frame growth and starts the sound cue panned to where the
/// object is. State 1 brightens and widens the ring each frame, drawing it at
/// two radii plus two arcs, and on the last frame snaps to full brightness and
/// hands over to state 2. State 2 shrinks and dims until the brightness falls
/// below 9, then releases the block.
///
/// A cancelled or interrupted cast stops the cue and releases immediately.
void ofudaEffectTask(Task* arg0)
{
    EffectWork*      mem;
    GfxCoord*        coord;
    AttachmentState* state;
    s32              pan;
    u8               rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_OFUDA_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        goto kill;
    }

    mem->age++;
    switch (arg0->state) {
        case 0:
            arg0->spawnArg1.value = 0x1E;
            mem->scale            = 0;
            mem->angle            = 0x100;
            mem->step             = 0x100 / arg0->spawnArg1.value;
            arg0->state           = 1;
            pan                   = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(SOUND_OFUDA_USE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            mem->scale += mem->step;
            mem->angle += mem->step << 3;
            arg0->spawnArg1.value--;
            rgb[0] = mem->scale;
            rgb[1] = mem->scale >> 2;
            rgb[2] = mem->scale >> 1;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)(mem->angle << 1), rgb);
            Gp_DrawArc(coord, (s16)((arg0->spawnArg1.value << 4) + 0x800), 0x100, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, (s16)((arg0->spawnArg1.value << 5) + 0xC00), 0xC0, rgb);
            if (arg0->spawnArg1.value == 0) {
                mem->scale    = 0xFF;
                arg0->state   = 2;
                mem->period   = 0x600;
                mem->step     = 0;
                state->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            }
            return;
        case 2:
            if (mem->scale < 9) {
                goto kill;
            }
            rgb[0] = mem->scale;
            rgb[1] = mem->scale >> 2;
            rgb[2] = mem->scale >> 1;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)(mem->angle << 1), rgb);
            mem->scale -= 8;
            mem->angle -= 0x30;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_DrawFadeQuad(rgb, 1);
            return;
    }
    return;
kill:
    effectKillTask(mem, arg0);
}
