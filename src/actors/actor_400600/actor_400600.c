#include "common.h"

#include "psyq/inline_c.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"
#include "actors/actors_shared_80139948.h"
#include "actors/actors_shared_80139c00.h"
#include "actors/actors_shared_80139dcc.h"
#include "actors/actors_shared_8013a0b0.h"
#include "actors/actors_shared_8016a538.h"
#include "actors/actor_400600_anim.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`; `D_80115417` is one byte of the run
 * of gameplay flags at 0x80115408..0x8011541B.
 *
 * Storing to a bare `extern` global next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `Actor400600Work` loads
 * that follow. Two remedies work and which one is needed was measured, not
 * chosen: the byte store to `D_80115417` matches with `SOFT_BARRIER()` after
 * it, so that one is declared as the scalar it is; the pointer store to
 * `D_800678F0` checksums wrong with the barrier and matches only as an
 * aggregate, so its one-element array stays and is doing real work.
 * `D_80115414`, from the same flag run, is declared as the aggregate
 * `actor_400600_7.c` needs, which matches here too. */
extern void* D_800678F0[1];
extern s8    D_80115414[1];
extern s8    D_80115417;

extern s32 Gp_LcgState;

extern u8 D_801153F4;

extern s32 D_80115738;
extern s32 D_8011574C;

extern GpU16Pair  D_actor_400600_80144EA8;
extern GpPairSrcE D_actor_400600_80144EB0; // enemy stat block (field_4 is max HP)

extern u8 D_actor_400600_80151A54[];       // animation bank handed to func_800B3F84
extern u8 D_actor_400600_80151AE0[];

extern u8 D_actor_400600_80151A48[];

extern TaskDesc D_actor_400600_80151AF8;

void func_8017D9B8(s32);

/* One of the sub-state tables in this overlay's leading rodata. The original
 * wrote it as a local array initializer, so GCC 2.8.1 put the four constant
 * pointers in the constant pool and had the dispatcher copy them onto the stack.
 * Writing that initializer here instead would emit the pool where the *function*
 * sits in the file, and so at the wrong address; reading the splat-owned table as
 * a `TaskFuncTable4` reproduces the same copy while leaving the rodata where it
 * is. The tables ahead of 0xB4 live in the asm `actor_400600_hdr` object
 * (`rodata_head`), so `func_actor_400600_80133434`'s jump table starts this
 * unit's `.rodata` instead of picking up GCC's `.align 3` pad mid-object. */
extern const TaskFuncTable6  D_actor_400600_80131E54;
extern const TaskFuncTable4  D_actor_400600_80131E6C;
extern const TaskFuncTable8  D_actor_400600_80131E7C;
extern const TaskFuncTable4  D_actor_400600_80131E9C;
extern const TaskFuncTable3  D_actor_400600_80131F34;
extern const TaskFuncTable8  D_actor_400600_80131F40;
extern const TaskFuncTable4  D_actor_400600_80131F60;
extern const TaskFuncTable3  D_actor_400600_80131F70;
extern const TaskFuncTable4  D_actor_400600_80131F7C;
extern const TaskFuncTable4  D_actor_400600_80131F8C;
extern const TaskFuncTable3  D_actor_400600_80131F9C;
extern const TaskFuncTable3  D_actor_400600_80132030;
extern const TaskFuncTable18 D_actor_400600_80131EEC;

extern u8 D_actor_400600_8014220C[];
extern u8 D_actor_400600_80143604[];
extern u8 D_actor_400600_80143B24[];
extern u8 D_actor_400600_80144994[];

/* Part indices into the model's coordinate array, terminated by -1. */
extern s16 D_actor_400600_80151B88[];

/* Still `INCLUDE_ASM` in this overlay; `func_actor_400600_80139CAC` is called
 * both with and without an argument, so it keeps an unprototyped declaration. */
void func_actor_400600_80135998(Task* arg0, s16 arg1);
s32  func_actor_400600_801376EC(Task* arg0);
void func_actor_400600_80138B40(Task* arg0);
void func_actor_400600_80136558(Task* arg0);
void func_actor_400600_80136670(Task* arg0);
void func_actor_400600_801383E4(SVECTOR* arg0, SVECTOR* arg1, s16 width, u8 shade);
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
void func_actor_400600_80136968(Task* arg0);
void func_actor_400600_80137840(Task* arg0);
void func_actor_400600_801387DC(Task* arg0, s32 arg1);
/* Defined in actor_400600_2.c returning `s16`; this unit sees an `int`
 * return, and the truncating copy into an `s16` local is what matches. */
