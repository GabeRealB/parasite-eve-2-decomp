#include "common.h"

#include "main/fs.h"
#include "main/sound.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_402200.h"
#include "actors/actors_shared_80136184.h"

#include "psyq/inline_c.h"

/// `rtv0` / `rtps`: the `inline_c.h` macros of those names assemble to
/// different words.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one.
extern s32 D_actor_402200_80138420[];

/// The spawn's tables: the task's next handler record, the `GpU16Pair`
/// `Gp_PackPair` packs into the third collision object, the `GpPairSrcE` whose
/// `hpMax` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
extern s32                D_actor_402200_8013839C[];
extern GpU16Pair          D_actor_402200_80153BEC;
extern GpPairSrcE         D_actor_402200_80153BFC;
extern Actor402200Spot    D_actor_402200_80153C78[];
extern Actor402200Region* D_actor_402200_80153FA8[];
extern s16*               D_actor_402200_80154144[];
extern u8                 D_actor_402200_80154194[];

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_5", ActorsShared80135df4Table);

void func_actor_402200_80135A24(Actor402200* arg0)
{
    Actor402200Work*  work;
    Actor402200Coord* coord;
    s32               state;
    s32               snd;
    s32               pan;
    s32               frames;
    s16               timer;

    work  = arg0->field_1C;
    state = work->field_6CE;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_6D4 = 0x42;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 1;
                work->field_6F0 = 2;
                work->field_6D4 = 0x31;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 1;
            work->field_6DC  = 0x14;
            work->field_6DE  = 0xA;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_714 == state) {
                work->field_714 = 2;
            }
            frames = 0x19;
            if (work->field_6F0 == state) {
                frames = 0x2C;
            }
            if (work->field_6C4 == frames) {
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                arg0->field_30  = 2;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
}

/// Fires the cue pair the work block's `field_712` selects: while the second
/// animation slot carries `flags` bit 0x20 or 0x10, a sound is queued on the
/// frame that bit has just dropped from `Actor402200Work::field_6CA`, panned
/// and depth-attenuated from the actor's display object. The cue id is the
/// matching word of `D_actor_402200_80138420` with the `GpEnemy` work id's high
/// nibble in bits 8-11, and a zero `field_712` disarms the body. The record's
/// two bits are latched for the next frame at the end.
void func_actor_402200_80135BE0(Actor402200* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor402200Work*  work;
    Actor402200Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_712 != 0) {
        rec = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->field_3C);
        if (rec != NULL) {
            if (!(rec->flags & 0x20) && (work->field_6CA & 0x20)) {
                snd = D_actor_402200_80138420[work->field_712 * 2 - 1] | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            if (!(rec->flags & 0x10) && (work->field_6CA & 0x10)) {
                snd  = D_actor_402200_80138420[work->field_712 * 2] | (((u16)arg0->field_20->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
            }
            work->field_6CA = (u16)(rec->flags & 0x30);
        }
    }
}

/// Aims the actor off its fourth part. While `field_6D6` is positive the
/// offset (-0x28, -0x78, 0xDC) through the root-to-part matrix lands in
/// `field_634`..`field_638` with bits 0xC000 of `field_62A` raised. Below 0x13
/// it projects two points into `field_6FC`..`field_704`: the same offset off
/// the part, and a point 0x514 up and the `field_644` target's distance out
/// from the root.
void func_actor_402200_80135D5C(Actor402200* arg0)
{
    u8*                    head;
    Actor402200AimScratch* sc;
    Actor402200Work*       work;
    Actor402200Coord*      coord;
    GsCOORDINATE2*         part;
    s32                    i;
    s16                    dist;

    coord                 = arg0->field_2C->field_8;
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor402200AimScratch);
    sc                    = (Actor402200AimScratch*)(head - sizeof(Actor402200AimScratch));
    work                  = arg0->field_1C;
    part                  = &coord->field_F0 + 1;
    coord->field_0.flg    = 0;
    part->flg             = 0;
    Gp_UpdateCoord(part);
    if (work->field_6D6 > 0) {
        Gp_WorldToLocal(&coord->field_0.workm, &part->workm, &sc->m);
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->m);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        work->field_634  = sc->m.t[0] + sc->out.vx;
        work->field_636  = sc->m.t[1] + sc->out.vy;
        work->field_638  = sc->m.t[2] + sc->out.vz;
        work->field_62A |= 0xC000;
    } else {
        work->field_62A &= 0x3FFF;
    }
    if (work->field_6D6 < 0x13) {
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&part->workm);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        sc->pts[1].vx = part->workm.t[0] + sc->out.vx;
        sc->pts[1].vy = part->workm.t[1] + sc->out.vy;
        sc->pts[1].vz = part->workm.t[2] + sc->out.vz;
        sc->pts[0].vx = 0;
        sc->pts[0].vy = -0x514;
        if (Gp_FindRec18(&work->field_644, 0) != 0) {
            sc->out.vx    = work->field_644.point.vx - sc->pts[1].vx;
            sc->out.vy    = work->field_644.point.vy - sc->pts[1].vy;
            sc->out.vz    = work->field_644.point.vz - sc->pts[1].vz;
            dist          = SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vy * sc->out.vy + sc->out.vz * sc->out.vz);
            sc->pts[0].vz = dist;
            if ((work->field_644.key & 0xFFFF0000) == 0x10000) {
                sc->pts[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(&work->field_644);
        } else {
            sc->pts[0].vz = 10000;
        }
        gte_SetRotMatrix(&coord->field_0.workm);
        gte_ldv0(&sc->pts[0]);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        sc->pts[0].vx = coord->field_0.workm.t[0] + sc->out.vx;
        sc->pts[0].vy = coord->field_0.workm.t[1] + sc->out.vy;
        sc->pts[0].vz = coord->field_0.workm.t[2] + sc->out.vz;
        for (i = 0; i < 2; i++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&sc->pts[i]);
            gte_rtps_real();
            gte_stsxy(&sc->sxy);
            gte_stszotz(&sc->otz);
            work->field_6FC[i] = sc->sxy;
            work->field_700[i] = sc->sxy >> 16;
            work->field_704[i] = sc->otz;
        }
        ActorsShared80136184((ActorShared80136184*)arg0);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200AimScratch);
}
