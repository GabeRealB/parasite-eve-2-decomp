#include "common.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/session.h"

#include "gameplay/D4.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/gameflag.h"
#include "main/wipsys.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"
#include "actors/actors_shared_8013bbe4.h"

void func_actor_510900_80135744(Actor510900* arg0);
void func_actor_510900_8013864C(Actor510900* arg0);
void func_actor_510900_801387F4(Actor510900* arg0);
void func_actor_510900_80138978(Actor510900* arg0);
void func_actor_510900_80138A9C(Actor510900* arg0);
void func_actor_510900_80138BF0(Actor510900* arg0);
void func_actor_510900_80138D38(Actor510900* arg0);
void func_actor_510900_80138F44(Actor510900* arg0);
void func_actor_510900_8013B804(Actor510900* arg0);
void func_actor_510900_8013BB20(Actor510900* arg0);
void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1);
void func_actor_510900_8013BC80(Actor510900* arg0);

extern u8  D_801153F4;
extern u32 Gp_LcgState;
extern s16 D_80073BA0;

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80131F24);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80132D4C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801332EC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013371C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80133C84);

void func_actor_510900_801340E8(Task* arg0)
{
    Actor510900Cam*         base;
    GsCOORDINATE2*          cam;
    Actor510900CamCoord*    ext;
    GpEffWork*              eff;
    Actor510900Coord*       coord;
    Actor510900MatrixWords* mat;
    s32                     i;

    base  = &D_8011505C;
    cam   = &base->cam.coord;
    eff   = arg0->spawnArg2;
    coord = ((Actor510900Obj2C*)arg0->extra)->field_8;
    ext   = (Actor510900CamCoord*)cam;
    if (Gp_State1C->field_4 != 0) {
        Gp_ReleaseState1CMem(eff, arg0);
        return;
    }
    mat                       = (Actor510900MatrixWords*)&coord->field_0.coord;
    coord->field_0.sub        = eff->field_8;
    mat->m00_m01              = 0x1000;
    mat->m02_m10              = 0;
    mat->m11_m12              = 0x1000;
    mat->m20_m21              = 0;
    mat->m22                  = 0x1000;
    coord->field_0.coord.t[0] = eff->field_18;
    coord->field_0.coord.t[1] = eff->field_1A;
    coord->field_0.coord.t[2] = eff->field_1C;
    coord->field_0.flg        = 0;
    Gp_UpdateCoord(&coord->field_0);
    eff->field_10 = -0x200;
    eff->field_12 = 0x40;
    eff->field_14 = 0;
    Gp_SpawnEff(0x6003B, &coord->field_0, 0x180, (SVECTOR*)&eff->field_10);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(0x60065, &coord->field_0, 0, (SVECTOR*)&eff->field_10);
        Gp_SpawnEff(0x600A4, &coord->field_0, 1, NULL);
    }
    base->field_0 = 4;
    ext->field_58 = 0xFA0;
    ext->field_5C = 0x12C0;
    ext->rot.vx   = 0xC00;
    ext->rot.vy   = 0x800;
    ext->rot.vz   = 0x400;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->field_0.workm, &cam->coord);
    cam->flg = 0;
    Gp_ReleaseState1CMem(eff, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134284);

