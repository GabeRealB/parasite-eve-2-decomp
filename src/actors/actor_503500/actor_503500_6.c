#include "common.h"

#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "actors/actor_503500.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_801366fc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_503500_80131E44;
extern TaskFuncTable3 D_actor_503500_80131F4C;
extern TaskFuncTable3 D_actor_503500_80131F9C;
extern TaskFuncTable3 D_actor_503500_80131FF0;
extern TaskFuncTable3 D_actor_503500_80132028;
extern TaskFuncTable3 D_actor_503500_80132060;
extern TaskFuncTable3 D_actor_503500_80132098;
extern TaskFuncTable3 D_actor_503500_801320D0;
extern TaskFuncTable3 D_actor_503500_80132108;
extern TaskFuncTable3 D_actor_503500_80132178;
extern TaskFuncTable3 D_actor_503500_801321DC;
extern TaskFuncTable3 D_actor_503500_801321E8;
extern TaskFuncTable3 D_actor_503500_801321F4;
extern TaskFuncTable3 D_actor_503500_80132218;
extern TaskFuncTable3 D_actor_503500_80132224;

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];
/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_8014642C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80176530[];
void              func_actor_503500_801324EC(Task* arg0);
extern Task*      D_actor_503500_80176558;
extern TaskDesc   D_actor_503500_8014B964;
extern s8         D_actor_503500_80176D5A;
extern s16        D_actor_503500_80176D2E;
/// 18-entry table of per-slot u16 counters; slot 0x11 is the shared counter
/// bumped by `func_actor_503500_80137290` and drained by `_801372AC`.
extern u16 D_actor_503500_80176D64[];
extern u16 D_actor_503500_80176D24;
/// Per-spawn enemy parameter table indexed by `Task::spawnArg1`;
/// `func_actor_503500_8013BEE4` and `func_actor_503500_8013ECBC` park the row
/// in `GpEnemy::field_50` and seed the enemy's HP from its `field_4`.
extern GpPairSrcE D_actor_503500_8016E7EC[];
/// Local offset the 0xF4 enemy applies to both its `GpEnemy::field_1C` and
/// its display node's 0x10 vector.
extern SVECTOR         D_actor_503500_8016F36C;
extern Actor503500Work D_actor_503500_80177A6C;
/// The same pair for the other 0xF4 enemy, the one at
/// `D_actor_503500_801776A0`.
extern SVECTOR         D_actor_503500_8016F1B0;
extern Actor503500Work D_actor_503500_801776A0;
/// The same pair for the 0xF0 enemy at `D_actor_503500_8017797C`.
extern SVECTOR         D_actor_503500_8016F2D8;
extern Actor503500Work D_actor_503500_8017797C;
/// The same pair for the 0x160 enemy at `D_actor_503500_80176D88`: a world
/// translation seeded into the task's own coordinate and the local offset its
/// `GpEnemy::field_1C` and display node share.
extern SVECTOR         D_actor_503500_8016F060;
extern SVECTOR         D_actor_503500_8016F068;
extern Actor503500Work D_actor_503500_80176D88;
/// Row of `Gp_PackPair` arguments, one slot per effect task in this file
/// (`func_actor_503500_801448E8` takes the slot before this one). Declared as
/// an array because the read has to alias the struct stores around it: GCC
/// 2.8.1's `fixed_scalar_and_varying_struct_p` lets a scalar global at a fixed
/// address float above them, and hoists the load out of the call sequence.
extern GpU16Pair* D_actor_503500_8016E7D4[];
/// Local offset of the display node `func_actor_503500_80144E8C` links, and the
/// offsets it seeds its `GpActorD4Rec` with.
extern Actor503500UVec D_actor_503500_801715C4;
extern Actor503500UVec D_actor_503500_801715CC;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_503500_8014CD98[];
extern u8 D_actor_503500_8014D098[];
/// whatever room overlay is resident owns the body.
extern void              func_8017E27C(s32 arg0);
extern Actor503500MsgPos D_actor_503500_8017655C;
/// Player-facing flag byte in the main executable; no module header owns it yet.
extern u8 D_80073BA9;
/// Main-executable globals with no module header yet: `D_80071075` gates the
/// "everything is dead" message, `D_80073BA0` is the remaining-enemy count and
/// `D_80114C12` the cutscene/among-us mode flag.
extern u8  D_80071075;
extern s16 D_80073BA0;
extern s8  D_80114C12;
s32        func_actor_503500_80133684(Actor503500* arg0);
/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32 func_actor_503500_80135E04(Task* arg0, s32 arg1);
/// Records the per-slot halfword for slot `arg1`; `arg0` is ignored the same
/// way `func_actor_503500_80135E04` ignores it.
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2);
void func_actor_503500_801338E8(Actor503500* arg0);
void func_actor_503500_80134408(Actor503500* arg0);
void func_actor_503500_801345F4(Actor503500* arg0);
void func_actor_503500_80134A24(Actor503500* arg0);
void func_actor_503500_80134C68(Actor503500* arg0);
/// Re-places a display node and its record table: `arg2` is the node's
/// `GpRec18` table and `arg3` the record count.
void func_actor_503500_80134EAC(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 arg2);
s32  func_actor_503500_80136014(Actor503500* arg0, s32 arg1);
void func_actor_503500_8013611C(s32 arg0);
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void func_actor_503500_801372AC(s32 arg0);
void func_actor_503500_80138288(Actor503500* arg0);
void func_actor_503500_80136450(Actor503500* arg0);
void func_actor_503500_801369E4(Actor503500* arg0);
void func_actor_503500_80136A80(Actor503500* arg0);
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1);
s32  func_actor_503500_80136FA8(Actor503500Work* work, s32 slot);
s32  func_actor_503500_80136FDC(Actor503500Work* work, s32 slot);
/// Asks slot `slot` to die: `arg2` becomes its `field_7E0`/`field_2A` flag
/// and `arg3` its `field_752` countdown.
void func_actor_503500_80136F40(Actor503500Work* work, s32 slot, s32 arg2, s32 arg3);
void func_actor_503500_801374BC(Actor503500* arg0);
void func_actor_503500_80137678(Actor503500* arg0);
void func_actor_503500_80138454(Actor503500* arg0);
void func_actor_503500_80138490(Actor503500* arg0, s32 arg1);
/// Defined as `s8` in actor_503500_4.c; an `s8` prototype makes the caller
/// sign-extend with `sll 24` before the zero test.
s32  func_actor_503500_80136208(void);
void func_actor_503500_8013AF60(Actor503500* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void func_actor_503500_8013CCBC(Actor503500* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void func_actor_503500_8013C088(Actor503500* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void func_actor_503500_8013DEB4(Actor503500* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void func_actor_503500_8013EA2C(Actor503500* arg0);
void func_actor_503500_8013EC20(Actor503500* arg0, s32 arg1);
void func_actor_503500_8013EE5C(Actor503500* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void func_actor_503500_80139A20(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_801431EC(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_80137C90(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_80140D38(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_8014215C(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_801437D0(Actor503500* arg0, GpRec18* arg1, s32 arg2);
void func_actor_503500_8013B460(Actor503500* arg0);
void func_actor_503500_8013B8D0(Actor503500* arg0);
void func_actor_503500_8013BE0C(Actor503500* arg0);
void func_actor_503500_8013BCB4(Actor503500* arg0);
void func_actor_503500_8013C900(Actor503500* arg0);
void func_actor_503500_8013C9DC(Actor503500* arg0);
void func_actor_503500_8013C960(Actor503500* arg0);
void func_actor_503500_8013CA34(Actor503500* arg0);
void func_actor_503500_8013CA74(Actor503500* arg0, s8 arg1);
void func_actor_503500_8013D8BC(Actor503500* arg0);
void func_actor_503500_8013D914(Actor503500* arg0);
void func_actor_503500_8013D990(Actor503500* arg0);
void func_actor_503500_8013BD0C(Actor503500* arg0);
void func_actor_503500_8013BD88(Actor503500* arg0);
void func_actor_503500_8013C558(Actor503500* arg0);
void func_actor_503500_8013E384(Actor503500* arg0);
void func_actor_503500_8013E740(Actor503500* arg0);
void func_actor_503500_8013EBE4(Actor503500* arg0);
void func_actor_503500_8013EA8C(Actor503500* arg0);
void func_actor_503500_8013EAE4(Actor503500* arg0);
void func_actor_503500_8013EB60(Actor503500* arg0);
void func_actor_503500_8013F778(Actor503500* arg0);
void func_actor_503500_8013F7D8(Actor503500* arg0);
void func_actor_503500_8013F830(Actor503500* arg0);
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_801398D0(Actor503500* arg0);
void      func_actor_503500_80139EFC(Actor503500* arg0);
void      func_actor_503500_8013A0D0(Actor503500* arg0);
void      func_actor_503500_8013A96C(Actor503500* arg0);
void      func_actor_503500_8013AA44(Actor503500* arg0);
void      func_actor_503500_8013AAC0(Actor503500* arg0);
void      func_actor_503500_8013AB38(Actor503500* arg0);
void      func_actor_503500_8013DBA8(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8013F328(Actor503500* arg0);
void      func_actor_503500_8013F4A4(Actor503500* arg0);
void      func_actor_503500_8013F948(Actor503500* arg0);
void      func_actor_503500_8013F984(Actor503500* arg0);
void      func_actor_503500_8013F9D4(Actor503500* arg0, s32 arg1);
void      func_actor_503500_80140BE8(Actor503500* arg0);
void      func_actor_503500_80141248(Actor503500* arg0);
void      func_actor_503500_80141448(Actor503500* arg0);
void      func_actor_503500_80141B94(Actor503500* arg0);
void      func_actor_503500_80141D7C(Actor503500* arg0);
void      func_actor_503500_801420C4(Actor503500* arg0);
void      func_actor_503500_801421A8(Actor503500* arg0);
void      func_actor_503500_80142310(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8014271C(Actor503500* arg0);
void      func_actor_503500_80142980(Actor503500* arg0);
void      func_actor_503500_80143FFC(Actor503500* arg0);
void      func_actor_503500_80144004(Actor503500* arg0);
void      func_actor_503500_80144098(Actor503500* arg0, s32 arg1, GpEnemy* arg2);
void      func_actor_503500_8014418C(Actor503500* arg0);
void      func_actor_503500_801441E8(Actor503500* arg0);
void      func_actor_503500_80144238(Actor503500* arg0, s32 arg1);
void      func_actor_503500_80144520(Actor503500* arg0);
void      func_actor_503500_80144778(Actor503500* arg0);
void      func_actor_503500_80144B40(Actor503500* arg0);
void      func_actor_503500_80144E10(Task* arg0);
void      func_actor_503500_80145480(Task* arg0);
void      func_actor_503500_801450A0(Actor503500* arg0);
void      func_actor_503500_801454E0(Actor503500* arg0);
void      func_actor_503500_80145754(Actor503500* arg0);
void      func_actor_503500_801459B0(Task* arg0);
void      func_actor_503500_80145C50(Actor503500* arg0);
void      func_actor_503500_80145F18(Actor503500* arg0);
/// Reports whether the boss-wide gate is open; the body ignores its
/// argument, and callers pass unrelated pointers they already hold.
s32       func_actor_503500_8013608C(void* arg0);
extern s8 D_80071090;
void      func_actor_503500_80137074(Actor503500* arg0, s8 arg1, s16 arg2);
void      func_actor_503500_80137048(Actor503500* arg0, s32 rate);
/// Applies preset `arg2` to the boss block's animation slots; `arg1` and `arg3`
/// are passed by every caller but the body ignores them.
void func_actor_503500_80135950(Actor503500* arg0, s32 arg1,
                                Actor503500AnimPreset* arg2, s32 arg3);
/// The animation preset `func_actor_503500_80136D30` re-applies when the boss
/// finishes the clip it was gating on.
extern Actor503500AnimPreset D_actor_503500_8016EAD4;

void func_actor_503500_80136304(Actor503500* arg0)
{
    Actor503500Work* work = arg0->field_1C;
    u16              timer;

    switch (work->field_7B0) {
        case 0:
            if (func_actor_503500_80133684(arg0) == 0) {
                func_actor_503500_80136450(arg0);
            }
            break;
        case 1:
            func_actor_503500_801338E8(arg0);
            break;
        case 2:
            func_actor_503500_801369E4(arg0);
            break;
        case 3:
            timer           = work->field_7B2 - 1;
            work->field_7B2 = timer;
            if ((s16)timer < 0) {
                func_actor_503500_80135FB4(arg0, 6, 0x10);
                work->field_7B2 = 3;
            }
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                func_actor_503500_80136EFC(arg0, 0);
                work->field_7CA = 0x3C;
            }
            break;
        case 4:
            func_actor_503500_80134408(arg0);
            break;
        case 5:
            func_actor_503500_80136A80(arg0);
            break;
        case 6:
            func_actor_503500_801345F4(arg0);
            break;
        case 7:
            func_actor_503500_80134A24(arg0);
            break;
    }
    if (work->field_7E0 != 0) {
        func_actor_503500_80134C68(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136450);
/// Script step for the boss's slot-0 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x1E once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801364D0);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013656C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013667C);

/// Script step for the boss's slot-12 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x96 once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136770);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013680C);

/// Script step for the boss's slot-9 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x1E once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136948);
void func_actor_503500_801369E4(Actor503500* arg0)
{
    Actor503500Work* work;

    work = arg0->field_1C;
    switch ((s8)work->field_7DA) {
        case 0:
            func_actor_503500_80135FB4(arg0, 0xE, 0x10);
            func_actor_503500_8013611C(arg0->spawnArg1);
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (func_actor_503500_80136014(arg0, 0xE) != 0) {
                work->field_7D2 = 0;
                func_actor_503500_80136EFC(arg0, 0);
            }
            break;
    }
}

void func_actor_503500_80136A80(Actor503500* arg0)
{
}

/// Ticks the boss's second-body-part countdown down to zero, then re-places
/// that part's display node and its 8-record collision table.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136A88);
/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `Gp_UpdateActorColor` with no blend parameters.
void func_actor_503500_80136AEC(Actor503500* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra->field_8->workm.t[0];
    vec.vy = arg0->extra->field_8->workm.t[1];
    vec.vz = arg0->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136B64);

/// Per-frame animation tick of the boss block. While the slot array is seeded
/// (`field_7D4`), a clear 0x100 bit in the animation flags means the clip is
/// still running, so every slot 1..0x13 is ticked; once the bit is set the clip
/// has finished, and in state 0 the boss resets the slot rates and re-applies
/// preset `D_actor_503500_8016EAD4`. The block is passed to `Gp_AnimTickIndex`
/// as the `GpAnimCtx` it is fronted by (`Actor503500WorkBoss::anim`).
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136D30);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136DDC);

/// Puts the boss into state `arg1`: clears the state's step counters and the two
/// per-state halfwords, asks `func_actor_503500_80137074` for sub-state 3 with
/// its flag set only for state 3, and drops the main-executable flag.
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7B0 = arg1;
    work->field_7DA = 0;
    work->field_7DB = 0;
    work->field_7BC = 0;
    work->field_7BE = 0;
    func_actor_503500_80137074(arg0, arg1 == 3, 3);
    D_80071090 = 0;
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136F40);

/// True when enemy slot `slot` is either unoccupied or has its `field_730`
/// counter at zero -- i.e. the slot has nothing left to wait for.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136FA8);
/// The stricter form of `func_actor_503500_80136FA8`: slot `slot` is ready when
/// it is occupied, not marked dying by `field_752`, and its `field_730` counter
/// has run out. Slot 0 stands for the boss itself, which is ready when the
/// main-executable flag `field_7E0` and its own `field_752` are both clear.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80136FDC);
/// Sets `GpAnimSlot::field_9` -- the per-slot value `Gp_AnimResetSlot` seeds
/// with 0x10 -- on animation slots 1..16 of the boss block, `rate` of 0
/// meaning that default.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137048);
void func_actor_503500_80137074(Actor503500* arg0, s8 arg1, s16 arg2)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7E2 = arg1;
    work->field_7CC = arg2;
}

/// Places the boss's part at `args`: drops the translation into the root
/// coordinate's local matrix, stores the Euler angles in the coordinate's own
/// `rot` slot and rebuilds the rotation from them, exactly as
/// `ActorsShared8013231c` does. It then keeps two derived copies in the work
/// block -- the yaw recovered from the matrix it just built, and the same
/// translation in 16.16 fixed point. Clearing `flg` makes `Gp_UpdateCoordTree`
/// recompute the world matrix from the new local one.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137088);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137158);

void func_actor_503500_80137238(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131E44;
    sp.funcs[task->state](task);
}

void func_actor_503500_80137290(s32 arg0)
{
    D_actor_503500_80176D64[0x11] += arg0;
}

void func_actor_503500_801372AC(s32 arg0)
{
    D_actor_503500_80176D64[0x11] -= arg0;
}

/// State-0 init of the 0x160 enemy at `D_actor_503500_80176D88`, built like
/// `func_actor_503500_8013BEE4`: clears the block, hangs the task's coordinate
/// off part 8 of the parent's model, republishes the parent's light and colour
/// matrices, links the enemy node and the display node parked at `slot40`, and
/// starts the block in sub-state 0.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801372C8);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801374BC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80131F4C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137678);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137C90);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013815C);

void func_actor_503500_80138288(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->slot40.obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
void func_actor_503500_801382F4(void)
{
}

/// Steps the 0x160 block's countdown at 0x158 down to zero, then, unless the
/// global freeze is on, runs the block's display node through its record table
/// before releasing the table.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801382FC);
void func_actor_503500_80138378(Actor503500* arg0)
{
    GpEnemy* obj;
    u8       flags;
    u8       flags2;

    obj   = arg0->field_20;
    flags = obj->field_4C;
    if (flags & 1) {
        obj->field_4C = flags & 0xFE;
    }
    if (obj->field_4C & 2) {
        obj->field_4C = obj->field_4C & 0xFD;
    }
    flags2 = obj->field_4C;
    if (flags2 & 0xC) {
        obj->field_4C = flags2 & 0xF3;
    }
}

void func_actor_503500_801383D0(Actor503500* arg0)
{
    switch (arg0->field_1C->field_15C) {
        case 0:
            func_actor_503500_80138454(arg0);
            break;
        case 1:
            func_actor_503500_801374BC(arg0);
            break;
        case 2:
            func_actor_503500_80137678(arg0);
            break;
    }
}

void func_actor_503500_80138454(Actor503500* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_80138490(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// Puts the 0x160 block into sub-state `arg1`: clears the phase and frame
/// counter that go with it, cancels a pending kill, and records the slot's
/// halfword as "asked to die" when the sub-state is non-zero.
void func_actor_503500_80138490(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->field_1C;

    work->field_15C     = arg1;
    work->field_15D     = 0;
    work->field_15A     = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_801384D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131F4C;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013852C);

void func_actor_503500_80138898(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s8               countdown;
    s32              slot;

    work      = arg0->field_1C;
    enemy     = arg0->field_20;
    countdown = work->field_2EB;
    /* `Task::extra` is a `TmdObject`: the model instance and the actor-ext
     * record documented in `main/session.h` are the same object. */
    tmd = (TmdObject*)arg0->extra;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(tmd);
        }
        work->field_2EB = (s8)((u8)work->field_2EB - 1);
    }
    if (Game_Session->field_1 != 0) {
        slot = 0xB;
        if (arg0->spawnArg1 < 3) {
            slot = 0xA;
        }
        if (func_actor_503500_80135E04(arg0->parent, slot) == 0) {
            tmd->field_C |= 4;
        } else {
            goto tick;
        }
    } else {
    tick:
        func_actor_503500_80135828(arg0, &work->field_2EB);
    }

    switch (D_801153F4) {
        case 1:
            if (!(tmd->field_C & 0x80)) {
                func_actor_503500_8013AAC0(arg0);
            }
            break;
        case 2:
            tmd->field_C        |= 0x80;
            enemy->node.field_4 |= 1;
            break;
        default:
            if (enemy->field_4C != 0) {
                func_actor_503500_801398D0(arg0);
            }
            func_actor_503500_8013AA44(arg0);
            func_actor_503500_8013A96C(arg0);
            if (work->field_2EA == 0) {
                func_actor_503500_80139EFC(arg0);
                func_actor_503500_8013A0D0(arg0);
            }
            func_actor_503500_8013AAC0(arg0);
            func_actor_503500_8013AB38(arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80138A30);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80138C08);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80139014);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801395BC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801398D0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80139A20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80139EFC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013A0D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013A470);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013A7B0);

void func_actor_503500_8013A900(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj160);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013A96C);

/// Steps the 0x2EC block's countdown at 0x2D8 down to zero, then, unless the
/// global freeze is on, runs the 0x160 display node through its record table
/// before releasing the table. Same shape as `func_actor_503500_8013BD0C`.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AA44);
void func_actor_503500_8013AAC0(Actor503500* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra->field_8->workm.t[0];
    vec.vy = arg0->extra->field_8->workm.t[1];
    vec.vz = arg0->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AB38);

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AC6C);
/// The 0x2EC block's counterpart of `func_actor_503500_80138490`: puts the
/// block into sub-state `arg1`, clears the phase and frame counter that go with
/// it, cancels a pending kill, and records the slot's halfword as "asked to
/// die" when the sub-state is non-zero.
void func_actor_503500_8013ACC4(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->field_1C;

    work->field_2D4     = arg1;
    work->field_2E4     = 0;
    work->field_2E5     = 0;
    work->field_2DE     = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_8013AD0C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131F9C;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AD64);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80131FF0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AF60);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013B460);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013B60C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013B8D0);

void func_actor_503500_8013BBCC(Actor503500* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->field_20;
    coord = arg0->extra->field_8;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.field_4 |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->field_4C != 0) {
        func_actor_503500_8013BCB4(arg0);
    }
    func_actor_503500_8013BD0C(arg0);
    func_actor_503500_8013BD88(arg0);
}

void func_actor_503500_8013BC54(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy                                       = arg0->field_20;
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013BCB4);

void func_actor_503500_8013BD0C(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->field_1C;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013AF60(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013BD88(Actor503500* arg0)
{
    switch (arg0->field_1C->field_ED) {
        case 0:
            func_actor_503500_8013BE0C(arg0);
            break;
        case 1:
            func_actor_503500_8013B460(arg0);
            break;
        case 2:
            func_actor_503500_8013B8D0(arg0);
            break;
    }
}

/// The 0xF0 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013BE0C(Actor503500* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013BE48(arg0, 1);
        arg0->killCountdown = 0;
    }
}
/// The 0xF0 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and frame counter that go with it,
/// cancels a pending kill, and records the slot's halfword as "asked to die"
/// when the sub-state is non-zero.
void func_actor_503500_8013BE48(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->field_1C;

    work->field_ED      = arg1;
    work->field_EE      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_8013BE8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131FF0;
    sp.funcs[task->state](task);
}

/// State-0 init of the 0xF4 enemy at `D_actor_503500_801776A0`, the twin of
/// `func_actor_503500_8013ECBC`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 16 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013BEE4);
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132028);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C088);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C558);

