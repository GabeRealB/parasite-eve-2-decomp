#include "common.h"
#include "main/stage.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "actors/actor.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_80131fc8.h"
#include <psyq/libgte.h>

/// State handlers copied onto the stack by func_actor_400500_80135770.
typedef struct Actor400500TaskFuncTable13 {
    TaskFunc funcs[13];
} Actor400500TaskFuncTable13;
STATIC_ASSERT_SIZEOF(Actor400500TaskFuncTable13, 0x34);

/// View-space sample written by `func_actor_400500_8013DBCC`: the X and Z of
/// the translation `Gp_WorldToLocal` produces for one of the actor's
/// coordinate nodes. `func_actor_400500_80132C54` passes
/// `Actor400500Work::field_9A0` as the destination, so the slot lives inside
/// the work block. Only `x` and `z` are ever written; the middle halfword is
/// kept so the layout matches the sibling `Actor400600ViewPos`.
typedef struct Actor400500ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor400500ViewPos;
STATIC_ASSERT_SIZEOF(Actor400500ViewPos, 0x6);

/// Overlay of the model-root `GpCoord` at `TmdObject::coords`.
/// `x` / `z` are the low halfwords of `coord.t[0]` / `coord.t[2]`.
typedef struct Actor400500RootXZ {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[6];
    /* 0x20 */ u16  z;
} Actor400500RootXZ;

/// Dual-width hit flags at `Actor400500Work` + 0x4C. Guards test bit 0 as a
/// halfword and then bits 0x102 as a word, the same shape as
/// `Actor341700Flags` / `ActorsShared8016974c`.
typedef union Actor400500HitFlags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor400500HitFlags;
STATIC_ASSERT_SIZEOF(Actor400500HitFlags, 0x4);

/// Prefix view of `Actor400500Work` for the dual-width flags at +0x4C.
/// That address is `slots[1].field_10` / `field_12` (0x10 into the second
/// animation slot), the same overlap `ActorsShared80168d3cWork` uses for
/// `flags_EC`.
typedef struct Actor400500HitView {
    /* 0x00 */ byte                pad[0x4C];
    /* 0x4C */ Actor400500HitFlags flags_4C;
} Actor400500HitView;

/// Per-actor state block for the `actor_400500` overlay.
///
/// `func_actor_400500_80135414` is the overlay's only allocator: it calls
/// `memCalloc(0xA50, 0)` and stores the result in the `Task::work` slot
/// (0x1C), which an enemy actor reuses for its own work block, so it is *not*
/// a `TaskIdMap` here. Reach it with `(Actor400500Work*)task->work`. The
/// same function hands `&work->lightMtx` / `&work->colorMtx` to the
/// `TmdObject` at `Task::extra` (`field_1C` / `field_20`) and `work->rec0`
/// to `GpEnemy::recs`; the size below is the allocation, not a guess.
/// `obj1`/`obj2` share `rec1`; `obj3`/`obj4` share `rec2`.
///
/// `field_A06` / `field_A08` are the state and sub-state indices the handler
/// tables walk and `field_A04` is the per-state frame counter, mirroring
/// `Actor400600Work::field_71C` / `field_71E` / `field_718`.
///
/// The block opens with the 0x14-byte animation context and eighteen 0x28-byte
/// slots. `func_actor_400500_8013DC4C` walks slots 1..17, copies the low byte
/// of `field_9F8` into each slot's `field_9`, resets them from `field_9FE`,
/// and latches that id in `field_9FC`. The second slot's `field_10` overlaps
/// `Actor400500HitView::flags_4C`.
typedef struct Actor400500Work {
    /* 0x000 */ GpAnimCtx          anim;
    /* 0x014 */ GpAnimSlot         slots[0x12];
    /* 0x2E4 */ byte               pad_2E4[0x524];
    /* 0x808 */ MATRIX             matrix_808; // model root coord, copied on the light-mode path
    /* 0x828 */ GpObj              obj0;
    /* 0x848 */ GpRec18            rec0[3];
    /* 0x890 */ GpObj              obj1;
    /* 0x8B0 */ GpObj              obj2;
    /* 0x8D0 */ GpRec18            rec1[1];
    /* 0x8E8 */ GpObj              obj3;
    /* 0x908 */ GpObj              obj4;
    /* 0x928 */ GpRec18            rec2[1];
    /* 0x940 */ GpEffArg           eff_940; // part-3 coord, scale 0x100, count 3
    /* 0x948 */ s16                field_948;
    /* 0x94A */ s16                field_94A;
    /* 0x94C */ s16                field_94C;
    /* 0x94E */ byte               pad_94E[2];
    /* 0x950 */ u16                field_950; // low half of root coord.t[0]
    /* 0x952 */ byte               pad_952[2];
    /* 0x954 */ u16                field_954; // low half of root coord.t[2]
    /* 0x956 */ byte               pad_956[6];
    /* 0x95C */ MATRIX             colorMtx;  // TmdObject::colorMtx
    /* 0x97C */ MATRIX             lightMtx;  // TmdObject::lightMtx
    /* 0x99C */ byte               pad_99C[4];
    /* 0x9A0 */ Actor400500ViewPos field_9A0;
    /* 0x9A6 */ byte               pad_9A6[0x16];
    /* 0x9BC */ s16                field_9BC;
    /* 0x9BE */ byte               pad_9BE[2];
    /* 0x9C0 */ VECTOR             field_9C0; // own position, copied from the model root coord.t
    /* 0x9D0 */ SVECTOR            field_9D0;
    /* 0x9D8 */ SVECTOR            field_9D8; // ApplyMatrixSV dest; vz is the former field_9DC
    /* 0x9E0 */ s16                field_9E0;
    /* 0x9E2 */ s16                field_9E2;
    /* 0x9E4 */ s16                field_9E4;
    /* 0x9E6 */ byte               pad_9E6[0xA];
    /* 0x9F0 */ Task*              field_9F0[2]; // child tasks, killed on death
    /* 0x9F8 */ s16                field_9F8;    // animation speed / step scale
    /* 0x9FA */ s16                field_9FA;    // animation request kind
    /* 0x9FC */ u16                field_9FC;    // last animation id the slots were reset to
    /* 0x9FE */ s16                field_9FE;    // animation id
    /* 0xA00 */ s16                field_A00;    // blend frame; incremented as u16, passed signed to 8013DD8C
    /* 0xA02 */ s16                field_A02;    // identity scale written with the matrix copy
    /* 0xA04 */ u16                field_A04;    // per-state frame counter
    /* 0xA06 */ u16                field_A06;    // state index
    /* 0xA08 */ u16                field_A08;    // sub-state index
    /* 0xA0A */ u16                field_A0A;
    /* 0xA0C */ byte               pad_A0C[2];
    /* 0xA0E */ s16                field_A0E; // extra arg forwarded to func_800B4114, then cleared
    /* 0xA10 */ s16                field_A10;
    /* 0xA12 */ s16                field_A12;
    /* 0xA14 */ byte               pad_A14[0x2];
    /* 0xA16 */ s16                field_A16; // distance, compared to a range
    /* 0xA18 */ s16                field_A18;
    /* 0xA1A */ s16                field_A1A; // 1: sample part 0xE when heading is 0x400/0xC00
    /* 0xA1C */ u16                field_A1C; // mode; 2, 3 and 6 take the heading-0 path
    /* 0xA1E */ u16                field_A1E; // flags; bit 0x1 and bit 0x2 gate animations
    /* 0xA20 */ s16                field_A20;
    /* 0xA22 */ u16                field_A22; // frame counter used when field_A1C == 5
    /* 0xA24 */ s16                field_A24; // copied to TmdObject::lightLevel
    /* 0xA26 */ u16                field_A26; // heading countdown, decremented by 0x80
    /* 0xA28 */ s16                field_A28;
    /* 0xA2A */ s16                field_A2A; // fade sub-state timer
    /* 0xA2C */ s16                field_A2C; // countdown written with message kind 1
    /* 0xA2E */ s16                field_A2E; // duration copied onto field_A30
    /* 0xA30 */ s16                field_A30; // blocks setting field_A46 to 0x80 while nonzero
    /* 0xA32 */ s16                field_A32; // heading; >>3 as u16, compared to 0 as s16
    /* 0xA34 */ s16                field_A34; // gates the field_A1A==3 sub-state write
    /* 0xA36 */ u16                field_A36; // angle, range-tested as (a - 0x300) <= 0xA00
    /* 0xA38 */ s16                field_A38;
    /* 0xA3A */ s16                field_A3A;
    /* 0xA3C */ s16                field_A3C;
    /* 0xA3E */ s16                field_A3E;
    /* 0xA40 */ s16                field_A40;
    /* 0xA42 */ s16                field_A42;
    /* 0xA44 */ s16                field_A44; // hit cooldown
    /* 0xA46 */ s8                 field_A46; // signed flag; 0x81 means active mode 1
    /* 0xA47 */ s8                 field_A47;
    /* 0xA48 */ s8                 field_A48; // session-message handshake state
    /* 0xA49 */ s8                 field_A49;
    /* 0xA4A */ s8                 field_A4A;
    /* 0xA4B */ s8                 field_A4B; // last message kind 1..4
    /* 0xA4C */ s8                 field_A4C; // set with kind 1
    /* 0xA4D */ u8                 field_A4D; // selects message 0x3FF instead of 0x3F4
    /* 0xA4E */ byte               pad_A4E[2];
} Actor400500Work;
STATIC_ASSERT_SIZEOF(Actor400500Work, 0xA50);

extern u8        D_actor_400500_80153CB0[];
extern ActorZone D_actor_400500_80153D6C[];

void func_8004BFF8(s16 angle, MATRIX* matrix);
/// Still called by actor_206100, which includes this header; remove once that
/// entry is demoted.
void ActorsShared80132c4c(MATRIX* src, MATRIX* dst);
void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade);
void func_actor_400500_80138088(Task* task);
s32  func_actor_400500_8013B720(GpCoord* coord, MATRIX* matrix);

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];
extern u8    D_80072170;

/* The records closing three of the overlay's model streams, selected through
   `D_800678F0`. */
extern TmdSource D_actor_400500_8014393C;
extern TmdSource D_actor_400500_80143F40;
extern TmdSource D_actor_400500_80144624;

extern GpPairSrcE D_actor_400500_80153C90;
extern u8         D_actor_400500_80153CA0[];
extern u8         D_actor_400500_80153CC0[];
extern TaskDesc   D_actor_400500_80153D48;
extern u16        D_actor_400500_80153DB4[];
extern u8         D_actor_400500_80153DD4[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_400500_80132438(Task* arg0);
void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2);
s32  func_actor_400500_80132D74(Task* arg0);
void func_actor_400500_80132E94(Task* arg0);
s32  func_actor_400500_80133160(Task* arg0);
s32  func_actor_400500_80133358(Task* arg0);
s32  func_actor_400500_80133460(Task* arg0);
void func_actor_400500_801335E8(Task* arg0);
void func_actor_400500_80133B14(Task* arg0);
void func_actor_400500_8013403C(Task* arg0);
void func_actor_400500_8013456C(Task* arg0);
void func_actor_400500_80134D6C(s32 otz);
void func_actor_400500_80135770(Task* arg0);
void func_actor_400500_80135EBC(Task* arg0);
void func_actor_400500_801361EC(Task* arg0);
void func_actor_400500_8013662C(Task* arg0);
void func_actor_400500_80136864(Task* arg0);
void func_actor_400500_801369A4(Task* arg0);
void func_actor_400500_80136B94(Task* arg0);
void func_actor_400500_80136D00(Task* arg0);
void func_actor_400500_80136EB8(Task* arg0);
void func_actor_400500_80137034(Task* arg0);
void func_actor_400500_801371A0(Task* arg0);
void func_actor_400500_80137338(Task* arg0);
void func_actor_400500_80137478(Task* arg0);
void func_actor_400500_801385D0(Task* arg0);
void func_actor_400500_801387E8(Task* arg0);
void func_actor_400500_8013899C(Task* arg0);
void func_actor_400500_80138B78(Task* arg0);
void func_actor_400500_80138CE8(Task* arg0);
void func_actor_400500_80138DC4(Task* arg0);
void func_actor_400500_80138EA0(Task* arg0);
void func_actor_400500_8013905C(Task* arg0);
void func_actor_400500_801391B0(Task* arg0);
void func_actor_400500_801392D8(Task* arg0);
void func_actor_400500_80139448(Task* arg0);
void func_actor_400500_801395D0(Task* arg0);
void func_actor_400500_8013973C(Task* arg0);
void func_actor_400500_80139AC4(Task* arg0);
void func_actor_400500_80139C1C(Task* arg0);
void func_actor_400500_80139D70(Task* arg0);
void func_actor_400500_80139F6C(Task* arg0);
void func_actor_400500_8013A0B8(Task* arg0);
void func_actor_400500_8013A484(Task* arg0);
void func_actor_400500_8013A5D8(Task* arg0);
void func_actor_400500_8013A700(Task* arg0);
void func_actor_400500_8013A8E4(Task* arg0);
void func_actor_400500_8013AA98(Task* arg0);
void func_actor_400500_8013ABE4(Task* arg0);
void func_actor_400500_8013AD60(Task* arg0);
void func_actor_400500_8013AF44(Task* arg0);
void func_actor_400500_8013B228(Task* arg0);
void func_actor_400500_8013B374(Task* arg0);
void func_actor_400500_8013B4A4(Task* arg0);
void func_actor_400500_8013B5E0(Task* arg0);
s32  func_actor_400500_8013B920(GpCoord* coord, SVECTOR* pos);
void func_actor_400500_8013BA24(Task* arg0);
void func_actor_400500_8013BAA4(Task* arg0);
void func_actor_400500_8013BB18(Task* arg0);
void func_actor_400500_8013BBB0(Task* arg0);
void func_actor_400500_8013BC9C(Task* arg0);
void func_actor_400500_8013BCCC(Task* arg0);
void func_actor_400500_8013BD64(Task* arg0);
void func_actor_400500_8013BE50(Task* arg0);
void func_actor_400500_8013BEC4(Task* arg0);
void func_actor_400500_8013BFB0(Task* arg0);
void func_actor_400500_8013C018(Task* arg0);
void func_actor_400500_8013C108(Task* arg0);
void func_actor_400500_8013C174(Task* arg0);
void func_actor_400500_8013C218(Task* arg0);
void func_actor_400500_8013C348(Task* arg0);
void func_actor_400500_8013C3C4(Task* arg0);
void func_actor_400500_8013C474(Task* arg0);
void func_actor_400500_8013C508(Task* arg0);
void func_actor_400500_8013C578(Task* arg0);
void func_actor_400500_8013C61C(Task* arg0);
void func_actor_400500_8013C750(Task* arg0);
void func_actor_400500_8013C7A4(Task* arg0);
void func_actor_400500_8013C818(Task* arg0);
void func_actor_400500_8013C908(Task* arg0);
void func_actor_400500_8013C9D4(Task* arg0);
void func_actor_400500_8013CA38(Task* arg0);
void func_actor_400500_8013CB0C(Task* arg0);
void func_actor_400500_8013CBD8(Task* arg0);
void func_actor_400500_8013CCDC(Task* arg0);
void func_actor_400500_8013CDA8(Task* arg0);
void func_actor_400500_8013CE9C(Task* arg0);
void func_actor_400500_8013CF68(Task* arg0);
void func_actor_400500_8013D078(Task* arg0);
void func_actor_400500_8013D144(Task* arg0);
void func_actor_400500_8013D210(Task* arg0);
void func_actor_400500_8013D274(Task* arg0);
void func_actor_400500_8013D2D8(Task* arg0);
void func_actor_400500_8013D3B8(Task* arg0);
void func_actor_400500_8013D420(Task* arg0);
void func_actor_400500_8013D4F0(Task* arg0);
void func_actor_400500_8013D59C(Task* arg0);
void func_actor_400500_8013D630(Task* arg0);
void func_actor_400500_8013D6A0(Task* arg0);
void func_actor_400500_8013D744(Task* arg0);
void func_actor_400500_8013D878(Task* arg0);
void func_actor_400500_8013D8CC(Task* arg0);
void func_actor_400500_8013D958(Task* arg0);
void func_actor_400500_8013D9DC(Task* arg0);
void func_actor_400500_8013D9F4(Task* arg0);
void func_actor_400500_8013DA24(Task* arg0);
void func_actor_400500_8013DA68(Task* arg0);
void func_actor_400500_8013DACC(Task* arg0);
void func_actor_400500_8013DB64(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DB78(Task* arg0);
void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2);
void func_actor_400500_8013DC4C(Task* arg0);
void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2);
void func_actor_400500_8013DCD4(Task* arg0);
s32  func_actor_400500_8013DD8C(Task* arg0, s16 arg1);
s32  func_actor_400500_8013DDEC(Task* arg0);
void func_actor_400500_8013DE2C(MATRIX* src, MATRIX* dst);
void func_actor_400500_8013DEFC(Task* arg0);
void func_actor_400500_8013DF50(Task* arg0);
void func_actor_400500_8013DF74(Task* arg0);
void func_actor_400500_8013DFE4(Task* arg0);

void func_actor_400500_80132000(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;

    work->obj0.coord    = &arg0->extra.tmd->coords[3];
    work->obj0.ctx.recs = work->rec0;
    work->obj0.pos.vz   = 0x110;
    work->obj0.pos.vx   = 0;
    work->obj0.pos.vy   = 0;
    work->obj0.key      = 0x30005;
    work->obj0.radius   = 0x260;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(work->rec0, 3, 0);
    work->obj0.flags |= 0x8000;

    work->obj1.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj1.coord    = &arg0->extra.tmd->coords[7];
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = -0x460;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.radius   = 0x290;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags &= 0x7FFF;

    work->obj2.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj2.coord    = &arg0->extra.tmd->coords[7];
    work->obj2.ctx.recs = work->rec1;
    work->obj2.pos.vx   = -0x200;
    work->obj2.pos.vy   = 0;
    work->obj2.pos.vz   = 0;
    work->obj2.radius   = 0x250;
    work->obj2.flags    = 1;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj2.flags &= 0x7FFF;

    work->obj3.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj3.coord    = &arg0->extra.tmd->coords[10];
    work->obj3.ctx.recs = work->rec2;
    work->obj3.pos.vx   = 0x460;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.radius   = 0x290;
    work->obj3.flags    = 1;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj3.flags &= 0x7FFF;

    work->obj4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj4.coord    = &arg0->extra.tmd->coords[10];
    work->obj4.ctx.recs = work->rec2;
    work->obj4.pos.vx   = 0x200;
    work->obj4.pos.vy   = 0;
    work->obj4.pos.vz   = 0;
    work->obj4.radius   = 0x250;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec2, 1, 0);
    work->obj4.flags &= 0x7FFF;
}

