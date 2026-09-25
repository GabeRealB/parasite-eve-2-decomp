#include "common.h"

#include "psyq/abs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "actors/actor.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actors_shared_80163354.h"
#include "actors/actors_shared_801673f8.h"

/// Status flags at `Actor104400Work` + 0xEC, read through two widths.
///
/// Guards test bit 0 as a halfword and then bits 0x102 as a word
/// (`Actor04400_Fn06618` is the out-of-line copy of the test).
typedef union Actor104400Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor104400Flags;
STATIC_ASSERT_SIZEOF(Actor104400Flags, 0x4);

/// Per-actor state block for the `actor_104400` overlay's enemy.
///
/// `Actor04400_Fn00B24` and `Actor04400_Fn00D3C` both allocate it with
/// `memCalloc(0x454, 0)` and store it in the `Task::work` slot (0x1C), so
/// the size below is the allocation, not a guess: this actor reuses that
/// pointer field for its own work block and it is *not* a `TaskIdMap` here.
/// Reach it with `(Actor104400Work*)task->work`.
///
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// table walks, `field_412` is the per-state frame counter, and
/// `field_414` .. `field_426` are the animation request the actor hands to
/// its player. The three `GpObj` nodes are the collision objects
/// `Actor04400_Fn08A40` hands back to `Gp_UnlinkObj`. `obj_2AC` and
/// `obj_2CC` share `rec_2EC`; `obj_3AC` has its own table at `rec_3CC`.
typedef struct Actor104400Work {
    /* 0x000 */ MATRIX    matrix_0; // model root coord, copied out on the kill path
    /* 0x020 */ MATRIX    colorMtx; // the model's `TmdObject::colorMtx`
    /* 0x040 */ MATRIX    lightMtx; // the model's `TmdObject::lightMtx`
    /* 0x060 */ VECTOR    field_60; // position Actor04400_Fn022A8 snaps the root back to when blocked
    /* 0x070 */ SVECTOR   field_70; // origin of slot 4 entry 0's coords[3], carried into view space by Actor04400_Fn05B08
    /* 0x078 */ s16       field_78; // pitch, fed to RotMatrixX
    /* 0x07A */ s16       field_7A; // heading
    /* 0x07C */ s16       field_7C; // roll, fed to RotMatrixZ
    /* 0x07E */ byte      pad_7E[0x2];
    /* 0x080 */ u16       field_80; // spawn position: root coord.t[0]
    /* 0x082 */ u16       field_82; // root coord.t[1], after lifting it by 0x3C
    /* 0x084 */ u16       field_84; // root coord.t[2]
    /* 0x086 */ byte      pad_86[0x2];
    /* 0x088 */ s16       field_88; // x of the vector turned towards
    /* 0x08A */ s16       field_8A;
    /* 0x08C */ s16       field_8C; // z of the vector turned towards
    /* 0x08E */ byte      pad_8E[0x2];
    /* 0x090 */ u16       field_90; // root coord.t[0], snapshotted with field_92 / field_94
    /* 0x092 */ u16       field_92; // root coord.t[1]
    /* 0x094 */ u16       field_94; // root coord.t[2]
    /* 0x096 */ byte      pad_96[0x2];
    /* 0x098 */ SVECTOR   field_98; // translation of coords[6] relative to the view
    /* 0x0A0 */ GpAnimCtx anim;
    /// First of the nine `GpAnimSlot`s (0xB4..0x21C); the second overlaps
    /// `flags_EC`, so only the first is spelled out.
    /* 0x0B4 */ GpAnimSlot       slot_B4;
    /* 0x0DC */ byte             pad_DC[0x10];
    /* 0x0EC */ Actor104400Flags flags_EC;
    /* 0x0F0 */ byte             pad_F0[0x12C];
    /* 0x21C */ byte             field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    /* 0x2AC */ GpObj            obj_2AC;
    /* 0x2CC */ GpObj            obj_2CC;
    /* 0x2EC */ GpRec18          rec_2EC[8];
    /* 0x3AC */ GpObj            obj_3AC;
    /* 0x3CC */ GpRec18          rec_3CC[2];
    /* 0x3FC */ GpEffArg         eff_3FC;   // field_0 is the model's second coord part
    /* 0x404 */ byte             pad_404[0x8];
    /* 0x40C */ s16              field_40C; // heading Actor04400_Fn017B0 moves the root along
    /* 0x40E */ s16              field_40E; // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    /* 0x410 */ s16              field_410; // random 0..0x7FF drawn from `Gp_LcgState`
    /* 0x412 */ u16              field_412; // per-state frame counter
    /* 0x414 */ s16              field_414; // animation request kind
    /* 0x416 */ s16              field_416; // animation id last applied to the slots
    /* 0x418 */ s16              field_418; // animation id
    /* 0x41A */ u16              field_41A; // frames since the animation was applied
    /* 0x41C */ s16              field_41C; // animation speed / step scale
    /* 0x41E */ s16              field_41E;
    /* 0x420 */ u16              field_420; // state index
    /* 0x422 */ u16              field_422; // sub-state index
    /* 0x424 */ s16              field_424; // yaw added to model parts 3..5, a third each
    /* 0x426 */ s16              field_426;
    /* 0x428 */ s16              field_428;
    /* 0x42A */ s16              field_42A;
    /* 0x42C */ s16              field_42C; // frames spent turning toward field_444; 16 enters state 3
    /* 0x42E */ byte             pad_42E[0x2];
    /* 0x430 */ s16              field_430;
    /* 0x432 */ s16              field_432; // 1 runs Actor04400_Fn06520 on the spawn position
    /* 0x434 */ s16              field_434; // pitch, eased back to zero while falling
    /* 0x436 */ s16              field_436; // step picked from `field_43A`'s distance band
    /* 0x438 */ s16              field_438; // 1 on the death path
    /* 0x43A */ s16              field_43A; // distance to the nearer player actor
    /* 0x43C */ byte             pad_43C[0x2];
    /* 0x43E */ s16              field_43E; // counted down each frame by Actor04400_Fn022A8
    /* 0x440 */ s16              field_440; // picks animation 5 (zero) or 6 after animation 8
    /* 0x442 */ u16              field_442;
    /* 0x444 */ u16              field_444; // heading to the nearer player actor, relative to field_7A
    /* 0x446 */ s16              field_446; // randomised hold compared against field_412
    /* 0x448 */ s16              field_448;
    /* 0x44A */ s16              field_44A;
    /* 0x44C */ u16              field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    /* 0x44E */ u8               field_44E; // set while the enemy carries status flag 4/8
    /* 0x44F */ u8               field_44F;
    /* 0x450 */ byte             pad_450[0x1];
    /* 0x451 */ u8               field_451; // 1 skips Actor04400_Fn00220 part-pair colour
    /* 0x452 */ byte             pad_452[0x2];
} Actor104400Work;
STATIC_ASSERT_SIZEOF(Actor104400Work, 0x454);

/// Payload the sender of message 0x2C00 passes as `Gp_DispatchMsg`'s `arg2`.
/// The overlay's 0x2C00 handler, `Actor04400_Fn0648C`, tests the id at 0x0 and
/// stores the halfword at 0x2 in `Actor104400Work::field_44C`.
typedef struct Actor104400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor104400Msg;
STATIC_ASSERT_SIZEOF(Actor104400Msg, 0x4);

extern u8                   Actor04400_D10814[]; // per animation id (1-based): the value to put in `field_44F`
extern u8                   Actor04400_D10828[]; // per animation id (1-based): the animation to follow it
extern GpPairSrcE           Actor04400_D0D318;   // the main enemy's `GpEnemy::param` record
extern u8                   Actor04400_D10778[]; // animation bank handed to `func_800B3F84`
extern u8                   Actor04400_D107CC[]; // stored into `Task::msgTable` by Actor04400_Fn00B24
extern const TaskFuncTable3 Actor04400_D00070;   // dispatcher table Actor04400_Fn06ACC copies onto its stack
extern const TaskFuncTable3 Actor04400_D0007C;   // dispatcher table Actor04400_Fn06870 copies onto its stack
extern const TaskFuncTable5 Actor04400_D00088;   // dispatcher table Actor04400_Fn068F8 copies onto its stack
extern const TaskFuncTable5 Actor04400_D0009C;   // dispatcher table Actor04400_Fn06964 copies onto its stack
extern const TaskFuncTable3 Actor04400_D00150;   // dispatcher table Actor04400_Fn07CF0 copies onto its stack
extern const TaskFuncTable3 Actor04400_D0015C;   // dispatcher table Actor04400_Fn07D78 copies onto its stack
extern const TaskFuncTable4 Actor04400_D00174;   // dispatcher table Actor04400_Fn07F04 copies onto its stack
extern const TaskFuncTable6 Actor04400_D001AC;   // dispatcher table Actor04400_Fn06B50 copies onto its stack

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits.
void func_8004BFF8(s32 angle, MATRIX* matrix);