/// Per-frame tick of the first 0xF4 block, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C878);
void func_actor_503500_8013C900(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy                                       = arg0->field_20;
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C960);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C9DC);

void func_actor_503500_8013CA34(Actor503500* arg0)
{
    switch (arg0->field_1C->field_F0) {
        case 0:
            break;
        case 1:
            func_actor_503500_8013C558(arg0);
            break;
    }
}
void func_actor_503500_8013CA74(Actor503500* arg0, s8 arg1)
{
    Actor503500Work* work;

    work           = arg0->field_1C;
    work->field_F0 = arg1;
    work->field_F1 = 0;
    work->field_EA = 0;
}

void func_actor_503500_8013CA8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132028;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013CAE4);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132060);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013CCBC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D1CC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D558);

/// Per-frame tick of the second 0xF4 block, the same shape as
/// `func_actor_503500_8013BBCC`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D7D4);
void func_actor_503500_8013D85C(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy                                       = arg0->field_20;
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D8BC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D914);
void func_actor_503500_8013D990(Actor503500* arg0)
{
    s8 temp_v1;

    temp_v1 = arg0->field_1C->field_F0;
    switch (temp_v1) {
        case 0:
            func_actor_503500_8013DC4C(arg0);
            break;
        case 1:
            func_actor_503500_8013DA2C(arg0, 0x258);
            break;
        case 2:
            func_actor_503500_8013D1CC(arg0);
            break;
        case 3:
            func_actor_503500_8013D558(arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013DA2C);

/// The second 0xF4 block's counterpart of `func_actor_503500_8013BE48`: puts
/// the block into sub-state `arg1` (the one `func_actor_503500_8013D990`
/// dispatches on), clears the phase and the two counters that go with it,
/// cancels a pending kill, and records the slot's halfword as "asked to die"
/// for every sub-state but 1.
void func_actor_503500_8013DBA8(Actor503500* arg0, s32 arg1)
{
    Actor503500Work770E8* work = (Actor503500Work770E8*)arg0->field_1C;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EC      = 0;
    work->field_EE      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 1);
}
void func_actor_503500_8013DBF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132060;
    sp.funcs[task->state](task);
}