void func_actor_400500_8013226C(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    GpCoord*         parts;
    GpCoord*         part7;
    GpCoord*         part10;
    GpCoord*         coord;
    Task*            child;
    TmdObject*       extra;
    TmdObject*       tmd;
    TmdObject*       parentTmd;

    parts              = arg0->extra.tmd->coords;
    work               = (Actor400500Work*)arg0->work;
    part7              = &parts[7];
    part10             = &parts[10];
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 0, 0, 0);
    work->field_9F0[0] = child;
    extra              = child->extra.tmd;
    coord              = extra->coords;
    extra->flags       = 0x80;
    coord->sub         = part10;
    coord->coord.t[0]  = 0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    src                = &rot;
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(-0x180, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    parentTmd  = arg0->extra.tmd;
    tmd        = child->extra.tmd;
    tmd->tpage = parentTmd->tpage;
    tmd->clut  = parentTmd->clut;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    child              = Task_SpawnFromTable(&D_actor_400500_80153D48, 1, 0, 0);
    work->field_9F0[1] = child;
    extra              = child->extra.tmd;
    coord              = extra->coords;
    extra->flags       = 0x80;
    coord->sub         = part7;
    coord->coord.t[0]  = -0x400;
    coord->coord.t[1]  = 0;
    coord->coord.t[2]  = 0;
    parentTmd          = arg0->extra.tmd;
    tmd                = child->extra.tmd;
    tmd->tpage         = parentTmd->tpage;
    tmd->clut          = parentTmd->clut;
    if (tmd->buffer != NULL) {
        tmdProcessStream(tmd);
        tmdProcessStream(tmd);
    }
    rot.ident.m00_m01  = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    func_8004BFF8(0x180, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
}

void func_actor_400500_80132438(Task* arg0)
{
    SVECTOR          dir;
    SVECTOR*         dirp;
    SVECTOR          delta;
    OverlayMat       rot;
    OverlayMat*      src;
    Actor400500Work* work;
    GpCoord*         coord;
    GpCoord*         other;
    s16              dist;
    s16              heading;
    s16              vz;
    s32              y;
    s32              z;
    s32              one;
    u16              counter;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (Gp_ActorSlots[0] != NULL) {
        other              = Gp_ActorSlots[0]->extra.tmd->coords;
        work->field_9C0.vx = coord->coord.t[0];
        work->field_9C0.vy = coord->coord.t[1];
        work->field_9C0.vz = coord->coord.t[2];
        if ((s16)work->field_A1C != 5) {
            dir.vx = (u16)other->coord.t[0] - (u16)coord->coord.t[0];
            dir.vy = (u16)other->coord.t[1] - (u16)coord->coord.t[1];
            dir.vz = (u16)other->coord.t[2] - (u16)coord->coord.t[2];
        } else {
            work->field_A32 = 2;
            counter         = work->field_A22 + 1;
            work->field_A22 = counter;
            if (!(counter & 0x100)) {
                dir.vx = 0x2710 - (u16)coord->coord.t[0];
            } else {
                dir.vx = 0x3E8 - (u16)coord->coord.t[0];
            }
            y      = -0x3E8;
            dir.vy = y - (u16)coord->coord.t[1];
            z      = -0x20D0;
            dir.vz = z - (u16)coord->coord.t[2];
        }
        dist = SquareRoot0((dir.vx * dir.vx) + (dir.vz * dir.vz));
        do {
            work->field_9E0 = (u16)dir.vx;
            dirp            = &dir;
            work->field_9E2 = (u16)dir.vy;
        } while (0);
        vz              = (u16)dir.vz;
        work->field_A16 = dist;
        work->field_9E4 = vz;
        VectorNormalSS(dirp, dirp);
        work->field_A36    = (ratan2(dir.vx, dir.vz) - (u16)work->field_94A) & 0xFFF;
        delta.vx           = (u16)other->coord.t[0] - (u16)work->field_9D0.vx;
        delta.vy           = (u16)other->coord.t[1] - (u16)work->field_9D0.vy;
        one                = 0x1000;
        delta.vz           = (u16)other->coord.t[2] - (u16)work->field_9D0.vz;
        src                = &rot;
        rot.ident.m00_m01  = one;
        rot.ident.m02_m10  = 0;
        src->ident.m11_m12 = one;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = one;
        heading            = work->field_94A;
        ((void (*)(s32, MATRIX*))func_8004BFF8)(-heading, &src->mat);
        ApplyMatrixSV(&src->mat, &delta, &work->field_9D8);
    }
}

void func_actor_400500_80132628(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, s32 shade)
{
    MATRIX    firstMatrix;
    MATRIX    secondMatrix;
    SVECTOR   first;
    SVECTOR   second;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    s32       screen0;
    s32       screen1;
    s32       screen2;
    s32       screen3;
    s32       perspective;
    s32       texU1;
    s32       flags;
    s16       angle;
    GpCoord*  secondCoord;
    GpCoord*  firstCoord;
    s32       offset0;
    s32       offset1;
    s32       offset2;
    s32       offset3;
    s32       halfX;
    s32       halfZ;
    s32       depth;
    GpCoord*  coords;
    GpCoord*  viewCoord;
    POLY_FT4* poly;
    u8        room;
    u8        col;
    u8        texU0;
    u8        texV0;
    s32       x3val;

    col         = shade;
    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &firstCoord->workm, &firstMatrix);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)height;
        second.vy  = (s16)height;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        angle      = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX      = (first.vx - second.vx) / 2;
        halfZ      = (first.vz - second.vz) / 2;
        offset0    = rcos(angle) * width;
        corner0.vy = (s16)height;
        corner0.vx = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1    = rcos(angle) * width;
        corner1.vy = (s16)height;
        corner1.vx = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2    = rcos(angle) * width;
        corner2.vy = (s16)height;
        corner2.vx = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3    = rcos(angle) * width;
        corner3.vy = (s16)height;
        corner3.vx = (second.vx + (offset3 >> 0xC)) - halfX;
        corner3.vz = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `Gfx_ViewWorldMtx`, whose high half the GTE loads below share. */
        viewCoord      = PARENT_OF(&Gfx_ViewWorldMtx, GpCoord, workm);
        viewCoord->flg = 0;
        Gp_UpdateCoord(viewCoord);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3,
                              &perspective, &flags);
        if (flags >= 0) {
            poly           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code       = 0x2E;
            texU0            = 0xC0;
            texV0            = 0x98;
            *(s32*)&poly->x0 = screen0;
            *(s32*)&poly->x1 = screen1;
            poly->tpage      = 0x48;
            *(s32*)&poly->x2 = screen2;
            x3val            = screen3;
            SOFT_COMPILER_BARRIER();
            texU1 = 0xF7;
            SOFT_COMPILER_BARRIER();
            poly->v0         = texV0;
            poly->v1         = texV0;
            poly->v2         = 0xCF;
            poly->v3         = 0xCF;
            poly->u0         = texU0;
            poly->u1         = texU1;
            poly->u2         = texU0;
            poly->u3         = texU1;
            *(s32*)&poly->x3 = x3val;
            poly->clut       = 0x4283;
            room             = gGameSession->at4.loc.room;
            if ((room == 1) || (room == 3) || (room == 5) || (room == 6)) {
                poly->r0 = col;
                poly->g0 = col;
                poly->b0 = col;
            } else {
                poly->r0 = shade;
                poly->g0 = col >> 1;
                poly->b0 = shade;
            }
            addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
    }
}

void func_actor_400500_80132AB0(Task* arg0, s16 arg1, s32 arg2)
{
    s32 temp_s2;

    temp_s2 = arg2 & 0xFF;
    func_actor_400500_80132628(arg0, 3, 9, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 9, 0xA, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xA, 0xB, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 3, 6, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 6, 7, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 7, 8, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 5, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xC, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xC, 0xD, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xD, 0xE, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 1, 0xF, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0xF, 0x10, 0x100, (s32)arg1, temp_s2);
    func_actor_400500_80132628(arg0, 0x10, 0x11, 0x100, (s32)arg1, temp_s2);
}

void func_actor_400500_80132C54(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    GpCoord*         coord;
    s32              tx;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (D_80072170) {
        case 1:
            tx                = 0x800;
            work->field_94A   = tx;
            work->field_94C   = tx;
            tx                = 0x14A0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 2:
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0x4074;
            work->field_94A   = 0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
        case 3:
            tx                = 0xC00;
            work->field_94A   = tx;
            tx                = 0x800;
            work->field_94C   = tx;
            tx                = 0xFA0;
            coord->coord.t[0] = tx;
            tx                = -0xFA0;
            coord->coord.t[1] = tx;
            tx                = -0x209E;
            coord->coord.t[2] = tx;
            break;
    }
    tx                 = 0x1000;
    src                = &rot;
    rot.ident.m00_m01  = tx;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = tx;
    src->ident.m20_m21 = 0;
    src->ident.m22     = tx;
    RotMatrixZ(work->field_94C, &src->mat);
    func_8004BFF8(work->field_94A, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    func_actor_400500_8013DBCC(arg0, 0xB, &work->field_9A0);
}

s32 func_actor_400500_80132D74(Task* arg0)
{
    Actor400500Work* work;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A1C != 5) {
        if ((work->field_A16 < (0x500 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x2E0) >= 0xA41U)) {
            if ((((u16)work->field_A32 >> 3) == 0) && !(work->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 1);
                return 1;
            }
            return 0;
        }
        if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) &&
            ((u32)(work->field_A36 - 0x300) >= 0xA01U) &&
            (work->field_A32 == 0)) {
            if (!(((Actor400500Work*)arg0->work)->field_A1E & 1)) {
                func_actor_400500_8013DB64(arg0, 2);
            } else {
                func_actor_400500_8013DB64(arg0, 3);
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

void func_actor_400500_80132E94(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TmdObject*       extra;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    extra = arg0->extra.tmd;
    if (work->field_A46 < 0) {
        if (!((u8)work->field_A46 & 1)) {
            switch (work->field_A47) {
                case 0:
                    work->field_A20 = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 2);
                    if (work->field_A20 >= 0xF8) {
                        work->field_A20 = 0xFF;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    func_8009EA50(work->field_A20);
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2C < work->field_A2A) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A24 = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 2);
                    work->field_A28 = (u16)work->field_A28 + (-work->field_A28 >> 2);
                    if (work->field_A24 == 0) {
                        enemy->node.state.b.flags = 1;
                        if ((u8)work->field_A4C == 0) {
                            enemy->node.state.b.flags = 5;
                        }
                        work->field_A28 = 0;
                        work->field_A46 = 0;
                        extra->flags   |= 0x80;
                    }
                    extra->lightLevel = work->field_A24;
                    break;
            }
        } else {
            switch (work->field_A47) {
                case 0:
                    enemy->node.state.b.flags = 0;
                    if ((u8)work->field_A4C == 0) {
                        enemy->node.state.b.flags = 4;
                    }
                    extra->flags   &= ~0x80;
                    work->field_A24 = (u16)work->field_A24 + ((s16)(0x1000 - (u16)work->field_A24) >> 2);
                    work->field_A28 = (u16)work->field_A28 + ((0xFF - work->field_A28) >> 2);
                    if (work->field_A24 >= 0xFF0) {
                        work->field_A28 = 0xFF;
                        work->field_A24 = 0x1000;
                        work->field_A2A = 0;
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    extra->lightLevel = work->field_A24;
                    break;
                case 1:
                    work->field_A2A = (u16)work->field_A2A + 1;
                    if (work->field_A2A >= 0x11) {
                        work->field_A47 = (u8)work->field_A47 + 1;
                    }
                    break;
                case 2:
                    work->field_A20 = (u16)work->field_A20 + ((s16) - (u16)work->field_A20 >> 2);
                    if (work->field_A20 < 9) {
                        work->field_A20 = 0;
                        work->field_A46 = 0;
                        func_actor_400500_8013B4A4(arg0);
                        if (work->field_A30 == 0) {
                            work->field_A30 = (u16)work->field_A2E;
                        }
                    }
                    func_8009EA50(work->field_A20);
                    break;
            }
        }
    }
    if (work->field_A30 > 0) {
        work->field_A30 = (u16)work->field_A30 - 1;
    }
}

s32 func_actor_400500_80133160(Task* arg0)
{
    Actor400500Work* work;
    s32              soundId;
    s32              pan;
    u16              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A38 == 1) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x17, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
            pan     = Gp_GetObjPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        heading         = (u16)work->field_94A - D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    if (work->field_A38 == 2) {
        if (work->field_A3A == 0) {
            func_actor_400500_8013DCBC(arg0, 0x18, 0x10);
            work->field_A04 = 0;
            work->field_A3A = 1;
        }
        work->field_A04 = work->field_A04 + 1;
        if ((work->field_A04 & 0xF) == 8) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
            pan     = Gp_GetObjPan(arg0->extra.tmd->coords);
            pan   <<= 24;
            pan   >>= 24;
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        heading         = (u16)work->field_94A + D_actor_400500_80153DB4[work->field_A04 & 0xF];
        work->field_94A = heading;
        if ((func_actor_400500_8013DDEC(arg0) << 0x10) != 0) {
            work->field_A04 = 0;
            work->field_A38 = 0;
            work->field_94A = (u16)work->field_94A & 0xE00;
        }
        func_actor_400500_8013DF50(arg0);
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133358(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;
    s32                 ret;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        ret = 0;
        sub = work->field_A3E;
        if (sub == mode) {
            goto zero_both;
        }
        if ((sub == 2) || (sub == 4)) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A0E = 4;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xA;
            work2->field_9FA = 1;
            work->field_A3E  = 0;
            goto check_hit;
        }
        if (sub != 3) {
            goto check_hit;
        }
        func_actor_400500_8013DB64(arg0, 0xC);
        ret = 1;
    zero_both:
        work->field_A3C = 0;
        work->field_A3E = 0;
        return ret;
    check_hit:
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

s32 func_actor_400500_80133460(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s16                 mode;
    s16                 sub;
    s32                 flag;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    mode = work->field_A3C;
    if (mode == 1) {
        sub = work->field_A3E;
        if (sub == mode) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x1C;
            work2->field_9FE = 0xB;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 2) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 4) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xC;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        } else if (sub == 3) {
            if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != mode)) {
                flag            = 0x81;
                work->field_A46 = flag;
                work->field_A47 = 0;
            }
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0xE;
            work2->field_9FA = 2;
            work->field_A3E  = 0;
        }
        hit = (Actor400500HitView*)arg0->work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A3C = 0;
        }
        return 1;
    }
    return 0;
}