s32  func_actor_400600_8013886C(Task* arg0);
s32  func_actor_400600_8013892C(Task* arg0);
void func_actor_400600_80138AB8(Task* arg0);
void ActorsShared8013a2c0(Task* arg0);
void func_actor_400600_801361AC();
s32  func_actor_400600_80136FA8();
s32  func_actor_400600_801370F4();
s32  func_actor_400600_80137AF0(Task* arg0);
s32  func_actor_400600_80137C34(Task* arg0);
void func_actor_400600_80137498(Task* arg0, s16 arg1);
void func_actor_400600_80138B5C(Task* arg0, s32 arg1);
void func_actor_400600_80139CAC();
void func_actor_400600_80139E68(Task* arg0, s16 arg1, Actor400600ViewPos* arg2);
void func_actor_400600_80139D98(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400600_8013B640(void);
void func_actor_400600_801356E0(Task* arg0);
void func_actor_400600_80138A24(Task* arg0, s16 arg1);
void func_actor_400600_80139DB0(Task* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_actor_400600_8013B6F4(Task* arg0);
void func_actor_400600_8013B740(Task* arg0);
void func_actor_400600_8013B830(Task* arg0);
void func_actor_400600_8013B8AC(Task* arg0);
void func_actor_400600_8013B984(Task* arg0);
void func_actor_400600_8013BA00(Task* arg0);
void func_actor_400600_8013BC68(Task* arg0);
void func_actor_400600_80133E38(Task* arg0);
void func_actor_400600_8013BBF4(Task* arg0);
void func_actor_400600_80133CB0(Task* arg0);
void func_actor_400600_8013BFD4(Task* arg0);
void func_actor_400600_80134B98(Task* arg0);
void func_actor_400600_8013C518(Task* arg0);
void func_actor_400600_8013C534(Task* arg0);
void func_actor_400600_8013C598(Task* arg0);
s32  func_actor_400600_8013CACC(Task* arg0);
void func_actor_400600_8013C5F8(Task* arg0);
void func_actor_400600_80132294(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, u8 arg5);
void func_actor_400600_80135DDC(Task* arg0);

void func_actor_400600_8013203C(Task* arg0)
{
    Actor400600Work* work;

    work                   = (Actor400600Work*)arg0->idMap;
    work->obj_4B4.field_8  = &((TmdObject*)arg0->extra)->field_8[3];
    work->obj_4B4.field_C  = work->rec_4D4;
    work->obj_4B4.field_10 = 0;
    work->obj_4B4.field_12 = 0x96;
    work->obj_4B4.field_14 = 0x110;
    work->obj_4B4.field_18 = 0x30006;
    if (Game_Session->field_7 == 3 && (Game_Session->field_6 == 0x1F || Game_Session->field_6 == 0x1D)) {
        work->obj_4B4.field_1C = 0x260;
    } else {
        work->obj_4B4.field_1C = 0x200;
    }
    work->obj_4B4.flags = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->rec_624.field_4  = 0xBB8;
    work->rec_624.field_10 = 0xA;
    work->rec_624.field_12 = 0xA;
    work->rec_624.field_0  = 0;
    work->rec_624.field_C  = 0;
    work->rec_624.field_8  = 0;
    work->rec_624.field_14 = work->rec_63C;
    work->obj_4B4.flags   |= 0x8000;
    work->obj_604.field_8  = ((TmdObject*)arg0->extra)->field_8;
    work->obj_604.field_C  = (GpRec18*)&work->rec_624;
    work->obj_604.field_10 = 0;
    work->obj_604.field_12 = -0x190;
    work->obj_604.field_14 = 0;
    work->obj_604.field_18 = 0x30006;
    work->obj_604.field_1C = 0;
    work->obj_604.flags    = 3;
    Gp_LinkObj(2, &work->obj_604);
    Gp_InitRec18Table(work->rec_63C, 8, 0);
    work->obj_604.flags   &= 0x3FFF;
    work->obj_594.field_18 = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_594.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    work->obj_594.field_C  = work->rec_5B4;
    work->obj_594.field_10 = -0x200;
    work->obj_594.field_12 = 0;
    work->obj_594.field_14 = 0;
    work->obj_594.field_1C = 0x190;
    work->obj_594.flags    = 1;
    Gp_LinkObj(3, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 1, 0);
    work->obj_594.flags   &= 0x7FFF;
    work->obj_5CC.field_18 = Gp_PackPair(&D_actor_400600_80144EA8, 0);
    work->obj_5CC.field_8  = &((TmdObject*)arg0->extra)->field_8[10];
    work->obj_5CC.field_C  = work->rec_5EC;
    work->obj_5CC.field_10 = 0x200;
    work->obj_5CC.field_12 = 0;
    work->obj_5CC.field_14 = 0;
    work->obj_5CC.field_1C = 0x190;
    work->obj_5CC.flags    = 1;
    Gp_LinkObj(3, &work->obj_5CC);
    Gp_InitRec18Table(work->rec_5EC, 1, 0);
    work->obj_5CC.flags &= 0x7FFF;
}

void func_actor_400600_80132294(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    Actor400600BeamScratch* s;
    s16                     angle;
    GsCOORDINATE2*          secondCoord;
    GsCOORDINATE2*          firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfY;
    GsCOORDINATE2*          coords;
    POLY_FT4*               poly;

    coords      = ((TmdObject*)task->extra)->field_8;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (Actor400600BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(Actor400600BeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vy       = s->firstMatrix.t[1];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vy      = s->secondMatrix.t[1];
        s->first.vz       = height;
        s->second.vz      = height;
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vy - s->first.vy);
        halfX             = (s->first.vx - s->second.vx) / 2;
        halfY             = (s->first.vy - s->second.vy) / 2;
        s->corner0.vx     = halfX + (s->first.vx - ((s32)(rcos(angle) * width) >> 0xC));
        offset0           = rsin(angle) * width;
        s->corner0.vz     = height;
        s->corner0.vy     = halfY + (s->first.vy + (offset0 >> 0xC));
        s->corner1.vx     = halfX + (s->first.vx + ((s32)(rcos(angle) * width) >> 0xC));
        offset1           = rsin(angle) * width;
        s->corner1.vz     = height;
        s->corner1.vy     = halfY + (s->first.vy - (offset1 >> 0xC));
        s->corner2.vx     = (s->second.vx - ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset2           = rsin(angle) * width;
        s->corner2.vz     = height;
        s->corner2.vy     = (s->second.vy + (offset2 >> 0xC)) - halfY;
        s->corner3.vx     = (s->second.vx + ((s32)(rcos(angle) * width) >> 0xC)) - halfX;
        offset3           = rsin(angle) * width;
        s->corner3.vz     = height;
        s->corner3.vy     = (s->second.vy - (offset3 >> 0xC)) - halfY;
        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        gte_SetRotMatrix(&Gfx_ViewCoord.workm);
        gte_SetTransMatrix(&Gfx_ViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = Gpu_PrimCursor;
            Gpu_PrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code       = 0x2E;
            *(s32*)&poly->x0 = s->screen0;
            *(s32*)&poly->x1 = s->screen1;
            *(s32*)&poly->x2 = s->screen2;
            *(s32*)&poly->x3 = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(Actor400600BeamScratch);
    }
}

void func_actor_400600_80132704(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_400600_80132294(arg0, 3, 9, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 9, 0xA, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xA, 0xB, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 3, 6, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 6, 7, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 7, 8, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 5, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xC, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xC, 0xD, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xD, 0xE, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 1, 0xF, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0xF, 0x10, 0x80, arg1, arg2);
    func_actor_400600_80132294(arg0, 0x10, 0x11, 0x80, arg1, arg2);
}

void func_actor_400600_801328A8(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work                = (Actor400600Work*)arg0->idMap;
    coords->coord.t[0] += (0x4364 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0x760 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= -0x508) {
        Gp_SpawnPadLerp(0xA, 0xC0, 0x80);
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x531A0009;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_8017D9B8(1);
        func_actor_400600_80139D98(arg0, 0x19, 0x30);
        coords->coord.t[1] = -0x508;
        work->field_71C++;
    }
}

void func_actor_400600_801329EC(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    if ((s16)work->field_718 == 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x531A000A;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    work->field_718++;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40060004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
        work3                = (Actor400600Work*)arg0->idMap;
        work3->field_71C     = 2;
        work3->field_71E     = 0;
    }
}

void func_actor_400600_80132B3C(Task* arg0)
{
    GsCOORDINATE2*   coords;
    Actor400600Work* work;
    s32              sound;
    s32              pan;

    coords              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work                = (Actor400600Work*)arg0->idMap;
    coords->coord.t[0] += (0x1C54 - coords->coord.t[0]) >> 2;
    coords->coord.t[2] += (0xED5 - coords->coord.t[2]) >> 2;
    work->field_722    += 2;
    work->field_724    += work->field_722;
    coords->coord.t[1] += work->field_724;
    if (coords->coord.t[1] >= 0) {
        Gp_SpawnPadLerp(0x10, 0x80, 0x40);
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x531A000A;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_actor_400600_80139D98(arg0, 0x19, 0x10);
        coords->coord.t[1] = 0;
        work->field_71C++;
    }
}

void func_actor_400600_80132C70(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    Actor400600Work* work4;
    GsCOORDINATE2*   coords;
    u8               mode;
    s16              yaw;

    work   = (Actor400600Work*)arg0->idMap;
    mode   = work->field_762;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (mode == 1) {
        coords->coord.t[0] = 0x36B0;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0x7D0;
        work->field_80     = 0;
        work->field_82     = 0x400;
        work->field_84     = 0x400;
        work->field_718    = 0;
        func_actor_400600_80139D98(arg0, 2, 0x10);
        work->field_73C = -0x7D0;
        work->field_71C++;
    } else if (mode == 2) {
        coords->coord.t[0] = 0x4A38;
        coords->coord.t[1] = -0x320;
        coords->coord.t[2] = -0xFA0;
        yaw                = -0x400;
        work->field_82     = yaw;
        work->field_80     = 0;
        work->field_84     = 0x400;
        work->field_718    = 0;
        func_actor_400600_80139D98(arg0, 2, 0x10);
        work->field_73C  = -0xFA0;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 5;
        work2->field_71E = 0;
    } else if (mode == 3) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coords->coord.t[0]   = 0x2AF8;
        coords->coord.t[2]   = -0x3E8;
        coords->coord.t[1]   = 0;
        work->field_80       = 0;
        work->field_82       = 0xC00;
        work3                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work3->field_71C     = 0;
        work3->field_71E     = 0;
    } else if (mode == 4) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coords->coord.t[0]   = 0x3A98;
        coords->coord.t[2]   = -0xBB8;
        coords->coord.t[1]   = 0;
        work->field_80       = 0;
        work->field_82       = 0x400;
        work4                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work4->field_71C     = 0;
        work4->field_71E     = 0;
    }
}

void func_actor_400600_80132E10(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    u32              sound;
    s32              pan;

    work->field_718 = work->field_718 + 1;
    func_actor_400600_801361AC();
    if ((s16)work->field_718 == 0x26) {
        func_actor_400600_80138B5C(arg0, 0);
    }
    if ((s16)work->field_718 >= 0x27) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 4);
    }
    if ((s16)work->field_718 == 0x5A) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A0004;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_718 = 0;
        work->field_722 = -0xA;
        work->field_724 = 0;
        func_actor_400600_80139DB0(arg0, 0x15, 0x10, 4);
        work->field_71C = work->field_71C + 1;
    }
}