void func_actor_503500_8013DC4C(Actor503500* arg0)
{
    Actor503500Work* work;
    s32              kind;
    s32              slotA;
    s32              slotB;

    kind = 3;
    if (arg0->spawnArg1 == 7) {
        kind  = 2;
        slotA = 0xD;
        slotB = 0xE;
    } else {
        slotA = 0xF;
        slotB = 0x10;
    }
    if ((func_actor_503500_80135E04(arg0->parent, kind) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotA) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotB) != 0)) {
        func_actor_503500_8013DBA8(arg0, 1);
        work             = arg0->field_1C;
        work->obj.flags |= 0x8000;
        Gp_LinkNode(&arg0->field_20->node);
    }
}

/// State-0 init of the 0xF0 enemy at `D_actor_503500_8017797C`, the same shape
/// as `func_actor_503500_8013BEE4`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 1 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013DD10);
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132098);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013DEB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013E384);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013E740);

/// The third 0xF0 block's per-frame tick, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013E9A4);
void func_actor_503500_8013EA2C(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy                                       = arg0->field_20;
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013EA8C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013EAE4);
void func_actor_503500_8013EB60(Actor503500* arg0)
{
    switch (arg0->field_1C->field_EC) {
        case 0:
            func_actor_503500_8013EBE4(arg0);
            break;
        case 1:
            func_actor_503500_8013E384(arg0);
            break;
        case 2:
            func_actor_503500_8013E740(arg0);
            break;
    }
}