void func_actor_400500_801335E8(Task* arg0)
{
    MATRIX              local;
    MATRIX              local2;
    MATRIX              world;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    Actor400500ViewPos* posA;
    Actor400500ViewPos* posB;
    Actor400500ViewPos* posC;
    Actor400500ViewPos* posD;
    Actor400500ViewPos* saveA;
    Actor400500ViewPos* saveB;
    Actor400500ViewPos* saveC;
    Actor400500ViewPos* saveD;
    GpCoord*            root;
    GpCoord*            coords;
    GpCoord*            coord;
    GpCoord*            soundCoords;
    GpCoord*            soundCoords2;
    s32                 i;
    s32                 cond;
    s32                 z;
    s32                 soundId;
    s32                 pan;
    s32                 pan2;
    s32                 sc0;
    s32                 sc1;
    s32                 cur;
    s32                 q0;
    s32                 q1;
    s32                 q2;
    s32                 dx;
    s32                 dz;
    s32                 posZ;
    s32                 dx2;
    s32                 dz2;
    s32                 posZ2;

    work = (Actor400500Work*)arg0->work;
    root = arg0->extra.tmd->coords;
    if (work->field_9FE != 2) {
        work->field_9F8 = 0x18;
        work->field_9FE = 2;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xB00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xC00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1500 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[0xB].workm, &local);
        posA                 = saveA;
        posA->x              = local.t[0];
        posA->z              = local.t[2];
        soundCoords[0xB].flg = 0;
        soundId              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
        pan                  = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords2[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[8].workm, &local);
        posB                = saveB;
        posB->x             = local.t[0];
        posB->z             = local.t[2];
        soundCoords2[8].flg = 0;
        soundId             = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
        pan2                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_80133B14(Task* arg0)
{
    MATRIX              local;
    MATRIX              local2;
    MATRIX              world;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    Actor400500ViewPos* posA;
    Actor400500ViewPos* posB;
    Actor400500ViewPos* posC;
    Actor400500ViewPos* posD;
    Actor400500ViewPos* saveA;
    Actor400500ViewPos* saveB;
    Actor400500ViewPos* saveC;
    Actor400500ViewPos* saveD;
    GpCoord*            root;
    GpCoord*            coords;
    GpCoord*            coord;
    GpCoord*            soundCoords;
    GpCoord*            soundCoords2;
    s32                 i;
    s32                 cond;
    s32                 z;
    s32                 soundId;
    s32                 pan;
    s32                 soundId2;
    s32                 pan2;
    s32                 sc0;
    s32                 sc1;
    s32                 cur;
    s32                 q0;
    s32                 q1;
    s32                 q2;
    s32                 dx;
    s32                 dz;
    s32                 posZ;
    s32                 dx2;
    s32                 dz2;
    s32                 posZ2;

    work = (Actor400500Work*)arg0->work;
    root = arg0->extra.tmd->coords;
    if (work->field_9FE != 2) {
        work->field_9FE = 2;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        (void)*(volatile u16*)&work->field_9F8;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xB00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xC00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1500 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[0xB].workm, &local);
        posA                 = saveA;
        posA->x              = local.t[0];
        posA->z              = local.t[2];
        soundCoords[0xB].flg = 0;
        soundId              = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
        pan                  = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords2[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[8].workm, &local);
        posB                = saveB;
        posB->x             = local.t[0];
        posB->z             = local.t[2];
        soundCoords2[8].flg = 0;
        soundId2            = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
        pan2                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_8013403C(Task* arg0)
{
    MATRIX              local;
    MATRIX              local2;
    MATRIX              world;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    Actor400500ViewPos* posA;
    Actor400500ViewPos* posB;
    Actor400500ViewPos* posC;
    Actor400500ViewPos* posD;
    Actor400500ViewPos* saveA;
    Actor400500ViewPos* saveB;
    Actor400500ViewPos* saveC;
    Actor400500ViewPos* saveD;
    GpCoord*            root;
    GpCoord*            coords;
    GpCoord*            coord;
    GpCoord*            soundCoords;
    GpCoord*            soundCoords2;
    s32                 i;
    s32                 cond;
    s32                 z;
    s32                 soundId;
    s32                 pan;
    s32                 pan2;
    s32                 sc0;
    s32                 sc1;
    s32                 cur;
    s32                 q0;
    s32                 q1;
    s32                 q2;
    s32                 dx;
    s32                 dz;
    s32                 posZ;
    s32                 dx2;
    s32                 dz2;
    s32                 posZ2;

    work = (Actor400500Work*)arg0->work;
    root = arg0->extra.tmd->coords;
    if (work->field_9FE != 4) {
        work->field_9F8 = 0x10;
        work->field_9FE = 4;
        work->field_9FA = 2;
        work2           = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }

    sc0 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc0 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xD00 / sc0) >> 4;
    }
    q0 = cur;

    sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    if (sc1 == 0) {
        cur = 0;
    } else {
        cur = (u32)(0xE00 / sc1) >> 4;
        COMPILER_BARRIER();
        sc1 = ((Actor400500Work*)arg0->work)->field_9F8;
    }
    q1 = cur;

    if (sc1 == 0) {
        sc0 = 0;
    } else {
        sc0 = (u32)(0x1B00 / sc1) >> 4;
    }
    q2 = sc0;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A00 = 0;
    }

    SOFT_MOVE_ZERO(z);
    if (work->field_A00 == z) {
        saveA       = &work->field_9A0;
        soundCoords = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords[8].workm, &local);
        posA               = saveA;
        posA->x            = local.t[0];
        posA->z            = local.t[2];
        soundCoords[8].flg = 0;
        soundId            = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
        pan                = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_A00 == (s32)(q1 & 0xFF)) {
        saveB        = &work->field_9A0;
        soundCoords2 = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&soundCoords2[0xB]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &soundCoords2[0xB].workm, &local);
        posB                  = saveB;
        posB->x               = local.t[0];
        posB->z               = local.t[2];
        soundCoords2[0xB].flg = 0;
        soundId               = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
        pan2                  = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((work->field_A00 >= z) && (work->field_A00 <= (s32)(q0 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[8];
        Gp_UpdateCoord(coord);
        saveC = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local2);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
        dx                 = world.t[0] - local2.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        posC               = saveC;
        posZ               = posC->z;
        dz                 = world.t[2] - local2.t[2];
        coords->flg        = 0;
        coords[8].flg      = 0;
        coords->coord.t[2] = posZ - dz;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    if ((work->field_A00 >= (s32)(q1 & 0xFF)) && (work->field_A00 <= (s32)(q2 & 0xFF))) {
        coords = arg0->extra.tmd->coords;
        coord  = &coords[0xB];
        Gp_UpdateCoord(coord);
        saveD = &work->field_9A0;
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local2);
        dx2                = local2.t[0] - local.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx2;
        posD               = saveD;
        posZ2              = posD->z;
        dz2                = local2.t[2] - local.t[2];
        coords->flg        = 0;
        coords[0xB].flg    = 0;
        coords->coord.t[2] = posZ2 - dz2;
        Gp_UpdateCoord(coord);
        Gp_UpdateCoord(coords);
    }
    root->flg = 0;
}