void func_actor_510900_801346D4(Task* arg0)
{
    GpEffWork*     eff;
    GsCOORDINATE2* coord;
    s16            mode;

    eff   = arg0->spawnArg2;
    mode  = Gp_State1C->field_4;
    coord = &((Actor510900Obj2C*)arg0->extra)->field_8->field_0;
    if (mode != 0) {
        if (mode >= 4 || arg0->state == 4) {
            Gp_ReleaseState1CMem(eff, arg0);
        }
        return;
    }
    eff->field_22++;
    switch (arg0->state) {
        case 0:
            eff = Gp_SpawnEff(0x60184, coord, 0x480, NULL);
            if (eff != NULL) {
                Task_Reparent(arg0, eff->field_0);
            }
            arg0->state++;
            break;
        case 1:
            if (eff->field_22 >= 9) {
                arg0->state++;
            }
            break;
        case 2:
            Gp_SpawnEff(0x60070, coord, 0x82004400, NULL);
            if (eff->field_22 >= 0x33) {
                arg0->state++;
            }
            break;
        case 3:
            Gp_SpawnEff(0x60070, coord, 0xD2004400, NULL);
            if (eff->field_22 >= 0x3D) {
                arg0->state++;
            }
            break;
        case 4:
            Gp_ReleaseState1CMem(eff, arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80134C90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801350F8);

void func_actor_510900_801355B4(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* coord;
    Actor510900Work*  work;
    s32               snd;
    s32               pan;
    s32               pan2;
    s32               i;

    work           = arg1->field_1C;
    coord          = arg1->field_2C->field_8;
    arg0->field_14 = 1;
    if (work->field_586 == 0x20 && work->field_58A == 0xD2) {
        work->field_594 = 1;
        work->field_598 = 0xFF;
        snd             = (((u16)arg0->field_8 >> 0xC) << 8) | 0x4078000E;
        pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        work->field_580 = (((u16)arg0->field_8 >> 0xC) << 8) | 0x40780011;
        pan2            = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(work->field_580, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    work->field_58A++;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    coord->field_0.flg = 0;
    Gp_UpdateCoord(&coord->field_0);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80135744);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80135E90);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013691C);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80136B70);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137008);

/// Handler that spawns the actor's companion enemy once the 0x14 animation has
/// blended in (`field_58A` == 0x14): the spawned task's model takes its texture
/// page and CLUT row from the current area record, indexed by the enemy's own
/// bank nibble, and a sound is queued from the actor's attach coordinate. Past
/// blend 0x46 the handler leaves for either state 5 (animation 0xB) or, on a
/// failed `Gp_LcgState` roll, state 1 with a fresh `field_59C`.
void func_actor_510900_801373B8(Actor510900* arg0)
{
    Actor510900Work*  work;
    GpEnemy*          enemy;
    Actor510900Coord* coord;
    GameSessionFrom4* sessionKey;
    TmdObject*        model;
    GpAreaRec*        rec;
    GpCdRec10*        entry;
    GpAreaKey         key;
    s32               idx;
    s32               snd;
    s32               pan;
    s32               roll;
    u32               rng;

    roll  = 0;
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    enemy = arg0->field_20;

    work->field_5A2 = 0;
    if (work->field_58A == 0x14) {
        work->field_5BA = 1;
        model           = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 4, roll, enemy)->task->extra;
        idx             = (u16)enemy->field_8 >> 0xC;
        sessionKey      = (GameSessionFrom4*)&Game_Session->field_4;
        key.field_3     = sessionKey->field_3;
        key.field_2     = sessionKey->field_2;
        key.field_1     = sessionKey->field_1;
        key.field_0     = Game_Session->field_4;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
        snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780012;
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    if (work->field_58A >= 0x46) {
        if (work->field_5AC >= 0xEA7) {
            /* Reading the global back is what keeps the store ahead of the
               shift: written as a local, the store sinks into the branch
               delay slot and the draw lands in a different register. */
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((Gp_LcgState >> 0x10) & 0xF) < 0xCU) {
                roll = 1;
            }
        }
        if (roll == 0) {
            work->field_58E = 1;
            work->field_586 = 1;
            work->field_590 = 0;
            work->field_59C = D_actor_510900_801679D0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 0x10) & 0xF];
            Gp_LcgState     = rng;
            return;
        }
        work->field_58E = 5;
        work->field_590 = 0;
        work->field_586 = 0xB;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801375D8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137868);