/// The third 0xF0 block's counterpart of `func_actor_503500_80138454`: when a
/// kill is pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013EBE4(Actor503500* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013EC20(arg0, 1);
        arg0->killCountdown = 0;
    }
}
/// The third 0xF0 block's counterpart of `func_actor_503500_8013BE48`: puts the
/// block into sub-state `arg1` (the one `func_actor_503500_8013EB60`
/// dispatches on), clears the phase and frame counter that go with it, cancels
/// a pending kill, and records the slot's halfword as "asked to die" when the
/// sub-state is non-zero.
void func_actor_503500_8013EC20(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->field_1C;

    work->field_EC      = arg1;
    work->field_ED      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_8013EC64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132098;
    sp.funcs[task->state](task);
}

/// State-0 init of the 0xF4 enemy at `D_actor_503500_80177A6C`: clears the
/// block, resets the task's own coordinate to a plain 4096 identity, parents
/// it to part 8 of the parent task's model, links the enemy node and its
/// display node, and hands the block to sub-state 3.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013ECBC);
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_801320D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013EE5C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F328);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F4A4);

/// The 0xF4 block's per-frame tick, the same shape as
/// `func_actor_503500_8013E9A4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F6F0);
void func_actor_503500_8013F778(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy                                       = arg0->field_20;
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F7D8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F830);
void func_actor_503500_8013F8AC(Actor503500* arg0)
{
    switch (arg0->field_1C->field_F0) {
        case 0:
            func_actor_503500_8013F948(arg0);
            break;
        case 1:
            func_actor_503500_8013F328(arg0);
            break;
        case 2:
            func_actor_503500_8013F4A4(arg0);
            break;
        case 3:
            func_actor_503500_8013F984(arg0);
            break;
    }
}