void func_actor_400500_8013456C(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    u32              dmg;
    u32              amount;
    s16              amount16;
    s16              hp;
    u8               flags;
    s32              tmp;
    s16              tick;
    s32              i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    for (i = 0; i < 3; i++) {
        if ((work->rec0[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_A44 == 0) {
                work->field_A3C = 1;
                dmg             = Gp_ComputeDamage(work->rec0[i].key, work->field_A16, 0, 0);
                amount          = dmg;
                work->field_A44 = Gp_GetIdParam2(work->rec0[i].key);
                if (Gp_RollEnemyChance(enemy, work->rec0[i].key, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                amount16 = amount;
                func_800E2C78(enemy, work->rec0[i].key, amount16, 0);
                func_800DA6E8(&enemy->node, amount16, 0);
                hp        = (u16)enemy->hp - amount;
                enemy->hp = hp;
                if ((hp << 16) <= 0) {
                    enemy->hp       = 0;
                    work->field_A42 = 1;
                }
                func_800FDB18(
                    Gp_GetIdParam1(work->rec0[i].key) & 0xFFFF,
                    &arg0->extra.tmd->coords[3],
                    NULL,
                    &work->eff_940);
                if (amount16 >= 0x32) {
                    work->field_A3E = 2;
                    amount          = dmg;
                    work->field_A40 = 2;
                } else {
                    work->field_A3E = 1;
                    work->field_A40 = 1;
                }
            } else if ((Gp_GetIdParam1(work->rec0[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_940);
            }
            switch (Gp_GetIdParam0(work->rec0[i].key) & 0xFFFF) {
                case 0:
                    break;
                case 1:
                    Gp_SetObjFlag1(enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, work->rec0[i].key, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4(enemy, work->rec0[i].key, 0);
                    break;
                case 4:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 5:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 6:
                    work->field_A3E = 4;
                    work->field_A40 = 4;
                    break;
                case 7:
                    work->field_A3E = 2;
                    work->field_A40 = 2;
                    break;
                case 8:
                case 9:
                    work->field_A4A = 1;
                    break;
            }
        }
    }

    flags = enemy->reactionFlags;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
        work->field_A3E      = 2;
        work->field_A40      = 2;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_A3E       = 3;
        work->field_A40       = 3;
    }
    if (enemy->reactionFlags & 0xC) {
        tmp  = Gp_TickObjFlag4(enemy);
        tick = tmp;
        if (tick != 0) {
            enemy->hp = (u16)enemy->hp - tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_A3C = 1;
            work->field_A3E = 2;
            work->field_A40 = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
    Gp_ClearRec18Occupied(work->rec0);
    if (work->field_A44 > 0) {
        work->field_A44 = (u16)work->field_A44 - 1;
        return;
    }
    work->field_A44 = 0;
}

void func_actor_400500_801348D8(Task* arg0, s32 arg1)
{
    SVECTOR          pos;
    GpCoord*         coords;
    GpCoord*         joint;
    GpCoord*         player;
    Task*            slot;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              i;
    s32              cur;
    s32              sample;

    coords = arg0->extra.tmd->coords;
    slot   = *Gp_ActorSlots;
    joint  = coords + 8;
    work   = (Actor400500Work*)arg0->work;
    if (slot != NULL) {
        player = slot->extra.tmd->coords;
        work2  = work;
        if (work->field_9FA == 1) {
            if ((s16)work->field_9FC != work->field_9FE) {
                work->field_A00 = 0;
            } else {
                work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work->field_9FA = 3;
            work->field_A00 = 0;
        } else if (work->field_9FA == 3) {
            work->field_A00 = (u16)work->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        joint->flg = 0;
        Gp_UpdateCoord(joint);
        pos.vx = 0x160;
        pos.vy = 0x148;
        pos.vz = 0x2C0;
        func_actor_400500_8013B920(joint, &pos);
        if ((arg1 << 0x10) == 0) {
            player->coord.t[0] = pos.vx;
            player->coord.t[2] = pos.vz;
        } else {
            sample             = pos.vx;
            cur                = player->coord.t[0];
            cur               += (sample - cur) >> 2;
            player->coord.t[0] = cur;
            sample             = pos.vz;
            cur                = player->coord.t[2];
            cur               += (sample - cur) >> 2;
            player->coord.t[2] = cur;
        }
        player->flg = 0;
        Gp_UpdateCoord(player);
        work->field_9F8 = -0x10;
        work3           = (Actor400500Work*)arg0->work;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i = 1;
        do {
            work3->slots[i].rate = (u8)work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
        } while (i < 0x12);
        work->field_9F8 = 0x10;
    }
}

void func_actor_400500_80134B88(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;

    D_800678F0[0] = &D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[3], 0x200, NULL);
    if (eff != NULL) {
        src        = arg0->extra.tmd;
        dst        = eff->task->extra.tmd;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = &D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = arg0->extra.tmd;
        dst2        = eff2->task->extra.tmd;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = arg0->extra.tmd;
        dst3        = eff3->task->extra.tmd;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[3], 0x200, NULL);
}

/// Queues at depth `otz` a run of primitives that, executed in reverse of
/// the order they are added, point drawing at the 320x240 area at VRAM
/// (0x1C0, 0x100), fill it with a near-black tile with mask-bit setting on,
/// draw two 160x240 raw-texture sprites copied from the current draw buffer
/// over it, and then restore the draw offset, mask setting and draw area for
/// the current buffer. The restored area is the view's sprite rectangle when
/// one is active and nearer than `otz`, full screen otherwise. The 0x14-byte
/// block holding the rectangle and offset is carved off the scratch head and
/// released before returning.
void func_actor_400500_80134D6C(s32 otz)
{
    u8*                head;
    u8*                allocated;
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;
    s32                val;
    s32                z;

    extra              = Gp_GetViewSprtExtra();
    head               = SCRATCH_HEAD(u8);
    area               = (DR_AREA*)gGpuPrimCursor;
    allocated          = head - 0x14;
    SCRATCH_HEAD(void) = allocated;
    gGpuPrimCursor     = (DR_TPAGE*)(area + 1);
    USE_REG(allocated);
    scratch      = (ActorsDrawScratch*)allocated;
    scratch->otz = otz;
    if (extra != NULL) {
        val = (extra->depth << gDisplayState.otDepthShift) & 0x3FFF;
        z   = otz;
        SOFT_TOUCH_REG(z);
        if ((val >> 4) < z) {
            scratch->rect   = extra->rect;
            scratch->rect.y = (u16)scratch->rect.y + gDisplayState.drawBuffer * 0x110;
        } else {
            goto block_4;
        }
    } else {
    block_4:
        scratch->rect.x = 0;
        scratch->rect.y = D_80070F87[0] * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_POP_BYTES(0x14);
}

void func_actor_400500_80135414(Task* arg0)
{
    TmdObject*       extra;
    GpEnemy*         enemy;
    GpCoord*         coord;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    Actor400500Work* work5;
    Actor400500Work* work6;
    Actor400500Work* work7;
    GpCoord*         player;
    GpCoord*         coord2;
    TmdObject*       extra2;
    s32              i;
    s32              flag;
    s32              val;
    u8               mode;

    extra      = arg0->extra.tmd;
    enemy      = arg0->spawnArg2;
    coord      = extra->coords;
    arg0->work = memCalloc(0xA50, 0);
    work       = (Actor400500Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    extra->lightMtx   = &work->lightMtx;
    extra->colorMtx   = &work->colorMtx;
    extra->flags      = 0;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->recs               = (s32)work->rec0;
    enemy->param              = &D_actor_400500_80153C90;
    enemy->hp = enemy->hpMax = D_actor_400500_80153C90.hpMax;
    func_800B3F84(&work->anim, D_actor_400500_80153CC0, extra, work->pad_2E4,
                  work->slots);
    coord->sub       = &gGfxViewCoord;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x18;
    work2->field_9FE = 2;
    work2->field_9FA = 2;
    work3            = (Actor400500Work*)arg0->work;
    if (work3->field_9FA == 1) {
        if ((s16)work3->field_9FC != work3->field_9FE) {
            work3->field_A00 = 0;
        } else {
            work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work3->field_9FA = 3;
    } else if (work3->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work3->field_9FA = 3;
        work3->field_A00 = 0;
    } else if (work3->field_9FA == 3) {
        work3->field_A00 = (u16)work3->field_A00 + 1;
    }
    i = 1;
    do {
        work3->slots[i].rate = (u8)work3->field_9F8;
        Gp_AnimTickIndex(&work3->anim, i);
        i++;
    } while (i < 0x12);
    arg0->msgTable = D_actor_400500_80153CA0;
    func_actor_400500_80132C54(arg0);
    work4 = (Actor400500Work*)arg0->work;
    if (Gp_ActorSlots[0] != NULL) {
        player              = Gp_ActorSlots[0]->extra.tmd->coords;
        work4->field_9D0.vx = (u16)player->coord.t[0];
        work4->field_9D0.vy = (u16)player->coord.t[1];
        work4->field_9D0.vz = (u16)player->coord.t[2];
    }
    work5  = (Actor400500Work*)arg0->work;
    mode   = gGameSession->at4.loc.room;
    extra2 = arg0->extra.tmd;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        val              = 0xFF;
        work5->field_A20 = val;
        val              = 0x10;
        work5->field_A24 = 0;
        work5->field_A28 = 0;
    } else {
        val              = 0x1000;
        work5->field_A24 = val;
        val              = 0xFF;
        work5->field_A28 = val;
        val              = 0x2000;
        work5->field_A20 = 0;
    }
    work5->field_A2C = val;
    SOFT_BARRIER();
    func_8009EA50(work5->field_A20);
    extra2->lightLevel = work5->field_A24;
    func_actor_400500_80132000(arg0);
    func_actor_400500_8013226C(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    coord2                   = arg0->extra.tmd->coords;
    work->eff_940.spawnArgLo = 0x100;
    work->eff_940.spawnArgHi = 3;
    work->eff_940.coord      = &coord2[3];
    work6                    = (Actor400500Work*)arg0->work;
    work6->field_A06         = 6;
    work6->field_A08         = 0;
    work7                    = (Actor400500Work*)arg0->work;
    gStageSceneMusicEntry    = 2;
    if (((work7->field_A46 >= 0) || ((u8)work7->field_A46 & 0x7F)) && (work7->field_A30 == 0)) {
        flag             = 0x80;
        work7->field_A46 = flag;
        work7->field_A47 = 0;
    }
    arg0->state = arg0->state + 1;
}

/// Handlers the task entry `func_actor_400500_8013DE98` runs by task state:
/// set-up, the per-frame state machine run by `field_A06`, a second one run by
/// `field_A06` from task state 2, and the teardown that kills the child tasks
/// and destroys the enemy 0x12D frames later.
const TaskFuncTable4 D_actor_400500_80131E4C = { {
    func_actor_400500_80135414,
    func_actor_400500_80135770,
    func_actor_400500_8013A700,
    func_actor_400500_8013DEFC,
} };

static __inline__ s32 lookup_zone(Task* task)
{
    ActorZone* zone;
    u16        id_u;
    s16        zone_id;
    GpCoord*   root;
    u16        px_u, pz_u;
    s16        px, pz;

    zone    = D_actor_400500_80153D6C;
    id_u    = (u16)zone->id;
    root    = task->extra.tmd->coords;
    zone_id = zone->id;
    px_u    = (u16)root->coord.t[0];
    pz_u    = (u16)root->coord.t[2];
    if (zone_id != -1) {
        px = (s16)px_u;
        pz = (s16)pz_u;
        do {
            if ((px >= zone->x) && ((zone->x + zone->w) >= px) &&
                (pz >= zone->z) && ((zone->z + zone->h) >= pz)) {
                return (s16)id_u;
            }
            zone++;
            id_u = (u16)zone->id;
        } while (zone->id != -1);
    }
    return 0;
}

static __inline__ VECTOR* push_color(GpCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_HEAD(u8) - 0x10);

    ((VECTOR*)(SCRATCH_HEAD(u8) - 0x10))->vx = coord->workm.t[0];
    block->vy                                = coord->workm.t[1];
    block->vz                                = coord->workm.t[2];
    SCRATCH_HEAD(VECTOR)                     = block;
    return block;
}

static __inline__ void pop_scratch(s32 n)
{
    SCRATCH_POP_BYTES(n);
}

static __inline__ u8* push_proj(void)
{
    u8*                  head  = SCRATCH_HEAD(u8);
    ActorProjectScratch* block = (ActorProjectScratch*)(head - 0x18);

    SCRATCH_HEAD(ActorProjectScratch)             = block;
    ((ActorProjectScratch*)(head - 0x18))->vec.vx = 0;
    block->vec.vy                                 = 0;
    block->vec.vz                                 = 0;
    return head;
}

const Actor400500TaskFuncTable13 D_actor_400500_80131E5C = { {
    func_actor_400500_80135EBC,
    func_actor_400500_8013BA24,
    func_actor_400500_801385D0,
    func_actor_400500_8013899C,
    func_actor_400500_80138EA0,
    func_actor_400500_8013905C,
    func_actor_400500_801392D8,
    func_actor_400500_801395D0,
    func_actor_400500_80139C1C,
    func_actor_400500_80139F6C,
    func_actor_400500_8013AD60,
    func_actor_400500_8013B5E0,
    func_actor_400500_8013A484,
} };

void func_actor_400500_80135770(Task* arg0)
{
    Actor400500Work*           work;
    GpEnemy*                   enemy;
    TmdObject*                 extra0;
    TmdObject*                 obj;
    Task*                      slot;
    GpCoord*                   part2;
    PlayerStatus*              cfg;
    GpAnimArg                  msg;
    Actor400500TaskFuncTable13 sp;
    OverlayMat                 rot;
    s8                         handshake;
    Actor400500Work*           work_pos;
    Actor400500Work*           work_dead;
    Actor400500Work*           work_rot;
    OverlayMat*                src;
    s32                        one;
    s16                        ang;
    s16                        ang_z;
    s16                        ang_y;
    GpCoord*                   rot_root;
    GpCoord*                   player;
    TmdObject*                 extra;
    TmdObject*                 extra2;
    TmdObject*                 trans_obj;
    VECTOR*                    color;
    u8*                        head;
    GpCoord*                   color_part;
    u8                         mode;
    s16                        trans;
    s16                        trans_y;
    ActorProjectScratch*       proj;
    SVECTOR*                   vecp;
    MATRIX*                    workm;

    cfg    = &Player_Status;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    extra0 = arg0->extra.tmd;
    part2  = extra0->coords + 2;
    obj    = extra0;
    slot   = *Gp_ActorSlots;
    sp     = D_actor_400500_80131E5C;

    handshake = work->field_A48;
    switch (handshake) {
        case 1:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                work->field_A48 = 2;
            }
            break;
        case 2:
            msg.animBlock.ptr = &D_actor_400500_80153CB0;
            msg.field_8       = 0;
            msg.field_C       = 0;
            msg.field_10      = 0;
            if (work->field_A4D != 0) {
                msg.field_4     = 3;
                work->field_A48 = 4;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&msg, 0);
            } else {
                msg.field_4     = handshake;
                work->field_A48 = 3;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
            }
            break;
        case 3:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                work->field_A48 = 0;
            }
            break;
    }

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            func_actor_400500_80132438(arg0);
            work->field_A1A = lookup_zone(arg0);
            if (slot == NULL) {
                work->field_A1C = 0;
            } else {
                work->field_A1C = lookup_zone((Task*)slot);
            }
            sp.funcs[(s16)work->field_A06](arg0);
            work_pos = (Actor400500Work*)arg0->work;
            if (*Gp_ActorSlots != NULL) {
                player                 = (*Gp_ActorSlots)->extra.tmd->coords;
                work_pos->field_9D0.vx = (u16)player->coord.t[0];
                work_pos->field_9D0.vy = (u16)player->coord.t[1];
                work_pos->field_9D0.vz = (u16)player->coord.t[2];
            }
            src                 = &rot;
            work_rot            = (Actor400500Work*)arg0->work;
            rot_root            = arg0->extra.tmd->coords;
            ang                 = work_rot->field_948;
            ang_y               = work_rot->field_94A;
            work_rot->field_948 = ang & 0xFFF;
            ang_z               = work_rot->field_94C;
            work_rot->field_94A = ang_y & 0xFFF;
            work_rot->field_94C = ang_z & 0xFFF;
            one                 = 0x1000;
            rot.ident.m00_m01   = one;
            rot.ident.m02_m10   = 0;
            src->ident.m11_m12  = one;
            rot.ident.m20_m21   = 0;
            src->ident.m22      = one;
            RotMatrixZ(work_rot->field_94C, &src->mat);
            RotMatrixX(work_rot->field_948, &src->mat);
            func_8004BFF8(work_rot->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &rot_root->coord);
            func_actor_400500_80132E94(arg0);
            if (work->field_A32 > 0) {
                work->field_A32 = (u16)work->field_A32 - 1;
                work->field_A34 = 1;
            } else {
                work->field_A34 = 0;
            }
            obj->flags &= 0xFF7F;
            if ((enemy->hp > 0) || (cfg->hp <= 0)) {
                func_actor_400500_8013456C(arg0);
            } else if ((work->field_A42 == 0) && (work->field_A48 == 0)) {
                work_dead            = (Actor400500Work*)arg0->work;
                arg0->state          = 2;
                work_dead->field_A06 = 0;
                work_dead->field_A08 = 0;
            }
        case 1:
            extra      = arg0->extra.tmd;
            extra2     = extra;
            color_part = extra->coords + 1;
            color      = push_color(color_part);
            Gp_UpdateActorColor(arg0->spawnArg2, color, 0, 0);
            mode = gGameSession->at4.loc.room;
            if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
                trans_obj = extra2;
                trans     = 0x200;
                trans_y   = trans;
            } else {
                trans_obj = extra;
                trans     = 0x400;
                trans_y   = 0x1000;
            }
            Gp_SetObjTrans(trans_obj, trans, trans_y, trans);
            pop_scratch(0x10);
            if (gGameSession->field_65 != 0) {
                func_actor_400500_80132AB0(arg0, -0xFA0, (u8)work->field_A28);
                return;
            }
            func_actor_400500_80132AB0(arg0, -0xFA0, ((u16)work->field_A28 >> 2) & 0xFF);
            func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            head = push_proj();
            proj = (ActorProjectScratch*)(head - 0x18);
            Gp_UpdateCoord(part2);
            vecp  = &proj->vec;
            workm = &part2->workm;
            gte_SetRotMatrix(workm);
            gte_SetTransMatrix(workm);
            gte_ldv0(vecp);
            gte_rtps();
            gte_stsxy(&((ActorProjectScratch*)(head - 0x18))->sxy);
            gte_stdp(&((ActorProjectScratch*)(head - 0x18))->dp);
            gte_stflg(&((ActorProjectScratch*)(head - 0x18))->flag);
            gte_stszotz(&((ActorProjectScratch*)(head - 0x18))->otz);
            if (proj->flag < 0) {
                proj->otz = 0;
            }
            proj->otz = (proj->otz >> 4) + 0x1E;
            func_actor_400500_80134D6C(proj->otz);
            pop_scratch(0x18);
            return;
    }
}

/// Sub-state handlers `func_actor_400500_80135EBC` copies onto the stack and runs
/// by `field_A08` every frame.
const TaskFuncTable11 D_actor_400500_80131E90 = { {
    func_actor_400500_801361EC,
    func_actor_400500_8013662C,
    func_actor_400500_80136864,
    func_actor_400500_801369A4,
    func_actor_400500_80136B94,
    func_actor_400500_80136D00,
    func_actor_400500_80136EB8,
    func_actor_400500_80137034,
    func_actor_400500_801371A0,
    func_actor_400500_80137338,
    func_actor_400500_80137478,
} };

/// Handlers `func_actor_400500_80135EBC` also runs, by `field_A0A`, while the
/// enemy is out of hit points.
const TaskFuncTable3 D_actor_400500_80131EBC = { {
    func_actor_400500_8013BAA4,
    func_actor_400500_8013BB18,
    func_actor_400500_8013BBB0,
} };

void func_actor_400500_80135EBC(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TaskFuncTable11  sp10;
    TaskFuncTable3   sp40;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              flag;
    Actor400500Work* work4;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp10  = D_actor_400500_80131E90;
    sp40  = D_actor_400500_80131EBC;
    if (enemy->hp <= 0) {
        sp40.funcs[(s16)work->field_A0A](arg0);
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        work3 = (Actor400500Work*)arg0->work;
        if ((work3->field_A46 >= 0) || (((u8)work3->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        return;
    }
    if ((s16)work->field_A08 != 0) {
        work4 = (Actor400500Work*)arg0->work;
        if (work4->field_9FA == 1) {
            if ((s16)work4->field_9FC != work4->field_9FE) {
                work4->field_A00 = 0;
            } else {
                work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work4->field_9FA = 3;
        } else if (work4->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work4->field_9FA = 3;
            work4->field_A00 = 0;
        } else if (work4->field_9FA == 3) {
            work4->field_A00 = (u16)work4->field_A00 + 1;
        }
        i = 1;
        do {
            work4->slots[i].rate = (u8)work4->field_9F8;
            Gp_AnimTickIndex(&work4->anim, i);
            i++;
        } while (i < 0x12);
    }
    sp10.funcs[(s16)work->field_A08](arg0);
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}

void func_actor_400500_801361EC(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local;
    OverlayMat*         src;
    MATRIX*             dst;
    Actor400500Work*    work;
    Actor400500Work*    workA;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500Work*    work4;
    GpCoord*            coord;
    GpCoord*            coords;
    GpCoord*            coords2;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    Actor400500ViewPos* pos3;
    Actor400500ViewPos* pos4;
    s32                 flag;
    s32                 flag2;
    s32                 heading;
    s32                 i;
    s32                 tx;
    s32                 a1c;

    work    = (Actor400500Work*)arg0->work;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A49 != 0) {
            workA->field_A49 = 0;
            if (workA->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0)) {
            work3            = (Actor400500Work*)arg0->work;
            work3->field_A38 = 0;
            work3->field_A3A = 0;
            work4            = (Actor400500Work*)arg0->work;
            work4->field_9F8 = 0x18;
            work4->field_9FE = 2;
            work4->field_9FA = 2;
            work2            = (Actor400500Work*)arg0->work;
            if (work2->field_9FA == 1) {
                if ((s16)work2->field_9FC != work2->field_9FE) {
                    work2->field_A00 = 0;
                } else {
                    work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
                }
                func_actor_400500_8013DCD4(arg0);
                work2->field_9FA = 3;
            } else if (work2->field_9FA == 2) {
                func_actor_400500_8013DC4C(arg0);
                work2->field_9FA = 3;
                work2->field_A00 = 0;
            } else if (work2->field_9FA == 3) {
                work2->field_A00 = (u16)work2->field_A00 + 1;
            }
            i = 1;
            do {
                work2->slots[i].rate = (u8)work2->field_9F8;
                Gp_AnimTickIndex(&work2->anim, i);
                i++;
            } while (i < 0x12);
            switch ((s16)((u16)work->field_A1A - 1)) {
                case 3:
                    if (heading != 0x400) {
                        goto case3_ne;
                    }
                    work->field_A08 = 1;
                    break;
                case3_ne:
                    work->field_A08 = 4;
                    break;
                case 0:
                    if (work->field_9E0 >= 0) {
                        goto case0_ge;
                    }
                    work->field_A08 = 3;
                    break;
                case0_ge:
                    work->field_A08 = 1;
                    break;
                case 1:
                    a1c = (s16)work->field_A1C;
                    if ((a1c == 1) || (a1c == 4) || (a1c == 5)) {
                        SOFT_BARRIER();
                        work->field_A08 = 3;
                    } else if ((a1c == 3) && (heading == 0) && (coord->coord.t[0] >= 0x4074)) {
                        work->field_A08 = 6;
                    } else if ((s16)work->field_A1C == 2) {
                        if (coord->coord.t[2] < -0x209D) {
                            goto a1c2_lt;
                        }
                        work->field_A08 = 6;
                        break;
                    a1c2_lt:
                        work->field_A08 = 1;
                        break;
                    } else {
                        work->field_A08 = 1;
                    }
                    break;
                case 2:
                    if (work->field_9E4 >= 0) {
                        goto case2_ge;
                    }
                    work->field_A08 = 8;
                    break;
                case2_ge:
                    work->field_A08 = 6;
                    break;
                case 5:
                    if (heading != 0x800) {
                        goto case5_ne;
                    }
                    work->field_A08 = 8;
                    break;
                case5_ne:
                    work->field_A08 = 7;
                    break;
                default:
                    tx                 = -0x3E8;
                    coord->coord.t[0]  = tx;
                    tx                 = -0xFA0;
                    coord->coord.t[1]  = tx;
                    tx                 = -0x2116;
                    coord->coord.t[2]  = tx;
                    tx                 = 0x400;
                    work->field_94A    = tx;
                    tx                 = 0x800;
                    work->field_94C    = tx;
                    tx                 = 0x1000;
                    src                = &rot;
                    work->field_948    = 0;
                    work->field_A1E    = 0;
                    rot.ident.m00_m01  = tx;
                    src->ident.m02_m10 = 0;
                    src->ident.m11_m12 = tx;
                    src->ident.m20_m21 = 0;
                    src->ident.m22     = tx;
                    RotMatrixZ(work->field_94C, &src->mat);
                    func_8004BFF8(work->field_94A, &src->mat);
                    dst          = &coord->coord;
                    dst->m[0][0] = src->mat.m[0][0];
                    dst->m[0][1] = src->mat.m[0][1];
                    dst->m[0][2] = src->mat.m[0][2];
                    dst->m[1][0] = src->mat.m[1][0];
                    dst->m[1][1] = src->mat.m[1][1];
                    dst->m[1][2] = src->mat.m[1][2];
                    dst->m[2][0] = src->mat.m[2][0];
                    dst->m[2][1] = src->mat.m[2][1];
                    dst->m[2][2] = src->mat.m[2][2];
                    pos2         = &work->field_9A0;
                    coords       = arg0->extra.tmd->coords;
                    Gp_UpdateCoord(&coords[11]);
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[11].workm, &local);
                    pos    = pos2;
                    pos->x = local.t[0];
                    pos->z = local.t[2];
                    SOFT_BARRIER();
                    coords[11].flg  = 0;
                    work->field_A08 = 1;
                    break;
            }
            pos4    = &work->field_9A0;
            coords2 = arg0->extra.tmd->coords;
            Gp_UpdateCoord(&coords2[11]);
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords2[11].workm, &local);
            pos3            = pos4;
            pos3->x         = local.t[0];
            pos3->z         = local.t[2];
            coords2[11].flg = 0;
        }
    }
}

void func_actor_400500_8013662C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;
    u16              a1c;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x400) {
                if (((0x400 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                        a1c = work->field_A1C;
                        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                            work->field_A08 = 5;
                        } else {
                            work->field_A08 = a1a;
                        }
                    } else {
                        func_actor_400500_801335E8(arg0);
                    }
                } else {
                    if (work->field_A34 == 0) {
                        if (work->field_9E0 <= 0) {
                            work->field_A08 = 2;
                        }
                    } else if (work->field_9E0 < -0xF9F) {
                        rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 0x10) & 0x1F) == 0) {
                            if (!(work->field_A1E & 1)) {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 7;
                            } else {
                                work2 = (Actor400500Work*)arg0->work;
                                val   = 8;
                            }
                            work2->field_A06 = val;
                            work2->field_A08 = 0;
                        }
                    }
                    func_actor_400500_801335E8(arg0);
                }
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

void func_actor_400500_80136864(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_801369A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;
    s32              flag2;
    s32              heading;
    s32              a1a;
    s32              val;
    u32              rnd;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0xC00) {
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                a1a = work->field_A1A;
                if (a1a != 1) {
                    if (a1a == 4) {
                        work->field_A08 = a1a;
                    }
                } else if (work->field_A34 == 0) {
                    if (work->field_9E0 >= 0) {
                        work->field_A08 = 4;
                    }
                } else if (work->field_9E0 >= 0xFA0) {
                    rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState = rnd;
                    if (((rnd >> 0x10) & 0x1F) == 0) {
                        if (!(work->field_A1E & 1)) {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 7;
                        } else {
                            work2 = (Actor400500Work*)arg0->work;
                            val   = 8;
                        }
                        work2->field_A06 = val;
                        work2->field_A08 = 0;
                    }
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[2] = -0x209E;
        }
    }
}

void func_actor_400500_80136B94(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x400) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 1;
                return;
            }
            if ((s16)work->field_A1C == 4) {
                if (work->field_9E4 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x400 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136D00(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            if (((u32)(work->field_A1C - 2) < 2U) || ((s16)work->field_A1C == 6)) {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 6;
                    return;
                }
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else {
                heading = (u16)work->field_94A;
                if ((heading & 0xFFF) == 0xC00) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_9F8 = 0x18;
                    work2->field_9FE = 2;
                    work2->field_9FA = 2;
                    work->field_A08  = 3;
                    return;
                }
                if (((0xC00 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_80136EB8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if (heading & 0xFFF) {
                if (((0 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                if (work->field_A1A != 3) {
                    if (work->field_A1A == 6) {
                        work->field_A08 = 7;
                    }
                } else if ((work->field_A34 == 0) && (work->field_9E4 < 0)) {
                    work->field_A08 = 7;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137034(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0x800) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 8;
                return;
            }
            if ((s16)work->field_A1C == 6) {
                if (work->field_9E0 > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
            } else if (((0x800 - heading) << 0x14) > 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 2;
            } else {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A38 = 1;
            }
            work2->field_A3A = 0;
        }
    }
}

void func_actor_400500_801371A0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;
    s32              flag2;
    s32              heading;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) != 0x800) {
                if (((0x800 - heading) << 0x14) > 0) {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 2;
                } else {
                    work2            = (Actor400500Work*)arg0->work;
                    work2->field_A38 = 1;
                }
                work2->field_A3A = 0;
            } else {
                switch (work->field_A1A) {
                    case 2:
                        if (coord->coord.t[2] < -0x209E) {
                            work->field_A08 = 9;
                        }
                        break;
                    case 3:
                        if ((work->field_A34 == 0) && (work->field_9E4 > 0)) {
                            work->field_A08 = 0xA;
                        }
                        break;
                }
                func_actor_400500_801335E8(arg0);
            }
            coord->coord.t[0] = 0x4074;
        }
    }
}

void func_actor_400500_80137338(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0xC00) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 3;
                return;
            }
            if (((0xC00 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_80137478(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              flag2;
    s32              heading;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag == 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A49 != 0) {
            work2->field_A49 = 0;
            if (work2->field_A1E & 1) {
                flag2 = 0;
            } else {
                func_actor_400500_8013DB64(arg0, 4);
                flag2 = 1;
            }
        } else {
            flag2 = 0;
        }
        if ((flag2 == 0) && ((func_actor_400500_80132D74(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133358(arg0) << 0x10) == 0) &&
            ((func_actor_400500_80133160(arg0) << 0x10) == 0)) {
            heading = (u16)work->field_94A;
            if ((heading & 0xFFF) == 0) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x18;
                work2->field_9FE = 2;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
            if (((0 - heading) << 0x14) > 0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A38 = 2;
                work3->field_A3A = 0;
            } else {
                work4            = (Actor400500Work*)arg0->work;
                work4->field_A38 = 1;
                work4->field_A3A = 0;
            }
        }
    }
}

void func_actor_400500_801375B8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    s32              i;

    work              = (Actor400500Work*)arg0->work;
    work->obj0.radius = 0x130;
    work2             = (Actor400500Work*)arg0->work;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 5;
    work3->field_9FA = 2;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_9FA == 1) {
        if ((s16)work4->field_9FC != work4->field_9FE) {
            work4->field_A00 = 0;
        } else {
            work4->field_A00 = func_actor_400500_8013DD8C(arg0, work4->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work4->field_9FA = 3;
    } else if (work4->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work4->field_9FA = 3;
        work4->field_A00 = 0;
    } else if (work4->field_9FA == 3) {
        work4->field_A00 = (u16)work4->field_A00 + 1;
    }
    i = 1;
    do {
        work4->slots[i].rate = (u8)work4->field_9F8;
        Gp_AnimTickIndex(&work4->anim, i);
        i++;
    } while (i < 0x12);
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_9BC = 0;
    work->field_A08 = work->field_A08 + 1;
}

void func_actor_400500_8013771C(Task* arg0)
{
    SVECTOR     in;
    SVECTOR     out;
    OverlayMat  rot;
    OverlayMat* src;
    union {
        MATRIX    mat;
        GpAnimArg msg;
    } slot;
    MATRIX              parent;
    MATRIX*             parentp;
    MATRIX*             tmp;
    MATRIX*             mtx;
    MATRIX*             scratch;
    MATRIX*             scratch2;
    MATRIX*             scratch3;
    u8*                 scratchBase;
    MATRIX*             viewWorld;
    TmdObject*          model2;
    GpCoord*            coords2;
    GpCoord*            coords;
    GpCoord*            coord8;
    GpCoord*            playerCoords;
    GpCoord*            walker;
    GpCoord*            viewCoord;
    GpCoord*            viewCoord2;
    GpCoord*            part;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500Work*    work4;
    Actor400500Work*    work5;
    Actor400500HitView* hit;
    GameActor*          player;
    void*               spawn;
    SVECTOR             dvec;
    s16                 vz;
    s32                 dist;
    s32                 delta;
    s32                 ident;
    s32                 one;
    s32                 i;
    s32                 flag;
    s32                 cond;
    s32                 soundId;
    s32                 soundId2;
    s32                 soundId3;
    s32                 pan;
    s32                 pan2;
    s32                 pan3;
    s32                 r;
    u16                 heading;
    u16                 heading2;
    s32                 cur;

    work            = (Actor400500Work*)arg0->work;
    player          = Gp_ActorSlots[0]->work;
    spawn           = arg0->spawnArg2;
    work->field_A04 = work->field_A04 + 1;
    work2           = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    src = &rot;
    if ((s16)work->field_A04 < 0xF) {
        in.vx              = (u16)work->field_9E0;
        in.vy              = 0;
        vz                 = (u16)work->field_9E4;
        ident              = 0x1000;
        rot.ident.m00_m01  = ident;
        rot.ident.m02_m10  = 0;
        in.vz              = vz;
        src->ident.m11_m12 = ident;
        rot.ident.m20_m21  = 0;
        src->ident.m22     = ident;
        func_8004BFF8(work->field_94A, &src->mat);
        ApplyMatrixSV(&src->mat, &in, &out);
        r     = ratan2(out.vx, work->field_9E2 - 0x6A0);
        cur   = (u16)work->field_9BC;
        delta = (-r - cur) << 20;
    } else {
        cur   = (u16)work->field_9BC;
        delta = -(cur << 20);
    }
    cur             = cur + (delta >> 23);
    work->field_9BC = cur;
    cur             = (s16)work->field_A04;
    if (cur == 7) {
        if (player->field_954 != 2) {
            coords = arg0->extra.tmd->coords;
            coord8 = coords + 8;
            if (Gp_ActorSlots[0] == NULL) {
                dist = 0x7FFF;
            } else {
                playerCoords = Gp_ActorSlots[0]->extra.tmd->coords;
                Gp_UpdateCoord(playerCoords + 4);
                Gp_UpdateCoord(coord8);
                viewWorld = &Gfx_ViewWorldMtx;
                Gp_WorldToLocal(viewWorld, &playerCoords[4].workm, &slot.mat);
                Gp_WorldToLocal(viewWorld, &coords[8].workm, &parent);
                dvec.vx = (u16)slot.mat.t[0] - (u16)parent.t[0];
                dvec.vz = (u16)slot.mat.t[2] - (u16)parent.t[2];
                dist    = SquareRoot0((dvec.vx * dvec.vx) + (dvec.vz * dvec.vz));
            }
            if ((s16)dist < 0x500) {
                one                    = 1;
                slot.msg.animBlock.ptr = D_actor_400500_80153CB0;
                slot.msg.field_4       = one;
                slot.msg.field_8       = 0;
                slot.msg.field_C       = 0;
                slot.msg.field_10      = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&slot.msg, 0);
                work->field_A48      = one;
                Gp_StateC08.field_6 |= one;
                work->field_A18      = one;
            }
        }
        cur = (s16)work->field_A04;
    }
    if ((cur == 0xE) && (work->field_A18 == 0)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A0E = 2;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 6;
        work3->field_9FA = 1;
        work->field_A04  = 0;
        work4            = (Actor400500Work*)arg0->work;
        if (((work4->field_A46 >= 0) || ((u8)work4->field_A46 & 0x7F)) && (work4->field_A30 == 0)) {
            flag             = 0x80;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work->field_A08 = work->field_A08 + 1;
    }
    viewCoord                        = &gGfxViewCoord;
    parentp                          = &parent;
    heading                          = work->field_9BC;
    part                             = arg0->extra.tmd->coords + 6;
    scratch                          = (SCRATCH_HEAD(MATRIX));
    walker                           = part->sub;
    scratchBase                      = (u8*)PSX_SCRATCH;
    *(MATRIX**)(scratchBase + 0x3FC) = scratch - 1;
    scratch[-1]                      = part->coord;
    mtx                              = scratch - 1;
    while (1) {
        if (walker == NULL) {
            break;
        }
        if (walker == viewCoord) {
            break;
        }
        parent = walker->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(mtx);
        MatrixNormal(mtx, &slot.mat);
        *mtx   = slot.mat;
        walker = walker->sub;
    }
    func_8004BFF8((s16)heading, mtx);
    func_actor_400500_8013B720(part, mtx);
    memcpy(&part->coord, mtx, 18);
    part->flg = 0;
    Gp_UpdateCoord(part);
    SOFT_BARRIER();
    viewCoord2 = &gGfxViewCoord;
    SOFT_USE_REG(work);
    parentp = &parent;
    model2  = arg0->extra.tmd;
    coords2 = model2->coords;
    __asm__("lui %0, 0x1F80" : "=r"(scratch2) : "r"(model2));
    scratch2 = *(MATRIX**)((u8*)scratch2 + 0x3FC);
    heading2 = (u16)work->field_9BC;
    part     = &coords2[9];
    walker   = part->sub;
    __asm__("move %0,%1" : "=r"(mtx) : "r"(scratch2), "r"(walker));
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch2 + 1) : "memory");
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch2) : "memory");
    *mtx = part->coord;
    while (1) {
        if (walker == NULL) {
            break;
        }
        if (walker == viewCoord2) {
            break;
        }
        parent = walker->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(mtx);
        MatrixNormal(mtx, &slot.mat);
        *mtx   = slot.mat;
        walker = walker->sub;
    }
    func_8004BFF8((s16)heading2, mtx);
    func_actor_400500_8013B720(part, mtx);
    memcpy(&part->coord, mtx, 18);
    part->flg = 0;
    Gp_UpdateCoord(part);
    scratch3                         = (SCRATCH_HEAD(MATRIX));
    scratchBase                      = (u8*)PSX_SCRATCH;
    *(MATRIX**)(scratchBase + 0x3FC) = scratch3 + 1;
    if (((u32)(work->field_A04 - 7) < 4U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 1);
    }
    if (((u32)(work->field_A04 - 0xB) < 0x14U) && (work->field_A18 == 1)) {
        func_actor_400500_801348D8(arg0, 0);
    }
    if (((s16)work->field_A04 == 0xC) && (work->field_A18 != 0)) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 == 0x1E) {
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050007;
        pan2     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 == 0x2A) {
        Gp_SpawnPadLerp(8, 0xC0U, 8U);
        soundId3 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050008;
        pan3     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId3, pan3, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 == 0x1F) {
        Gp_SpawnPadLerp(6, 0xFFU, 0x80U);
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(spawn, 1), 0) != 0) {
            player->field_956 = 0xA;
            work->field_A4D   = 1;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work5             = (Actor400500Work*)arg0->work;
        work5->field_A06  = 0;
        work5->field_A08  = 0;
        work->field_A32   = 0x3C;
        work->obj0.radius = 0x260;
    }
}