void Actor04400_Fn00B24(Task* arg0);
void Actor04400_Fn00D3C(Task* arg0);
void Actor04400_Fn00F7C(Task* arg0);
void Actor04400_Fn01418(Task* arg0);
void Actor04400_Fn01584(Task* arg0);
void Actor04400_Fn017B0(Task* arg0);
void Actor04400_Fn01B70(Task* arg0);
void Actor04400_Fn01CA0(Task* arg0);
void Actor04400_Fn01E08(Task* arg0);
void Actor04400_Fn02008(Task* arg0);
void Actor04400_Fn0216C(Task* arg0);
void Actor04400_Fn022A8(Task* arg0, s16 arg1);
void Actor04400_Fn02B8C(Task* arg0);
void Actor04400_Fn02D18(Task* arg0);
void Actor04400_Fn02E8C(Task* arg0);
void Actor04400_Fn0304C(Task* arg0);
void Actor04400_Fn031B8(Task* arg0);
void Actor04400_Fn03538(Task* arg0);
void Actor04400_Fn039EC(Task* arg0);
void Actor04400_Fn03B34(Task* arg0);
void Actor04400_Fn03CA0(Task* arg0);
void Actor04400_Fn03E20(Task* arg0);
void Actor04400_Fn03F8C(Task* arg0);
void Actor04400_Fn042C4(Task* arg0);
void Actor04400_Fn045A0(Task* arg0);
void Actor04400_Fn04718(Task* arg0);
void Actor04400_Fn048A0(Task* arg0);
void Actor04400_Fn04A3C(Task* arg0);
void Actor04400_Fn04BA8(Task* arg0);
void Actor04400_Fn04D44(Task* arg0);
void Actor04400_Fn04EDC(Task* arg0);
void Actor04400_Fn05040(Task* arg0);
void Actor04400_Fn05260(Task* arg0);
void Actor04400_Fn053FC(Task* arg0);
void Actor04400_Fn058F4(Task* arg0);
void Actor04400_Fn05A40(Task* arg0);
void Actor04400_Fn05DE0(Task* arg0);
void Actor04400_Fn05FC8(Task* arg0);
void Actor04400_Fn061B4(void);
void Actor04400_Fn062D4(Task* arg0);
s16  Actor04400_Fn06328(Task* arg0);
void Actor04400_Fn06374(Task* arg0, s32 arg1);
s32  Actor04400_Fn063E4(Task* arg0);
void Actor04400_Fn06520(Task* arg0, s16 part, SVECTOR3* pos);
s32  Actor04400_Fn065F4(Task* arg0, s16 value);
s16  Actor04400_Fn06618(Task* arg0);
void Actor04400_Fn0674C(Task* arg0);
void Actor04400_Fn067A0(Task* arg0, s32 step);
void Actor04400_Fn06834(Task* arg0);
void Actor04400_Fn06848(Task* arg0);
void Actor04400_Fn0685C(Task* arg0);
void Actor04400_Fn06870(Task* arg0);
void Actor04400_Fn068F8(Task* arg0);
void Actor04400_Fn06964(Task* arg0);
void Actor04400_Fn069D0(Task* arg0);
void Actor04400_Fn06A24(Task* arg0);
void Actor04400_Fn06A78(Task* arg0);
void Actor04400_Fn06ACC(Task* arg0);
void Actor04400_Fn06B50(Task* arg0);
void Actor04400_Fn06BC4(Task* arg0);
void Actor04400_Fn06BF8(Task* arg0);
void Actor04400_Fn06C70(Task* arg0);
void Actor04400_Fn06CF0(Task* arg0);
void Actor04400_Fn06D90(Task* arg0);
void Actor04400_Fn06DFC(Task* arg0);
void Actor04400_Fn06EEC(Task* arg0);
void Actor04400_Fn06F50(Task* arg0);
void Actor04400_Fn07050(Task* arg0);
void Actor04400_Fn0710C(Task* arg0);
void Actor04400_Fn0714C(Task* arg0);
void Actor04400_Fn071C8(Task* arg0);
void Actor04400_Fn0723C(Task* arg0);
void Actor04400_Fn07360(Task* arg0);
void Actor04400_Fn073C8(Task* arg0);
void Actor04400_Fn07404(Task* arg0);
void Actor04400_Fn07530(Task* arg0);
void Actor04400_Fn075F0(Task* arg0);
void Actor04400_Fn076D0(Task* arg0);
void Actor04400_Fn07750(Task* arg0);
void Actor04400_Fn0781C(Task* arg0);
void Actor04400_Fn07878(Task* arg0);
void Actor04400_Fn07890(Task* arg0);
void Actor04400_Fn078D4(Task* arg0);
void Actor04400_Fn07968(Task* arg0);
void Actor04400_Fn07984(Task* arg0);
void Actor04400_Fn07A38(Task* arg0);
void Actor04400_Fn07B4C(Task* arg0);
void Actor04400_Fn07C60(Task* arg0);
void Actor04400_Fn07CF0(Task* arg0);
void Actor04400_Fn07D78(Task* arg0);
void Actor04400_Fn07E00(Task* arg0);
void Actor04400_Fn07E74(Task* arg0);
void Actor04400_Fn07F04(Task* arg0);
void Actor04400_Fn07F6C(Task* arg0);
void Actor04400_Fn07FD0(Task* arg0);
void Actor04400_Fn08094(Task* arg0);
void Actor04400_Fn080E8(Task* arg0);
void Actor04400_Fn08160(Task* arg0);
void Actor04400_Fn08208(Task* arg0);
void Actor04400_Fn0823C(Task* arg0);
void Actor04400_Fn08290(Task* arg0);
void Actor04400_Fn082E0(Task* arg0);
void Actor04400_Fn08358(Task* arg0);
void Actor04400_Fn083CC(Task* arg0);
void Actor04400_Fn0847C(Task* arg0);
void Actor04400_Fn08610(Task* arg0);
void Actor04400_Fn08718(Task* arg0);
void Actor04400_Fn087E0(Task* arg0);
void Actor04400_Fn08870(Task* arg0);
void Actor04400_Fn08908(Task* arg0);
void Actor04400_Fn089C0(Task* arg0);
void Actor04400_Fn08A40(Task* arg0);
void Actor04400_Fn08A9C(Task* arg0);
void Actor04400_Fn08AA4(Task* arg0);
void Actor04400_Fn08B3C(Task* arg0);
void Actor04400_Fn08C08(Task* arg0);
void Actor04400_Fn08C64(Task* arg0);
void Actor04400_Fn08DA4(Task* arg0);
s32  Actor04400_Fn08DBC(Task* arg0);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s8 D_80115415; // absolute; set once CD command 0x21 is queued

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. Declared as a one-element array so GCC 2.8.1
 * cannot treat the store as a non-aliasing scalar and sink it past the
 * `TmdObject` loads. */
extern void* D_800678F0[1];
extern u8    Actor04400_D098FC[];
extern u8    Actor04400_D09FA0[];
extern u8    Actor04400_D0A510[];