/// The 0xF4 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013F948(Actor503500* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013F9D4(arg0, 1);
        arg0->killCountdown = 0;
    }
}
/// The 0xF4 block's sub-state 3 handler: when a kill is pending, hands the
/// block to sub-state 0 and hides its display node.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013F984);
/// The 0xF4 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and the two counters that go with
/// it, cancels a pending kill, and records the slot's halfword as "asked to
/// die" when the sub-state is non-zero.
void func_actor_503500_8013F9D4(Actor503500* arg0, s32 arg1)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->field_1C;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EA      = 0;
    work->field_EC      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_8013FA1C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801320D0;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013FA74);

void func_actor_503500_8013FF0C(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s8               countdown;
    s32              slot;

    work      = arg0->field_1C;
    enemy     = arg0->field_20;
    countdown = work->field_3D7;
    tmd       = (TmdObject*)arg0->extra;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(tmd);
        }
        work->field_3D7 = (s8)((u8)work->field_3D7 - 1);
    }
    if (Game_Session->field_1 != 0) {
        slot = 0xB;
        if (arg0->spawnArg1 < 0xF) {
            slot = 0xA;
        }
        if (func_actor_503500_80135E04(arg0->parent, slot) == 0) {
            tmd->field_C |= 4;
        } else {
            goto tick;
        }
    } else {
    tick:
        func_actor_503500_80135828(arg0, &work->field_3D7);
    }

    switch (D_801153F4) {
        case 1:
            if (!(tmd->field_C & 0x80)) {
                func_actor_503500_801421A8(arg0);
            }
            break;
        case 2:
            tmd->field_C        |= 0x80;
            enemy->node.field_4 |= 1;
            break;
        default:
            if (enemy->field_4C != 0) {
                func_actor_503500_80140BE8(arg0);
            }
            func_actor_503500_801420C4(arg0);
            func_actor_503500_80141D7C(arg0);
            if (work->field_3D6 == 0) {
                func_actor_503500_80141248(arg0);
                func_actor_503500_80141448(arg0);
            }
            func_actor_503500_801421A8(arg0);
            func_actor_503500_80141B94(arg0);
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132108);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801400A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80140654);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80140BE8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80140D38);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141248);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141448);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014176C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141A44);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141B94);