void func_actor_510900_80137E20(Actor510900* arg0)
{
    Actor510900Work*  work;
    GpEnemy*          enemy;
    Actor510900Coord* coord;
    s32               snd;
    s32               pan;
    s32               rng;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    coord = arg0->field_2C->field_8;
    switch (work->field_590) {
        case 0:
            work->field_586 = 0x10;
            work->field_5A2 = 0;
            work->field_590 = 1;
            snd             = (((u16)enemy->field_8 >> 0xC) << 8) | 0x40780005;
            pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            break;
        case 1:
            if (work->field_5B0 >= 0xC8 && work->field_5AA == work->field_5A8) {
                if ((u16)work->field_58A >= 3 && (u16)work->field_58A < 13) {
                    work->field_5A2 = -0x2C;
                } else if ((u16)work->field_58A >= 0x12 && (u16)work->field_58A < 0x27) {
                    work->field_5A2 = -0x1E;
                } else {
                    work->field_5A2 = 0;
                }
            } else {
                work->field_5A2 = 0;
            }
            if (work->field_58A >= 0x53) {
                if (enemy->field_40 <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = 1;
                    work->field_586 = 1;
                    work->field_590 = 0;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                    Gp_LcgState = rng;
                }
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80137FBC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138250);

void func_actor_510900_801384C4(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    s32               snd;
    s32               pan;
    u32               rng;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (D_80073BA0 <= 0) {
        arg0->field_20->field_40 = 1;
        work->field_58E          = 1;
        work->field_586          = 1;
        work->field_590          = 0;
        work->field_59C          = D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState              = rng;
        return;
    }
    if (work->field_594 != 0) {
        work->field_594 = 3;
    }
    work->field_598 = 0;
    if (work->field_578 != 0) {
        SndEvt_EnqueueType7(work->field_578, 0);
        work->field_578 = 0;
    }
    if (work->field_57C != 0) {
        SndEvt_EnqueueType7(work->field_57C, 0);
        work->field_57C = 0;
    }
    if (work->field_580 != 0) {
        SndEvt_EnqueueType7(work->field_580, 0);
        work->field_580 = 0;
    }
    arg0->field_20->node.field_4 = 1;
    if (work->field_58A == 0x70) {
        snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780007;
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    Gp_StateC08.field_6 &= 1;
    work->field_592      = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013864C);

void func_actor_510900_801387F4(Actor510900* arg0)
{
    Actor510900Work*  work;
    Actor510900Coord* coord;
    SVECTOR*          rot;
    void*             head;
    u16               target;
    u16               cur;
    s32               d;
    s16               diff;
    s32               sdiff;
    s32               mag;
    s32               prev;
    s32               prev2;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_5A2 != 0) {
        head                = *(void**)0x1F8003FC;
        *(void**)0x1F8003FC = (u8*)head - 8;
        rot                 = (SVECTOR*)((u8*)head - 8);
        if (work->field_5AE < 0x3E8) {
            target = D_actor_510900_80167B9C[work->field_5A8 + 1];
        } else {
            target = D_actor_510900_80167B9C[work->field_5A8];
        }
        cur             = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
        d               = target - cur;
        diff            = d;
        sdiff           = (s16)d;
        mag             = __builtin_abs(sdiff);
        work->field_5A0 = cur;
        if (mag < 0x800) {
            if (mag < 0x1F) {
                work->field_5A0 = target;
            } else {
                prev = work->field_5A0;
                if (sdiff > 0) {
                    work->field_5A0 = prev + 0x1E;
                } else {
                    work->field_5A0 = prev - 0x1E;
                }
            }
        } else if (sdiff > 0 ? (0x1000 - sdiff) < 0x1F : (sdiff + 0x1000) < 0x1F) {
            work->field_5A0 = target;
        } else {
            prev2 = work->field_5A0;
            if (diff > 0) {
                work->field_5A0 = prev2 - 0x1E;
            } else {
                work->field_5A0 = prev2 + 0x1E;
            }
        }
        rot->vx = 0;
        rot->vy = work->field_5A0;
        rot->vz = 0;
        RotMatrix(rot, &coord->field_0.coord);
        *(u32*)0x1F8003FC += 8;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138978);

/// Fires the actor's step sounds: while the current animation record carries
/// `field_3` bit 0x20 or 0x10, a sound is queued on the frame that bit has just
/// dropped from `Actor510900Work::field_59A`, panned and depth-attenuated from
/// the actor's attach coordinate. The record's two bits are latched for the
/// next frame at the end.
void func_actor_510900_80138A9C(Actor510900* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor510900Work*  work;
    Actor510900Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_59A & 0x20)) {
            snd = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780001;
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_59A & 0x10)) {
            snd  = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40780002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_59A = (u16)(rec->field_3 & 0x30);
    }
}