void func_actor_400600_80132F3C(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->idMap;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x4588 - coords->coord.t[0]) >> 2;
        work->field_82     += (0xC00 - (s16)work->field_82) >> 2;
        work->field_84     += -(s16)work->field_84 >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(D_8011574C, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x404A0003;
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->field_71C++;
        }
    }
}

void func_actor_400600_80133118(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coords;
    s32              sound;
    s32              pan;

    work   = (Actor400600Work*)arg0->idMap;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work->field_718++;
    if ((s16)work->field_718 >= 0x11) {
        work->field_73A    += -work->field_73A >> 3;
        work->field_722    += 2;
        work->field_724    += work->field_722;
        coords->coord.t[1] += work->field_724;
        coords->coord.t[2] += (-0xBB8 - coords->coord.t[2]) >> 3;
        coords->coord.t[0] += (0x3A98 - coords->coord.t[0]) >> 2;
        work->field_82     += (0x400 - (s16)work->field_82) >> 2;
        work->field_84     += -(s16)work->field_84 >> 3;
        if ((s16)work->field_718 == 0x22) {
            func_actor_400600_8013896C(arg0, 0);
        }
        if ((s16)work->field_718 == 0x23 || (s16)work->field_718 == 0x25) {
            Gp_SpawnEff(D_8011574C, coords, 0x38, NULL);
        }
        if (coords->coord.t[1] >= 0) {
            sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x404A0003;
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            func_actor_400600_80139D98(arg0, 0x19, 0x10);
            coords->coord.t[1] = 0;
            work->field_71C++;
        }
    }
}