void func_actor_503500_80141D04(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj160);
    Gp_UnlinkObj(&arg0->field_1C->obj240);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141D7C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141E64);

void func_actor_503500_80141F48(Actor503500* arg0)
{
    Actor503500Work* work;
    u16              level;

    work            = arg0->field_1C;
    level           = work->field_3B2 + 0x10;
    work->field_3B2 = level;
    if ((s16)level >= 0x1001) {
        Gp_LinkNode(&arg0->field_20->node);
        work->field_3B2     = 0x1000;
        work->obj160.flags |= 0x8000;
        func_actor_503500_80142310(arg0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80141FC8);

/// Steps the 0x3D8 block's countdown at 0x3A8 down to zero, then, unless the
/// global freeze is on, runs both display nodes through their record tables
/// before releasing the tables. Same shape as `func_actor_503500_80144004`.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801420C4);
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014215C);

/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `Gp_UpdateActorColor` with no blend parameters. Same body as
/// `func_actor_503500_80136AEC`.
void func_actor_503500_801421A8(Actor503500* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra->field_8->workm.t[0];
    vec.vy = arg0->extra->field_8->workm.t[1];
    vec.vz = arg0->extra->field_8->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
/// Re-aims coordinate nodes 2..7 of the model: each node's rotation is read
/// back as Euler angles, its pitch replaced with the caller's per-node angle,
/// and the matrix rebuilt from the result. The identity splat before
/// `RotMatrixZYX` clears the node's rotation with five aligned stores, the same
/// idiom `func_800B0928` uses.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80142220);
/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801422B8);
void func_actor_503500_80142310(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work;

    work                = arg0->field_1C;
    work->field_3A4     = arg1;
    work->field_3D0     = 0;
    work->field_3D1     = 0;
    work->field_3AE     = 0;
    work->field_3B0     = 0;
    work->field_39C     = 0x600000;
    work->obj240.flags &= 0x7FFF;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}
void func_actor_503500_80142370(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132108;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801423C8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014271C);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80142980);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801431EC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801437D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80143AC0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_801321DC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_801321E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80143EB4);
void func_actor_503500_80143F78(Actor503500* arg0)
{
    GpEnemy*            enemy;
    Actor503500Work224* work;

    enemy = arg0->field_20;
    work  = (Actor503500Work224*)arg0->field_1C;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)arg0->extra->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj2);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