/// Aims the head coordinate (`field_8[4]`) at the player. Takes the head into
/// view space, offsets the player position by 0x600 in Y, rotates that delta
/// into the body's frame, clamps it to +/-0x400 yaw, +/-0x300 pitch and a
/// minimum 0x200 forward, then builds the head rotation from it.
///
/// `head` is kept as its own pointer rather than indexing `coord` twice: CSE
/// folds `head->workm` back onto `coord + 0x164` while `head` stays live, which
/// is what puts the `coord += 0x140` in the clamp's branch delay slot. The
/// `+ 0x600` likewise needs the temporary, or it is sunk into the subtrahend as
/// `- 0x600` on the player coordinate.
void func_actor_510900_80138BF0(Actor510900* arg0)
{
    Actor510900AimScratch* scratch;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         head;
    s32                    offsetY;

    coord                   = &arg0->field_2C->field_8->field_0;
    head                    = &coord[4];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor510900AimScratch);
    scratch                 = (Actor510900AimScratch*)*(void**)G_SCRATCH_HEAD;

    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &head->workm, &scratch->view);
    scratch->delta.vx = Wip_SysConfig.field_4->t[0] - scratch->view.t[0];
    offsetY           = scratch->view.t[1] + 0x600;
    scratch->delta.vy = Wip_SysConfig.field_4->t[1] - offsetY;
    scratch->delta.vz = Wip_SysConfig.field_4->t[2] - scratch->view.t[2];
    ApplyTransposeMatrixLV(&coord->coord, &scratch->delta, &scratch->local);

    if (scratch->local.vx < -0x400) {
        scratch->local.vx = -0x400;
    } else if (scratch->local.vx > 0x400) {
        scratch->local.vx = 0x400;
    }
    if (scratch->local.vy < -0x300) {
        scratch->local.vy = -0x300;
    } else if (scratch->local.vy > 0x300) {
        scratch->local.vy = 0x300;
    }
    if (scratch->local.vz < 0x200) {
        scratch->local.vz = 0x200;
    }
    Gp_OrientAlong(&scratch->local, &head->coord, 0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor510900AimScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138D38);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80138F44);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801391B8);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801395AC);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_801397F0);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_80139C10);

void func_actor_510900_8013A310(Task* task);