void func_actor_400500_80138088(Task* arg0)
{
    MATRIX              normal;
    MATRIX              parent;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    GpCoord*            view;
    GpCoord*            view2;
    MATRIX*             parentp;
    MATRIX*             tmp;
    u8*                 head;
    u8*                 head2;
    u8*                 head3;
    MATRIX*             allocated;
    MATRIX*             matrix;
    GpCoord*            coords;
    GpCoord*            coords2;
    GpCoord*            coord;
    GpCoord*            current;
    GpCoord*            dest;
    Actor400500HitView* hit;
    Actor400500Work*    work3;
    Actor400500Work*    work4;
    s32                 i;
    s32                 cond;
    s32                 flag;
    u16                 angle;
    u16                 addend;
    s32                 delta;
    TmdObject*          model2;
    TmdObject*          model;

    work            = (Actor400500Work*)arg0->work;
    work->field_A04 = work->field_A04 + 1;
    work2           = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);

    addend = *(volatile u16*)&work->field_9BC;
    SOFT_USE_REG(work);
    __asm__("lui %0,%%hi(%1)" : "=r"(model) : "i"(&gGfxViewCoord));
    __asm__("addiu %0,%1,%%lo(%2)" : "=r"(view) : "r"(model), "i"(&gGfxViewCoord));
    SOFT_USE_REG(work);
    delta = work->field_9BC;
    __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(delta));
    head   = *(u8**)(head + 0x3FC);
    addend = addend + ((s32) - (delta * 0x10) >> 7);
    SOFT_TOUCH_REG(addend);
    allocated = (MATRIX*)(head - sizeof(MATRIX));
    SOFT_TOUCH_REG(allocated);
    work->field_9BC = (s16)addend;
    model           = arg0->extra.tmd;
    __asm__("move %0,%1" : "=r"(matrix) : "r"(allocated), "r"(model));
    SOFT_TOUCH_REG_USE(model, matrix);
    coords = model->coords;
    SOFT_USE_REG(work);
    parentp                           = &parent;
    coord                             = &coords[6];
    current                           = coord->sub;
    angle                             = addend;
    SCRATCH_HEAD(void)                = matrix;
    *(MATRIX*)(head - sizeof(MATRIX)) = coords[6].coord;
    __asm__("" : "+r"(current), "=r"(head), "=r"(addend));
    while (1) {
        if (current == NULL) {
            break;
        }
        if (current == view) {
            break;
        }
        parent = current->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &normal);
        *matrix = normal;
        current = current->sub;
    }
    func_8004BFF8((s16)angle, matrix);
    func_actor_400500_8013B720(coord, matrix);
    dest = coord;
    memcpy(&dest->coord, matrix, 18);
    SOFT_TOUCH_REG_USE(dest, work);
    dest->flg = 0;
    Gp_UpdateCoord(dest);
    SOFT_BARRIER();

    view2 = &gGfxViewCoord;
    SOFT_USE_REG(work);
    parentp = &parent;
    model2  = arg0->extra.tmd;
    coords2 = model2->coords;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(model2));
    head2   = *(u8**)(head2 + 0x3FC);
    angle   = (u16)work->field_9BC;
    coord   = &coords2[9];
    current = coord->sub;
    __asm__("move %0,%1" : "=r"(matrix) : "r"(head2), "r"(current));
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2 + sizeof(MATRIX)) : "memory");
    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(head2) : "memory");
    *matrix = coords2[9].coord;
    while (1) {
        if (current == NULL) {
            break;
        }
        if (current == view2) {
            break;
        }
        parent = current->coord;
        tmp    = &parent;
        MatrixNormal(tmp, tmp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &normal);
        *matrix = normal;
        current = current->sub;
    }
    func_8004BFF8((s16)angle, matrix);
    func_actor_400500_8013B720(coord, matrix);
    dest = coord;
    memcpy(&dest->coord, matrix, 18);
    SOFT_TOUCH_REG_USE(dest, work);
    dest->flg = 0;
    Gp_UpdateCoord(dest);

    hit                = (Actor400500HitView*)arg0->work;
    head3              = (u8*)PSX_SCRATCH;
    head3              = *(u8**)(head3 + 0x3FC);
    SCRATCH_HEAD(void) = head3 + sizeof(MATRIX);
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj0.radius = 0x260;
        work4             = (Actor400500Work*)arg0->work;
        work4->field_A06  = 0;
        work4->field_A08  = 0;
        work3             = (Actor400500Work*)arg0->work;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013BA24` copies onto the stack and runs by `field_A08`.
const TaskFuncTable3 D_actor_400500_80131EE4 = { {
    func_actor_400500_801375B8,
    func_actor_400500_8013771C,
    func_actor_400500_80138088,
} };

/// Sub-state handlers `func_actor_400500_801385D0` copies onto the stack and runs
/// by `field_A08` while the enemy is alive.
const TaskFuncTable3 D_actor_400500_80131EF0 = { {
    func_actor_400500_8013BE50,
    func_actor_400500_8013BEC4,
    func_actor_400500_801387E8,
} };

/// Handlers `func_actor_400500_801385D0` runs by `field_A0A`, instead of the
/// sub-state, once the enemy is out of hit points.
const TaskFuncTable3 D_actor_400500_80131EFC = { {
    func_actor_400500_8013BC9C,
    func_actor_400500_8013BCCC,
    func_actor_400500_8013BD64,
} };

void func_actor_400500_801385D0(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TaskFuncTable3   sp10;
    TaskFuncTable3   sp20;
    Actor400500Work* workA;
    Actor400500Work* work2;
    s32              skip;
    s32              i;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp10  = D_actor_400500_80131EF0;
    sp20  = D_actor_400500_80131EFC;
    if (enemy->hp <= 0) {
        if (work->field_A40 == 4) {
            work->field_A42 = 0;
        } else {
            sp20.funcs[(s16)work->field_A0A](arg0);
        }
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        goto common;
    }
    workA = (Actor400500Work*)arg0->work;
    if (workA->field_A4A != 0) {
        workA->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        skip = 1;
    } else {
        skip = 0;
    }
    if (skip == 0) {
        sp10.funcs[(s16)work->field_A08](arg0);
        ((Actor400500Work*)arg0->work)->field_A49 = 0;
        func_actor_400500_80133358(arg0);
    common:
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
}

void func_actor_400500_801387E8(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    OverlayMat          rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    s32                 flag;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xB) {
        work->obj1.flags |= 0x8000;
        work->obj2.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 >= 0x12) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = 0x80;
            work->obj1.flags                                              &= 0x7FFF;
            work->obj2.flags                                              &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
        work3            = (Actor400500Work*)arg0->work;
        if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
            flag             = 0x80;
            work3->field_A46 = flag;
            work3->field_A47 = 0;
        }
        work->field_A32 = 0x3C;
    }
}

/// Sub-state handlers `func_actor_400500_8013899C` copies onto the stack and runs by `field_A08`.
const TaskFuncTable5 D_actor_400500_80131F08 = { {
    func_actor_400500_8013BFB0,
    func_actor_400500_8013C018,
    func_actor_400500_80138B78,
    func_actor_400500_80138CE8,
    func_actor_400500_80138DC4,
} };

void func_actor_400500_8013899C(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TaskFuncTable5   sp;
    Actor400500Work* workA;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              skip;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F08;
    if (enemy->hp > 0) {
        workA = (Actor400500Work*)arg0->work;
        if (workA->field_A4A != 0) {
            workA->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            skip = 1;
        } else {
            skip = 0;
        }
        if (skip == 0) {
            sp.funcs[(s16)work->field_A08](arg0);
            goto common;
        }
    } else {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
    common:
        func_actor_400500_80133358(arg0);
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A49 = 0;
    }
}

void func_actor_400500_80138B78(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    OverlayMat          rot;
    s32                 soundId;
    s32                 pan;
    s32                 cond;
    u16                 frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0xD) {
        work->obj3.flags |= 0x8000;
        work->obj4.flags |= 0x8000;
        soundId           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050005;
        pan               = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_A04 >= 0x10) {
        if ((s16)work->field_A26 != 0) {
            work->field_A26 = (u16)work->field_A26 - 0x80;
        } else {
            ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = 0x80;
            work->obj3.flags                                              &= 0x7FFF;
            work->obj4.flags                                              &= 0x7FFF;
        }
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_80138CE8(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GpCoord*         coord;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    angle                   = work->field_A26 - 0x80;
    work->field_A26         = angle;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[0];
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    func_8004BFF8(-angle, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 <= 0) {
        ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = 0x80;
        work->field_A08                                                = work->field_A08 + 1;
    }
}

void func_actor_400500_80138DC4(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    s32                 cond;
    s32                 flag;
    u32                 rnd;

    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work = (Actor400500Work*)arg0->work;
        if (((work->field_A46 >= 0) || ((u8)work->field_A46 & 0x7F)) && (work->field_A30 == 0)) {
            flag            = 0x80;
            work->field_A46 = flag;
            work->field_A47 = 0;
        }
        ((Actor400500Work*)hit)->field_A32 = 0x3C;
        rnd                                = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState                        = rnd;
        if (!((rnd >> 0x10) & 3)) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 0;
            work2->field_A08 = 0;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 8;
        work3->field_A08 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80138EA0` copies onto the stack and runs by `field_A08`.
const TaskFuncTable4 D_actor_400500_80131F1C = { {
    func_actor_400500_8013C108,
    func_actor_400500_8013C174,
    func_actor_400500_8013C218,
    func_actor_400500_8013C348,
} };