/// Puts the task in `state` with its work block's state machine reset to 0/0.
static __inline__ void Actor04400_SetTaskState(Task* task, s32 state)
{
    Actor104400Work* work = (Actor104400Work*)task->work;

    task->state     = state;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Jumps the work block's state machine to `state`, sub-state 0, leaving the
/// task's own state alone.
static __inline__ void Actor04400_SetWorkState(Task* task, s16 state)
{
    Actor104400Work* work = (Actor104400Work*)task->work;

    work->field_420 = state;
    work->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world position through a 0x10-byte `VECTOR`
/// taken off `G_SCRATCH_HEAD`.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 Actor04400_TakeHit(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            Actor04400_SetTaskState(arg0, 3);
            w2            = (Actor104400Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        Actor04400_SetTaskState(arg0, 7);
        return 1;
    }
    return 0;
}

/// A narrower `Actor04400_TakeHit`: only low nibble 3 of
/// message 0x2C00 counts, consuming it into task state 7 with a fresh state
/// machine. Returns 1 when it did, so the caller skips this frame's handler.
static __inline__ s16 Actor04400_TakeHit3(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              hit  = 0;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor104400Work*)arg0->work;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// While `field_41E` is 1, consumes the request in `field_448` (1..5 jump to
/// states 6, 7, 8, 7, 9) and returns 1; otherwise returns 0. The inlined form
/// of `Actor04400_Fn063E4`.
static __inline__ s32 Actor04400_TakeRequest(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                Actor04400_SetWorkState(arg0, 6);
                break;
            case 1:
                Actor04400_SetWorkState(arg0, 7);
                break;
            case 2:
                Actor04400_SetWorkState(arg0, 8);
                break;
            case 3:
                Actor04400_SetWorkState(arg0, 7);
                break;
            case 4:
                Actor04400_SetWorkState(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Whether `flags_EC` reports a hit: bit 0 as a halfword, or 0x102 as a word.
/// The inlined form of `Actor04400_Fn06618`.
static __inline__ s32 Actor04400_IsHit(Task* arg0)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    if ((w->flags_EC.half & 1) || (w->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`.
static __inline__ void Actor04400_UpdateRotation(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          dst;

    work->field_78           &= 0xFFF;
    work->field_7A           &= 0xFFF;
    work->field_7C           &= 0xFFF;
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    func_8004BFF8(work->field_7A, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 Actor04400_PickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`depth`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void Actor04400_CalcPush(Task* arg0, GsCOORDINATE2* coord, GpRec18* rec, SVECTOR* out)
{
    SVECTOR        pos;
    VECTOR         d;
    VECTOR         n;
    GsCOORDINATE2* c2;
    s32            t;
    s32            pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = ((TmdObject*)arg0->extra)->coords;
    d.vx   = pos.vx - rec->point.vx;
    d.vy   = 0;
    d.vz   = pos.vz - rec->point.vz;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->depth - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->point.vx;
    d.vy = c2->workm.t[1] - rec->point.vy;
    d.vz = c2->workm.t[2] - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Task-state handlers of the first enemy form, dispatched by
/// `Actor04400_Fn066DC` on `Task::state`.
const TaskFuncTable6 Actor04400_D00004 = { {
    Actor04400_Fn00B24,
    Actor04400_Fn03538,
    Actor04400_Fn01E08,
    Actor04400_Fn00F7C,
    Actor04400_Fn02E8C,
    Actor04400_Fn0674C,
} };

/// Task-state handlers of the second enemy form, dispatched by
/// `Actor04400_Fn06658` on `Task::state`.
const TaskFuncTable10 Actor04400_D0001C = { {
    Actor04400_Fn00D3C,
    Actor04400_Fn03538,
    Actor04400_Fn01E08,
    Actor04400_Fn00F7C,
    Actor04400_Fn02E8C,
    Actor04400_Fn0674C,
    Actor04400_Fn03F8C,
    Actor04400_Fn062D4,
    Actor04400_Fn05DE0,
    Actor04400_Fn05FC8,
} };

/// State handlers `Actor04400_Fn00F7C` dispatches by `field_420`.
const TaskFuncTable11 Actor04400_D00044 = { {
    Actor04400_Fn06834,
    Actor04400_Fn06848,
    Actor04400_Fn0685C,
    Actor04400_Fn06870,
    Actor04400_Fn068F8,
    Actor04400_Fn06964,
    Actor04400_Fn069D0,
    Actor04400_Fn06A24,
    Actor04400_Fn06A78,
    Actor04400_Fn06ACC,
    Actor04400_Fn06B50,
} };

/// Sub-state handlers `Actor04400_Fn06ACC` dispatches by `field_422`.
const TaskFuncTable3 Actor04400_D00070 = { {
    Actor04400_Fn06C70,
    Actor04400_Fn06CF0,
    Actor04400_Fn06D90,
} };

/// Sub-state handlers `Actor04400_Fn06870` dispatches by `field_422`.
const TaskFuncTable3 Actor04400_D0007C = { {
    Actor04400_Fn01418,
    Actor04400_Fn01584,
    Actor04400_Fn06DFC,
} };

/// Sub-state handlers `Actor04400_Fn068F8` dispatches by `field_422`.
const TaskFuncTable5 Actor04400_D00088 = { {
    Actor04400_Fn06EEC,
    Actor04400_Fn017B0,
    Actor04400_Fn01B70,
    Actor04400_Fn01CA0,
    Actor04400_Fn06F50,
} };

/// Sub-state handlers `Actor04400_Fn06964` dispatches by `field_422`.
const TaskFuncTable5 Actor04400_D0009C = { {
    Actor04400_Fn07050,
    Actor04400_Fn0710C,
    Actor04400_Fn0714C,
    Actor04400_Fn071C8,
    Actor04400_Fn0723C,
} };

/// Sub-state handlers `Actor04400_Fn07360` dispatches by `field_422`.
const TaskFuncTable4 Actor04400_D000B0 = { {
    Actor04400_Fn073C8,
    Actor04400_Fn07404,
    Actor04400_Fn02008,
    Actor04400_Fn0216C,
} };

/// Draws a semi-transparent textured quad between model parts `firstJoint`
/// and `secondJoint`, `width` either side at height `height`, shaded grey
/// `shade`.
void Actor04400_Fn00220(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade)
{
    ActorsShared80163354Scratch* s;
    s16                          angle;
    GsCOORDINATE2*               secondCoord;
    GsCOORDINATE2*               firstCoord;
    s32                          offset0;
    s32                          offset1;
    s32                          offset2;
    s32                          offset3;
    GsCOORDINATE2*               coords;
    POLY_FT4*                    poly;

    coords      = ((TmdObject*)task->extra)->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorsShared80163354Scratch*)(*(u8**)G_SCRATCH_HEAD -= sizeof(ActorsShared80163354Scratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy       = (s16)height;
        s->second.vy      = (s16)height;
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vz       = s->firstMatrix.t[2];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vz      = s->secondMatrix.t[2];
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        s->halfX          = (s->first.vx - s->second.vx) / 2;
        s->halfZ          = (s->first.vz - s->second.vz) / 2;
        offset0           = rcos(angle) * width;
        s->corner0.vy     = (s16)height;
        s->corner0.vx     = s->halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz     = s->halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1           = rcos(angle) * width;
        s->corner1.vy     = (s16)height;
        s->corner1.vx     = s->halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz     = s->halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2           = rcos(angle) * width;
        s->corner2.vy     = (s16)height;
        s->corner2.vx     = (s->second.vx - (offset2 >> 0xC)) - s->halfX;
        s->corner2.vz     = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        offset3           = rcos(angle) * width;
        s->corner3.vy     = (s16)height;
        s->corner3.vx     = (s->second.vx + (offset3 >> 0xC)) - s->halfX;
        s->corner3.vz     = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - s->halfZ;
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
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
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(ActorsShared80163354Scratch);
    }
}

void Actor04400_Fn006A8(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = Actor04400_D098FC;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[6], 0x200, NULL);
    if (eff != NULL) {
        src        = (TmdObject*)arg0->extra;
        dst        = (TmdObject*)eff->task->extra;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = Actor04400_D09FA0;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[8], 0x200, NULL);
    } else {
        D_800678F0[0] = Actor04400_D0A510;
        eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2        = (TmdObject*)arg0->extra;
        dst2        = (TmdObject*)eff2->task->extra;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)[4], 0x200, NULL);
}

/// Turns model parts 5, 4 and 3 about Y by a third of `field_424` each: reads
/// each part's rotation back as Euler angles, adds to the yaw, rebuilds the
/// 3x3 and marks the coordinate dirty.
void Actor04400_Fn00874(Task* arg0)
{
    SVECTOR          rot;
    ActorMat         mtx;
    ActorMatWords*   ident;
    Actor104400Work* work;
    GsCOORDINATE2*   coords;
    MATRIX*          m5;
    MATRIX*          m4;
    MATRIX*          m3;

    work   = (Actor104400Work*)arg0->work;
    ident  = &mtx.ident;
    coords = ((TmdObject*)arg0->extra)->coords;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m5                = &coords[5].coord;
    Gp_MtxToEuler(m5, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m5->m[0][0]   = (u16)mtx.mat.m[0][0];
    m5->m[0][1]   = (u16)mtx.mat.m[0][1];
    m5->m[0][2]   = (u16)mtx.mat.m[0][2];
    m5->m[1][0]   = (u16)mtx.mat.m[1][0];
    m5->m[1][1]   = (u16)mtx.mat.m[1][1];
    m5->m[1][2]   = (u16)mtx.mat.m[1][2];
    m5->m[2][0]   = (u16)mtx.mat.m[2][0];
    m5->m[2][1]   = (u16)mtx.mat.m[2][1];
    m5->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[5].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m4                = &coords[4].coord;
    Gp_MtxToEuler(m4, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m4->m[0][0]   = (u16)mtx.mat.m[0][0];
    m4->m[0][1]   = (u16)mtx.mat.m[0][1];
    m4->m[0][2]   = (u16)mtx.mat.m[0][2];
    m4->m[1][0]   = (u16)mtx.mat.m[1][0];
    m4->m[1][1]   = (u16)mtx.mat.m[1][1];
    m4->m[1][2]   = (u16)mtx.mat.m[1][2];
    m4->m[2][0]   = (u16)mtx.mat.m[2][0];
    m4->m[2][1]   = (u16)mtx.mat.m[2][1];
    m4->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[4].flg = 0;

    mtx.ident.m00_m01 = 0x1000;
    mtx.ident.m02_m10 = 0;
    ident->m11_m12    = 0x1000;
    mtx.ident.m20_m21 = 0;
    ident->m22        = 0x1000;
    m3                = &coords[3].coord;
    Gp_MtxToEuler(m3, &rot);
    rot.vy = (u16)rot.vy + work->field_424 / 3;
    RotMatrix(&rot, &mtx.mat);
    m3->m[0][0]   = (u16)mtx.mat.m[0][0];
    m3->m[0][1]   = (u16)mtx.mat.m[0][1];
    m3->m[0][2]   = (u16)mtx.mat.m[0][2];
    m3->m[1][0]   = (u16)mtx.mat.m[1][0];
    m3->m[1][1]   = (u16)mtx.mat.m[1][1];
    m3->m[1][2]   = (u16)mtx.mat.m[1][2];
    m3->m[2][0]   = (u16)mtx.mat.m[2][0];
    m3->m[2][1]   = (u16)mtx.mat.m[2][1];
    m3->m[2][2]   = (u16)mtx.mat.m[2][2];
    coords[3].flg = 0;
}

/// Main enemy init. Allocates the 0x454-byte `Actor104400Work`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// `one` is a separate variable set before `Gp_IncStateF0Ref`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
void Actor04400_Fn00B24(Task* arg0)
{
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor104400Work* work;
    TmdObject*       obj;
    Actor104400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor104400Work* w2;
    Actor104400Work* w3;
    Actor104400Work* w4;
    s32              one;

    enemy      = arg0->spawnArg2;
    root       = ((TmdObject*)arg0->extra)->coords;
    arg0->work = memCalloc(0x454, 0);
    work       = (Actor104400Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    Actor04400_Fn061B4();
    obj                   = arg0->extra;
    w                     = (Actor104400Work*)arg0->work;
    e                     = arg0->spawnArg2;
    coord                 = obj->coords;
    arg0->msgTable        = Actor04400_D107CC;
    obj->lightMtx         = &w->lightMtx;
    obj->colorMtx         = &w->colorMtx;
    e->param              = &Actor04400_D0D318;
    e->recs               = w->rec_2EC;
    w->eff_3FC.coord      = &((TmdObject*)arg0->extra)->coords[1];
    w->eff_3FC.spawnArgLo = 0x140;
    w->eff_3FC.spawnArgHi = 2;
    e->hp = e->hpMax = Actor04400_D0D318.hpMax;
    func_800B3F84(&w->anim, Actor04400_D10778, obj, w->field_21C, &w->slot_B4);
    w2            = (Actor104400Work*)arg0->work;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = 2;
    Actor04400_Fn02B8C(arg0);
    coord->sub = &gGfxViewCoord;
    Actor04400_Fn02D18(arg0);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    enemy       = arg0->spawnArg2;
    Gp_LinkNode(&enemy->node);
    enemy->field_4    = &((TmdObject*)arg0->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)arg0->extra)->coords[1];
    enemy->node.flags = 4;
    one               = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((arg0->spawnArg1 & 0xF) == one) {
        w3            = (Actor104400Work*)arg0->work;
        arg0->state   = 2;
        w3->field_420 = 0;
        w3->field_422 = 0;
    } else {
        w4            = (Actor104400Work*)arg0->work;
        arg0->state   = one;
        w4->field_420 = 0;
        w4->field_422 = 0;
    }
    work->field_80    = root->coord.t[0];
    root->coord.t[1] -= 0x3C;
    work->field_82    = root->coord.t[1];
    work->field_84    = root->coord.t[2];
}

/// Variant of `Actor04400_Fn00B24`'s init: also destroys the enemy when bit 16
/// of `spawnArg1` is set, sets bit 0x80 of the model's `field_C` for spawn
/// kind 2, and enters state 6 with `field_451` set and the collision flags
/// 0x8000 / 0x4000 cleared on `obj_2AC` / `obj_2CC`.
///
/// `two` is a variable for the same reason as `one` in `Actor04400_Fn00B24`: the ROM
/// holds the constant in `$s5` across the calls. `kind` has to be its own
/// variable too - masking `flags` in place reuses `$v1` for the result.
void Actor04400_Fn00D3C(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GsCOORDINATE2*   root;
    Actor104400Work* work;
    TmdObject*       obj;
    Actor104400Work* w;
    GpEnemy*         e;
    GsCOORDINATE2*   coord;
    Actor104400Work* w2;
    Actor104400Work* w3;
    GpEnemy*         e2;
    s32              flags;
    s32              kind;
    s32              two;

    model      = arg0->extra;
    enemy      = arg0->spawnArg2;
    root       = model->coords;
    arg0->work = memCalloc(0x454, 0);
    work       = (Actor104400Work*)arg0->work;
    if (work == NULL) {
        goto destroy;
    }
    Actor04400_Fn061B4();
    flags = arg0->spawnArg1;
    if ((flags >> 16) & 1) {
    destroy:
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    kind = flags & 0xF;
    two  = 2;
    if (kind == two) {
        model->flags |= 0x80;
    }
    obj                   = arg0->extra;
    w                     = (Actor104400Work*)arg0->work;
    e                     = arg0->spawnArg2;
    coord                 = obj->coords;
    arg0->msgTable        = Actor04400_D107CC;
    obj->lightMtx         = &w->lightMtx;
    obj->colorMtx         = &w->colorMtx;
    e->param              = &Actor04400_D0D318;
    e->recs               = w->rec_2EC;
    w->eff_3FC.coord      = &((TmdObject*)arg0->extra)->coords[1];
    w->eff_3FC.spawnArgLo = 0x140;
    w->eff_3FC.spawnArgHi = two;
    e->hp = e->hpMax = Actor04400_D0D318.hpMax;
    func_800B3F84(&w->anim, Actor04400_D10778, obj, w->field_21C, &w->slot_B4);
    w2            = (Actor104400Work*)arg0->work;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    Actor04400_Fn02B8C(arg0);
    coord->sub = &gGfxViewCoord;
    Actor04400_Fn02D18(arg0);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    e2 = arg0->spawnArg2;
    Gp_LinkNode(&e2->node);
    e2->field_4          = &((TmdObject*)arg0->extra)->coords->coord;
    e2->field_48         = 0;
    e2->bodyPos.vx       = 0;
    e2->bodyPos.vy       = 0;
    e2->bodyPos.vz       = 0;
    e2->coord            = &((TmdObject*)arg0->extra)->coords[1];
    e2->node.flags       = 1;
    work->field_80       = root->coord.t[0];
    root->coord.t[1]    -= 0x3C;
    work->field_82       = root->coord.t[1];
    work->field_84       = root->coord.t[2];
    work->field_451      = 1;
    work->obj_2AC.flags &= 0x7FFF;
    work->obj_2CC.flags &= 0xBFFF;
    w3                   = (Actor104400Work*)arg0->work;
    arg0->state          = 6;
    w3->field_420        = 0;
    w3->field_422        = 0;
}

/// Per-frame callback for the main enemy. In mode 0 it aims at the nearest actor (`Actor04400_Fn031B8`), lets
/// a pending hit (`Actor04400_TakeHit`) replace the state handler, eases
/// `field_424` toward zero, rebuilds the root rotation, and then picks the
/// next state: the `field_448` request once dead, state 4 when dead, 8 / 9 for
/// messages 4 / 5 while `field_438` is clear.
void Actor04400_Fn00F7C(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable11  sp    = Actor04400_D00044;
    s32              cur;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            Actor04400_Fn031B8(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            Actor04400_Fn00874(arg0);
            if (work->field_432 == 1) {
                Actor04400_Fn06520(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                Actor04400_SetTaskState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                Actor04400_SetTaskState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                Actor04400_SetTaskState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                Actor04400_SetTaskState(arg0, 9);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

/// Sub-state handler: requests animation 7 (kind 1, speed 0x10, `field_426`
/// 8), advances the sub-state and plays sound 0x402C0001 at the enemy's pan
/// and depth. Draws a random 0..0x7FF into `field_410`, then picks the
/// animation speed `field_41C` and the step `field_436` from the band the
/// distance `field_43A` falls in (under 1000, then per 1000 up to 5000).
void Actor04400_Fn01418(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;
    s16              step;

    work            = (Actor104400Work*)arg0->work;
    work->field_426 = 8;
    work->field_418 = 7;
    work->field_41C = 0x10;
    work->field_414 = 1;
    work->field_422++;
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0001;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_410 = (Gp_LcgState >> 0x10) & 0x7FF;
    if (work->field_43A < 1000) {
        work->field_41C = 0x10;
        work->field_436 = 0x10;
        return;
    }
    if (work->field_43A < 2000) {
        work->field_41C = 0x14;
        step            = 0x12;
    } else if (work->field_43A < 3000) {
        work->field_41C = 0x18;
        step            = 0x14;
    } else if (work->field_43A < 4000) {
        work->field_41C = 0x1C;
        step            = 0x16;
    } else if (work->field_43A < 5000) {
        work->field_41C = 0x20;
        step            = 0x18;
    } else {
        work->field_41C = 0x40;
        step            = 0x20;
    }
    work->field_436 = step;
}

void Actor04400_Fn01584(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              dist;
    s16              limit;
    s16              step;
    s16              angle;
    s16              speed;
    s32              soundId;
    s32              pan;

    dist = work->field_43A;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->field_41C < limit) {
        work->field_41C = limit;
        work->field_436 = step;
    }
    Actor04400_Fn067A0(arg0, work->field_436);
    speed                                          = Actor04400_Fn065F4(arg0, -0x10);
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0001;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (work->field_43A < work->field_410 + 2000 && (work->field_43A < 1500 || work->field_44A == 0) &&
        (u16)(((work->field_444 + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->field_422++;
    }
}

/// Sub-state handler of a leap, counting frames in `field_412`. Before frame 40
/// a consumed request (`Actor04400_Fn063E4`) ends it; from then on
/// `field_438` is set. Frame 43 snapshots part 6's view-space translation into
/// `field_98`, where frames 43..46 hold that part (`field_432`); frame 45
/// picks the leap heading `field_40C` (towards the target when it is roughly
/// behind, the current heading otherwise), frame 46 plays sound 0x402C0005,
/// and frames 45..53 move the root 250 a frame against that heading with
/// `obj_3AC` armed. Within 0x171 of the target on frames 45..48 it requests
/// animation 0x10 and skips a sub-state; from frame 47 the root falls under
/// `field_428` / `field_42A` until it is back at `field_92`, which requests
/// animation 0x12 and advances the sub-state.
void Actor04400_Fn017B0(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   root = ((TmdObject*)arg0->extra)->coords;
    MATRIX           local;
    s16              angle;
    s32              soundId;
    s32              pan;
    s16              facing;
    s16              speed;

    if ((s16)++work->field_412 < 40) {
        if ((s16)Actor04400_Fn063E4(arg0)) {
            return;
        }
    } else {
        work->field_438 = 1;
    }
    if ((s16)work->field_412 == 43) {
        GsCOORDINATE2* coords = ((TmdObject*)arg0->extra)->coords;
        SVECTOR*       v;

        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        coords[6].flg = 0;
        Gp_UpdateCoord(&coords[6]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[6].workm, &local);
        v             = &work->field_98;
        v->vx         = local.t[0];
        v->vy         = local.t[1];
        v->vz         = local.t[2];
        coords[6].flg = 0;
    }
    if (work->field_412 >= 43 && work->field_412 <= 46) {
        work->field_432 = 1;
    } else {
        work->field_432 = 0;
    }
    if ((s16)work->field_412 == 46) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0005;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_412 == 45) {
        facing = (work->field_444 + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else {
            work->field_40C = work->field_7A;
        }
    }
    if (work->field_412 >= 45 && work->field_412 <= 53) {
        angle                                          = work->field_40C;
        speed                                          = -250;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
        work->obj_3AC.flags                           |= 0x8000;
    } else {
        work->obj_3AC.flags &= 0x7FFF;
    }
    if (work->field_412 >= 45 && work->field_412 <= 48 && work->field_43A < 0x171) {
        Actor104400Work* w;

        work->field_428      = 0;
        work->field_42A      = -200;
        w                    = (Actor104400Work*)arg0->work;
        w->field_426         = 2;
        w->field_41C         = 0x10;
        w->field_418         = 0x10;
        w->field_414         = 1;
        work->field_432      = 0;
        work->obj_3AC.flags &= 0x7FFF;
        work->field_422     += 2;
        return;
    }
    if ((s16)work->field_412 >= 47) {
        root->coord.t[1]     += work->field_42A;
        work->obj_2CC.pos.vy += work->field_42A;
        work->field_428      += 30;
        work->field_42A      += work->field_428;
        if (root->coord.t[1] >= (s16)work->field_92) {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_426         = 2;
            w->field_41C         = 0x10;
            w->field_418         = 0x12;
            w->field_414         = 1;
            root->coord.t[1]     = (s16)work->field_92;
            work->obj_2CC.pos.vy = 0;
            work->field_412      = 0;
            work->field_422++;
        }
    }
}

void Actor04400_Fn01B70(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u32              rand;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        rand             = Gp_LcgState * 5 + 0x71357911;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work->field_7A  += 0x800;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 2;
        work3            = (Actor104400Work*)arg0->work;
        Gp_LcgState      = rand;
        arg0->state      = 1;
        work3->field_420 = 0;
        work3->field_422 = 0;
    }
}

/// Moves the root along `field_40C`
/// and applies the fall velocity; on landing it requests animation 0x13 and
/// advances the sub-state.
void Actor04400_Fn01CA0(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor104400Work*)arg0->work;
    angle                                          = work->field_40C;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = 0xC8;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->obj_2CC.pos.vy                          += work->field_42A;
    work->field_428                               += 0xE;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                 = (Actor104400Work*)arg0->work;
        anim->field_426      = 2;
        anim->field_41C      = 0x10;
        anim->field_418      = 0x13;
        anim->field_414      = 1;
        coord->coord.t[1]    = (s16)work->field_92;
        work->obj_2CC.pos.vy = 0;
        work->field_412      = 0;
        work->field_422++;
    }
}

/// Per-frame callback with a one-entry handler table. `Gp_StateF0.field_4` 2 hides the model; 0 runs the state
/// handler and the follow-up steps, then moves the task to state 4 when
/// `field_448` requests it and the enemy is out of HP; 0 and 1 both colour
/// it, run `Actor04400_Fn00220` for three part pairs and unhide it. The work
/// block is reloaded through its own local for the state reset, as the
/// original does.
void Actor04400_Fn01E08(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFunc         sp[1] = { Actor04400_Fn07360 };

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp[(s16)work->field_420](arg0);
            Actor04400_Fn022A8(arg0, 1);
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                Actor104400Work* w = (Actor104400Work*)arg0->work;

                arg0->state  = work->field_448;
                w->field_420 = 0;
                w->field_422 = 0;
            }
            Actor04400_Fn02B8C(arg0);
            if (work->field_432 == 1) {
                Actor04400_Fn06520(arg0, 6, (SVECTOR3*)&work->field_80);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}

/// Eases the pitch `field_434` a quarter
/// of the way back to zero, rebuild the model root's rotation from it and the
/// heading `field_7A`, then drop the root under an accelerating fall
/// (`field_428` the acceleration, `field_42A` the speed). Once it reaches the
/// floor (Y 0) it stops there, requests animation 12 (kind 2, speed 0x20) and
/// advances `field_422`.
void Actor04400_Fn02008(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    ActorMat         rot;
    ActorMat*        src;
    MATRIX*          dst;
    Actor104400Work* anim;

    work               = (Actor104400Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    work->field_434   += -work->field_434 >> 2;
    RotMatrixX(work->field_434, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
    dst                = &coord->coord;
    dst->m[0][0]       = src->mat.m[0][0];
    dst->m[0][1]       = src->mat.m[0][1];
    dst->m[0][2]       = src->mat.m[0][2];
    dst->m[1][0]       = src->mat.m[1][0];
    dst->m[1][1]       = src->mat.m[1][1];
    dst->m[1][2]       = src->mat.m[1][2];
    dst->m[2][0]       = src->mat.m[2][0];
    dst->m[2][1]       = src->mat.m[2][1];
    dst->m[2][2]       = src->mat.m[2][2];
    work->field_428   += 2;
    work->field_42A   += work->field_428;
    coord->coord.t[1] += work->field_42A;
    if (coord->coord.t[1] > 0) {
        work->field_412   = 0;
        coord->coord.t[1] = 0;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x20;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_422++;
    }
}

/// Landing: plays sound 0x402C0004 on the first frame and 0x402C0003 on the
/// second at the enemy's pan and depth; a hit (`Actor04400_Fn06618`) puts the
/// task in state 3 with its state machine at state 3.
void Actor04400_Fn0216C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* next;
    Actor104400Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = Gp_GetObjPan(((TmdObject*)arg0->extra)->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if (Actor04400_Fn06618(arg0)) {
        next             = (Actor104400Work*)arg0->work;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor104400Work*)arg0->work;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}

/// Per-frame contact handling for the enemy. Walks the eight contact records: kind 1 (skipped when
/// `arg1` is set) and kind 3 push the model out, kind 2 applies a hit -
/// damage, status effects and the pending state request in `field_448` -
/// unless `field_40E` is still cooling down. Then ticks the status flags,
/// applies `func_800E0C10`'s collision step (snapping back to `field_60` when
/// it reports a conflict) and moves the root by the combined step and
/// push-out.
void Actor04400_Fn022A8(Task* arg0, s16 arg1)
{
    GpDeltaScratch   delta;
    SVECTOR          push;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    u8               blocked;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;

    stepZ                  = 0;
    maxX                   = 0;
    maxZ                   = 0;
    stepX                  = 0;
    blocked                = 0;
    work                   = (Actor104400Work*)arg0->work;
    coord                  = ((TmdObject*)arg0->extra)->coords;
    enemy                  = arg0->spawnArg2;
    *(u8**)G_SCRATCH_HEAD -= 8;
    work->field_41E        = 0;
    for (i = 0; i < 8; i++) {
        switch (work->rec_2EC[i].key & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                Actor04400_CalcPush(arg0, coord, &work->rec_2EC[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->field_40E == 0) {
                    work->field_41E = 1;
                    dmg             = Gp_ComputeDamage(work->rec_2EC[i].key, work->field_43A, 0, 0);
                    amount          = dmg;
                    work->field_40E = Gp_GetIdParam2(work->rec_2EC[i].key);
                    if (Gp_RollEnemyChance(enemy, work->rec_2EC[i].key, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 0, NULL);
                    }
                    func_800E2C78(enemy, work->rec_2EC[i].key, amount, 0);
                    func_800DA6E8(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    func_800FDB18(Gp_GetIdParam1(work->rec_2EC[i].key) & 0xFFFF,
                                  &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_3FC);
                    if (amount >= 0x28) {
                        work->field_448 = 2;
                    } else {
                        work->field_448 = 1;
                    }
                    switch (Gp_GetIdParam0(work->rec_2EC[i].key) & 0xFFFF) {
                        case 0:
                            break;
                        case 1:
                            Gp_SetObjFlag1(enemy);
                            break;
                        case 2:
                            Gp_SetObjFlag2(enemy, work->rec_2EC[i].key, 0);
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, work->rec_2EC[i].key, 0);
                            break;
                        case 4:
                            work->field_448 = 4;
                            break;
                        case 5:
                            work->field_448 = 2;
                            break;
                        case 6:
                            work->field_448 = 4;
                            break;
                        case 7:
                            work->field_448 = 2;
                            break;
                        case 8:
                            work->field_448 = 3;
                            break;
                        case 9:
                            work->field_448 = 3;
                            break;
                    }
                } else if ((Gp_GetIdParam1(work->rec_2EC[i].key) & 0xFFFF) == 0xD) {
                    func_800FDB18(0xD, &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_3FC);
                }
                break;
        }
    }

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= 0xFE;
        work->field_448       = 5;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_448       = 3;
    }
    if (enemy->reactionFlags & 0xC) {
        work->field_44E = 1;
        tmp             = Gp_TickObjFlag4(enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_41E = 1;
            work->field_448 = 2;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_2EC, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_60.vx;
            coord->coord.t[2] = work->field_60.vz;
            coord->flg        = 0;
            blocked           = 1;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_2EC);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->field_40E > 0) {
        work->field_40E--;
    }
    if (blocked == 0) {
        work->field_80    += Actor04400_PickStep(stepX, maxX >> 3);
        work->field_84    += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->coord.t[0] += Actor04400_PickStep(stepX, maxX >> 3);
        coord->coord.t[2] += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->flg         = 0;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Applies the pending animation request and ticks the animation. Kind 1
/// blends slots 1..8 to animation `field_418` at speed `field_41C`, passing
/// `field_426` (cleared when the animation changes); kind 2 resets the slots
/// onto it. Either records the animation in `field_416` and moves on to kind
/// 3, which counts frames in `field_41A`. Every frame each slot then takes
/// the speed and ticks.
void Actor04400_Fn02B8C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* start;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor104400Work*)arg0->work;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            Gp_AnimResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].rate = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].rate = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}

/// Links the actor's three collision
/// objects onto `Gp_ObjLists[2]` and clears their record tables.
void Actor04400_Fn02D18(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->obj_2AC.coord    = &((TmdObject*)arg0->extra)->coords[1];
    work->obj_2AC.ctx.recs = work->rec_2EC;
    work->obj_2AC.pos.vx   = 0;
    work->obj_2AC.pos.vy   = 0;
    work->obj_2AC.pos.vz   = 0;
    work->obj_2AC.key      = 0x3002C;
    work->obj_2AC.radius   = 0x170;
    work->obj_2AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= 0x8000;

    work->obj_3AC.coord    = &((TmdObject*)arg0->extra)->coords[1];
    work->obj_3AC.ctx.recs = work->rec_3CC;
    work->obj_3AC.pos.vx   = 0;
    work->obj_3AC.pos.vy   = 0;
    work->obj_3AC.pos.vz   = 0;
    work->obj_3AC.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_3AC.radius   = 0x170;
    work->obj_3AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= 0x7FFF;

    work->obj_2CC.coord    = &((TmdObject*)arg0->extra)->coords[1];
    work->obj_2CC.ctx.recs = work->rec_2EC;
    work->obj_2CC.pos.vx   = 0;
    work->obj_2CC.pos.vy   = 0;
    work->obj_2CC.pos.vz   = 0;
    work->obj_2CC.key      = 0x3002C;
    work->obj_2CC.radius   = 0x224;
    work->obj_2CC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= 0x4000;
}

/// State handlers `Actor04400_Fn02E8C` dispatches by `field_420`.
const TaskFuncTable9 Actor04400_D000EC = { {
    Actor04400_Fn07530,
    Actor04400_Fn075F0,
    Actor04400_Fn076D0,
    Actor04400_Fn07750,
    Actor04400_Fn0781C,
    Actor04400_Fn0304C,
    Actor04400_Fn07878,
    Actor04400_Fn07890,
    Actor04400_Fn078D4,
} };

/// Per-frame callback of the main enemy. `Gp_StateF0.field_4` 2 hides the model, 0 runs the current state handler
/// (then colours it), 1 only colours it. Unless `field_451` is set, it then
/// runs `Actor04400_Fn00220` for three part pairs.
void Actor04400_Fn02E8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable9   sp    = Actor04400_D000EC;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Death squash: copy `matrix_0` onto the
/// model root, scale Y by the shrinking `field_430`, spawn spark 0x600A5 on
/// frame 4, switch the light mode on frame 16, and hide the model after frame
/// 32.
void Actor04400_Fn0304C(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    ActorMat         m;
    ActorMatWords*   ident;
    SVECTOR          ofs;

    work             = (Actor104400Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 4) {
        ofs.vx = 0;
        ofs.vy = 0;
        ofs.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &ofs);
    }
    if ((s16)work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->flags |= 0x80;
        work->field_420++;
    }
}

/// Latch the model root position into `field_60`, then aim at the nearer of
/// the two `Gp_ActorSlots` actors (distance measured in XZ): its offset goes
/// to `field_88`..`field_8C`, the distance to `field_43A`, and its heading
/// relative to `field_7A` to `field_444`. Nothing is written when slot 0 is
/// empty.
void Actor04400_Fn031B8(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   other;
    Task*            player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist;
    s32              dist2;

    work              = (Actor104400Work*)arg0->work;
    coord             = ((TmdObject*)arg0->extra)->coords;
    player            = Gp_ActorSlots[0];
    work->field_60.vx = coord->coord.t[0];
    work->field_60.vy = coord->coord.t[1];
    work->field_60.vz = coord->coord.t[2];
    if (player != NULL) {
        other = ((TmdObject*)player->extra)->coords;
        d0.vx = other->coord.t[0] - coord->coord.t[0];
        d0.vy = other->coord.t[1] - coord->coord.t[1];
        d0.vz = other->coord.t[2] - coord->coord.t[2];
        dist  = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] != NULL) {
            other = ((TmdObject*)Gp_ActorSlots[1]->extra)->coords;
            d1.vx = other->coord.t[0] - coord->coord.t[0];
            d1.vy = other->coord.t[1] - coord->coord.t[1];
            d1.vz = other->coord.t[2] - coord->coord.t[2];
            dist2 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist2 < dist) {
                dist  = dist2;
                d0.vx = d1.vx;
                d0.vy = d1.vy;
                d0.vz = d1.vz;
            }
        }
        do {
            work->field_88  = d0.vx;
            work->field_8A  = d0.vy;
            work->field_8C  = d0.vz;
            work->field_43A = dist;
        } while (0);
        VectorNormalSS(&d0, &d0);
        work->field_444 = (ratan2(d0.vx, d0.vz) - work->field_7A) & 0xFFF;
    }
}

/// State handler: with `field_44F` 1, a pending request 1 while `field_41E`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `field_44F` clear, a hit
/// calls `Actor04400_Fn06374` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `field_44F`
/// register; writing `== work->field_44F` reloads the byte instead.
void Actor04400_Fn03390(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_44F == 1) {
        if (work->field_41E != 0 && work->field_448 == 1) {
            work->field_41C = 0x20;
            work->field_418 = 0xB;
            work->field_414 = 2;
            return;
        }
        if (Actor04400_TakeRequest(arg0) == 0 && Actor04400_IsHit(arg0)) {
            Actor04400_SetWorkState(arg0, 3);
        }
    } else if (Actor04400_IsHit(arg0)) {
        Actor04400_Fn06374(arg0, 1);
        Actor04400_SetWorkState(arg0, 5);
    }
}

/// State handlers `Actor04400_Fn03538` dispatches by `field_420`.
const TaskFuncTable5 Actor04400_D00128 = { {
    Actor04400_Fn07CF0,
    Actor04400_Fn07D78,
    Actor04400_Fn07E00,
    Actor04400_Fn07E74,
    Actor04400_Fn07F04,
} };

/// The five-state per-frame callback of the enemy's state machine, the
/// counterpart of `Actor04400_Fn05DE0`. Mode 0 counts `field_442` up, aims
/// (`Actor04400_Fn031B8`), lets `Actor04400_TakeHit` replace the handler
/// `field_420` selects from `Actor04400_D00128`, rebuilds the model root
/// rotation through part 0's coordinate, and picks the next state: 4 once the
/// `field_40` hold is empty, 8 / 9 for messages 4 / 5, and 3 after a consumed
/// `field_448` request. Mode 1 recolours from part 1's world position; both
/// clear bit 0x80 of the model flags, which mode 2 sets.
void Actor04400_Fn03538(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable5   sp    = Actor04400_D00128;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            Actor04400_Fn031B8(arg0);
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            Actor04400_Fn00874(arg0);
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                Actor04400_SetTaskState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                Actor04400_SetTaskState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                Actor04400_SetTaskState(arg0, 9);
            } else if (Actor04400_TakeRequest(arg0)) {
                work->field_438 = 0;
                Actor04400_SetTaskState(arg0, 3);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
            Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
            Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~0x80;
            return;
    }
}

/// Sub-state handlers `Actor04400_Fn07CF0` dispatches by `field_422`.
const TaskFuncTable3 Actor04400_D00150 = { {
    Actor04400_Fn07F6C,
    Actor04400_Fn07FD0,
    Actor04400_Fn08094,
} };

/// Sub-state handlers `Actor04400_Fn07D78` dispatches by `field_422`.
const TaskFuncTable3 Actor04400_D0015C = { {
    Actor04400_Fn080E8,
    Actor04400_Fn08160,
    Actor04400_Fn039EC,
} };

/// Sub-state handlers `Actor04400_Fn07E74` dispatches by `field_422`.
const TaskFuncTable3 Actor04400_D00168 = { {
    Actor04400_Fn08290,
    Actor04400_Fn082E0,
    Actor04400_Fn03B34,
} };

/// Sub-state handlers `Actor04400_Fn07F04` dispatches by `field_422`.
const TaskFuncTable4 Actor04400_D00174 = { {
    Actor04400_Fn08358,
    Actor04400_Fn083CC,
    Actor04400_Fn03CA0,
    Actor04400_Fn03E20,
} };

/// State handlers `Actor04400_Fn03F8C` dispatches by `field_420`.
const TaskFuncTable10 Actor04400_D00184 = { {
    Actor04400_Fn042C4,
    Actor04400_Fn045A0,
    Actor04400_Fn04718,
    Actor04400_Fn048A0,
    Actor04400_Fn04A3C,
    Actor04400_Fn04BA8,
    Actor04400_Fn04D44,
    Actor04400_Fn04EDC,
    Actor04400_Fn05040,
    Actor04400_Fn05260,
} };

/// Sub-state handlers `Actor04400_Fn06B50` dispatches by `field_422`.
const TaskFuncTable6 Actor04400_D001AC = { {
    Actor04400_Fn0847C,
    Actor04400_Fn08610,
    Actor04400_Fn053FC,
    Actor04400_Fn058F4,
    Actor04400_Fn05A40,
    Actor04400_Fn058F4,
} };

/// State handlers `Actor04400_Fn05DE0` dispatches by `field_420`.
const TaskFuncTable5 Actor04400_D001C4 = { {
    Actor04400_Fn08870,
    Actor04400_Fn08908,
    Actor04400_Fn089C0,
    Actor04400_Fn08A40,
    Actor04400_Fn08A9C,
} };

/// State handlers `Actor04400_Fn05FC8` dispatches by `field_420`.
const TaskFuncTable7 Actor04400_D001D8 = { {
    Actor04400_Fn08AA4,
    Actor04400_Fn08908,
    Actor04400_Fn089C0,
    Actor04400_Fn08B3C,
    Actor04400_Fn08C08,
    Actor04400_Fn08C64,
    Actor04400_Fn08DA4,
} };

/// Counts `field_412` up against the `field_446` hold and enters state 2 once
/// it runs out; over its last 0x30 frames, eases the yaw `field_424` back to
/// zero. Before that, while `field_43A` (the distance to the nearer player
/// actor) is under 0xDAC and the heading `field_444` is outside 0x3C0..0xC40,
/// eases the yaw toward it and enters state 3 (arming `Gp_ArmStateF0`) after
/// 16 such frames; otherwise swings it toward +-0x380 on bit 6 of
/// `field_442`.
void Actor04400_Fn039EC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    Actor104400Work* state;
    Actor104400Work* state2;
    s32              angle;
    s32              cur;
    s32              aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (Actor104400Work*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (Actor104400Work*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102
/// as a word), clears `field_438` and puts the task in state 3 with its work
/// block at state 3.
void Actor04400_Fn03B34(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        Actor04400_SetTaskState(arg0, 3);
        Actor04400_SetWorkState(arg0, 3);
    }
}

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102 as
/// a word), clears `field_438`, requests animation 3 at speed 0x10 and rewinds
/// the frame counter so the next state starts fresh.
void Actor04400_Fn03CA0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                          = 0x1E;
        angle                                          = work->field_7A + 0x400;
        speed                                          = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}

/// Unless `Actor04400_Fn06328` claims the frame: count the frame in
/// `field_412` and, on frames 0x17..0x23, push the model root along the
/// heading `field_7A` turned a quarter circle, by `field_41C` scaled -30/16.
/// Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102 as a word),
/// `field_438` is cleared and the state machine rewinds to state 0.
void Actor04400_Fn03E20(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                          = -0x1E;
            angle                                          = work->field_7A + 0x400;
            speed                                          = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->coords->flg         = 0;
        }
        work2 = (Actor104400Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (Actor104400Work*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}

/// Per-frame callback of the enemy this overlay drives, and the ten-state
/// counterpart of `Actor04400_Fn05DE0`: its handlers come from the
/// `Actor04400_D00184` table copied onto the stack, and in mode 0 a pending hit
/// (`Actor04400_TakeHit3`) replaces this frame's handler. `Actor04400_Fn02B8C`
/// advances the animation, the root rotation is rebuilt from 0x78..0x7C, and
/// `Actor04400_Fn022A8` applies the frame's motion before the root coordinate
/// is marked dirty. Mode 1 re-pushes the model's second coordinate for
/// `Gp_UpdateActorColor` and rebuilds the part-pair colour quads while
/// `field_451` is clear. `Gp_StateF0.field_4` short-circuits both: 1 runs mode 1 only,
/// 2 hides the model instead.
void Actor04400_Fn03F8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable10  sp    = Actor04400_D00184;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            if (Actor04400_TakeHit3(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Message 0x2C00 with low nibble 1 (see `field_44C`): reveals the model,
/// places its root at the spawn point bits 8..11 select from the current map's
/// table (playing the appearance sound on map 0x427), and starts state 1, 4
/// or 7 by bits 4..7.
void Actor04400_Fn042C4(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->coords;
    Actor104400Work* w2;
    s32              id;
    s32              pan;
    u32              map;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= 0x8000;
        work->obj_2CC.flags &= 0xBFFF;
        obj->flags          &= 0xFF7F;
        if ((arg0->spawnArg1 & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->flags &= 0xFFFB;
        }
        enemy->node.flags = 0;
        map               = *(u32*)&gGameSession->at4.loc & 0xFFFF0000;
        if (map == 0x4270000) {
            work->field_78    = 0;
            work->field_7A    = (D_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        } else if (map == 0x4280000) {
            work->field_78    = 0;
            work->field_7A    = (D_801874C4[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_801874C4[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_801874C4[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_801874C4[(work->field_44C >> 8) & 0xF].z;
        }
        work->field_428 = 0;
        work->field_42A = 100;
        w2              = (Actor104400Work*)arg0->work;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                Actor04400_SetWorkState(arg0, 1);
                break;
            case 1:
                Actor04400_SetWorkState(arg0, 4);
                break;
            default:
                Actor04400_SetWorkState(arg0, 7);
                break;
        }
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}

/// Same body as `Actor04400_Fn04A3C` with a slower push and a wider pitch
/// step: the root moves 0x8C back along the heading `field_7A`, `field_78`
/// eases an eighth of the way to 0x800 rather than a thirty-second of the way
/// to 0x200, and the heading turns half a circle before animation 0x11 is
/// requested at speed 0x10.
void Actor04400_Fn045A0(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor104400Work*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x800 - work->field_78) >> 3;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 forward along the heading
/// `field_7A`, then walks the root's y by `field_42A` while `field_428` ramps
/// it by 4 and feeds that back into `field_42A`. Once the root y passes zero it
/// snaps back to -0x3C and starts the cycle again, clearing the frame counter
/// and stepping the state `field_420`.
void Actor04400_Fn04718(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = 0x50;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, pushes the root 0x14 forward along the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 5,
/// sub-state 0.
void Actor04400_Fn048A0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = 0x14;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work2                                          = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Pushes the model root 0x8C back against the heading `field_7A`, eases
/// `field_78` a 32nd of the way to 0x200, and falls under an accelerating drop
/// (`field_428` the acceleration, `field_42A` the speed). Once the root passes
/// above y = 0 it is pinned at -0x3C, `field_78` / `field_7C` clear, the fall
/// rearms at speed -0x6E, animation 12 (kind 2, speed 0x10) is requested and the
/// state advances.
void Actor04400_Fn04A3C(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor104400Work*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Counts the frame and decays `field_78` by a thirty-second towards 0. On the
/// first frame plays sound 0x402C0009 (bank from the enemy's `field_8` high
/// nibble) panned and attenuated from the model root. Every frame, pushes the
/// root 0x50 back against the heading `field_7A`. Once the root passes above
/// y = 0 it is pinned at -0x3C, the frame counter clears and the state
/// advances.
void Actor04400_Fn04BA8(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_412++;
    work->field_78 += -work->field_78 >> 5;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x50;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 back against the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 3,
/// sub-state 0.
void Actor04400_Fn04D44(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x14;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work2                                          = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}

/// Pushes the model root 0x8C back against the heading `field_7A`, eases
/// `field_78` a 32nd of the way to 0x200, and falls under an accelerating drop
/// (`field_428` the acceleration, `field_42A` the speed). Once the root passes
/// above y = 0 it is pinned at -0x3C, the fall rearms at speed -0x12C,
/// animation 12 (kind 2, speed 0x10) is requested and the state advances.
void Actor04400_Fn04EDC(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                           = (Actor104400Work*)arg0->work;
    angle                                          = work->field_7A;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    dx                                             = rsin(angle) << 4;
    speed                                          = -0x8C;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work->field_78                                += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 2;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}

/// Counts `field_412` up and eases `field_78` an eighth of the way to 0x800. On
/// the first frame plays sounds 0x402C0009 and 0x402C0003 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, pushes the root 0x5A back against the heading `field_7A` and
/// falls as `Actor04400_Fn04BA8` does. Once the root passes above y = 0 it is
/// pinned at -0x3C, `field_78` / `field_7C` clear, the heading turns half a
/// circle, animation 0x11 is requested at speed 0x10 and the state advances.
void Actor04400_Fn05040(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* anim;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    work->field_412++;
    work->field_78 += (0x800 - work->field_78) >> 3;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan2     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = -0x5A;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    coord->coord.t[1]                             += work->field_42A;
    work->field_428                               += 4;
    work->field_42A                               += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Same body as `Actor04400_Fn04D44` with the opposite step: the push is 0x14
/// forward along the heading and the state it lands on is 5 rather than 3.
///
/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 forward along the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 5,
/// sub-state 0.
void Actor04400_Fn05260(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    speed                                          = 0x14;
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    work2                                          = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Steering counterpart of `Actor04400_Fn048A0`. While the enemy lives it ramps `field_41C` up to 0x40 along `field_44F`,
/// turns `field_7A` toward `field_70` and pushes the root back along it by
/// `field_41C` scaled -0x10, then slides toward `field_70` at the accelerating
/// `field_42A` scaled 1/64. Past 120 frames it eases the root y toward
/// `field_70`'s (a quarter within 3000 units, a thirty-second beyond) and marks
/// `field_438`; while alive a hit flag plays sound 0x402C0001 at the enemy's
/// pan and depth. Within 800 units it advances `field_422`, otherwise a dead
/// enemy queues its follow-up animation.
void Actor04400_Fn053FC(Task* arg0)
{
    TmdObject*       obj;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s32              dist;
    s32              cond;
    s32              soundId;
    s32              pan;

    obj   = arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    coord = obj->coords;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work->field_412++;
    if (enemy->hp > 0) {
        Actor104400Work* w;
        s32              diff;
        s32              k;
        s32              step;

        if ((u32)(work->field_44F >> 1) < 0x40) {
            work->field_41C = work->field_44F >> 2;
            work->field_44F++;
        } else {
            work->field_41C = 0x40;
        }
        w      = (Actor104400Work*)arg0->work;
        c      = ((TmdObject*)arg0->extra)->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->field_7A - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->field_7A -= 0x18;
        } else if (diff < -0x100) {
            w->field_7A += 0x18;
        }
        angle                                          = work->field_7A;
        k                                              = -0x10;
        step                                           = ((((Actor104400Work*)arg0->work)->field_41C * k) << 12) >> 16;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    work->field_428++;
    work->field_42A += work->field_428;
    {
        s32 step = work->field_42A >> 6;

        c      = ((TmdObject*)arg0->extra)->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                          = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->field_412 > 120) {
        if (dist <= 3000) {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 4;
        } else {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->hp > 0) {
        Actor104400Work* w2 = (Actor104400Work*)arg0->work;

        if ((w2->flags_EC.half & 1) || (w2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0001;
            pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
    } else {
        work->field_438    = 1;
        coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor104400Work* w;
                s16              next;

                next         = Actor04400_D10828[work->field_418 - 1];
                w            = (Actor104400Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Death: marks `field_438`, plays sound 0x402C0003 unless the HP is below
/// zero, releases the spawn place claimed in `Gp_StateF0.field_1F`, unlinks
/// the enemy node and the three collision objects, puts the task in state 5,
/// sends message 0x13F4 to slot 4's task and hides the model.
void Actor04400_Fn058F4(Task* arg0)
{
    Actor104400Work* objs;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    tmd             = (TmdObject*)arg0->extra;
    work->field_438 = 1;
    if (enemy->hp >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    objs        = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    Actor04400_SetTaskState(arg0, 5);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->flags |= 0x80;
}

/// Sub-state handler: slides the model's root toward `field_70` in x/z,
/// accelerating with `field_42A`; after 90 frames it also eases y in and marks
/// `field_438`. Within 800 units it advances `field_422`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `field_438` when
/// state 4 is pending). The animation to follow comes from
/// `Actor04400_D10828`.
void Actor04400_Fn05A40(Task* arg0)
{
    TmdObject*       obj;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s16              next;

    obj   = arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    coord = obj->coords;
    work->field_412++;
    work->field_428++;
    work->field_42A += work->field_428;
    if ((s16)work->field_412 < 0x5A) {
        s32 step = work->field_42A >> 6;

        c      = ((TmdObject*)arg0->extra)->coords;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                          = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    } else {
        s32 step;

        work->field_438 = 1;
        step            = work->field_42A >> 5;
        c               = ((TmdObject*)arg0->extra)->coords;
        dir.vx          = work->field_70.vx - c->coord.t[0];
        dir.vy          = 0;
        dir.vz          = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                          = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
        coord->coord.t[1]                             += (work->field_70.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    gte_sqr0();
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->field_422++;
        return;
    }
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(0x402C0002, 1);
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor104400Work* w;

                next         = Actor04400_D10828[work->field_418 - 1];
                w            = (Actor104400Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// The per-frame callback the actor's AI states are dispatched from: state 0
/// counts `field_442` up, runs the handler `field_420` selects from
/// `Actor04400_D001C4` and spawns effect 3 on the model's second coordinate
/// part every 32 frames, then falls into state 1, which re-pushes that
/// coordinate's world position for `Gp_UpdateActorColor` and rebuilds the
/// part-pair colour quads while `field_451` is clear. `Gp_StateF0.field_4` short-
/// circuits both: nonzero runs state 1 only, 2 hides the model instead.
void Actor04400_Fn05DE0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable5   sp    = Actor04400_D001C4;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~0x80;
            return;
    }
}

/// `Actor04400_Fn05DE0`'s seven-state counterpart, and the only difference is
/// the exit: mode 1 ends without clearing bit 0x80 of the model flags, which
/// leaves `obj` live only as far as mode 2 and lets it stay in `$a0` instead of
/// a saved register.
void Actor04400_Fn05FC8(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->coords;
    TaskFuncTable7   sp    = Actor04400_D001D8;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->coords[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->coords[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Queues CD command 0x21 once, guarded by `D_80115415`: the first parameter
/// block selects 2 or 3 when session `field_7` is 4, `field_6` is 0x27 or 0x28
/// and `field_9` is 1 or 2 respectively, and 1 otherwise.
void Actor04400_Fn061B4(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (gGameSession->at4.loc.stage == 4 && (u32)(gGameSession->at4.loc.area - 0x27) < 2 && gGameSession->at4.loc.place == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        D_80115415 = 1;
    }
}

/// Walks the death sequence's two-state handler table on the work block's
/// state index.
void Actor04400_Fn062D4(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08718,
        Actor04400_Fn087E0,
    };

    states[(s16)work->field_420](arg0);
}

/// Once bit 7 of `Gp_StateF0.field_1F` is set, puts the task in state 3 with
/// its state machine at state 5 and returns 1; otherwise returns 0.
s16 Actor04400_Fn06328(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        Actor04400_SetTaskState(arg0, 3);
        Actor04400_SetWorkState(arg0, 5);
        return 1;
    }
    return 0;
}

/// Claims or releases this actor's spawn place in `Gp_StateF0.field_1F`:
/// `arg1` non-zero sets bit 7 from the place id in bits 12+ of the spawn
/// descriptor (unless the place is already claimed), and `arg1` zero clears the
/// byte when its low nibble still matches that place. Spawn paths pass 1,
/// despawn paths pass 0; `Actor04400_Fn06328` reads bit 7 back.
void Actor04400_Fn06374(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 Actor04400_Fn063E4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test. `arg1` is the dispatch's
/// handler index and is unused here.
void Actor04400_Fn0648C(Task* arg0, s32 arg1, Actor104400Msg* arg2)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (arg2->field_0 == 0x2C00) {
        switch (arg2->field_2 & 0xF) {
            case 1:
                work->field_44C = arg2->field_2;
                break;
            case 2:
                work->field_44C = arg2->field_2;
                break;
            case 3:
                work->field_44C = arg2->field_2;
                break;
            case 4:
                work->field_44C = arg2->field_2;
                break;
            case 5:
                work->field_44C = arg2->field_2;
                break;
        }
    }
}

void Actor04400_Fn064EC(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}

/// Moves the model so that part `part` lands on `pos`: sets the root
/// translation to `pos` less the part's view-space offset from the root, and
/// marks the part's coordinate dirty.
void Actor04400_Fn06520(Task* arg0, s16 part, SVECTOR3* pos)
{
    MATRIX         local;
    MATRIX         world;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord  = &coords[part];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
    coords->coord.t[0] = pos->vx - (world.t[0] - local.t[0]);
    coords->coord.t[1] = pos->vy - (world.t[1] - local.t[1]);
    coords->coord.t[2] = pos->vz - (world.t[2] - local.t[2]);
    coord->flg         = 0;
}

/// Scales `value` by the animation speed `field_41C`, in 1/16 units.
s32 Actor04400_Fn065F4(Task* arg0, s16 value)
{
    return (s32)((((Actor104400Work*)arg0->work)->field_41C * value) << 0xC) >> 0x10;
}

/// Whether `flags_EC` reports a hit: bit 0 as a halfword, or 0x102 as a word.
s16 Actor04400_Fn06618(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// Dispatches the second form's `Actor04400_D0001C` table by `Task::state`.
void Actor04400_Fn06658(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = Actor04400_D0001C;
    sp.funcs[arg0->state](arg0);
}

/// Dispatches the first form's `Actor04400_D00004` table by `Task::state`.
void Actor04400_Fn066DC(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Actor04400_D00004;
    sp.funcs[arg0->state](arg0);
}

/// Runs the intro's two-state handler table on the work block's state index,
/// the same shape as `Actor04400_Fn062D4`.
void Actor04400_Fn0674C(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07968,
        Actor04400_Fn07984,
    };

    states[(s16)work->field_420](arg0);
}

/// Turns the heading `field_7A` by `step` towards the target offset
/// (`field_88`, `field_8C`) when it is more than 0x100 off.
void Actor04400_Fn067A0(Task* arg0, s32 step)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}

void Actor04400_Fn06834(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn06848(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn0685C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Copies this overlay's `Actor04400_D0007C` dispatcher table onto the stack and
/// lets the pending-request handler `Actor04400_Fn063E4` consume the request
/// first: the table entry `field_422` selects runs only when nothing was
/// consumed.
void Actor04400_Fn06870(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0007C;
    if ((s16)Actor04400_Fn063E4(arg0) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Copies this overlay's five-entry `Actor04400_D00088` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06964` with the other table.
void Actor04400_Fn068F8(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D00088;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Copies this overlay's five-entry `Actor04400_D0009C` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06870` without the pending-request handler in front of it.
void Actor04400_Fn06964(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D0009C;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// encounter's animation (`Actor04400_Fn07A38`, which then advances `field_422`
/// itself), entry 1 runs the handler that answers a pending request or a hit
/// (`Actor04400_Fn03390`), chosen by the sub-state index `field_422`.
void Actor04400_Fn069D0(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07A38,
        Actor04400_Fn03390,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// animation the encounter asked for (`Actor04400_Fn07B4C`), entry 1 finishes
/// the encounter (`Actor04400_Fn07C60`), chosen by the sub-state index
/// `field_422`.
void Actor04400_Fn06A24(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07B4C,
        Actor04400_Fn07C60,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 advances the
/// animation sub-state (`Actor04400_Fn06BC4`), entry 1 runs the pending-request
/// handler (`Actor04400_Fn06BF8`), chosen by the sub-state index `field_422`.
void Actor04400_Fn06A78(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn06BC4,
        Actor04400_Fn06BF8,
    };

    states[(s16)work->field_422](arg0);
}

void Actor04400_Fn06ACC(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00070;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        Actor04400_Fn08DBC(arg0);
    }
}

/// Copies this overlay's six-entry `Actor04400_D001AC` dispatcher table onto the
/// stack and calls the entry `field_422` selects.
void Actor04400_Fn06B50(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable6   sp;

    sp = Actor04400_D001AC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC (kind 1, speed 0x10, `field_426` 8) and advances
/// the sub-state.
void Actor04400_Fn06BC4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

void Actor04400_Fn06BF8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work            = (Actor104400Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

void Actor04400_Fn06C70(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    work->field_44F = Actor04400_D10814[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor104400Work* w = (Actor104400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor104400Work* w = (Actor104400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

void Actor04400_Fn06CF0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* slow;
    Actor104400Work* fast;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor104400Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor104400Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn06D90(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (Actor04400_Fn06618(arg0)) {
        if (work->field_44F == 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

void Actor04400_Fn06DFC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              angle;
    s16              speed;

    Actor04400_Fn067A0(arg0, 0x10);
    speed                                          = Actor04400_Fn065F4(arg0, -0x10);
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor104400Work* next = (Actor104400Work*)arg0->work;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}

/// Seeds the model's root coord.t[1] from
/// the animation's, then resets the whole animation/state block. The `tmp`
/// variable is deliberate - reusing one halfword for 8 and then 1 is what the
/// original did, and it is what puts both constants in the same register.
void Actor04400_Fn06EEC(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s16              tmp;

    work             = (Actor104400Work*)arg0->work;
    work->field_92   = *(u16*)&((TmdObject*)arg0->extra)->coords->coord.t[1];
    work2            = (Actor104400Work*)arg0->work;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}

/// Counts `field_412` up, clearing the death flag `field_438` on the way. On
/// frame 1 it plays the enemy's hit sound at the model's pan and depth, with
/// the id's high half taken from `GpEnemy::placeKey`. Then, when
/// `Actor04400_Fn06618` accepts the frame, draws `field_44A` as 0x5A..0xD9 from
/// `Gp_LcgState` and enters state 3.
void Actor04400_Fn06F50(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              soundId;
    s32              pan;
    u32              rand;

    work            = (Actor104400Work*)arg0->work;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Requests animation 9 (kind 1, speed 0x10, `field_426` 4), clears the frame
/// counter, advances the sub-state and, while the enemy has HP, plays sound
/// 0x402C0002 at its pan and depth.
void Actor04400_Fn07050(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->hp > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}

void Actor04400_Fn0710C(Task* arg0)
{
    u16              ticks;
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn0714C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor04400_Fn06374(arg0, 0);
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void Actor04400_Fn071C8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Frames 0x1D..0x29 of the state: each frame `Actor04400_Fn065F4` gives the
/// step speed, which pushes the model's root coordinate along `field_7A` +
/// 0x400 with `rsin`/`rcos` and clears the root flag. On the frame
/// `Actor04400_Fn06618` accepts, clears the death flag `field_438` and enters
/// state 3.
void Actor04400_Fn0723C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s16              angle;
    s16              speed;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((u32)((ticks - 0x1D) & 0xFFFF) < 0xDU) {
        speed                                          = Actor04400_Fn065F4(arg0, 0x1E);
        angle                                          = work->field_7A + 0x400;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Dispatches this overlay's `Actor04400_D000B0` dispatcher table by the
/// sub-state index `field_422`. Entry 2 is the fall-to-floor handler
/// `Actor04400_Fn02008` and entry 3 the landing it triggers
/// (`Actor04400_Fn0216C`).
void Actor04400_Fn07360(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D000B0;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, which makes the per-frame callbacks hold part 6 in
/// place, requests animation 7 (kind 2, speed 0x10) and advances the
/// sub-state.
void Actor04400_Fn073C8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work             = (Actor104400Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor104400Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}

/// Rebuild the model root's rotation: pitch about X by a sine sway driven by
/// `field_442`, then turn by the heading `field_7A`, and copy the 3x3 into the
/// root coordinate. When `field_41E` is 1, latch that pitch into `field_434`,
/// clear the flag and three motion halfwords, and advance `field_422`.
/// The `field_442` read is signed even though the field is a `u16`, because the
/// sway phase turns negative.
void Actor04400_Fn07404(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    ActorMat         rot;
    ActorMat*        src;
    MATRIX*          dst;
    s16              pitch;

    work               = (Actor104400Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->coords;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    pitch              = ((rsin((s16)work->field_442 << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
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
    if (work->field_41E == 1) {
        work->field_41E = 0;
        work->field_432 = 0;
        work->field_428 = 0;
        work->field_42A = 0;
        work->field_434 = pitch;
        work->field_422++;
    }
}

void Actor04400_Fn07530(Task* arg0)
{
    GpEnemy*         enemy;
    Actor104400Work* work;
    TmdObject*       model;
    Actor104400Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    Actor04400_Fn06374(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->flags     = model->flags | 0x80;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

/// Releases the `Gp_StateF0` reference and requests the animation that
/// follows the current one: after animation 8, 5 or 6 by `field_440`,
/// otherwise the entry of `Actor04400_D10828`. Applies it at once and
/// advances the state.
void Actor04400_Fn075F0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    Actor104400Work* work4;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->work;
    Gp_ReleaseStateF0Add(arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor104400Work*)arg0->work;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor104400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = Actor04400_D10828[anim - 1];
        work4            = (Actor104400Work*)arg0->work;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    Actor04400_Fn02B8C(arg0);
    work->field_420++;
}

void Actor04400_Fn076D0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    Actor04400_Fn02B8C(arg0);
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07750(Task* arg0)
{
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    Actor104400Work* objWork;

    enemy->recs = 0;

    objWork = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode(arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.flags` and returns the actor to the state that follows this one.
void Actor04400_Fn0781C(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags   |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void Actor04400_Fn07890(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn078D4(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    Actor04400_Fn006A8(arg0);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    work        = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor104400Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void Actor04400_Fn07968(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn07984(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->at4.loc.stage == 4) && ((u32)(gGameSession->at4.loc.area - 0x27) < 2U) && (gGameSession->at4.loc.place == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// Same body as `Actor04400_Fn07B4C` up to the two animation requests it
/// writes: this one asks for animation 0xB at speed 8 when the value overlapped
/// onto `field_44F` is 1, and animation 0x11 otherwise. Plays the same
/// encounter sound at the enemy's pan and depth and counts the sub-state up.
void Actor04400_Fn07A38(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->work;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->work;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    work->field_422 = (u16)(work->field_422 + 1);
}

/// Applies the animation `field_418` asks for: overlaps the values at
/// `Actor04400_D10814[field_418 - 1]` onto `field_44F`, then requests either
/// animation 0xC (kind 1, with sound 0x402C0002) or animation 0x11 for the
/// enemy encountered, and plays the encounter sound at its pan and depth.
/// Counts the sub-state up.
void Actor04400_Fn07B4C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->work;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->work;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    work->field_422 = (u16)(work->field_422 + 1);
}

void Actor04400_Fn07C60(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor104400Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            Actor04400_Fn06374(arg0, 1);
            work            = (Actor104400Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

void Actor04400_Fn07CF0(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00150;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07D78(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0015C;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07E00(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08208,
        Actor04400_Fn0823C,
    };

    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07E74(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00168;
    if ((Actor04400_Fn06328(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

void Actor04400_Fn07F04(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00174;
    sp.funcs[(s16)work->field_422](arg0);
}

void Actor04400_Fn07F6C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}

/// Counts `field_412` against the hold `field_446`; once it runs out, enters
/// state 4 or 1 at random. Before that, a target under 0xDAC away enters
/// state 3, and one under 0x1388 away advances the sub-state.
void Actor04400_Fn07FD0(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor104400Work* next = (Actor104400Work*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

void Actor04400_Fn08094(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor104400Work*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

void Actor04400_Fn080E8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void Actor04400_Fn08160(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = (s16)((((u32)Gp_LcgState >> 16) & 0x3F) + 0xB0);
        work->field_422++;
    }
}

/// Requests animation 0xF (kind 1, speed 0x10, `field_426` 4) and advances
/// the sub-state.
void Actor04400_Fn08208(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

void Actor04400_Fn0823C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}

/// Requests animation 0xF (kind 1, speed 0x10, `field_426` 8), advances the
/// sub-state and arms `Gp_StateF0`.
void Actor04400_Fn08290(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
    Gp_ArmStateF0(1);
}

void Actor04400_Fn082E0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void Actor04400_Fn08358(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Unless `Actor04400_Fn06328` claims the frame, a hit (`flags_EC` bit 0 as a
/// halfword, or 0x102 as a word) clears the frame counter, sets `field_438`,
/// requests animation 4 (kind 1, speed 0x10, `field_426` 4) and advances the
/// sub-state.
void Actor04400_Fn083CC(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2 = (Actor104400Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor104400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}

/// Resets the actor's slide state, then re-derives `field_70` as the view-space
/// position of slot 4 entry 0's `coords[3]`: zeroes it and walks up the `sub`
/// chain from that joint towards `&gGfxViewCoord`, transforming the point
/// through each coord's rotation and translation with the GTE and writing the
/// result into `field_70` on arrival. Snapshots the root coord's translation
/// into `field_90`..`field_94`, clears `field_412` and the `field_428` /
/// `field_42A` slide accumulators, plays the encounter sound 0x402C0002 and
/// counts the sub-state up.
void Actor04400_Fn0847C(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coords;
    GsCOORDINATE2*   current;
    SVECTOR*         pos;
    SVECTOR          local;
    VECTOR           result;
    s32              flag;

    work   = (Actor104400Work*)arg0->work;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    SndEvt_EnqueueType7(0x402C0002, 1);
    work->field_90  = coords->coord.t[0];
    work->field_92  = coords->coord.t[1];
    work->field_94  = coords->coord.t[2];
    work->field_412 = 0;
    work->field_428 = 0;
    work->field_42A = 0;
    work->field_422++;
    pos     = &work->field_70;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &((GsCOORDINATE2*)((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords)[3];
    local.vx                    = pos->vx;
    local.vy                    = pos->vy;
    local.vz                    = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return;
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

/// When `Actor04400_D10814[field_418 - 1]` is 0, requests animation
/// 9 with `field_426` 4 and plays the encounter sound 0x402C0002 at the
/// enemy's pan and depth, then sets the sub-state to 4. Otherwise requests
/// animation 7 with `field_426` 8, folds `field_41C * 4` onto `field_44F` and
/// counts the sub-state up.
void Actor04400_Fn08610(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->work;
    if (Actor04400_D10814[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

void Actor04400_Fn08718(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;
    GpEnemy*         enemy;
    TmdObject*       model;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    model           = (TmdObject*)arg0->extra;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->recs = 0;
    work2       = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->flags    = model->flags | 0x80;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn087E0(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks == 3) {
        Tmd_FreeBuffers(model);
        model->flags |= 4;
    }
    if ((s16)work->field_412 >= 0x24) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// First step of the despawn: queues sound 0x402C0002, releases the spawn
/// place claimed in `Gp_StateF0.field_1F`, unlinks the enemy node and
/// advances the state.
void Actor04400_Fn08870(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Requests the animation that follows the current one: after animation 8,
/// 5 or 6 by `field_440`, otherwise the entry of `Actor04400_D10828`. Applies
/// it at once and advances the state.
void Actor04400_Fn08908(Task* arg0)
{
    Actor104400Work* work;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->work;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = Actor04400_D10828[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    Actor04400_Fn02B8C(arg0);
    work->field_420++;
}

void Actor04400_Fn089C0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    Actor04400_Fn02B8C(arg0);
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn08A40(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;

    work                              = (Actor104400Work*)arg0->work;
    ((GpEnemy*)arg0->spawnArg2)->recs = 0;
    work2                             = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn08A9C(Task* arg0)
{
}

/// Same body as `Actor04400_Fn08870`.
void Actor04400_Fn08AA4(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Starts the death squash: drops the enemy's contact records, unlinks the
/// three collision objects, snapshots the root coordinate into `matrix_0`
/// with `field_430` at 0x1000, switches the light mode to 1, clears the frame
/// counter and advances the state.
///
/// `work` is declared before `coord` on purpose: sched1 promotes all four
/// loads to `LAUNCH_PRIORITY` and breaks that tie by descending `INSN_LUID`,
/// so the order the initialisers are emitted in is the order the loads land.
void Actor04400_Fn08B3C(Task* arg0)
{
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    Actor104400Work* objWork;

    enemy->recs = 0;

    objWork = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode(arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.flags`. Body is identical to `Actor04400_Fn0781C`'s.
void Actor04400_Fn08C08(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags   |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

/// Squashes the model vertically by the shrinking `field_430`: the root
/// coordinate takes `matrix_0` scaled by (1, field_430, 1) through a local
/// identity rotation. Frame 0x10 rotates the light mode to 2; from frame 0x21
/// the model is hidden (flag 0x80) and the state advances.
void Actor04400_Fn08C64(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    VECTOR           scale;
    ActorMat         m;
    ActorMatWords*   ident;

    work             = (Actor104400Work*)arg0->work;
    ident            = &m.ident;
    obj              = (TmdObject*)arg0->extra;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 >= 0x21) {
        obj->flags      = obj->flags | 0x80;
        work->field_412 = 0;
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn08DA4(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

s32 Actor04400_Fn08DBC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