void func_actor_503500_80143FFC(Actor503500* arg0)
{
}

void func_actor_503500_80144004(Actor503500* arg0)
{
    Actor503500Work224* work;
    s16                 timer;

    work = (Actor503500Work224*)arg0->field_1C;
    if (work->field_218 != 0) {
        timer           = (u16)work->field_218 - 1;
        work->field_218 = timer;
        if (timer < 0) {
            work->field_218 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_801431EC(arg0, &work->obj0, work->rec0, 8);
        func_actor_503500_801437D0(arg0, work->rec1, 4);
    }
    Gp_ClearRec18Occupied(work->rec0);
    Gp_ClearRec18Occupied(work->rec1);
}
INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144098);

void func_actor_503500_801440F0(Actor503500* arg0)
{
    switch (arg0->field_1C->field_221) {
        case 0:
            func_actor_503500_8014418C(arg0);
            break;
        case 1:
            func_actor_503500_8014271C(arg0);
            break;
        case 2:
            func_actor_503500_80142980(arg0);
            break;
        case 3:
            func_actor_503500_801441E8(arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014418C);
void func_actor_503500_801441E8(Actor503500* arg0)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7E |= 0x8000;
    Gp_LinkNode(&arg0->field_20->node);
    func_actor_503500_80144238(arg0, 0);
}

void func_actor_503500_80144238(Actor503500* arg0, s32 arg1)
{
    Actor503500Work224* work;

    work                = (Actor503500Work224*)arg0->field_1C;
    work->field_221     = arg1;
    work->field_222     = 0;
    work->field_21A     = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
    work->obj1.flags &= 0x7FFF;
    work->obj2.flags &= 0x7FFF;
    SndEvt_EnqueueType7(0x40230009, 1);
}
void func_actor_503500_801442A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132178;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144300);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144520);