void func_actor_400600_801332F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    s32              mode;

    work  = (Actor400600Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;
    mode  = work->field_762;
    coord = model->field_8;
    if (mode == 1) {
        Gp_SpawnPadLerp(0x14, 0xFF, 0x80);
        coord->coord.t[0] = 0xCE4;
        coord->coord.t[1] = -0xBB8;
        coord->coord.t[2] = 0;
        work->field_80    = 0;
        work->field_82    = 0xC00;
        work->field_84    = 0;
        model->field_C   &= 0xFF7F;
        work->field_718   = 0;
        work->field_722   = 0;
        work->field_724   = 0;
        func_actor_400600_80139D98(arg0, 0x15, 0x10);
        func_actor_400600_80138B5C(arg0, 0);
        work->field_71C = work->field_71C + 1;
    } else if (mode == 2) {
        work->obj_4B4.flags |= 0x8000;
        work->obj_594.flags &= 0x7FFF;
        work->obj_5CC.flags &= 0x7FFF;
        coord->coord.t[0]    = -0x6A4;
        coord->coord.t[2]    = -0x514;
        coord->coord.t[1]    = 0;
        work->field_82       = 0x400;
        work->field_73A      = 0xFF;
        work->field_80       = 0;
        work->field_84       = 0;
        D_80115414[0]        = mode;
        work2                = (Actor400600Work*)arg0->idMap;
        arg0->state          = 1;
        work2->field_71C     = 0;
        work2->field_71E     = 0;
    }
}

void func_actor_400600_80133434(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    Actor400600Work* work;
    Actor400600Work* w2;
    Actor400600Work* w3;
    Actor400600Work* w4;
    u32              rnd;

    model       = (TmdObject*)arg0->extra;
    enemy       = (GpEnemy*)arg0->spawnArg2;
    coord       = model->field_8;
    arg0->idMap = Mem_Calloc(0x770U, false);
    work        = (Actor400600Work*)arg0->idMap;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    func_actor_400600_8013B640();
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x03140000) {
        work->field_76E = 1;
    }
    model->field_1C    = &work->matrix_40;
    model->field_20    = &work->matrix_20;
    model->field_C     = 0;
    arg0->field_24     = D_actor_400600_80151AE0;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)arg0->extra)->field_8[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 5;
    enemy->field_50     = &D_actor_400600_80144EB0;
    enemy->field_54     = (s32)work->rec_4D4;
    work->field_6FC     = &((TmdObject*)arg0->extra)->field_8[3];
    work->field_700     = 0x300;
    work->field_702     = 2;
    enemy->field_40 = enemy->field_42 = D_actor_400600_80144EB0.field_4;
    func_800B3F84(&work->anim, D_actor_400600_80151A54, (GpAnimObj*)model, work->pad_394, work->slots);

    w2            = (Actor400600Work*)arg0->idMap;
    w2->field_726 = 0x10;
    w2->field_746 = 1;
    w2->field_742 = 2;

    Actor400600_TickAnim(arg0);

    coord->sub     = &Gfx_ViewCoord;
    work->field_82 = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    func_actor_400600_8013203C(arg0);
    work->obj_4B4.flags |= 0x4000;
    func_actor_400600_801356E0(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    func_actor_400600_80138A24(arg0, 1);
    w3              = (Actor400600Work*)arg0->idMap;
    w3->field_71C   = 0;
    w3->field_71E   = 0;
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_716 = rnd >> 0x10;
    work->field_73E = work->field_92;
    switch ((u8)arg0->spawnArg1 >> 4) {
        case 0:
            w4            = (Actor400600Work*)arg0->idMap;
            arg0->state   = 1;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 1:
            work->field_76C = 1;
            w4              = (Actor400600Work*)arg0->idMap;
            arg0->state     = 4;
            w4->field_71C   = 0;
            w4->field_71E   = 0;
            break;
        case 2:
            w4            = (Actor400600Work*)arg0->idMap;
            arg0->state   = 5;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 3:
            w4            = (Actor400600Work*)arg0->idMap;
            arg0->state   = 6;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 4:
            w4            = (Actor400600Work*)arg0->idMap;
            arg0->state   = 7;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
        case 5:
            w4            = (Actor400600Work*)arg0->idMap;
            arg0->state   = 8;
            w4->field_71C = 0;
            w4->field_71E = 0;
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131EEC);

void func_actor_400600_801337A8(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    TaskFuncTable18  fns   = D_actor_400600_80131EEC;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case 0:
            work->field_716++;
            func_actor_400600_80136670(arg0);
            fns.funcs[(s16)work->field_71C](arg0);
            func_actor_400600_80138AB8(arg0);
            func_actor_400600_80137840(arg0);
            func_actor_400600_80136558(arg0);
            Actor400600_TickAnim(arg0);
            work->field_710.h.flags = work->slots[1].field_10;
            Actor400600_RebuildRotation(arg0);
            func_actor_400600_80136968(arg0);
            if (enemy->field_40 <= 0 && (u8)work->field_767 == 0) {
                Actor400600Work* w = (Actor400600Work*)arg0->idMap;
                arg0->state        = 2;
                w->field_71C       = 0;
                w->field_71E       = 0;
            }
        case 1:
            Gp_ClearRec18Occupied(work->rec_4D4);
            Gp_ClearRec18Occupied(work->rec_63C);
            Actor400600_UpdateColor(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            if (work->field_75C.b.field_75E == 0) {
                model->field_C &= ~0x80;
                func_actor_400600_801387DC(arg0, -1);
            }
            break;
    }
}

void func_actor_400600_80133B88(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    s32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_728 < 0xBB8 || D_80115417 != 0) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40060004;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        func_actor_400600_80138B5C(arg0, 0);
        Gp_ArmStateF0(1);
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_758  = ((rnd >> 0x10) & 0x3F) + 0x1E;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 2;
        work2->field_71E = 0;
        return;
    }
    if ((s16)func_actor_400600_80136FA8(arg0) != 0) {
        Gp_ArmStateF0(1);
    }
}

