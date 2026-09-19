#include "common.h"

#include "actors/actors_shared_80133c1c.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// Sound the actor plays when it closes on the player. Stages the XZ offset to
/// the player in an `SVECTOR` carved off the scratchpad stack, and takes the
/// sound when that offset is under 0x708 units long, or while the player is
/// mid-action (`Gp_StateF0.field_2` low nibble) or holding the aim button
/// (`field_19` bit 0). The id is the enemy's work id packed above the 0x4025
/// bank base, with the return flag as its low bit.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700`, at different
/// link addresses - which is why it lives here; the shared span is in
/// `configs/USA/overlays.toml`.
s32 ActorsShared80133c1c(ActorShared80133c1c* arg0)
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
    coord    = arg0->field_2C->coords;
    vec->vx  = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    dz       = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    *scratch = vec;
    vec->vz  = dz;
    dx       = ((SVECTOR*)(head - 8))->vx;
    ret      = 0;
    if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x708) || (Gp_StateF0.field_2 & 0xF) || (Gp_StateF0.field_19 & 1)) {
        ret = 1;
        Gp_ArmStateF0(ret);
        soundId   = arg0->field_20->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x40250000 | ret;
        pan       = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(coord));
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
    return ret;
}