void func_actor_400500_80138EA0(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    TaskFuncTable4   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131F1C;
    if ((enemy->hp <= 0) && (work->field_A40 == 4)) {
        work->field_A42   = 0;
        work->obj1.flags &= 0x7FFF;
        work->obj2.flags &= 0x7FFF;
        work->obj3.flags &= 0x7FFF;
        work->obj4.flags &= 0x7FFF;
        return;
    }
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

/// Sub-state handlers `func_actor_400500_8013905C` copies onto the stack and runs by `field_A08`.
const TaskFuncTable7 D_actor_400500_80131F2C = { {
    func_actor_400500_801391B0,
    func_actor_400500_8013C3C4,
    func_actor_400500_8013C474,
    func_actor_400500_8013C508,
    func_actor_400500_8013C578,
    func_actor_400500_8013C61C,
    func_actor_400500_8013C750,
} };

void func_actor_400500_8013905C(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable7   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F2C;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

void func_actor_400500_801391B0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = 0x80;
    work->obj1.flags                                              &= 0x7FFF;
    work->obj2.flags                                              &= 0x7FFF;
    ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = 0x80;
    work->obj3.flags                                              &= 0x7FFF;
    flags                                                          = work->field_A1E;
    work->obj4.flags                                              &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

void func_actor_400500_801392D8(Task* arg0)
{
    Actor400500Work* work             = (Actor400500Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400500_8013C7A4, func_actor_400500_80139448 };
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
    work4            = (Actor400500Work*)arg0->work;
    if (work4->field_A4A != 0) {
        work4->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
    }
}

void func_actor_400500_80139448(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    Actor400500Work* work5;
    s16              mode;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A16 < 0x9C4) {
        Gp_ArmStateF0(1);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050004;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work2 = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 2;
        work3->field_A08 = 0;
        return;
    }
    mode = work->field_A3C;
    if (mode == 1) {
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050004;
        pan2     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work4 = (Actor400500Work*)arg0->work;
        if ((work4->field_A46 >= 0) || (((u8)work4->field_A46 & 0x7F) != mode)) {
            flag             = 0x81;
            work4->field_A46 = flag;
            work4->field_A47 = 0;
        }
        work5            = (Actor400500Work*)arg0->work;
        work5->field_A06 = 0;
        work5->field_A08 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_801395D0` copies onto the stack and runs by `field_A08`.
const TaskFuncTable3 D_actor_400500_80131F48 = { {
    func_actor_400500_8013C818,
    func_actor_400500_8013973C,
    func_actor_400500_80139AC4,
} };

void func_actor_400500_801395D0(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable3   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F48;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
    sp.funcs[(s16)work->field_A08](arg0);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_8013973C(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local0;
    MATRIX              local3;
    OverlayMat*         src;
    MATRIX*             view;
    Actor400500Work*    work;
    Actor400500Work*    workRot;
    Actor400500Work*    workAnim;
    Actor400500Work*    work3;
    GpCoord*            coordsEarly;
    GpCoord*            coordsMain;
    GpCoord*            coordsRot;
    GpCoord*            part3;
    GpCoord*            root;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    Actor400500ViewPos* posMain;
    Actor400500ViewPos* posMain2;
    s32                 i;
    s32                 three;
    s32                 curX;
    s32                 curZ;
    s32                 tgtX;
    s32                 tgtZ;
    s32                 dx;
    s32                 dz;
    u16                 step;
    u16                 accum;
    u16                 pitch;
    s32                 y;
    s32                 viewZ;

    work = (Actor400500Work*)arg0->work;
    root = arg0->extra.tmd->coords;
    if ((s16)++work->field_A04 < 8) {
        pos2        = &work->field_9A0;
        coordsEarly = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coordsEarly[3]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coordsEarly[3].workm, &rot.mat);
        pos                = pos2;
        pos->x             = rot.mat.t[0];
        pos->z             = rot.mat.t[2];
        coordsEarly[3].flg = 0;
        return;
    }
    tgtX              = (s16)work->field_950;
    curX              = work->field_9A0.x;
    tgtZ              = (s16)work->field_954;
    curZ              = work->field_9A0.z;
    work->field_9A0.x = (u16)work->field_9A0.x + ((tgtX - curX) >> 2);
    work->field_9A0.z = (u16)work->field_9A0.z + ((tgtZ - curZ) >> 2);
    posMain2          = &work->field_9A0;
    coordsMain        = arg0->extra.tmd->coords;
    part3             = &coordsMain[3];
    Gp_UpdateCoord(part3);
    view = &Gfx_ViewWorldMtx;
    Gp_WorldToLocal(view, &coordsMain->workm, &local0);
    Gp_WorldToLocal(view, &coordsMain[3].workm, &local3);
    posMain                = posMain2;
    dx                     = local3.t[0] - local0.t[0];
    coordsMain->coord.t[0] = posMain->x - dx;
    SCHED_BARRIER();
    viewZ                  = posMain->z;
    dz                     = local3.t[2] - local0.t[2];
    coordsMain->coord.t[2] = viewZ - dz;
    coordsMain->flg        = 0;
    coordsMain[3].flg      = 0;
    Gp_UpdateCoord(part3);
    Gp_UpdateCoord(coordsMain);
    step             = (u16)work->field_A10 + 2;
    accum            = (u16)work->field_A12 + step;
    work->field_A12  = accum;
    work->field_A10  = step;
    y                = root->coord.t[1] + (s16)accum;
    root->coord.t[1] = y;
    pitch            = work->field_948;
    three            = 3;
    if ((pitch & 0xFFF) != 0x800) {
        work->field_948 = pitch - 0x80;
    }
    if (root->coord.t[1] >= -0x3E7) {
        y                   = -0x3E8;
        root->coord.t[0]    = (s16)work->field_950;
        root->coord.t[2]    = (s16)work->field_954;
        root->coord.t[1]    = y;
        work->field_A08     = work->field_A08 + 1;
        root->coord.t[1]    = y;
        src                 = &rot;
        work->field_948     = 0;
        work->field_94C     = 0;
        work->field_94A     = (u16)work->field_94A + 0x800;
        workRot             = (Actor400500Work*)arg0->work;
        coordsRot           = arg0->extra.tmd->coords;
        workRot->field_948 &= 0xFFF;
        workRot->field_94A &= 0xFFF;
        workRot->field_94C &= 0xFFF;
        rot.ident.m00_m01   = 0x1000;
        rot.ident.m02_m10   = 0;
        src->ident.m11_m12  = 0x1000;
        rot.ident.m20_m21   = 0;
        src->ident.m22      = 0x1000;
        RotMatrixZ(workRot->field_94C, &src->mat);
        RotMatrixX(workRot->field_948, &src->mat);
        func_8004BFF8(workRot->field_94A, &src->mat);
        func_actor_400500_8013DE2C(&src->mat, &coordsRot->coord);
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0x19;
        work3->field_9FA = 2;
        workAnim         = (Actor400500Work*)arg0->work;
        if (workAnim->field_9FA == 1) {
            if ((s16)workAnim->field_9FC != workAnim->field_9FE) {
                workAnim->field_A00 = 0;
            } else {
                workAnim->field_A00 = func_actor_400500_8013DD8C(arg0, workAnim->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            workAnim->field_9FA = 3;
        } else if (workAnim->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            workAnim->field_9FA = three;
            workAnim->field_A00 = 0;
        } else if (workAnim->field_9FA == three) {
            workAnim->field_A00 = (u16)workAnim->field_A00 + 1;
        }
        i = 1;
        do {
            workAnim->slots[i].rate = (u8)workAnim->field_9F8;
            Gp_AnimTickIndex(&workAnim->anim, i);
            i++;
        } while (i < 0x12);
        root->flg = 0;
        Gp_UpdateCoord(root);
        work->field_A04 = 0;
    }
}

void func_actor_400500_80139AC4(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 flag;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        if ((s16)++work->field_A04 == 1) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050003;
            pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag == 0) {
            hit = (Actor400500HitView*)arg0->work;
            if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
                cond = 1;
            } else {
                cond = 0;
            }
            if (cond) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0;
                work3->field_A08 = 0;
                work->field_A1E |= 1;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

/// Sub-state handlers `func_actor_400500_80139C1C` copies onto the stack and runs by `field_A08`.
const TaskFuncTable3 D_actor_400500_80131F54 = { {
    func_actor_400500_8013C908,
    func_actor_400500_80139D70,
    func_actor_400500_8013C9D4,
} };

void func_actor_400500_80139C1C(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable3   sp;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F54;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_80139D70(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    GpEnemy*         enemy;
    u16              step;
    u16              accum;
    s32              y;
    s16              angle;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;

    coord = arg0->extra.tmd->coords;
    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)++work->field_A04 < 9) {
        if (enemy->hp <= 0) {
            work->field_A42 = 0;
            return;
        }
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_A4A != 0) {
            work2->field_A4A = 0;
            func_actor_400500_8013DB64(arg0, 5);
        }
    } else {
        step              = (u16)work->field_A10 - 2;
        accum             = (u16)work->field_A12 + step;
        work->field_A12   = accum;
        work->field_A10   = step;
        y                 = coord->coord.t[1] - (s16)accum;
        coord->coord.t[1] = y;
        if (work->field_948 < 0x800) {
            angle           = (u16)work->field_948 + 0x98;
            work->field_948 = angle;
            if (angle >= 0x801) {
                work->field_948 = 0x800;
            }
        }
        if (coord->coord.t[1] < -0xFA0) {
            coord->coord.t[1] = -0xFA0;
            work->field_A08   = work->field_A08 + 1;
            coord->coord.t[1] = -0xFA0;
            work->field_948   = 0;
            work->field_94C   = 0x800;
            work->field_94A   = (u16)work->field_94A + 0x800;
            work2             = (Actor400500Work*)arg0->work;
            work2->field_9F8  = 0x10;
            work2->field_9FE  = 0x19;
            work2->field_9FA  = 2;
            soundId           = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050001;
            pan               = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            soundId2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050002;
            pan2     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
    }
}

void func_actor_400500_80139F6C(Task* arg0)
{
    Actor400500Work* work             = (Actor400500Work*)arg0->work;
    void             (*fns[2])(Task*) = { func_actor_400500_8013CA38, func_actor_400500_8013A0B8 };
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              i;

    fns[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A3C = 0;
    work3->field_A3E = 0;
    work3            = (Actor400500Work*)arg0->work;
    work3->field_A49 = 0;
}

void func_actor_400500_8013A0B8(Task* arg0)
{
    OverlayMat          rot;
    MATRIX              local2;
    OverlayMat*         src;
    Actor400500Work*    work;
    Actor400500Work*    ang;
    Actor400500Work*    work3;
    Actor400500Work*    nextWork;
    Actor400500Work*    anim;
    Actor400500HitView* hit;
    Actor400500ViewPos* pos;
    GpCoord*            coords;
    GpCoord*            coord;
    GpCoord*            coord14;
    GpEnemy*            enemy;
    MATRIX*             view;
    Actor400500ViewPos* pos2;
    s32                 z;
    s32                 cond;
    s32                 flag;
    s32                 i;
    s32                 dx;
    s32                 dz;
    s32                 delta;
    s32                 neg;
    u32                 rnd;

    neg    = -1;
    coords = arg0->extra.tmd->coords;
    work   = (Actor400500Work*)arg0->work;
    enemy  = (GpEnemy*)arg0->spawnArg2;
    if ((s16)work->field_A04 == neg) {
        coord14 = &coords[0xE];
        Gp_UpdateCoord(coord14);
        pos2 = &work->field_9A0;
        view = &Gfx_ViewWorldMtx;
        Gp_WorldToLocal(view, &coords->workm, &rot.mat);
        Gp_WorldToLocal(view, &coord14->workm, &local2);
        dx                 = local2.t[0] - rot.mat.t[0];
        coords->coord.t[0] = work->field_9A0.x - dx;
        pos                = pos2;
        z                  = pos->z;
        delta              = local2.t[2] - rot.mat.t[2];
        coords->flg        = 0;
        coord14->flg       = 0;
        dz                 = delta;
        coords->coord.t[2] = z - dz;
        Gp_UpdateCoord(coord14);
        Gp_UpdateCoord(coords);
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        src = &rot;
        if (enemy->hp > 0) {
            work->field_A1E   &= 0xFFFD;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = arg0->extra.tmd->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            func_8004BFF8(ang->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &coord->coord);
            work3            = (Actor400500Work*)arg0->work;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 1;
            work3->field_9FA = 2;
            anim             = (Actor400500Work*)arg0->work;
            if (anim->field_9FA == 1) {
                if ((s16)anim->field_9FC != anim->field_9FE) {
                    anim->field_A00 = 0;
                } else {
                    anim->field_A00 = func_actor_400500_8013DD8C(arg0, anim->field_A00);
                }
                func_actor_400500_8013DCD4(arg0);
                anim->field_9FA = 3;
            } else if (anim->field_9FA == 2) {
                func_actor_400500_8013DC4C(arg0);
                anim->field_9FA = 3;
                anim->field_A00 = 0;
            } else if (anim->field_9FA == 3) {
                anim->field_A00 = (u16)anim->field_A00 + 1;
            }
            i = 1;
            do {
                anim->slots[i].rate = (u8)anim->field_9F8;
                Gp_AnimTickIndex(&anim->anim, i);
                i++;
            } while (i < 0x12);
            coords->flg = 0;
            Gp_UpdateCoord(coords);
            work3 = (Actor400500Work*)arg0->work;
            if (work3->field_A4A != 0) {
                work3->field_A4A = 0;
                func_actor_400500_8013DB64(arg0, 5);
                flag = 1;
            } else {
                flag = 0;
            }
            if (flag == 0) {
                rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 0x10) & 1) == 0) {
                    nextWork            = (Actor400500Work*)arg0->work;
                    nextWork->field_A06 = 0;
                    nextWork->field_A08 = 0;
                    return;
                }
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 8;
                work3->field_A08 = 0;
            }
        } else {
            work->field_A42    = 0;
            work->field_94A    = ((u16)work->field_94A + 0x800) & 0xFFF;
            ang                = (Actor400500Work*)arg0->work;
            coord              = arg0->extra.tmd->coords;
            ang->field_948    &= 0xFFF;
            ang->field_94A    &= 0xFFF;
            ang->field_94C    &= 0xFFF;
            rot.ident.m00_m01  = 0x1000;
            rot.ident.m02_m10  = 0;
            src->ident.m11_m12 = 0x1000;
            rot.ident.m20_m21  = 0;
            src->ident.m22     = 0x1000;
            RotMatrixZ(ang->field_94C, &src->mat);
            RotMatrixX(ang->field_948, &src->mat);
            func_8004BFF8(ang->field_94A, &src->mat);
            func_actor_400500_8013DE2C(&src->mat, &coord->coord);
            coords->flg = 0;
            Gp_UpdateCoord(coords);
        }
    }
}

/// Sub-state handlers `func_actor_400500_8013A484` copies onto the stack and runs by `field_A08`.
const TaskFuncTable7 D_actor_400500_80131F60 = { {
    func_actor_400500_8013A5D8,
    func_actor_400500_8013D4F0,
    func_actor_400500_8013D59C,
    func_actor_400500_8013D630,
    func_actor_400500_8013D6A0,
    func_actor_400500_8013D744,
    func_actor_400500_8013D878,
} };

void func_actor_400500_8013A484(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable7   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131F60;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

void func_actor_400500_8013A5D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              flags;
    u8               unused[0x30];

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    ((Actor400500Work*)arg0->work)->field_9F0[1]->extra.tmd->flags = 0x80;
    work->obj1.flags                                              &= 0x7FFF;
    work->obj2.flags                                              &= 0x7FFF;
    ((Actor400500Work*)arg0->work)->field_9F0[0]->extra.tmd->flags = 0x80;
    work->obj3.flags                                              &= 0x7FFF;
    flags                                                          = work->field_A1E;
    work->obj4.flags                                              &= 0x7FFF;
    if (!(flags & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 0xF;
        work2->field_9FA = 2;
        work->field_A08  = 3;
    } else if (!(flags & 2)) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 0xF;
        work3->field_9FA = 2;
        work->field_A08  = 1;
    } else {
        work4            = (Actor400500Work*)arg0->work;
        work4->field_9F8 = 0x10;
        work4->field_9FE = 0x11;
        work4->field_9FA = 2;
        work->field_A08  = 1;
    }
}

/// State handlers `func_actor_400500_8013A700` copies onto the stack and runs
/// by `field_A06`.
const TaskFuncTable10 D_actor_400500_80131F7C = { {
    func_actor_400500_8013A8E4,
    func_actor_400500_8013AA98,
    func_actor_400500_8013D8CC,
    func_actor_400500_8013D958,
    func_actor_400500_8013ABE4,
    func_actor_400500_8013D9DC,
    func_actor_400500_8013D9F4,
    func_actor_400500_8013DA24,
    func_actor_400500_8013DA68,
    func_actor_400500_8013DACC,
} };

void func_actor_400500_8013A700(Task* arg0)
{
    TmdObject*       extra;
    Actor400500Work* work;
    TaskFuncTable10  sp;
    TmdObject*       extra2;
    TmdObject*       extraCopy;
    u8*              head;
    u8*              head2;
    VECTOR*          block;
    GpCoord*         coord;
    u8               session;
    u16              a28;
    u32              flags;
    u32              shifted;

    extra = arg0->extra.tmd;
    work  = (Actor400500Work*)arg0->work;
    sp    = D_actor_400500_80131F7C;
    switch (Gp_StateF0.field_4) {
        case 2:
            extra->flags |= 0x80;
            return;
        case 0:
            sp.funcs[(s16)work->field_A06](arg0);
        case 1:
            extra2 = arg0->extra.tmd;
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            SOFT_USE_REG(extra2);
            coord = extra2->coords;
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            head                   = *(u8**)(head + 0x3FC);
            coord                  = coord + 1;
            ((VECTOR*)head)[-1].vx = coord->workm.t[0];
            block                  = (VECTOR*)(head - 0x10);
            block->vy              = coord->workm.t[1];
            block->vz              = coord->workm.t[2];
            SCRATCH_HEAD(VECTOR)   = block;
            Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
            extraCopy = extra2;
            session   = gGameSession->at4.loc.room;
            if (session != 1) {
                SOFT_TOUCH_REG(extraCopy);
            }
            if ((session == 1) || (session == 3) || (session == 5) || (session == 6)) {
                Gp_SetObjTrans(extraCopy, 0x200, 0x200, 0x200);
            } else {
                Gp_SetObjTrans(extra2, 0x400, 0x1000, 0x400);
            }
            SOFT_USE_REG(extraCopy);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head2) : "r"(work));
            head2            = *(u8**)(head2 + 0x3FC);
            a28              = work->field_A28;
            head2           += 0x10;
            flags            = (u32)a28 << 0x10;
            SCRATCH_HEAD(u8) = head2;
            if (flags != 0) {
                shifted = flags >> 0x12;
                SOFT_TOUCH_REG(shifted);
                func_actor_400500_80132AB0(arg0, -0xFA0, shifted & 0xFF);
                func_actor_400500_80132AB0(arg0, -0x3E8, (u8)work->field_A28);
            }
            return;
    }
}

void func_actor_400500_8013A8E4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GpEnemy*         enemy;
    s32              mapped;
    s32              i;

    work            = (Actor400500Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    mapped          = D_actor_400500_80153DD4[work->field_9FE];
    work->field_9F8 = 0x10;
    work->field_9FA = 2;
    work->field_9FE = mapped;
    work2           = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj3);
    Gp_UnlinkObj(&work->obj2);
    Gp_UnlinkObj(&work->obj4);
    GameFlag_SetNibble(0xCE, 1);
    if (work->field_A40 == 4) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 6;
        work3->field_A08 = 0;
        return;
    }
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013AA98(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500HitView* hit;
    s32                 i;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    work2 = work;
    if (work->field_9FA == 1) {
        if ((s16)work->field_9FC != work->field_9FE) {
            work->field_A00 = 0;
        } else {
            work->field_A00 = func_actor_400500_8013DD8C(arg0, work->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work->field_9FA = 3;
        work->field_A00 = 0;
    } else if (work->field_9FA == 3) {
        work->field_A00 = (u16)work->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013ABE4(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GpCoord*         coord;
    VECTOR           scale;
    SVECTOR          pos;
    u16              frame;

    work  = (Actor400500Work*)arg0->work;
    model = arg0->extra.tmd;
    coord = model->coords;

    work->field_A20   = (u16)work->field_A20 + ((s16)(0xFF - (u16)work->field_A20) >> 4);
    work->field_A24   = (u16)work->field_A24 + ((s16) - (u16)work->field_A24 >> 4);
    work->field_A28   = (u16)work->field_A28 + (-work->field_A28 >> 4);
    model->lightLevel = work->field_A24;
    func_8009EA50(work->field_A20);

    work->field_A02 = (u16)work->field_A02 - 0x30;
    scale.vx        = 0x1000;
    scale.vy        = work->field_A02;
    scale.vz        = 0x1000;
    coord->coord    = work->matrix_808;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;

    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame == 0x10) {
        pos.vx = 0;
        pos.vy = 0;
        pos.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 5, &pos);
    }
    if ((s16)work->field_A04 >= 0x41) {
        model->flags   |= 0x80;
        work->field_A06 = work->field_A06 + 1;
    }
}

/// Sub-state handlers `func_actor_400500_8013AD60` copies onto the stack and runs by `field_A08`.
const TaskFuncTable11 D_actor_400500_80131FA4 = { {
    func_actor_400500_8013AF44,
    func_actor_400500_8013B228,
    func_actor_400500_8013CB0C,
    func_actor_400500_8013CBD8,
    func_actor_400500_8013CCDC,
    func_actor_400500_8013B374,
    func_actor_400500_8013CDA8,
    func_actor_400500_8013CE9C,
    func_actor_400500_8013CF68,
    func_actor_400500_8013D078,
    func_actor_400500_8013D144,
} };

void func_actor_400500_8013AD60(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable11  sp;
    GpEnemy*         enemy;
    Actor400500Work* work2;
    s32              i;
    Actor400500Work* work3;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    sp    = D_actor_400500_80131FA4;
    if ((s16)work->field_A08 != 0) {
        work2 = (Actor400500Work*)arg0->work;
        if (work2->field_9FA == 1) {
            if ((s16)work2->field_9FC != work2->field_9FE) {
                work2->field_A00 = 0;
            } else {
                work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work2->field_9FA = 3;
        } else if (work2->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work2->field_9FA = 3;
            work2->field_A00 = 0;
        } else if (work2->field_9FA == 3) {
            work2->field_A00 = (u16)work2->field_A00 + 1;
        }
        i = 1;
        do {
            work2->slots[i].rate = (u8)work2->field_9F8;
            Gp_AnimTickIndex(&work2->anim, i);
            i++;
        } while (i < 0x12);
    }
    if (enemy->hp > 0) {
        sp.funcs[(s16)work->field_A08](arg0);
    } else {
        work->field_A42 = 0;
    }
    work3 = (Actor400500Work*)arg0->work;
    if (((work3->field_A46 >= 0) || ((u8)work3->field_A46 & 0x7F)) && (work3->field_A30 == 0)) {
        flag             = 0x80;
        work3->field_A46 = flag;
        work3->field_A47 = 0;
    }
}

void func_actor_400500_8013AF44(Task* arg0)
{
    MATRIX              local;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    GpCoord*            coord;
    GpCoord*            coords;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    s32                 flag;
    s32                 heading;
    s32                 i;
    u16                 a1c;

    work    = (Actor400500Work*)arg0->work;
    heading = (u16)work->field_94A & 0xFFF;
    coord   = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0)) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 4;
        work2->field_9FA = 2;
        work3            = (Actor400500Work*)arg0->work;
        if (work3->field_9FA == 1) {
            if ((s16)work3->field_9FC != work3->field_9FE) {
                work3->field_A00 = 0;
            } else {
                work3->field_A00 = func_actor_400500_8013DD8C(arg0, work3->field_A00);
            }
            func_actor_400500_8013DCD4(arg0);
            work3->field_9FA = 3;
        } else if (work3->field_9FA == 2) {
            func_actor_400500_8013DC4C(arg0);
            work3->field_9FA = 3;
            work3->field_A00 = 0;
        } else if (work3->field_9FA == 3) {
            work3->field_A00 = (u16)work3->field_A00 + 1;
        }
        i = 1;
        do {
            work3->slots[i].rate = (u8)work3->field_9F8;
            Gp_AnimTickIndex(&work3->anim, i);
            i++;
        } while (i < 0x12);
        switch ((s16)((u16)work->field_A1A - 1)) {
            case 3:
                if (heading != 0x400) {
                    work->field_A08 = 4;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 0:
                if (work->field_9E0 >= 0) {
                    work->field_A08 = 3;
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 1:
                a1c = work->field_A1C;
                if ((u32)(a1c - 1) < 2U) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 4) || ((s16)work->field_A1C == 5)) {
                    work->field_A08 = 3;
                } else if (((s16)a1c == 3) && (heading == 0)) {
                    if (coord->coord.t[0] >= 0x4074) {
                        work->field_A08 = 6;
                    } else {
                        work->field_A08 = 1;
                    }
                } else {
                    work->field_A08 = 1;
                }
                break;
            case 2:
                if (work->field_9E4 < 0) {
                    work->field_A08 = 6;
                } else {
                    work->field_A08 = 8;
                }
                break;
            case 5:
                if (heading == 0x800) {
                    work->field_A08 = 8;
                } else {
                    work->field_A08 = 7;
                }
                break;
            default:
                coord->coord.t[0] = -0x3E8;
                coord->coord.t[1] = -0xFA0;
                coord->coord.t[2] = -0x2116;
                work->field_94A   = 0x400;
                work->field_A08   = 1;
                break;
        }
        pos2   = &work->field_9A0;
        coords = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coords[8]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[8].workm, &local);
        pos           = pos2;
        pos->x        = local.t[0];
        pos->z        = local.t[2];
        coords[8].flg = 0;
    }
}

void func_actor_400500_8013B228(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GpCoord*         coord;
    s32              flag;
    s32              a1a;
    u16              a1c;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0x400) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            a1a = work->field_A1A;
            if (a1a != 1) {
                if ((a1a == 2) && (coord->coord.t[0] >= 0x4075)) {
                    a1c = work->field_A1C;
                    if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
                        work->field_A08 = 5;
                    } else {
                        work->field_A08 = a1a;
                    }
                } else {
                    func_actor_400500_8013403C(arg0);
                }
            } else {
                if (work->field_9E0 < -0xF9F) {
                    work3            = (Actor400500Work*)arg0->work;
                    work3->field_A06 = 9;
                    work3->field_A08 = 0;
                }
                func_actor_400500_8013403C(arg0);
            }
        }
        coord->coord.t[2] = -0x209E;
    }
}

void func_actor_400500_8013B374(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    Actor400500Work* work4;
    s32              flag;
    u16              a1c;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        a1c = work->field_A1C;
        if (((u32)(a1c - 2) < 2U) || ((s16)a1c == 6)) {
            if (!((u16)work->field_94A & 0xFFF)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 4;
                work2->field_9FA = 2;
                work->field_A08  = 6;
                return;
            }
        } else if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work3            = (Actor400500Work*)arg0->work;
            work3->field_9F8 = 0x10;
            work3->field_9FE = 4;
            work3->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work4            = (Actor400500Work*)arg0->work;
        work4->field_A06 = 9;
        work4->field_A08 = 0;
    }
}

void func_actor_400500_8013B4A4(Task* arg0)
{
    Actor400500Work* work;
    GpEnemy*         enemy;
    u8               mode;

    work  = (Actor400500Work*)arg0->work;
    mode  = gGameSession->at4.loc.room;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((mode == 1) || (mode == 3) || (mode == 5) || (mode == 6)) {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->field_A2C = 0x10;
            work->field_A2E = 0;
            return;
        }
        if (quarter < hp) {
            work->field_A2C = 0x20;
            work->field_A2E = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->field_A2C = 0x30;
            work->field_A2E = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->field_A2C = 0x40;
            work->field_A2E = 0xC0;
            return;
        }
        work->field_A2C = 0x50;
        work->field_A2E = 0x100;
        return;
    } else {
        s16 hp;
        s32 maxHp;
        s32 quarter;

        hp      = enemy->hp;
        maxHp   = enemy->hpMax << 0x10;
        quarter = maxHp >> 0x12;
        if ((quarter + (maxHp >> 0x11)) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x20;
            return;
        }
        if (quarter < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x40;
            return;
        }
        if ((maxHp >> 0x13) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0x80;
            return;
        }
        if ((maxHp >> 0x14) < hp) {
            work->field_A2C = 0x2000;
            work->field_A2E = 0xC0;
            return;
        }
        work->field_A2C = 0x2000;
        work->field_A2E = 0x100;
    }
}

/// Sub-state handlers `func_actor_400500_8013B5E0` copies onto the stack and runs by `field_A08`.
const TaskFuncTable5 D_actor_400500_80131FEC = { {
    func_actor_400500_8013D210,
    func_actor_400500_8013D274,
    func_actor_400500_8013D2D8,
    func_actor_400500_8013D3B8,
    func_actor_400500_8013D420,
} };

void func_actor_400500_8013B5E0(Task* arg0)
{
    Actor400500Work* work;
    TaskFuncTable5   sp;
    Actor400500Work* work2;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131FEC;
    sp.funcs[(s16)work->field_A08](arg0);
    work2 = (Actor400500Work*)arg0->work;
    if (work2->field_9FA == 1) {
        if ((s16)work2->field_9FC != work2->field_9FE) {
            work2->field_A00 = 0;
        } else {
            work2->field_A00 = func_actor_400500_8013DD8C(arg0, work2->field_A00);
        }
        func_actor_400500_8013DCD4(arg0);
        work2->field_9FA = 3;
    } else if (work2->field_9FA == 2) {
        func_actor_400500_8013DC4C(arg0);
        work2->field_9FA = 3;
        work2->field_A00 = 0;
    } else if (work2->field_9FA == 3) {
        work2->field_A00 = (u16)work2->field_A00 + 1;
    }
    i = 1;
    do {
        work2->slots[i].rate = (u8)work2->field_9F8;
        Gp_AnimTickIndex(&work2->anim, i);
        i++;
    } while (i < 0x12);
}

s32 func_actor_400500_8013B720(GpCoord* arg0, MATRIX* arg1)
{
    MATRIX   matrix;
    MATRIX   parent;
    MATRIX   normal;
    MATRIX   transposed;
    GpCoord* coord;
    GpCoord* view;
    MATRIX*  parentp;

    coord = arg0->sub;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    view    = &gGfxViewCoord;
    parentp = &parent;
    matrix  = coord->coord;
    while (1) {
        coord = coord->sub;
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            break;
        }
        parent = coord->coord;
        MatrixNormal(parentp, parentp);
        gte_SetRotMatrix(parentp);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    __asm__ volatile(
        "lhu $12, 0(%0);"
        "lhu $13, 6(%0);"
        "lhu $14, 12(%0);"
        "sh $12, 0(%1);"
        "sh $13, 2(%1);"
        "sh $14, 4(%1);"
        "lhu $12, 2(%0);"
        "lhu $13, 8(%0);"
        "lhu $14, 14(%0);"
        "sh $12, 6(%1);"
        "sh $13, 8(%1);"
        "sh $14, 10(%1);"
        "lhu $12, 4(%0);"
        "lhu $13, 10(%0);"
        "lhu $14, 16(%0);"
        "sh $12, 12(%1);"
        "sh $13, 14(%1);"
        "sh $14, 16(%1);"
        : : "r"(&matrix), "r"(&transposed) : "$12", "$13", "$14", "memory");
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}

/// Carries `pos`, a point local to `coord`, up the `sub` chain by applying
/// each level's matrix. If the chain reaches the view coordinate the
/// transformed point is written back to `pos` and 1 is returned; if it ends
/// first, `pos` is left untouched and 0 is returned.
s32 func_actor_400500_8013B920(GpCoord* coord, SVECTOR* pos)
{
    SVECTOR  local;
    VECTOR   result;
    s32      flag;
    GpCoord* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

void func_actor_400500_8013BA24(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    TaskFuncTable3   sp;

    work = (Actor400500Work*)arg0->work;
    sp   = D_actor_400500_80131EE4;
    sp.funcs[(s16)work->field_A08](arg0);
    work2            = (Actor400500Work*)arg0->work;
    work2->field_A3C = 0;
    work2->field_A3E = 0;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_A49 = 0;
}

void func_actor_400500_8013BAA4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A40 == 4) {
        work->field_A42 = 0;
        return;
    }
    if (!(work->field_A1E & 1)) {
        work->field_A42  = 1;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 1;
        work2->field_9FA = 2;
        work->field_A04  = 0;
        work->field_A18  = 0;
        work->field_A10  = 0;
        work->field_A12  = 0;
        work->field_A0A  = work->field_A0A + 1;
        return;
    }
    work->field_A42 = 0;
}

void func_actor_400500_8013BB18(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    u16              step;
    u16              accum;
    s32              y;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    SCHED_BARRIER();
    step              = (u16)work->field_A10 + 2;
    accum             = (u16)work->field_A12 + step;
    work->field_A12   = accum;
    work->field_A10   = step;
    y                 = coord->coord.t[1] + (s16)accum;
    coord->coord.t[1] = y;
    if (y >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A0A   = work->field_A0A + 1;
        work2             = (Actor400500Work*)arg0->work;
        work2->field_9F8  = 0x10;
        work2->field_9FE  = 0x13;
        work2->field_9FA  = 2;
        work->field_94C   = (u16)work->field_94C + 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
    }
}

void func_actor_400500_8013BBB0(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013BC9C(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A42 = 1;
    work->field_A04 = 0;
    work->field_A18 = 0;
    work->field_A10 = 0;
    work->field_A12 = 0;
    work->field_A0A = work->field_A0A + 1;
}

void func_actor_400500_8013BCCC(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    u16              step;
    u16              accum;
    s32              y;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    SCHED_BARRIER();
    step              = (u16)work->field_A10 + 2;
    accum             = (u16)work->field_A12 + step;
    work->field_A12   = accum;
    work->field_A10   = step;
    y                 = coord->coord.t[1] + (s16)accum;
    coord->coord.t[1] = y;
    if (y >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A0A   = work->field_A0A + 1;
        work2             = (Actor400500Work*)arg0->work;
        work2->field_9F8  = 0x10;
        work2->field_9FE  = 0x13;
        work2->field_9FA  = 2;
        work->field_94C   = (u16)work->field_94C + 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
    }
}

void func_actor_400500_8013BD64(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work = (Actor400500Work*)arg0->work;
    if ((s16)work->field_A04 == 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013BE50(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work            = (Actor400500Work*)arg0->work;
    work->field_9F8 = 0x10;
    work2           = (Actor400500Work*)arg0->work;
    if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
        flag             = 0x81;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 1;
    work3->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A26  = 0;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013BEC4(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GpCoord*         coord;
    Actor400500Work* work2;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    work->field_A26         = work->field_A26 + 0x80;
    work->field_A04         = work->field_A04 + 1;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[1];
    angle                   = work->field_A26;
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    func_8004BFF8(angle, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 8;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

void func_actor_400500_8013BFB0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 1;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A26  = 0;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013C018(Task* arg0)
{
    Actor400500Work* work;
    OverlayMat       rot;
    OverlayMat*      src;
    Task*            child;
    GpCoord*         coord;
    Actor400500Work* work2;
    s32              angle;

    work                    = (Actor400500Work*)arg0->work;
    src                     = &rot;
    work->field_A26         = work->field_A26 + 0x80;
    work->field_A04         = work->field_A04 + 1;
    child                   = ((Actor400500Work*)arg0->work)->field_9F0[0];
    angle                   = work->field_A26;
    child->extra.tmd->flags = 0;
    coord                   = child->extra.tmd->coords;
    rot.ident.m00_m01       = 0x1000;
    rot.ident.m02_m10       = 0;
    src->ident.m11_m12      = 0x1000;
    rot.ident.m20_m21       = 0;
    src->ident.m22          = 0x1000;
    func_8004BFF8(-angle, &src->mat);
    func_actor_400500_8013DE2C(&src->mat, &coord->coord);
    if ((s16)work->field_A26 >= 0x200) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_9F8 = 0x10;
        work2->field_9FE = 7;
        work2->field_9FA = 2;
        work->field_A18  = 0;
        work->field_A04  = 0;
        work->field_A08  = work->field_A08 + 1;
    }
}

void func_actor_400500_8013C108(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((work->field_A46 >= 0) || (((u8)work->field_A46 & 0x7F) != 1)) {
        flag            = 0x81;
        work->field_A46 = flag;
        work->field_A47 = 0;
    }
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 1;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0;
    work->field_A12  = 0;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013C174(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    u16              step;
    u16              accum;
    s32              y;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    SCHED_BARRIER();
    step              = (u16)work->field_A10 + 2;
    accum             = (u16)work->field_A12 + step;
    work->field_A12   = accum;
    work->field_A10   = step;
    y                 = coord->coord.t[1] + (s16)accum;
    coord->coord.t[1] = y;
    if (y >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08   = work->field_A08 + 1;
        work2             = (Actor400500Work*)arg0->work;
        work2->field_9F8  = 0x10;
        work2->field_9FE  = 0x13;
        work2->field_9FA  = 2;
        work->field_94C   = (u16)work->field_94C + 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E   = work->field_A1E | 2;
    }
}

void func_actor_400500_8013C218(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

void func_actor_400500_8013C348(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    GpEnemy*            enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (enemy->hp > 0) {
            work2                              = (Actor400500Work*)arg0->work;
            work2->field_A06                   = 0xA;
            work2->field_A08                   = 0;
            ((Actor400500Work*)hit)->field_A1E = ((Actor400500Work*)hit)->field_A1E | 1;
            return;
        }
        ((Actor400500Work*)hit)->field_A42 = 0;
    }
}

void func_actor_400500_8013C3C4(Task* arg0)
{
    GpEnemy*            enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013C474(Task* arg0)
{
    GpEnemy*            enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A4A = 0;
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A06 = 0;
                work2->field_A08 = 0;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0xA;
                work3->field_A08 = 0;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013C508(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    GpEnemy*            enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work            = (Actor400500Work*)hit;
        work->field_A04 = 0;
        work->field_A18 = 0;
        work->field_A10 = 0;
        work->field_A12 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_8013C578(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    u16              step;
    u16              accum;
    s32              y;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    SCHED_BARRIER();
    step              = (u16)work->field_A10 + 2;
    accum             = (u16)work->field_A12 + step;
    work->field_A12   = accum;
    work->field_A10   = step;
    y                 = coord->coord.t[1] + (s16)accum;
    coord->coord.t[1] = y;
    if (y >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08   = work->field_A08 + 1;
        work2             = (Actor400500Work*)arg0->work;
        work2->field_9F8  = 0x10;
        work2->field_9FE  = 0x13;
        work2->field_9FA  = 2;
        work->field_94C   = (u16)work->field_94C + 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E   = work->field_A1E | 1;
    }
}

void func_actor_400500_8013C61C(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A4A = 0;
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

void func_actor_400500_8013C750(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

void func_actor_400500_8013C7A4(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work            = (Actor400500Work*)arg0->work;
    work->field_9F8 = 0x10;
    work2           = (Actor400500Work*)arg0->work;
    if (((work2->field_A46 >= 0) || ((u8)work2->field_A46 & 0x7F)) && (work2->field_A30 == 0)) {
        flag             = 0x80;
        work2->field_A46 = flag;
        work2->field_A47 = 0;
    }
    work3            = (Actor400500Work*)arg0->work;
    work3->field_9F8 = 0x10;
    work3->field_9FE = 1;
    work3->field_9FA = 2;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013C818(Task* arg0)
{
    Actor400500Work*   work;
    Actor400500Work*   work2;
    Actor400500RootXZ* coord;
    s32                soundId;
    s32                pan;

    coord   = (Actor400500RootXZ*)arg0->extra.tmd->coords;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050004;
    work    = (Actor400500Work*)arg0->work;
    pan     = (s8)Gp_GetObjPan((GpCoord*)coord);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x20;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0x80;
    work->field_A12  = 0;
    work->field_A04  = 0;
    work->field_A08  = work->field_A08 + 1;
    work->field_950  = coord->x;
    work->field_954  = coord->z;
}

void func_actor_400500_8013C908(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work    = (Actor400500Work*)arg0->work;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050004;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x15;
    work2->field_9FA = 2;
    work->field_A04  = 0;
    work->field_A18  = 0;
    work->field_A10  = 0;
    work->field_A12  = 0x12C;
    work->field_948  = 0;
    work->field_A08  = work->field_A08 + 1;
}

void func_actor_400500_8013C9D4(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2                                = (Actor400500Work*)arg0->work;
        work2->field_A06                     = 0;
        work2->field_A08                     = 0;
        ((Actor400500Work*)work)->field_A1E &= 0xFFFE;
    }
}

void func_actor_400500_8013CA38(Task* arg0)
{
    MATRIX              local;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    GpCoord*            coords;
    Actor400500ViewPos* pos;
    Actor400500ViewPos* pos2;
    s32                 heading;
    s32                 masked;
    s32                 neg;

    work             = (Actor400500Work*)arg0->work;
    heading          = (u16)work->field_94A;
    work->field_A04  = 0;
    work2            = (Actor400500Work*)arg0->work;
    work2->field_9F8 = 0x10;
    work2->field_9FE = 0x16;
    work2->field_9FA = 2;
    masked           = heading & 0xFFF;
    if ((work->field_A1A == 1) && ((masked == 0x400) || (masked == 0xC00))) {
        neg             = -1;
        work->field_A04 = neg;
        pos2            = &work->field_9A0;
        coords          = arg0->extra.tmd->coords;
        Gp_UpdateCoord(&coords[0xE]);
        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[0xE].workm, &local);
        pos             = pos2;
        pos->x          = local.t[0];
        pos->z          = local.t[2];
        coords[0xE].flg = 0;
    }
    work->field_A08 = work->field_A08 + 1;
}

void func_actor_400500_8013CB0C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013CBD8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GpCoord*         coord;
    s32              flag;
    s32              a1a;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            a1a = work->field_A1A;
            if (a1a != 1) {
                if (a1a == 4) {
                    work->field_A08 = a1a;
                }
            } else if (work->field_9E0 >= 0xFA0) {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 9;
                work3->field_A08 = 0;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[2] = -0x209E;
    }
}

void func_actor_400500_8013CCDC(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0x400) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 1;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013CDA8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if ((u16)work->field_94A & 0xFFF) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            if (work->field_A1A != 3) {
                if (work->field_A1A == 6) {
                    work->field_A08 = 7;
                }
            } else if (work->field_9E4 > 0) {
                work->field_A08 = 7;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

void func_actor_400500_8013CE9C(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0x800) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 8;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013CF68(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) != 0x800) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_A06 = 9;
            work2->field_A08 = 0;
        } else {
            switch (work->field_A1A) {
                case 2:
                    if (coord->coord.t[2] < -0x209E) {
                        work->field_A08 = 9;
                    }
                    break;
                case 3:
                    if (work->field_9E4 < 0) {
                        work->field_A08 = 0xA;
                    }
                    break;
            }
            func_actor_400500_8013403C(arg0);
        }
        coord->coord.t[0] = 0x4074;
    }
}

void func_actor_400500_8013D078(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0xC00) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 3;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013D144(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if (work->field_A4A != 0) {
        work->field_A4A = 0;
        func_actor_400500_8013DB64(arg0, 5);
        flag = 1;
    } else {
        flag = 0;
    }
    if ((flag == 0) && ((func_actor_400500_8013DB78(arg0) << 0x10) == 0) &&
        ((func_actor_400500_80133460(arg0) << 0x10) == 0)) {
        if (((u16)work->field_94A & 0xFFF) == 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 4;
            work2->field_9FA = 2;
            work->field_A08  = 6;
            return;
        }
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A06 = 9;
        work3->field_A08 = 0;
    }
}

void func_actor_400500_8013D210(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;

    work              = (Actor400500Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    work->field_94C   = 0x800;
    work->field_94A   = 0;
    coord->coord.t[0] = 0x4074;
    coord->coord.t[1] = -0xFA0;
    coord->coord.t[2] = -0x2710;
    work->field_A04   = 0;
    work2             = (Actor400500Work*)arg0->work;
    work2->field_9F8  = 4;
    work2->field_9FE  = 1;
    work2->field_9FA  = 2;
    work->field_A08   = work->field_A08 + 1;
}

void func_actor_400500_8013D274(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((u8)work->field_A4B == 2) {
        work->field_A2C = 0x258;
        work2           = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work->field_A04 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_8013D2D8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;
    GpCoord*         coord;
    s32              flag;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if ((s16)++work->field_A04 == 1) {
        work2 = (Actor400500Work*)arg0->work;
        if (((work2->field_A46 >= 0) || ((u8)work2->field_A46 & 0x7F)) && (work2->field_A30 == 0)) {
            flag             = 0x80;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
    }
    func_actor_400500_80133B14(arg0);
    if (coord->coord.t[2] >= -0x225F) {
        work3            = (Actor400500Work*)arg0->work;
        work3->field_A0E = 0xA;
        work3->field_9F8 = 0x10;
        work3->field_9FE = 1;
        work3->field_9FA = 1;
        work->field_A08  = work->field_A08 + 1;
    }
}

void func_actor_400500_8013D3B8(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              flag;

    work = (Actor400500Work*)arg0->work;
    if ((u8)work->field_A4B == 3) {
        work->field_A04 = 0;
        work->field_A2C = 0x10;
        work2           = (Actor400500Work*)arg0->work;
        if ((work2->field_A46 >= 0) || (((u8)work2->field_A46 & 0x7F) != 1)) {
            flag             = 0x81;
            work2->field_A46 = flag;
            work2->field_A47 = 0;
        }
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_8013D420(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    s32              soundId;
    s32              pan;

    work = (Actor400500Work*)arg0->work;
    if ((s16)++work->field_A04 == 0x1E) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40050004;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((u8)work->field_A4B == 4) {
        work->field_A4C  = 0;
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0;
        work2->field_A08 = 0;
    }
}

void func_actor_400500_8013D4F0(Task* arg0)
{
    GpEnemy*         enemy;
    Actor400500Work* work;
    Actor400500Work* work2;
    Actor400500Work* work3;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        if (Gp_TickObjFlag2(enemy) != 0) {
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_9F8 = 0x10;
                work2->field_9FE = 0x10;
                work2->field_9FA = 2;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_9F8 = 0x10;
                work3->field_9FE = 0x12;
                work3->field_9FA = 2;
            }
            work->field_A08 = 2;
        }
    } else {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013D59C(Task* arg0)
{
    GpEnemy*            enemy;
    Actor400500Work*    work;
    Actor400500Work*    work2;
    Actor400500Work*    work3;
    Actor400500HitView* hit;
    s32                 cond;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor400500Work*)arg0->work;
    if (enemy->hp > 0) {
        hit = (Actor400500HitView*)work;
        if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_A4A = 0;
            if (!(work->field_A1E & 2)) {
                work2            = (Actor400500Work*)arg0->work;
                work2->field_A06 = 0;
                work2->field_A08 = 0;
            } else {
                work3            = (Actor400500Work*)arg0->work;
                work3->field_A06 = 0xA;
                work3->field_A08 = 0;
            }
        }
    } else {
        work->field_A42 = 0;
    }
}

void func_actor_400500_8013D630(Task* arg0)
{
    Actor400500HitView* hit;
    Actor400500Work*    work;
    GpEnemy*            enemy;
    s32                 cond;

    hit   = (Actor400500HitView*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond || (enemy->hp <= 0)) {
        work            = (Actor400500Work*)hit;
        work->field_A04 = 0;
        work->field_A18 = 0;
        work->field_A10 = 0;
        work->field_A12 = 0;
        work->field_A08 = work->field_A08 + 1;
    }
}

void func_actor_400500_8013D6A0(Task* arg0)
{
    Actor400500Work* work;
    Actor400500Work* work2;
    GpCoord*         coord;
    u16              step;
    u16              accum;
    s32              y;

    work  = (Actor400500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    SCHED_BARRIER();
    step              = (u16)work->field_A10 + 2;
    accum             = (u16)work->field_A12 + step;
    work->field_A12   = accum;
    work->field_A10   = step;
    y                 = coord->coord.t[1] + (s16)accum;
    coord->coord.t[1] = y;
    if (y >= -0x897) {
        coord->coord.t[1] = -0x898;
        work->field_A08   = work->field_A08 + 1;
        work2             = (Actor400500Work*)arg0->work;
        work2->field_9F8  = 0x10;
        work2->field_9FE  = 0x13;
        work2->field_9FA  = 2;
        work->field_94C   = (u16)work->field_94C + 0x800;
        coord->coord.t[1] = -0x3E8;
        work->field_A04   = 0;
        work->field_A1E   = work->field_A1E | 1;
    }
}

void func_actor_400500_8013D744(Task* arg0)
{
    Actor400500Work*    work;
    Actor400500HitView* hit;
    Actor400500Work*    work2;
    GpEnemy*            enemy;
    s32                 soundId;
    s32                 pan;
    s32                 cond;

    work  = (Actor400500Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    if ((s16)work->field_A04 == 0) {
        soundId = ((enemy->placeKey >> 0xC) << 8) | 0x40050006;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_A04 = work->field_A04 + 1;
    }
    hit = (Actor400500HitView*)arg0->work;
    if ((hit->flags_4C.half & 1) || (hit->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_A4A = 0;
        if (enemy->hp > 0) {
            work2            = (Actor400500Work*)arg0->work;
            work2->field_9F8 = 0x10;
            work2->field_9FE = 0x14;
            work2->field_9FA = 2;
            work->field_A08  = work->field_A08 + 1;
        } else {
            work->field_A42 = 0;
        }
    }
}

void func_actor_400500_8013D878(Task* arg0)
{
    Actor400500HitView* work;
    Actor400500Work*    work2;
    s32                 cond;

    work = (Actor400500HitView*)arg0->work;
    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor400500Work*)arg0->work;
        work2->field_A06 = 0xA;
        work2->field_A08 = 0;
    }
}

void func_actor_400500_8013D8CC(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    GpCoord*         coord;

    model            = arg0->extra.tmd;
    work             = (Actor400500Work*)arg0->work;
    coord            = model->coords;
    work->field_A02  = 0x1000;
    work->matrix_808 = coord->coord;
    Gp_SetLightMode(arg0->spawnArg2, 1);
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013D958(Task* arg0)
{
    Actor400500Work* work;
    TmdObject*       model;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    model           = arg0->extra.tmd;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x18) {
        work->field_A20 = 0;
        work->field_A24 = 0x1000;
        work->field_A28 = 0xFF;
        func_8009EA50(work->field_A20);
        model->lightLevel = work->field_A24;
        work->field_A04   = 0;
        work->field_A06   = work->field_A06 + 1;
    }
}

void func_actor_400500_8013D9DC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013D9F4(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model           = arg0->extra.tmd;
    work            = (Actor400500Work*)arg0->work;
    model->flags   |= 0x80;
    work->field_A04 = 0;
    work->field_A28 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DA24(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 2) {
        work->field_A06 = work->field_A06 + 1;
    }
}

void func_actor_400500_8013DA68(Task* arg0)
{
    TmdObject*       model;
    Actor400500Work* work;

    model = arg0->extra.tmd;
    work  = (Actor400500Work*)arg0->work;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_400500_80134B88(arg0);
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DACC(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    arg0->state     = 3;
    work->field_A06 = 0;
    work->field_A08 = 0;
}

void func_actor_400500_8013DAE4(Task* arg0, s32 arg1, u16* arg2)
{
    Actor400500Work* work;
    s32              kind;

    work = (Actor400500Work*)arg0->work;
    kind = arg2[1];
    switch (kind) {
        case 1:
            work->field_A4C = kind;
            work->field_A2C = 0x1E;
            work->field_A4B = kind;
            func_actor_400500_8013DB64(arg0, 0xB);
            break;
        case 2:
            work->field_A4B = kind;
            break;
        case 3:
            work->field_A4B = kind;
            break;
        case 4:
            work->field_A4B = kind;
            break;
    }
}

void func_actor_400500_8013DB64(Task* arg0, s16 arg1)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A06 = arg1;
    work->field_A08 = 0;
}

s32 func_actor_400500_8013DB78(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    if ((work->field_A16 < (0x640 - (work->field_9D8.vz * 8))) && ((u32)(work->field_A36 - 0x300) >= 0xA01U)) {
        work->field_A06 = 9;
        work->field_A08 = 0;
        return 1;
    }
    return 0;
}

void func_actor_400500_8013DBCC(Task* arg0, s16 arg1, Actor400500ViewPos* arg2)
{
    MATRIX   local;
    GpCoord* coord;

    coord = &arg0->extra.tmd->coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &local);
    arg2->x    = local.t[0];
    arg2->z    = local.t[2];
    coord->flg = 0;
}

void func_actor_400500_8013DC4C(Task* arg0)
{
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    i    = 1;
    do {
        work->slots[i].rate = (u8)work->field_9F8;
        Gp_AnimResetSlot(&work->anim, i, work->field_9FE);
        i++;
    } while (i < 0x12);
    work->field_9FC = work->field_9FE;
}

void func_actor_400500_8013DCBC(Task* arg0, s16 arg1, s16 arg2)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_9F8 = arg2;
    work->field_9FE = arg1;
    work->field_9FA = 2;
}

void func_actor_400500_8013DCD4(Task* arg0)
{
    s32              same;
    Actor400500Work* work;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    i    = 1;
    same = (s16)work->field_9FC == work->field_9FE;
    do {
        if (same) {
            do {
                work->slots[i].rate = (u8)work->field_9F8;
                i++;
            } while (i < 0x12);
        } else {
            do {
                work->slots[i].rate = (u8)work->field_9F8;
                func_800B4114(&work->anim, i, work->field_9FE, 0, work->field_A0E);
                i++;
            } while (i < 0x12);
            work->field_A0E = 0;
        }
    } while (0);
    work->field_9FC = work->field_9FE;
}

s32 func_actor_400500_8013DD8C(Task* arg0, s16 arg1)
{
    Actor400500Work* work;
    s16              scale;

    work  = (Actor400500Work*)arg0->work;
    scale = work->field_9F8;
    if (scale == 0) {
        return 0;
    }
    return (((arg1 << 0x10) >> 8) / scale << 0xC) >> 0x10;
}

s32 func_actor_400500_8013DDEC(Task* arg0)
{
    Actor400500HitView* work = (Actor400500HitView*)arg0->work;

    if ((work->flags_4C.half & 1) || (work->flags_4C.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone. The actor builds a part's rotation in a scratch matrix and pushes
/// it into the part's `GpCoord::coord` with this, so the part keeps its
/// position.
void func_actor_400500_8013DE2C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

/// Per-frame entry point of the actor's task: runs the handler its state
/// selects from `D_actor_400500_80131E4C` - set-up, the per-frame state
/// machine, the death sequence and the post-death timeout. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_400500_8013DE98(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_400500_80131E4C;
    states.funcs[task->state](task);
}

void func_actor_400500_8013DEFC(Task* arg0)
{
    Actor400500Work* work                = (Actor400500Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_400500_8013DF74,
        func_actor_400500_8013DFE4,
    };

    states[(s16)work->field_A06](arg0);
}

void func_actor_400500_8013DF50(Task* arg0)
{
    Actor400500Work* work = (Actor400500Work*)arg0->work;

    work->field_A3C = 0;
    work->field_A3E = 0;
}

void func_actor_400500_8013DF64(void)
{
}

void func_actor_400500_8013DF6C(void)
{
}

void func_actor_400500_8013DF74(Task* arg0)
{
    Actor400500Work* work;
    Task*            child;
    s32              i;

    work = (Actor400500Work*)arg0->work;
    for (i = 0; i < 2; i++) {
        child = work->field_9F0[i];
        if (child != NULL) {
            taskKill(child);
        }
    }
    work->field_A04 = 0;
    work->field_A06 = work->field_A06 + 1;
}

void func_actor_400500_8013DFE4(Task* arg0)
{
    Actor400500Work* work;
    u16              frame;

    work            = (Actor400500Work*)arg0->work;
    frame           = work->field_A04 + 1;
    work->field_A04 = frame;
    if ((s16)frame >= 0x12D) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}