void func_actor_400600_80133CB0(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_5CC.flags |= 0x8000;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_5CC.flags &= 0x7FFF;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        func_actor_400600_80138AF0(arg0, 0x2D);
        func_actor_400600_80138AA4(arg0);
        if (work->field_768 == 0 && work->field_728 < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 9;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 2;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_80133E38(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor400600Work*)arg0->idMap;
    work->field_718++;
    if ((s16)work->field_718 == 0x15) {
        id = 0x40060005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_594.flags |= 0x8000;
    }
    if ((s16)work->field_718 == 0x1C) {
        work->obj_594.flags &= 0x7FFF;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        func_actor_400600_80138AF0(arg0, 0x2D);
        func_actor_400600_80138AA4(arg0);
        if (work->field_768 == 0 && work->field_728 < 0x578 && (u16)(work->field_72C - 0x200) > 0xC00 && (u16)(work->field_72A - 0x200) > 0xC00) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_71C = 9;
            work2->field_71E = 0;
        } else {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 2;
            work3->field_71E = 0;
        }
    }
}

void func_actor_400600_80133FC0(Task* arg0)
{
    Actor400600Msg3FF msg;
    Actor400600Msg3F8 query;
    Actor400600Work*  work;
    Actor400600Work*  work2;
    Actor400600Work*  work3;
    s32               base;
    s32               sound;
    s32               pan;

    work = (Actor400600Work*)arg0->idMap;
    if (Gp_ActorSlots[0]->actor->field_954 == 2 || (func_actor_400600_801376EC(arg0) << 0x10) != 0 || work->field_728 >= 0x7D0 || (u32)(work->field_72C - 0x200) < 0xC01U) {
        func_actor_400600_80138B40(arg0);
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 2;
        work2->field_71E = 0;
        func_actor_400600_80135998(arg0, work->field_752);
        return;
    }
    query.field_14 = 8;
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_768 == 0) {
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 2;
            work3->field_71E = 0;
            return;
        }
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_71C = 0xD;
        work2->field_71E = 0;
        return;
    }
    work->field_73E = work->field_92;
    func_actor_400600_80138B40(arg0);
    work->field_768      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_767      = 1;
    work->field_9A       = work->field_92;
    msg.field_0          = D_actor_400600_80151A48;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 1;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x3FFF;
    work2                = (Actor400600Work*)arg0->idMap;
    work2->field_720     = 4;
    work2->field_726     = 0x10;
    work2->field_746     = 5;
    work2->field_742     = 1;
    work->field_718      = 0;
    work->field_71A      = 0;
    base                 = 0x40060004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_75C.b.field_75C = 0;
    work->field_71E++;
}

void func_actor_400600_80134218(Task* arg0)
{
    Actor400600Msg3FF msg;
    SVECTOR           vec;
    Actor400600Work*  work;
    Actor400600Work*  work2;
    GpEnemy*          enemy;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    player;
    GsCOORDINATE2*    root;
    s32               id;
    s32               sound;
    s32               pan;
    s32               sound2;
    s32               pan2;
    s32               y;
    s32               ty;

    work               = (Actor400600Work*)arg0->idMap;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    enemy              = (GpEnemy*)arg0->spawnArg2;
    player             = Gp_ActorSlots[0]->extra->field_8;
    work->field_84    += -(s16)work->field_84 >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    y                  = coord->coord.t[1];
    ty                 = y + 900;
    coord->coord.t[1]  = y + ((player->coord.t[1] - ty) >> 2);
    work->field_718++;
    if (++work->field_71A == 8) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 6;
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_763 == 1 || work->field_764 == 1 || enemy->field_40 <= 0 || work->field_75C.b.field_75C >= 3) {
        work->field_763 = 0;
        if (work->field_764 == 0) {
            msg.field_0  = D_actor_400600_80151A48;
            msg.field_8  = 1;
            msg.field_C  = 8;
            msg.field_10 = 0;
            msg.field_4  = 2;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
        }
        work2                = (Actor400600Work*)arg0->idMap;
        work2->field_720     = 8;
        work2->field_726     = 0x10;
        work2->field_746     = 6;
        work2->field_742     = 1;
        work->field_722      = 0;
        work->field_724      = 0;
        work->field_718      = 0;
        work->obj_4B4.flags |= 0x4000;
        work->field_71E++;
        return;
    }
    if ((s16)work->field_718 == 0xD || (s16)work->field_718 == 0x1A) {
        root = &Gp_ActorSlots[0]->extra->field_8[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40060009;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan2   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 1), 0) != 0) {
            work->field_764 = 1;
        }
        vec.vx = 0;
        vec.vy = -200;
        vec.vz = 0;
        Gp_SpawnEff(0x6009B, root, 0x10100, &vec);
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_718 = 0;
        work->field_75C.b.field_75C++;
    }
}