/// Frame handler of the effect child task: state 0 fades the object in over
/// 0x10 frames, state 1 holds it until its `GpRec18` reports a hit or 0x1F
/// frames pass, state 2 runs the hit handler, and state 3 unlinks the object
/// and destroys the enemy.
void func_actor_510900_8013A100(GpEnemy* enemy, Task* task)
{
    Actor510900ChildFx* work;
    Actor510900Work*    parent;
    u16                 tick;

    work   = (Actor510900ChildFx*)task->idMap;
    parent = (Actor510900Work*)task->parent->idMap;
    if (D_801153F4 == 0) {
        switch (work->field_CA) {
            case 0:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x10) {
                    work->obj40.field_1C = 0x258;
                    work->field_CA       = 1;
                    work->field_C8       = 0;
                    work->obj40.flags   |= 0x8000;
                    return;
                }
                if (parent->field_592 == 0) {
                    work->obj40.flags &= 0x7FFF;
                    Gp_UnlinkObj(&work->obj78);
                    work->field_C8                            = 0;
                    ((Actor510900Obj2C*)task->extra)->field_C = 0x80;
                    task->state                               = 2;
                    work->field_CA                            = 3;
                    return;
                }
                break;
            case 1:
                if ((work->rec60.field_4 & 0xFFFF0000) == 0x10000) {
                    work->field_CA     = 2;
                    work->field_CC     = 0;
                    work->obj40.flags &= 0x7FFF;
                } else {
                    tick           = work->field_C8 + 1;
                    work->field_C8 = tick;
                    if (tick >= 0x1F) {
                        work->field_CA = 3;
                        work->field_C8 = 0;
                    } else if (parent->field_592 == 0) {
                        work->obj40.flags &= 0x7FFF;
                        Gp_UnlinkObj(&work->obj78);
                        work->field_C8                            = 0;
                        ((Actor510900Obj2C*)task->extra)->field_C = 0x80;
                        task->state                               = 2;
                        work->field_CA                            = 3;
                    }
                }
                Gp_ClearRec18Occupied(&work->rec60);
                return;
            case 2:
                func_actor_510900_8013A310(task);
                return;
            case 3:
                tick           = work->field_C8 + 1;
                work->field_C8 = tick;
                if (tick >= 0x1F) {
                    parent->field_5BA = 0;
                    Gp_UnlinkObj(&work->obj40);
                    Gp_DestroyEnemy(enemy, task);
                }
                break;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A310);

/// Spawn handler of the child task: allocates the animation work block, seeds
/// the model's root coordinate from the spawn-index tables, resets animation
/// slots 1..10 and links the two render objects.
void func_actor_510900_8013A5B8(GpEnemy* enemy, Task* task)
{
    TmdObject*            tmd;
    GsCOORDINATE2*        coords;
    Actor510900ChildAnim* work;
    GsCOORDINATE2*        coord;
    SVECTOR*              rot;
    void*                 head;
    s32                   i;

    tmd    = task->extra;
    coords = tmd->field_8;
    work   = Mem_Calloc(sizeof(Actor510900ChildAnim), 0);
    coord  = &coords[10];
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->idMap         = (TaskIdMap*)work;
    tmd->field_C        = 0x80;
    coords->flg         = 0;
    tmd->field_1C       = &work->lightMtx;
    tmd->field_20       = &work->colorMtx;
    head                = *(void**)0x1F8003FC;
    enemy->field_4      = &coords->coord;
    rot                 = (SVECTOR*)(head - 8);
    *(void**)0x1F8003FC = head - 8;
    enemy->field_48     = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4        = 1;
    enemy->field_18            = coord;
    enemy->field_1C.vx         = -0xC8;
    enemy->field_1C.vy         = 0;
    enemy->field_1C.vz         = 0;
    work->field_334            = task->spawnArg1;
    ((SVECTOR*)(head - 8))->vx = 0;
    rot->vy                    = D_actor_510900_80167CD0[work->field_334];
    rot->vz                    = 0;
    RotMatrix(rot, &coords->coord);
    i                  = 1;
    coords->coord.t[0] = D_actor_510900_80167CB8[work->field_334].vx;
    coords->coord.t[1] = D_actor_510900_80167CB8[work->field_334].vy;
    coords->coord.t[2] = D_actor_510900_80167CB8[work->field_334].vz;
    coords->sub        = &Gfx_ViewCoord;
    func_800B3F84(&work->anim, D_actor_510900_80167CAC, (GpAnimObj*)tmd, work->poses, work->slots);
    do {
        Gp_AnimResetSlot(&work->anim, i, 1);
        i++;
    } while (i < 0xB);
    work->obj2BC.field_10 = -0xC8;
    work->obj2BC.field_8  = coord;
    work->obj2BC.field_12 = 0;
    work->obj2BC.field_14 = 0;
    work->obj2BC.field_C  = &work->rec2DC;
    work->obj2BC.field_18 = 0;
    work->obj2BC.field_1C = 0xC8;
    work->obj2BC.flags    = 1;
    Gp_LinkObj(2, &work->obj2BC);
    Gp_InitRec18Table(&work->rec2DC, 1, 0);
    work->obj2F4.field_18 = 0x50002;
    work->obj2F4.field_8  = coord;
    work->obj2F4.field_10 = 0;
    work->obj2F4.field_12 = 0;
    work->obj2F4.field_14 = 0;
    work->obj2F4.field_C  = &work->rec314;
    work->obj2F4.field_1C = 0x15E;
    work->obj2F4.flags    = 1;
    work->obj2BC.flags   &= 0x7FFF;
    Gp_LinkObj(8, &work->obj2F4);
    Gp_InitRec18Table(&work->rec314, 1, 0);
    work->obj2F4.flags &= 0x7FFF;
    task->exitCallback  = (TaskFunc)func_actor_510900_8013C380;
    task->state         = 1;
    *(u32*)0x1F8003FC  += 8;
}

void func_actor_510900_8013A9BC(Task* task);
s32  func_actor_510900_8013C240(Task* task);
void func_actor_510900_8013C338(Actor510900* arg0, Actor510900Coord* arg1);

/// Frame handler (state 1) of the child task. Mode 1 of `D_801153F4` only
/// redraws, mode 2 hides the model and flags the context, and mode 0 ticks the
/// animation until `func_actor_510900_8013C240` reports ready before falling
/// into the normal body.
void func_actor_510900_8013A85C(Actor510900Ctx* arg0, Task* arg1)
{
    Actor510900Obj2C*     obj;
    Actor510900ChildAnim* work;
    Actor510900Coord*     coord;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   i;
    s32                   one;

    obj    = (Actor510900Obj2C*)arg1->extra;
    work   = (Actor510900ChildAnim*)arg1->idMap;
    coord  = obj->field_8;
    parent = (Actor510900Work*)arg1->parent->idMap;
    mode   = D_801153F4;
    one    = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if (func_actor_510900_8013C240(arg1) == 0) {
        i = 1;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0xB);
        return;
    }
    ((Actor510900Obj2C*)arg1->extra)->field_C = 0;
    arg0->field_14                            = one;
    goto body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
body:
    func_actor_510900_8013A9BC(arg1);
    if (work->field_334 < 2) {
        GameFlag_SetNibble(work->field_334 + 0xB, work->field_336);
    } else {
        parent->field_5C2 = work->field_336;
    }
    i = 1;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 0xB);
    coord->field_0.flg = 0;
    Gp_UpdateCoord(&coord->field_0);
