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
/// `field_4` seeds the enemy's HP, the stage / room box-table index run, the
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
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
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
                snd = D_actor_402200_80138420[work->field_712 * 2 - 1] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (!(rec->flags & 0x10) && (work->field_6CA & 0x10)) {
                snd  = D_actor_402200_80138420[work->field_712 * 2] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            work->field_6CA = (u16)(rec->flags & 0x30);
        }
    }
}

void func_actor_402200_80136184(Actor402200* arg0);

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
        func_actor_402200_80136184(arg0);
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200AimScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_5", func_actor_402200_80136184);

void      func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void      Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void      func_actor_402200_80138208(GsCOORDINATE2* arg0, s32 arg1);
void      func_actor_402200_80134968(Actor402200* arg0);
extern u8 D_801153F4;

/// Inlined copy of `func_actor_402200_80137EEC`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void Actor402200_ReseedAnim(Actor402200* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_402200_801383AC[work->field_6C0];
        for (; i < 0x13; i++) {
            func_800B4114(work, i, work->field_6C0, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_6C4 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}

/// Inlined copy of `ActorsShared80137fb0`: relights the actor from its root
/// coordinate and consumes a pending `field_6EA` tint request.
static inline void Actor402200_UpdateTint(Actor402200* arg0)
{
    Actor402200Work* work;
    GpObj38*         obj;
    VECTOR           vec;
    s16              r;
    s16              g;
    s16              b;

    obj    = (GpObj38*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->field_24.t[0];
    vec.vy = obj->field_24.t[1];
    vec.vz = obj->field_24.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
    switch (work->field_6EA) {
        case 1:
            r = 0;
            g = 0;
            b = 0x400;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 2:
            r = 0xFFF;
            g = 0xFFF;
            b = 0xFFF;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 0:
        default:
            return;
    }
}

/// Inlined copy of `ActorsShared8013806c`: draws the ground shadow quad.
static inline void Actor402200_DrawShadow(Actor402200* arg0)
{
    Actor402200Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   sub;
    VECTOR3          vec;

    work  = arg0->field_1C;
    coord = &arg0->field_2C->field_8->field_0;
    sub   = &arg0->field_2C->field_8->field_F0;
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}

/// Frame handler for the scene's `D_801153F4` mode. Mode 1 only refreshes the
/// coordinates, tint and shadow and mode 2 hides the model, both returning
/// without giving back the 8-byte `G_SCRATCH_HEAD` block. Otherwise the
/// `field_6CE` sequence runs: state 0 unlinks the actor and saves its pose,
/// state 1 sprays a randomly angled effect every fourth frame, and state 2
/// projects the actor before moving on to 3.
void func_actor_402200_801368E0(GpEnemy* arg0, Actor402200* arg1)
{
    u8*              head;
    SVECTOR*         sc;
    Actor402200Work* work;
    GsCOORDINATE2*   coord;
    s32              mode;
    u32              random;
    s16              anim;

    work                  = arg1->field_1C;
    coord                 = &arg1->field_2C->field_8->field_0;
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(SVECTOR);
    sc                    = (SVECTOR*)(head - sizeof(SVECTOR));
    mode                  = D_801153F4;
    switch (mode) {
        case 0:
            arg1->field_2C->field_C = 0;
            break;
        case 1:
            coord->flg                            = 0;
            arg1->field_2C->field_8->field_F0.flg = 0;
            Gp_UpdateCoord(coord);
            Actor402200_UpdateTint(arg1);
            Actor402200_DrawShadow(arg1);
            return;
        case 2:
            arg1->field_2C->field_C = 0x80;
            return;
    }
    switch (work->field_6CE) {
        case 0:
            arg0->field_54 = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((GpObj*)work->field_4E4);
            Gp_UnlinkObj((GpObj*)work->field_47C);
            Gp_UnlinkObj((GpObj*)work->field_564);
            Gp_ReleaseStateF0Add((GpObj20E*)arg1, work->field_716);
            anim = 0x14;
            if (work->field_6F0 == 1) {
                anim = 0x10;
            }
            work->field_6C0 = anim;
            work->field_6CE = 1;
            arg0->field_4B  = work->field_6F0;
            Gp_SaveEnemyPose(arg0);
            break;
        case 1:
            if (!(work->field_6C4 & 3)) {
                sc->vx      = 0;
                sc->vz      = 0;
                random      = Gp_LcgState * 5 + 0x71357911;
                sc->vy      = -((random >> 16) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &arg1->field_2C->field_8->field_F0, 0x400, sc);
            }
            break;
        case 2:
            func_actor_402200_80138208(&arg1->field_2C->field_8->field_F0, 0xC);
            func_actor_402200_80134968(arg1);
            func_8009EA50(work->field_6D8);
            work->field_6CE = 3;
            break;
    }
    func_actor_402200_801380D8(arg1);
    Actor402200_ReseedAnim(arg1);
    coord->flg                            = 0;
    arg1->field_2C->field_8->field_F0.flg = 0;
    Gp_UpdateCoord(coord);
    Actor402200_UpdateTint(arg1);
    Actor402200_DrawShadow(arg1);
    *(u8**)G_SCRATCH_HEAD += sizeof(SVECTOR);
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_5", func_actor_402200_80136D9C);

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `field_4B` variant. Variant 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `D_actor_402200_80153C78`, requests the room's cue bank, and links the
/// work block's five collision objects with their `GpRec18` tables before
/// moving the task on (`field_30` 1). Variants 1 and 2 only seed the animation
/// and sequence state.
void func_actor_402200_80137444(GpEnemy* arg0, Actor402200* arg1)
{
    u8                param1[4];
    u8                param2[4];
    Actor402200Work*  work;
    Actor402200Obj2C* obj;
    Actor402200Coord* coord;
    s16*              cues;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    GpRec18*          records4;
    GpRec18*          records5;
    s32               i;
    s32               kind;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x71C, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->field_1C          = work;
    obj->field_C            = 0;
    coord->field_0.flg      = 0;
    obj->field_1C           = &work->field_45C;
    obj->field_20           = &work->field_43C;
    work->field_65C.field_0 = &arg1->field_2C->field_8->field_F0;
    work->field_65C.field_4 = 0x500;
    work->field_65C.field_6 = 2;
    func_800B3F84((GpAnimCtx*)work, D_actor_402200_80154194, (GpAnimObj*)obj, work->field_30C, &work->field_14);
    work->field_6C0 = 0xB;
    work->field_6C2 = 0xB;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_6C0);
    }
    kind = arg0->field_4B;
    switch (kind) {
        case 0:
            work->field_6D8 = 0xFF;
            obj->field_2C   = 0;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = -1;
            arg0->field_4   = &coord->field_0.coord;
            arg0->field_48  = 0;
            Gp_LinkNode(&arg0->node);
            arg0->field_18    = &arg1->field_2C->field_8->field_F0;
            arg0->field_1C.vx = 0;
            arg0->field_1C.vy = 0;
            arg0->field_1C.vz = 0;
            arg0->field_50    = &D_actor_402200_80153BFC;
            arg0->field_54    = (s32)work->field_49C;
            arg0->field_40    = D_actor_402200_80153BFC.field_4;
            for (i = 0; D_actor_402200_80153C78[i].field_0 != 0; i++) {
                if (gGameSession->at4.loc.stage == D_actor_402200_80153C78[i].field_2 && gGameSession->at4.loc.area == D_actor_402200_80153C78[i].field_4) {
                    work->field_6B4 = D_actor_402200_80153FA8[D_actor_402200_80153C78[i].field_0];
                    work->field_6FA = D_actor_402200_80153C78[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            work->field_716 = 0x16;
            cues            = D_actor_402200_80154144[gGameSession->at4.loc.stage];
            if (cues != NULL) {
                work->field_712 = cues[gGameSession->at4.loc.area];
            }
            if (work->field_712 != 0) {
                param1[3] = 0;
                param1[2] = 0x28;
                param1[0] = work->field_712;
                param2[0] = 0x16;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }
            work->field_484 = &arg1->field_2C->field_8->field_F0;
            records1        = work->field_49C;
            work->field_488 = records1;
            work->field_48C = 0;
            work->field_48E = 0;
            work->field_490 = 0;
            work->field_494 = 0x30016;
            work->field_498 = 0x15E;
            work->field_49A = 1;
            Gp_LinkObj(2, (GpObj*)work->field_47C);
            Gp_InitRec18Table(records1, 3, 0);
            work->field_49A |= 0x8000;
            work->field_4EC  = arg1->field_2C->field_8;
            records2         = work->field_504;
            work->field_4F0  = records2;
            work->field_4F4  = 0;
            work->field_4F6  = -0x1F4;
            work->field_4F8  = 0;
            work->field_4FC  = 0x30016;
            work->field_500  = 0x1F4;
            work->field_502  = 1;
            Gp_LinkObj(2, (GpObj*)work->field_4E4);
            Gp_InitRec18Table(records2, 4, 0);
            work->field_502 |= 0x4200;
            work->field_56C  = &((GsCOORDINATE2*)arg1->field_2C->field_8)[8];
            records3         = &work->field_584;
            work->field_570  = records3;
            work->field_574  = 0;
            work->field_576  = 0;
            work->field_578  = 0;
            work->field_57C  = Gp_PackPair(&D_actor_402200_80153BEC, 1);
            work->field_580  = 0x12C;
            work->field_582  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_564);
            Gp_InitRec18Table(records3, 1, 0);
            work->field_582 &= 0x7FFF;
            work->field_5DC  = 0;
            work->field_5DE  = -0x3E8;
            work->field_5E0  = -0x7D0;
            work->field_5E4  = 0;
            work->field_5E6  = -0x3E8;
            work->field_5E8  = 0;
            work->field_5EC  = 0x1F4;
            work->field_5EE  = 0x1F4;
            records4         = &work->field_5F4;
            work->field_5F0  = records4;
            work->field_5A4  = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
            work->field_5A8  = &work->field_5DC;
            work->field_5AC  = 0;
            work->field_5AE  = 0;
            work->field_5B0  = 0;
            work->field_5B4  = 0;
            work->field_5B8  = 0;
            work->field_5BA  = 3;
            Gp_LinkObj(3, (GpObj*)work->field_59C);
            Gp_InitRec18Table(records4, 1, 0);
            work->field_5BA &= 0xBFFF;
            work->field_5C4  = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
            work->field_5C8  = records4;
            work->field_5CC  = 0;
            work->field_5CE  = -0x320;
            work->field_5D0  = -0x5AA;
            work->field_5D4  = 0;
            work->field_5D8  = 0x1F4;
            work->field_5DA  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_5BC);
            work->field_62C  = 0;
            work->field_62E  = -0x514;
            work->field_630  = 0x2710;
            work->field_634  = 0;
            work->field_636  = 0;
            work->field_638  = 0;
            work->field_63C  = 1;
            work->field_63E  = 1;
            records5         = &work->field_644;
            work->field_640  = records5;
            work->field_614  = coord;
            work->field_618  = &work->field_62C;
            work->field_61C  = 0;
            work->field_61E  = 0;
            work->field_620  = 0;
            work->field_624  = 0;
            work->field_628  = 0;
            work->field_62A  = 3;
            work->field_5DA &= 0xBFFF;
            Gp_LinkObj(3, (GpObj*)work->field_60C);
            Gp_InitRec18Table(records5, 1, 0);
            work->field_62A = (work->field_62A & 0x3FFF) | 0xC00;
            arg1->field_24  = D_actor_402200_8013839C;
            arg1->field_30  = 1;
            break;
        case 1:
            work->field_6C0 = 0x10;
            work->field_6CE = 2;
            arg1->field_30  = 2;
            work->field_6D8 = 0;
            obj->field_2C   = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
        case 2:
            work->field_6C0 = 0x14;
            work->field_6CE = kind;
            arg1->field_30  = kind;
            work->field_6D8 = 0;
            obj->field_2C   = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
    }
}