void func_actor_400600_80134570(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    GsCOORDINATE2*   root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work            = (Actor400600Work*)arg0->idMap;
    coord           = ((TmdObject*)arg0->extra)->field_8;
    player          = Gp_ActorSlots[0]->extra->field_8;
    work->field_84 += -(s16)work->field_84 >> 2;
    work->field_718++;
    if ((s16)work->field_718 >= 8) {
        work->obj_4B4.flags |= 0x8000;
        work->field_722     += 2;
        work->field_724     += work->field_722;
        y                    = coord->coord.t[1] + work->field_724;
        coord->coord.t[1]    = y;
        if (y >= (s16)work->field_9A) {
            coord->coord.t[1] = (s16)work->field_9A;
            player->flg       = 0;
            Gp_UpdateCoord(player);
            if (work->field_76C != 0) {
                vy   = -0x1A4;
                root = ((TmdObject*)arg0->extra)->field_8;
                Gp_SpawnEff(D_8011574C, root, 0x40, NULL);
                for (i = 0; i < 16; i++) {
                    vec.vx = (u32)rsin(i << 8) >> 3;
                    vec.vy = vy;
                    vec.vz = (u32)rcos(i << 8) >> 3;
                    Gp_SpawnEff(D_80115738, root, 0x01202148, &vec);
                }
            }
            id = 0x40060003;
            if ((arg0->spawnArg1 & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            work->obj_4B4.flags |= 0x4000;
            work2                = (Actor400600Work*)arg0->idMap;
            work2->field_720     = 2;
            work2->field_746     = 0x19;
            work2->field_726     = 0x10;
            work2->field_742     = 1;
            work->field_71E++;
        }
    }
}

void func_actor_400600_8013479C(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    s16              v;

    work = (Actor400600Work*)arg0->idMap;
    v    = func_actor_400600_801376EC(arg0);
    if (v != 0) {
        if ((u16)(v - 0x4E9) >= 0x6D0U) {
            func_actor_400600_80138B40(arg0);
            work3            = (Actor400600Work*)arg0->idMap;
            work3->field_71C = 2;
            work3->field_71E = 0;
            return;
        }
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12) / 20;
    } else {
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
    }
    func_actor_400600_80138B40(arg0);
    work2            = (Actor400600Work*)arg0->idMap;
    work2->field_720 = 4;
    work2->field_726 = 0x10;
    work2->field_746 = 0x15;
    work2->field_742 = 1;
    work->field_722  = -0x2A;
    work->field_724  = 0;
    work->field_718  = 0;
    work->field_71E++;
}

void func_actor_400600_80134970(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    SVECTOR          vec;
    s32              i;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;
    s16              vy;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->field_730 = 0;
        work->field_767 = 1;
    }
    coord->coord.t[0] += (s16)work->field_98;
    coord->coord.t[2] += (s16)work->field_9C;
    work->field_722   += 6;
    work->field_724   += work->field_722;
    y                  = coord->coord.t[1] + work->field_724;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        vy                = -0x1A4;
        if (work->field_76C != 0) {
            root = ((TmdObject*)arg0->extra)->field_8;
            Gp_SpawnEff(D_8011574C, root, 0x40, NULL);
            for (i = 0; i < 16; i++) {
                vec.vx = (u32)rsin(i << 8) >> 3;
                vec.vy = vy;
                vec.vz = (u32)rcos(i << 8) >> 3;
                Gp_SpawnEff(D_80115738, root, 0x01202148, &vec);
            }
        }
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_720 = 2;
        work2->field_746 = 0x19;
        work2->field_726 = 0x10;
        work2->field_742 = 1;
        work->field_767  = 0;
        work->field_71E++;
    }
}

void func_actor_400600_80134B98(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    func_actor_400600_80139E68(arg0, 0xE, &work->field_88);
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_82   = (work->field_82 + 0x800) & 0xFFF;
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_726 = 0x10;
        work2->field_746 = 2;
        work2->field_742 = 2;
        Actor400600_RebuildRotation(arg0);
        Actor400600_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        work->field_769  = 0;
        work3            = (Actor400600Work*)arg0->idMap;
        work3->field_71C = 2;
        work3->field_71E = 0;
    }
}

void func_actor_400600_80134E28(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_718++;
    if ((s16)work->field_718 < 0x11) {
        func_actor_400600_80136FA8(arg0);
        return;
    }
    if ((s16)work->field_718 == 0x11) {
        work->field_730      = 0;
        work->field_767      = 1;
        work->field_73E      = work->field_9A;
        work->obj_4B4.flags &= ~0x4000;
    }
    /* A separate statement: written inline, fold turns `a - (y + 400)` into
     * `(a - 400) - y`. */
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->field_80    += (0x800 - (s16)work->field_80) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->obj_4B4.flags |= 0x4000;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->field_80       = 0;
        work->field_84       = 0x800;
        work->field_82      += 0x800;
        Actor400600_RebuildRotation(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_720 = 2;
        work2->field_726 = 0x10;
        work2->field_746 = 0x19;
        work2->field_742 = 1;
        work->field_767  = 0;
        work->field_768  = 1;
        work->field_71E++;
    }
}

void func_actor_400600_801350F4(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    GsCOORDINATE2*   coord;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_718++;
    if ((s16)work->field_718 < 8) {
        ActorsShared80139dcc(arg0, 3, (ActorsShared80139dccPos*)&work->field_88);
        return;
    }
    work->field_88.x += ((s16)work->field_98 - work->field_88.x) >> 2;
    work->field_88.z += ((s16)work->field_9C - work->field_88.z) >> 2;
    func_actor_400600_80139E68(arg0, 3, &work->field_88);
    work->field_722   += 2;
    work->field_724   += work->field_722;
    coord->coord.t[1] += work->field_724;
    if ((work->field_80 & 0xFFF) != 0x800) {
        work->field_80 -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->field_768   = 0;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->field_80    = 0;
        work->field_84    = 0;
        work->field_82   += 0x800;
        Actor400600_RebuildRotation(arg0);
        work2            = (Actor400600Work*)arg0->idMap;
        work2->field_720 = 2;
        work2->field_726 = 0x10;
        work2->field_746 = 0x19;
        work2->field_742 = 1;
        Actor400600_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_718 = 0;
        work->field_71E++;
    }
}

void func_actor_400600_80135450(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if ((s16)work->field_718 == 0) {
        id = 0x40060003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_767 = 0;
        work->field_718++;
    }
    if ((func_actor_400600_801370F4(arg0) << 0x10) == 0 && (ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        rnd                     = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState             = rnd;
        work->field_710.h.timer = ((rnd >> 0x10) & 0x1F) + 0xD2;
        work2                   = (Actor400600Work*)arg0->idMap;
        work2->field_71C        = 2;
        work2->field_71E        = 0;
    }
}

void func_actor_400600_80135578(Task* arg0)
{
    Actor400600Work* work;
    Actor400600Work* work2;
    Actor400600Work* work3;
    u32              sound;
    s32              id;
    s32              pan;
    u32              rnd;

    work = (Actor400600Work*)arg0->idMap;
    if ((s16)work->field_718 == 0) {
        work->field_767 = 0;
        id              = 0x40060006;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0006;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8);
        pan   = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_718++;
    }
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        if (work->field_730 != 3) {
            work2            = (Actor400600Work*)arg0->idMap;
            work2->field_720 = 2;
            work2->field_726 = 0x10;
            work2->field_746 = 0x14;
            work2->field_742 = 1;
            work->field_71E++;
            return;
        }
        work->field_730  = 0;
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_750  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3            = (Actor400600Work*)arg0->idMap;
        work3->field_71C = 5;
        work3->field_71E = 0;
    }
}

