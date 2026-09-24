#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// Tests whether the actor has noticed the player: true when the player is
/// under 0x708 units away on the XZ plane (the offset is staged on the
/// scratchpad stack), mid-action (`Gp_StateF0.field_2` low nibble) or holding
/// the aim button (`field_19` bit 0). On noticing, it arms `Gp_StateF0` and
/// plays the alert cue from the placement's sound bank. Returns 1 when noticed.
s32 Actor03700_Fn01DFC(Task* task)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GsCOORDINATE2* coord;
    s16            dx;
    s16            dz;
    s32            ret;
    u32            soundId;
    s32            pan;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (SVECTOR*)(head - 8);
    coord    = ((TmdObject*)task->extra)->coords;
    vec->vx  = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    dz       = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    *scratch = vec;
    vec->vz  = dz;
    dx       = ((SVECTOR*)(head - 8))->vx;
    ret      = 0;
    if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x708) || (Gp_StateF0.field_2 & 0xF) || (Gp_StateF0.field_19 & 1)) {
        ret = 1;
        Gp_ArmStateF0(ret);
        soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x40250000 | ret;
        pan       = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(coord));
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
    return ret;
}