void func_actor_503500_801446E4(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    coord = arg0->extra->field_8;
    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->flg = 0;
    func_actor_503500_80144778(arg0);
    func_actor_503500_80144520(arg0);
}

void func_actor_503500_8014473C(Task* arg0)
{
    func_actor_503500_801372AC(1);
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->idMap)->obj);
    Task_Kill(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144778);

void func_actor_503500_80144890(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321DC;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801448E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144B40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144D50);
void func_actor_503500_80144DA8(Task* arg0)
{
    func_actor_503500_801372AC(3);
    if (arg0->spawnArg1 == 0) {
        SndEvt_EnqueueType7(0x40230008, 1);
    } else {
        SndEvt_EnqueueType7(0x40230007, 1);
    }
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->idMap)->obj);
    Task_Kill(arg0);
}
void func_actor_503500_80144E10(Task* arg0)
{
    Gp_ClearRec18Occupied(&((Actor503500ObjWork*)arg0->idMap)->rec);
}

void func_actor_503500_80144E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321E8;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80144E8C);
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801450A0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132218);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132224);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145428);
void func_actor_503500_80145480(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(6);
    SndEvt_EnqueueType7(0x4023000B, 1);
    ext                                 = arg0->extra;
    ((GsCOORDINATE2*)ext->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->idMap)->obj);
    Task_Kill(arg0);
}

void func_actor_503500_801454E0(Actor503500* arg0)
{
    Actor503500WorkRec4* work;
    GpRec18*             rec;
    s32                  i;

    work = (Actor503500WorkRec4*)arg0->field_1C;
    rec  = work->rec;
    for (i = 0; i < 4; i++) {
        if ((rec[i].field_4 & 0xFFFF0000) == 0x10000) {
            work->obj.flags &= 0x7FFF;
        }
    }
    Gp_ClearRec18Occupied(rec);
}

void func_actor_503500_8014554C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321F4;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801455A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145754);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801458F8);
void func_actor_503500_80145950(Task* arg0)
{
    TmdObject* ext;

    SndEvt_EnqueueType7(0x4023000C, 1);
    func_actor_503500_801372AC(6);
    ext                                 = arg0->extra;
    ((GsCOORDINATE2*)ext->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->idMap)->obj);
    Task_Kill(arg0);
}

void func_actor_503500_801459B0(Task* arg0)
{
    Gp_ClearRec18Occupied(&((Actor503500ObjWork*)arg0->idMap)->rec);
}

void func_actor_503500_801459D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132218;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145A2C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145C50);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145E1C);
void func_actor_503500_80145E98(Task* arg0)
{
    TmdObject* ext;

    func_actor_503500_801372AC(8);
    SndEvt_EnqueueType7(0x4023000E, 1);
    SndEvt_EnqueueType7(0x40230013, 1);
    SndEvt_EnqueueType7(0x4023000F, 1);
    ext                                 = arg0->extra;
    ((GsCOORDINATE2*)ext->field_8)->sub = &Gfx_ViewCoord;
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->idMap)->obj);
    Task_Kill(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145F18);
void func_actor_503500_80145F84(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132224;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80145FDC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014618C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_801463C0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8014642C);