/// Spawn the two child models from `D_actor_400600_80151AF8`, parent them to
/// root parts 10 and 7 at +/-0x200 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
void func_actor_400600_801356E0(Task* arg0)
{
    Actor400600Work*         work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           root;
    GsCOORDINATE2*           parent;
    GsCOORDINATE2*           parent2;
    Task*                    task;
    TmdObject*               obj;
    TmdObject*               dst;
    TmdObject*               src;
    MATRIX*                  mdst;
    ActorsShared8016a538Mat* pm;
    ActorsShared8016a538Mat* pm2;
    ActorsShared8016a538Mat  m;

    root            = ((TmdObject*)arg0->extra)->field_8;
    work            = (Actor400600Work*)arg0->idMap;
    parent          = &root[7];
    parent2         = &root[10];
    task            = Task_SpawnFromTable(&D_actor_400600_80151AF8, 0, 0, 0);
    work->field_704 = task;
    if (task != NULL) {
        obj               = (TmdObject*)task->extra;
        coord             = obj->field_8;
        obj->field_C      = 0x80;
        coord->coord.t[0] = 0x200;
        coord->sub        = parent2;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0;
        pm                = &m;
        pm->ident.m00_m01 = 0x1000;
        pm->ident.m02_m10 = 0;
        pm->ident.m11_m12 = 0x1000;
        pm->ident.m20_m21 = 0;
        pm->ident.m22     = 0x1000;
        func_8004BFF8(-0x180, &pm->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm->mat.m[0][0];
        mdst->m[0][1] = pm->mat.m[0][1];
        mdst->m[0][2] = pm->mat.m[0][2];
        mdst->m[1][0] = pm->mat.m[1][0];
        mdst->m[1][1] = pm->mat.m[1][1];
        mdst->m[1][2] = pm->mat.m[1][2];
        mdst->m[2][0] = pm->mat.m[2][0];
        mdst->m[2][1] = pm->mat.m[2][1];
        mdst->m[2][2] = pm->mat.m[2][2];
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)task->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
        obj->field_1C = &work->matrix_40;
        obj->field_20 = &work->matrix_20;
    }
    task = work->field_708 = Task_SpawnFromTable(&D_actor_400600_80151AF8, 1, 0, 0);
    if (task != NULL) {
        obj               = (TmdObject*)task->extra;
        coord             = obj->field_8;
        obj->field_C      = 0x80;
        coord->sub        = parent;
        coord->coord.t[0] = -0x200;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0;
        src               = (TmdObject*)arg0->extra;
        dst               = (TmdObject*)task->extra;
        dst->field_24     = src->field_24;
        dst->field_25     = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
        pm2                = &m;
        pm2->ident.m00_m01 = 0x1000;
        pm2->ident.m02_m10 = 0;
        pm2->ident.m11_m12 = 0x1000;
        pm2->ident.m20_m21 = 0;
        pm2->ident.m22     = 0x1000;
        func_8004BFF8(0x180, &pm2->mat);
        mdst          = &coord->coord;
        mdst->m[0][0] = pm2->mat.m[0][0];
        mdst->m[0][1] = pm2->mat.m[0][1];
        mdst->m[0][2] = pm2->mat.m[0][2];
        mdst->m[1][0] = pm2->mat.m[1][0];
        mdst->m[1][1] = pm2->mat.m[1][1];
        mdst->m[1][2] = pm2->mat.m[1][2];
        mdst->m[2][0] = pm2->mat.m[2][0];
        mdst->m[2][1] = pm2->mat.m[2][1];
        mdst->m[2][2] = pm2->mat.m[2][2];
        obj->field_1C = &work->matrix_40;
        obj->field_20 = &work->matrix_20;
    }
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135DDC`, plus the dust ring each window spawns when
/// `field_76C` says the actor is over ground.
void func_actor_400600_80135998(Task* arg0, s16 arg1)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The first window starts at frame 0, written as the literal both of its
     * tests fold against; the wider first temp is what keeps each zero arm a
     * fresh constant instead of a copy of the previous one. */
    u32     tmp0;
    u8      tmp1;
    u8      tmp2;
    u8      end0;
    u8      start1;
    u8      end1;
    s32     id;
    u32     sound;
    u32     voice;
    s32     pan;
    SVECTOR vec;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (work->field_746 != 2) {
        work->field_726 = 0x10;
        work->field_746 = 2;
        work->field_742 = 2;
        Actor400600_TickAnim(arg0);
    }
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_400600_8013892C(arg0) << 0x10) != 0) {
        work->field_748 = 0;
        work->field_726 = arg1;
    }
    if (work->field_748 == 0) {
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40060001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, 0x40, &vec);
        }
    }
    if (work->field_748 == start1) {
        ActorsShared80139dcc(arg0, 8, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40060002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        if (work->field_76C != 0) {
            vec.vx = 0;
            vec.vy = -0x1A4;
            vec.vz = 0;
            Gp_SpawnEff(D_8011574C, coord, 0x40, &vec);
        }
    }
    if (work->field_748 >= 0 && work->field_748 <= end0) {
        func_actor_400600_80139E68(arg0, 0xB, &work->field_88);
        work->field_734 = 8;
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139E68(arg0, 8, &work->field_88);
        work->field_734 = 0xB;
    }
    coord->flg = 0;
}

/// Animation state 4: drives the two sound/tracking windows the same way
/// `func_actor_400600_801361AC` does, one frame-count pair per sound event.
void func_actor_400600_80135DDC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (work->field_746 != 4) {
        work->field_726 = 0x10;
        work->field_720 = 4;
        work->field_746 = 4;
        work->field_742 = 1;
        Actor400600_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_748 = 0;
    }
    if (work->field_748 == start0) {
        ActorsShared80139dcc(arg0, 8, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40060001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_748 == start1) {
        ActorsShared80139dcc(arg0, 0xB, (ActorsShared80139dccPos*)&work->field_88);
        id = 0x40060002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_748 >= start0 && work->field_748 <= end0) {
        func_actor_400600_80139E68(arg0, 8, &work->field_88);
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139E68(arg0, 0xB, &work->field_88);
    }
    coord->flg = 0;
}

/// Drives the two sound/tracking windows of the current animation. The window
/// bounds are frame counts derived from the playback rate (`field_726`), each
/// read back through `arg0->idMap` rather than the cached `work` pointer.
void func_actor_400600_801361AC(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    /* The bounds are computed into their own temporaries first; a `u8` temp is
     * what keeps the zero arm of each test out of the surrounding block. */
    u8  tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 start0;
    u32 sound;
    s32 pan;

    work  = (Actor400600Work*)arg0->idMap;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (work->field_746 != 2) {
        work->field_726 = 0x10;
        work->field_746 = 2;
        work->field_742 = 2;
        Actor400600_TickAnim(arg0);
    }
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end0 = tmp0;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    start1 = tmp1;
    if (((Actor400600Work*)arg0->idMap)->field_726 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor400600Work*)arg0->idMap)->field_726) >> 4;
    }
    end1 = tmp2;
    if ((ActorsShared8013a0b0(arg0) << 0x10) != 0) {
        work->field_748 = 0;
    }
    /* The first window starts at frame 0, and the original compares against it
     * in a register: a literal 0 is folded into `$zero` by CSE. */
    SOFT_MOVE_ZERO(start0);
    if (work->field_748 == start0) {
        func_actor_400600_80139F4C(arg0, 0xB, &work->field_88);
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000A;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_748 == start1) {
        func_actor_400600_80139F4C(arg0, 8, &work->field_88);
        sound   = ((GpEnemy*)arg0->spawnArg2)->field_8;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x404A000B;
        pan     = Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if (work->field_748 >= start0 && work->field_748 <= end0) {
        func_actor_400600_80139FE0(arg0, 0xB, &work->field_88);
        work->field_734 = 8;
    }
    if (work->field_748 >= start1 && work->field_748 <= end1) {
        func_actor_400600_80139FE0(arg0, 8, &work->field_88);
        work->field_734 = 0xB;
    }
    coord->flg = 0;
}

void func_actor_400600_80136558(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TmdObject*       model = (TmdObject*)arg0->extra;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    s32              state = work->field_75C.word & 0xFFFF0000;
    s16              count;

    if (state == 0x1000000) {
        work->field_73A = (u16)work->field_73A + ((0xFF - work->field_73A) >> 5);
        work->field_740++;
        if (work->field_740 >= 0x20) {
            model->field_C &= ~2;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
        }
    } else if (state == 0x1010000) {
        work->field_73A = (u16)work->field_73A + (-work->field_73A >> 3);
        count           = work->field_740 + 1;
        work->field_740 = count;
        if (count >= 0x12) {
            if (work->field_75A != 0) {
                enemy->node.field_4 = 4;
            } else {
                enemy->node.field_4 = 5;
            }
            model->field_C |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            work->field_740             = 0;
            work->field_75C.b.field_75F = 0;
            work->field_73A             = 0;
        }
    }
}

void func_actor_400600_80136670(Task* arg0)
{
    Actor400600Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   player;
    GameActor*       actor;
    GpActorWork*     slot;
    SVECTOR          v;
    s16              a;
    s16              b;

    work              = (Actor400600Work*)arg0->idMap;
    coord             = ((TmdObject*)arg0->extra)->field_8;
    slot              = Gp_ActorSlots[0];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (slot == NULL) {
        return;
    }
    player = slot->extra->field_8;
    actor  = slot->actor;
    if (work->field_76E != 0) {
        a = func_actor_400600_8013886C(arg0);
        b = func_actor_400600_8013886C((Task*)slot);
        if (a == 2 && (b == 1 || b == 6)) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else if (a == 3 && ((b >= 1 && b <= 2) || b == 6)) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = -0x1194;
        } else if (a == 4 && ((b >= 1 && b <= 3) || b == 6)) {
            work->field_A8.x = -0x125C;
            work->field_A8.y = 0;
            work->field_A8.z = -0x1194;
        } else if (a == 5 && ((b >= 1 && b <= 4) || b == 6)) {
            work->field_A8.x = -0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0x1194;
        } else if (b == 1 && a == 6) {
            work->field_A8.x = 0;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else if (b != 1 && a == 1) {
            work->field_A8.x = 0x1194;
            work->field_A8.y = 0;
            work->field_A8.z = 0;
        } else {
            work->field_A8.x = player->coord.t[0];
            work->field_A8.y = player->coord.t[1];
            work->field_A8.z = player->coord.t[2];
        }
    } else {
        work->field_A8.x = player->coord.t[0];
        work->field_A8.y = player->coord.t[1];
        work->field_A8.z = player->coord.t[2];
    }
    v.vx            = work->field_A8.x - coord->coord.t[0];
    v.vy            = work->field_A8.y - coord->coord.t[1];
    v.vz            = work->field_A8.z - coord->coord.t[2];
    work->field_728 = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_72C = (ratan2(v.vx, v.vz) - work->field_82) & 0xFFF;
    work->field_72A = (ratan2(-v.vx, -v.vz) - actor->field_52) & 0xFFF;
}

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F34);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F40);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F60);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F70);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F7C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F8C);

INCLUDE_RODATA("actors/nonmatchings/actor_400600/actor_400600", D_actor_400600_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_400600/actor_400600", func_actor_400600_80136968);