case1:
    func_actor_510900_8013C338((Actor510900*)arg1, coord);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013A9BC);

void func_actor_510900_8013AD90(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2*          coord;
    Actor510900MatrixWords* mat;
    Actor510900ChildWork*   work;

    coord = ((Actor510900Obj2C*)task->extra)->field_8;
    work  = Mem_Calloc(sizeof(Actor510900ChildWork), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    mat               = (Actor510900MatrixWords*)&coord->coord;
    task->idMap       = (TaskIdMap*)work;
    mat->m00_m01      = 0x1000;
    mat->m11_m12      = 0x1000;
    mat->m22          = 0x1000;
    mat->m02_m10      = 0;
    mat->m20_m21      = 0;
    coord->coord.t[0] = -0x17D4;
    coord->coord.t[1] = -0x456;
    coord->coord.t[2] = 0x17C;
    coord->sub        = &Gfx_ViewCoord;
    coord->flg        = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = 1;
    enemy->field_1C.vx  = 0;
    enemy->field_1C.vy  = 0;
    enemy->field_1C.vz  = 0;
    work->obj0.field_8  = coord;
    work->obj0.field_10 = 0;
    work->obj0.field_12 = 0;
    work->obj0.field_14 = 0;
    work->obj0.field_C  = &work->rec20;
    work->obj0.field_18 = 0;
    work->obj0.field_1C = 0x12C;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(&work->rec20, 1, 0);
    work->obj38.field_12 = -0x200;
    work->obj38.field_8  = coord;
    work->obj38.field_10 = 0;
    work->obj38.field_14 = 0;
    work->obj38.field_C  = &work->rec58;
    work->obj38.field_18 = 0;
    work->obj38.field_1C = 0x200;
    work->obj38.flags    = 1;
    work->obj0.flags    &= 0x7FFF;
    Gp_LinkObj(8, &work->obj38);
    Gp_InitRec18Table(&work->rec58, 1, 0);
    work->obj38.flags &= 0x7FFF;
    task->exitCallback = (TaskFunc)func_actor_510900_8013C430;
    task->state        = 1;
}

s32  Gp_GetViewIndex(void);
void func_actor_510900_8013B0D8(Task* arg0);

/// View index the child keeps running in; any other view parks it.
extern u16 D_actor_510900_80167CE4;
/// Game-flag nibble 0xD values, indexed by the child's `field_74` and the
/// parent work's `field_5C2`.
extern u16 D_actor_510900_80167CEC[][4];

void func_actor_510900_8013AF38(Actor510900Ctx* arg0, Task* arg1)
{
    Actor510900ChildWork* work;
    Actor510900Work*      parent;
    s32                   mode;
    s32                   one;
    u32                   random;
    Task*                 child;

    work   = (Actor510900ChildWork*)arg1->idMap;
    parent = (Actor510900Work*)arg1->parent->idMap;
    mode   = D_801153F4;
    one    = 1;
    if (mode == one) {
        return;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    if ((Gp_GetViewIndex() & 0xFF) != D_actor_510900_80167CE4) {
        arg0->field_14     = one;
        work->obj0.flags  &= 0x7FFF;
        work->obj38.flags &= 0x7FFF;
        if (work->field_76 != 0) {
            work->field_76--;
        }
        child = work->field_70;
        if (child != NULL) {
            child->state   = 3;
            work->field_70 = NULL;
        }
        return;
    }
    arg0->field_14 = one;
    goto body;
case2:
    arg0->field_14 = one;
    return;
body:
    func_actor_510900_8013B0D8(arg1);
    GameFlag_SetNibble(0xD, D_actor_510900_80167CEC[work->field_74][parent->field_5C2]);
    random      = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState = random;
    if ((u16)((random >> 16) % 3) == 0) {
        Gp_SpawnEff(0x6005A, ((Actor510900Obj2C*)arg1->extra)->field_8, 0, NULL);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_510900/actor_510900", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013B0D8);

void func_actor_510900_801350F8(Actor510900Ctx* arg0, Actor510900* arg1);
void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1);

void func_actor_510900_8013B3D0(Task* task)
{
    void (*fns[2])(Actor510900Ctx*, Actor510900*) = { func_actor_510900_801350F8, func_actor_510900_8013B658 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900", func_actor_510900_8013B424);

void func_actor_510900_8013B524(void)
{
    s32         i;
    SVECTOR*    normals = Gp_GridParams->field_4;
    SVECTOR*    verts   = Gp_GridParams->field_8;
    GpGridFace* faces   = Gp_GridParams->field_C;

    for (i = 0; i < 12; i++) {
        verts[i] = D_actor_510900_80167BDC[i];
    }

    for (i = 0; i < 3; i++) {
        normals[i] = D_actor_510900_80167BC4[i];
        faces[i]   = D_actor_510900_80167C3C[i];
    }
}

void func_actor_510900_8013B608(Actor510900* arg0)
{
    Actor510900Work* work = arg0->field_1C;

    Gp_UnlinkObj(&work->obj47C);
    Gp_UnlinkObj(&work->obj4E4);
    Gp_UnlinkObj(&work->obj504);
    Gp_DestroyEnemy(arg0->field_20, (Task*)arg0);
}

void func_actor_510900_8013B658(Actor510900Ctx* arg0, Actor510900* arg1)
{
    if (Game_Session->field_1 != 0) {
        func_actor_510900_801355B4(arg0, arg1);
        return;
    }
    func_actor_510900_8013B6A0(arg0, arg1);
}

void func_actor_510900_8013B6A0(Actor510900Ctx* arg0, Actor510900* arg1)
{
    Actor510900Coord* temp_s1;
    Actor510900Obj2C* temp_a1;
    Actor510900Work*  temp_s2;
    s32               one;
    s32               sp;

    temp_s2 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    if (temp_s2->field_5A4 != 0) {
        sp  = D_801153F4;
        one = 1;
        if (sp == one) {
            goto case1;
        }
        if (sp >= 2) {
            goto ge2;
        }
        if (sp == 0) {
            goto case0;
        }
        goto default_body;
    ge2:
        if (sp == 2) {
            goto case2;
        }
        goto default_body;
    case0:
        temp_a1->field_C = 0;
        arg0->field_14   = 8;
        goto default_body;
    case1:
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        return;
    case2:
        temp_a1->field_C = 0x80;
        arg0->field_14   = one;
        return;
    default_body:
        if (arg0->field_4C != 0) {
            func_actor_510900_8013B804(arg1);
        }
        func_actor_510900_80135744(arg1);
        func_actor_510900_8013864C(arg1);
        func_actor_510900_8013B870(arg1);
        func_actor_510900_801387F4(arg1);
        func_actor_510900_80138978(arg1);
        func_actor_510900_80138A9C(arg1);
        func_actor_510900_8013BB20(arg1);
        func_actor_510900_80138BF0(arg1);
        if (temp_s2->field_584 != 0) {
            func_actor_510900_80138D38(arg1);
        }
        temp_s1->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_50.flg = 0;
        Gp_UpdateCoord(&temp_s1->field_0);
        ActorsShared8013bbe4((ActorShared8013bbe4*)arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        func_actor_510900_80138F44(arg1);
        func_actor_510900_8013BC80(arg1);
    }
}

void func_actor_510900_8013B804(Actor510900* arg0)
{
    Actor510900Work* work;
    GpEnemy*         enemy;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_58E  = 7;
        work->field_590  = 0;
        work->field_5B8  = 1;
    }
    flags = enemy->field_4C;
    if (flags & 0xC) {
        enemy->field_4C = flags & 0xF3;
    }
}
