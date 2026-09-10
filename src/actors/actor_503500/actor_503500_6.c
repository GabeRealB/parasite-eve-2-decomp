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
extern TaskFuncTable3 D_actor_503500_80132230;

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];
/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_8014642C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80176530[];
void              func_actor_503500_801324EC(Task* arg0);
extern Task*      D_actor_503500_80176558;
extern TaskDesc   D_actor_503500_8014B964;
extern TaskDesc   D_actor_503500_8016E9F0;
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
/// Per-slot parent part index and local offset of the two 0xF0 enemies in
/// `D_actor_503500_801774C0`, indexed by `spawnArg1 - 4`.
extern s32                  D_actor_503500_8016F0E8[];
extern SVECTOR              D_actor_503500_8016F0F0[];
extern Actor503500Work774C0 D_actor_503500_801774C0[];
extern RECT                 D_actor_503500_8016F100;
/// Offsets `func_actor_503500_8013F4A4` spawns its two effects at: rows 0-5
/// for the per-frame effect, rows 3-5 for the odd-frame one.
extern SVECTOR D_actor_503500_8016F374[];
extern RECT    D_actor_503500_8016F3A4;
/// The same pair for `func_actor_503500_8013E740`: nine effect offsets, one
/// picked by frame and one by `Gp_LcgState`, and the rect it moves on frame 8.
extern SVECTOR D_actor_503500_8016F31C[];
extern RECT    D_actor_503500_8016F364;
extern u32     Gp_LcgState;
void           func_actor_503500_8013BC54(Actor503500* arg0);
/// Per-slot local offset of the 0xF4 enemies in `D_actor_503500_801770E8`,
/// indexed by `spawnArg1`.
extern SVECTOR              D_actor_503500_8016F210[];
extern Actor503500Work770E8 D_actor_503500_801770E8[];
void                        func_actor_503500_8013D85C(Actor503500* arg0);
/// The same pair for the 0x160 enemy at `D_actor_503500_80176D88`: a world
/// translation seeded into the task's own coordinate and the local offset its
/// `GpEnemy::field_1C` and display node share.
extern SVECTOR         D_actor_503500_8016F060;
extern SVECTOR         D_actor_503500_8016F068;
extern Actor503500Work D_actor_503500_80176D88;
/// Translation `func_actor_503500_801374BC` seeds into each of the two effect
/// tasks it hangs off the task's own coordinate.
extern SVECTOR D_actor_503500_8016F070;
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
/// Local offset of the display node `func_actor_503500_801455A4` links.
extern Actor503500UVec D_actor_503500_801715D4;
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
void func_actor_503500_80135CE8(Task* arg0, s32 arg1);
void func_actor_503500_80136048(Actor503500* arg0);
s32  func_actor_503500_801360BC(s32 arg0, s32 arg1);
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void func_actor_503500_801372AC(s32 arg0);
void func_actor_503500_80138288(Actor503500* arg0);
void func_actor_503500_801382F4(Actor503500* arg0);
void func_actor_503500_801382FC(Actor503500* arg0);
void func_actor_503500_80138378(Actor503500* arg0);
void func_actor_503500_801383D0(Actor503500* arg0);
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
void func_actor_503500_8013ACC4(Actor503500* arg0, s32 arg1);
void func_actor_503500_80138A30(Actor503500* arg0);
void func_actor_503500_80138C08(Actor503500* arg0);
void func_actor_503500_80139014(Actor503500* arg0);
void func_actor_503500_801395BC(Actor503500* arg0);
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
void      func_actor_503500_801400A4(Actor503500* arg0);
void      func_actor_503500_80140654(Actor503500* arg0);
void      func_actor_503500_80140BE8(Actor503500* arg0);
void      func_actor_503500_80141248(Actor503500* arg0);
void      func_actor_503500_80141448(Actor503500* arg0);
void      func_actor_503500_80141B94(Actor503500* arg0);
void      func_actor_503500_80141D7C(Actor503500* arg0);
void      func_actor_503500_80141E64(Actor503500* arg0);
void      func_actor_503500_80141F48(Actor503500* arg0);
void      func_actor_503500_80141FC8(Actor503500* arg0);
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
void      func_actor_503500_80145950(Task* arg0);
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

void func_actor_503500_80136450(Actor503500* arg0)
{
    Actor503500Work* work = arg0->field_1C;
    u16              timer;

    if ((s8)work->field_7DA == 0 && work->field_7D5 >= 2) {
        func_actor_503500_80135FB4(arg0, 1, 0x10);
    }
    timer           = work->field_7CA - 1;
    work->field_7CA = timer;
    if ((s16)timer < 0) {
        func_actor_503500_80136EFC(arg0, 1);
    }
}
/// Script step for the boss's slot-0 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x1E once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
s32 func_actor_503500_801364D0(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (func_actor_503500_80136FDC(work, 0) != 0) {
                func_actor_503500_80136F40(work, 0, 2, 0x3C);
                work->field_7DB = 1;
                ret             = 0;
                work->field_7D2 = 0;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, 0) != 0) {
                ret = 0x1E;
            }
            break;
    }
    return ret;
}

/// Script step that dismisses both of the boss's slot-7/8 helpers: state 0
/// asks whichever slots are ready to die and arms `field_7D2`, state 1 waits
/// for either slot to finish dying or for `field_7BE` to pass 90 frames.
/// Returns 0x1E while neither slot is ready, 0 the frame a request is issued
/// or while waiting, and 0xF0 once the wait is over.
s32 func_actor_503500_8013656C(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            ret             = 0x1E;
            work->field_7D2 = 0;
            if (func_actor_503500_80136FDC(work, 7) != 0) {
                func_actor_503500_80136F40(work, 7, 2, 0x96);
                ret = 0;
            }
            if (func_actor_503500_80136FDC(work, 8) != 0) {
                func_actor_503500_80136F40(work, 8, 2, 0x96);
                ret = 0;
            }
            if (ret == 0) {
                work->field_7DB = 1;
                work->field_7D2 = 0x7D0;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, 7) != 0 || func_actor_503500_80136FA8(work, 8) != 0 ||
                ++work->field_7BE > 0x5A) {
                ret = 0xF0;
            }
            break;
    }
    return ret;
}

/// Script step that dismisses one of two helpers: state 0 asks slot 1 to die
/// when it is ready and the boss is within 500 units on `field_7BA`, otherwise
/// falls back to slot 12; state 1 waits for the chosen slot (`field_7C2`) to
/// finish dying. Returns 1 while busy, 0 the frame the request is issued, and
/// 0x5A once the slot has gone quiet.
s32 func_actor_503500_8013667C(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (func_actor_503500_80136FDC(work, 1) != 0) {
                if (__builtin_abs(work->field_7BA) < 500) {
                    func_actor_503500_80136F40(work, 1, 2, 0x96);
                    work->field_7C2 = 1;
                    work->field_7D2 = 0;
                    work->field_7DB = 1;
                    ret             = 0;
                    break;
                }
            }
            if (func_actor_503500_80136FDC(work, 0xC) != 0) {
                func_actor_503500_80136F40(work, 0xC, 2, 0x96);
                ret             = 0;
                work->field_7C2 = 0xC;
                work->field_7D2 = 0;
                work->field_7DB = 1;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, work->field_7C2) != 0) {
                ret = 0x5A;
            }
            break;
    }
    return ret;
}

/// Script step for the boss's slot-12 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x96 once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
s32 func_actor_503500_80136770(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (func_actor_503500_80136FDC(work, 0xC) != 0) {
                func_actor_503500_80136F40(work, 0xC, 2, 0x96);
                work->field_7DB = 1;
                ret             = 0;
                work->field_7D2 = 0;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, 0xC) != 0) {
                ret = 0x96;
            }
            break;
    }
    return ret;
}

/// Script step for the boss's slot-4/5 helper pair: state 0 asks both slots to
/// die, led by slot 4 unless `field_7BA` is in [-0x5FF, -0x201] and by slot 5
/// unless it is in [0x201, 0x5FF]; state 1 waits until both have finished.
/// Returns 1 while still busy, 0 the frame a request is issued, 0xA once both
/// slots have gone quiet. `arg0` is ignored, as in the sibling steps.
/// The first range check compares `field_7BA` directly rather than `x`: fold
/// merges two tests on one operand into a single unsigned range check.
s32 func_actor_503500_8013680C(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;
    s16 x;

    x   = work->field_7BA;
    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (x < -0x5FF || work->field_7BA >= -0x200) {
                if (func_actor_503500_80136FDC(work, 4) != 0) {
                    func_actor_503500_80136F40(work, 4, 2, 0xB4);
                    func_actor_503500_80136F40(work, 5, 2, 0xB4);
                    ret = 0;
                }
            }
            if (x < 0x201 || x >= 0x600) {
                if (func_actor_503500_80136FDC(work, 5) != 0) {
                    func_actor_503500_80136F40(work, 5, 2, 0xB4);
                    func_actor_503500_80136F40(work, 4, 2, 0xB4);
                    ret = 0;
                }
            }
            if (ret == 0) {
                work->field_7DB = 1;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, 4) != 0) {
                if (func_actor_503500_80136FA8(work, 5) != 0) {
                    ret = 0xA;
                }
            }
            break;
    }
    return ret;
}

/// Script step for the boss's slot-9 helper: state 0 waits for the slot to be
/// ready and then asks it to die, state 1 waits for that death to finish.
/// Returns the number of frames the script should wait -- 1 while still busy,
/// 0 the frame the request is issued, 0x1E once the slot has gone quiet.
/// `arg0` is passed by every caller through the step table and ignored here.
s32 func_actor_503500_80136948(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            work->field_7D2 = 0;
            if (func_actor_503500_80136FDC(work, 9) != 0) {
                func_actor_503500_80136F40(work, 9, 2, 0x3C);
                work->field_7DB = 1;
                ret             = 0;
            }
            break;
        case 1:
            ret = 0;
            if (func_actor_503500_80136FA8(work, 9) != 0) {
                ret = 0x1E;
            }
            break;
    }
    return ret;
}
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
void func_actor_503500_80136A88(Actor503500* arg0)
{
    Actor503500Work* work = arg0->field_1C;
    GpRec18*         rec;

    if (work->field_7B4 != 0) {
        work->field_7B4 -= 1;
        if (work->field_7B4 < 0) {
            work->field_7B4 = 0;
        }
    }

    rec = work->rec5F4;
    func_actor_503500_80134EAC(arg0, &work->field_5D4, rec, 8);
    Gp_ClearRec18Occupied(rec);
}
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
void func_actor_503500_80136D30(Actor503500* arg0)
{
    Actor503500Work* work;
    s32              i;

    work = arg0->field_1C;
    if (work->field_7D4 != 0) {
        if (work->slot40.boss.flags_4C & 0x100) {
            if (work->field_7B0 == 0) {
                func_actor_503500_80137048(arg0, 0);
                func_actor_503500_80135950(arg0, 0x7D3, &D_actor_503500_8016EAD4, 0);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
    }
}
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
/// Asks slot `slot` to die: arms its `field_730` flag with `arg3` in
/// `field_752`. Slot 0 is the boss itself and keeps `arg2` in its own work
/// block; any other slot writes `arg2` into its enemy task's `killCountdown`,
/// and an empty slot is only cleared.
void func_actor_503500_80136F40(Actor503500Work* work, s32 slot, s32 arg2, s32 arg3)
{
    GpEnemy* enemy;

    if (slot == 0) {
        work->field_7E0    = arg2;
        work->field_7E1    = 0;
        work->field_7C0    = 0;
        work->field_730[0] = 1;
        work->field_752[0] = arg3;
        return;
    }
    enemy = work->enemies[slot];
    if (enemy != NULL) {
        enemy->task->killCountdown = arg2;
        work->field_730[slot]      = 1;
        work->field_752[slot]      = arg3;
        return;
    }
    work->field_730[slot] = 0;
}

/// True when enemy slot `slot` is either unoccupied or has its `field_730`
/// counter at zero -- i.e. the slot has nothing left to wait for.
s32 func_actor_503500_80136FA8(Actor503500Work* work, s32 slot)
{
    s32 ret;

    ret = 1;
    if (work->enemies[slot] != NULL) {
        ret = work->field_730[slot] == 0;
    }
    return ret;
}
/// The stricter form of `func_actor_503500_80136FA8`: slot `slot` is ready when
/// it is occupied, not marked dying by `field_752`, and its `field_730` counter
/// has run out. Slot 0 stands for the boss itself, which is ready when the
/// main-executable flag `field_7E0` and its own `field_752` are both clear.
s32 func_actor_503500_80136FDC(Actor503500Work* work, s32 slot)
{
    s32 ret;

    ret = 0;
    if (slot != 0) {
        if (work->enemies[slot] != NULL) {
            if (work->field_752[slot] == 0) {
                ret = work->field_730[slot] == 0;
            }
        }
    } else if ((work->field_7E0 == 0) && (work->field_752[0] == 0)) {
        ret = 1;
    }
    return ret;
}
/// Sets `GpAnimSlot::field_9` -- the per-slot value `Gp_AnimResetSlot` seeds
/// with 0x10 -- on animation slots 1..16 of the boss block, `rate` of 0
/// meaning that default.
void func_actor_503500_80137048(Actor503500* arg0, s32 rate)
{
    Actor503500WorkBoss* work;
    GpAnimSlot*          slot;
    s32                  i;

    work = (Actor503500WorkBoss*)arg0->field_1C;
    slot = &work->slots[1];
    if (rate == 0) {
        rate = 0x10;
    }
    for (i = 0xF; i >= 0; i--) {
        slot->field_9 = rate;
        slot++;
    }
}
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
s32 func_actor_503500_80137088(Actor503500* arg0, s32 arg1, Actor503500PlaceArgs* args)
{
    Actor503500Work*  work;
    Actor503500Coord* coord;

    work              = arg0->field_1C;
    coord             = (Actor503500Coord*)arg0->extra->field_8;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg         = 0;
    work->field_7B6    = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    work->field_6C4.vx = args->pos.vx << 16;
    work->field_6C4.vy = args->pos.vy << 16;
    work->field_6C4.vz = args->pos.vz << 16;
    return 0;
}
s32 func_actor_503500_80137158(Actor503500* arg0, s32 arg1, s32 mode)
{
    TmdObject* ext;
    s32        ret;

    ext = arg0->extra;
    ret = 0;
    switch (mode) {
        case 0:
            ext->field_C = (ext->field_C | 0x80) & ~4;
            break;
        case 1:
            ext->field_C &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->field_C &= ~4;
            break;
        case 2:
            ext->field_C             |= 0x80;
            arg0->field_1C->field_7D9 = mode;
            ext->field_C             |= 4;
            break;
        case 3:
            ext->field_C = (ext->field_C & ~0x80) | 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

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
void func_actor_503500_801372C8(Actor503500* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    TmdObject*     tmd;
    TmdObject*     parentTmd;
    GsCOORDINATE2* coord;
    GpRec18*       rec;

    enemy     = arg0->field_20;
    tmd       = arg0->extra;
    parent    = arg0->parent;
    coord     = tmd->field_8;
    parentTmd = parent->extra;
    Mem_Set(&D_actor_503500_80176D88, 0, 0x160);
    arg0->field_1C = &D_actor_503500_80176D88;

    coord->sub        = &((TmdObject*)parent->extra)->field_8[8];
    coord->coord.t[0] = D_actor_503500_8016F060.vx;
    coord->coord.t[1] = D_actor_503500_8016F060.vy;
    coord->coord.t[2] = D_actor_503500_8016F060.vz;
    tmd->field_1C     = parentTmd->field_1C;
    tmd->field_20     = parentTmd->field_20;
    tmd->field_E      = 0x13;
    coord->flg        = 0;

    D_actor_503500_80176D88.field_15E = -1;
    enemy->field_4                    = &coord->coord;
    enemy->field_48                   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx  = D_actor_503500_8016F068.vx;
    enemy->field_1C.vy  = D_actor_503500_8016F068.vy;
    enemy->field_1C.vz  = D_actor_503500_8016F068.vz;
    rec                 = D_actor_503500_80176D88.rec60;
    enemy->field_50     = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54     = (s32)rec;
    enemy->field_40     = enemy->field_50->field_4;

    D_actor_503500_80176D88.slot40.obj.field_8  = coord;
    D_actor_503500_80176D88.slot40.obj.field_C  = rec;
    D_actor_503500_80176D88.slot40.obj.field_18 = 0x30023;
    D_actor_503500_80176D88.slot40.obj.field_1C = 0x320;
    D_actor_503500_80176D88.slot40.obj.flags    = 1;
    D_actor_503500_80176D88.slot40.obj.field_10 = D_actor_503500_8016F068.vx;
    D_actor_503500_80176D88.slot40.obj.field_12 = D_actor_503500_8016F068.vy;
    D_actor_503500_80176D88.slot40.obj.field_14 = D_actor_503500_8016F068.vz;
    Gp_LinkObj(2, &D_actor_503500_80176D88.slot40.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_80176D88.field_124         = 0x400;
    D_actor_503500_80176D88.field_120         = coord;
    D_actor_503500_80176D88.field_126         = 3;
    D_actor_503500_80176D88.slot40.obj.flags |= 0x8000;
    func_actor_503500_80138490(arg0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_80138288;
    arg0->state       += 1;
}
/// Sub-state tick of the 0x160 enemy at `D_actor_503500_80176D88`: phase 0
/// starts mode 0xF on the parent, phase 1 spawns two tasks hung off this
/// task's coordinate, phase 2 waits on the parent's mode 0xF and starts 0x10,
/// phase 3 counts 0x47 frames. Leaves early once `_8013608C` reports the
/// parent done.
void func_actor_503500_801374BC(Actor503500* arg0)
{
    Actor503500Work* work;
    Task*            task;
    GsCOORDINATE2*   coord;
    s32              i;
    s32              id;

    work = arg0->field_1C;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80138490(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_15D) {
        case 0:
            id = 0xF;
            goto play;
        case 1:
            if ((s16)++work->field_15A > 0) {
                for (i = 0; i < 2; i++) {
                    task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 2, i, 0);
                    if (task != NULL) {
                        coord             = ((TmdObject*)task->extra)->field_8;
                        coord->sub        = arg0->extra->field_8;
                        coord->coord.t[0] = D_actor_503500_8016F070.vx;
                        coord->coord.t[1] = D_actor_503500_8016F070.vy;
                        coord->coord.t[2] = D_actor_503500_8016F070.vz;
                        Task_Reparent((Task*)arg0, task);
                    }
                }
                work->field_15A = 0;
                work->field_15D++;
            }
            break;
        case 2:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 0xF) != 0) {
                id = 0x10;
            play:
                func_actor_503500_80135FB4((Actor503500*)arg0->parent, id, 0x10);
                work->field_15D++;
            }
            break;
        case 3:
            if ((s16)++work->field_15A >= 0x47) {
                func_actor_503500_80138490(arg0, 0);
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80131F4C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137678);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_80137C90);

void func_actor_503500_8013815C(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s8               countdown;

    work      = arg0->field_1C;
    enemy     = arg0->field_20;
    countdown = work->field_15E;
    tmd       = (TmdObject*)arg0->extra;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(tmd);
        }
        work->field_15E = (s8)((u8)work->field_15E - 1);
    }
    if (work->field_15C != 2) {
        func_actor_503500_80135828(arg0, &work->field_15E);
    }

    switch (D_801153F4) {
        case 1:
            if (!(tmd->field_C & 0x80)) {
                func_actor_503500_801382F4(arg0);
            }
            break;
        case 2:
            tmd->field_C        |= 0x80;
            enemy->node.field_4 |= 1;
            break;
        default:
            if (enemy->field_4C != 0) {
                func_actor_503500_80138378(arg0);
            }
            func_actor_503500_801382F4(arg0);
            func_actor_503500_801382FC(arg0);
            func_actor_503500_801383D0(arg0);
            break;
    }
}

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
void func_actor_503500_801382F4(Actor503500* arg0)
{
}

/// Steps the 0x160 block's countdown at 0x158 down to zero, then, unless the
/// global freeze is on, runs the block's display node through its record table
/// before releasing the table.
void func_actor_503500_801382FC(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->field_1C;
    if (work->field_158 != 0) {
        timer           = (u16)work->field_158 - 1;
        work->field_158 = timer;
        if (timer < 0) {
            work->field_158 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_80137C90(arg0, &work->slot40.obj, work->rec60, 8);
    }
    Gp_ClearRec18Occupied(work->rec60);
}
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

void func_actor_503500_801398D0(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              dmg;
    u8               flags;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if ((func_actor_503500_80136208() == 0) && (Game_Session->field_1 == 0)) {
        flags = enemy->field_4C;
        if (flags & 1) {
            enemy->field_4C = flags & 0xFE;
            func_actor_503500_8013ACC4(arg0, 0);
            work->field_2D6 = 5;
            work->field_2DA = 8;
        }
        if (enemy->field_4C & 2) {
            enemy->field_4C &= 0xFD;
        }
        if (enemy->field_4C & 0xC) {
            func_actor_503500_8013ACC4(arg0, 4);
            if (Gp_ObjFlag4Expired((GpObj5C*)arg0->field_20) != 0) {
                enemy->field_4C &= 0xF3;
                func_actor_503500_8013ACC4(arg0, 0);
            } else {
                dmg = Gp_TickObjFlag4((GpObj5C*)enemy);
                if (dmg != 0) {
                    enemy->field_40 -= dmg;
                    func_800DA6E8(&enemy->node, dmg, 0);
                    work->field_2DA = 8;
                    if (enemy->field_40 <= 0) {
                        enemy->field_4C &= 0xF3;
                        func_actor_503500_8013ACC4(arg0, 5);
                    } else {
                        func_actor_503500_8013ACC4(arg0, 0);
                    }
                }
            }
        }
    }
}

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
void func_actor_503500_8013A96C(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->field_1C;
    switch (work->field_2D4) {
        case 0:
            func_actor_503500_80138A30(arg0);
            break;
        case 1:
            func_actor_503500_80138C08(arg0);
            break;
        case 2:
            timer           = (u16)work->field_2D6 - 1;
            work->field_2D6 = timer;
            if (timer < 0) {
                func_actor_503500_8013ACC4(arg0, 0);
            }
            break;
        case 5:
            func_actor_503500_80139014(arg0);
            break;
        case 6:
            func_actor_503500_801395BC(arg0);
            break;
    }
    timer           = (u16)work->field_2DA - 1;
    work->field_2DA = timer;
    if (timer < 0) {
        work->field_2DA = 0;
    }
}

/// Steps the 0x2EC block's countdown at 0x2D8 down to zero, then, unless the
/// global freeze is on, runs the 0x160 display node through its record table
/// before releasing the table. Same shape as `func_actor_503500_8013BD0C`.
void func_actor_503500_8013AA44(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->field_1C;
    if (work->field_2D8 != 0) {
        timer           = (u16)work->field_2D8 - 1;
        work->field_2D8 = timer;
        if (timer < 0) {
            work->field_2D8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_80139A20(arg0, &work->obj160, work->rec180, 8);
    }
    Gp_ClearRec18Occupied(work->rec180);
}
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
void func_actor_503500_8013AC6C(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}
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

/// State-0 init of the 0xF0 enemies in spawn slots 4 and 5: clears the slot's
/// block in `D_actor_503500_801774C0`, hangs the task's coordinate off the
/// parent part the slot names, links the enemy and its display node, and
/// hands the task to its exit callback.
void func_actor_503500_8013AD64(Actor503500* arg0)
{
    GpEnemy*              enemy;
    Task*                 parent;
    GsCOORDINATE2*        coord;
    MATRIX*               mtx;
    GpRec18*              rec;
    Actor503500Work774C0* work;
    s32                   idx;

    idx    = arg0->spawnArg1 - 4;
    enemy  = arg0->field_20;
    parent = arg0->parent;
    work   = &D_actor_503500_801774C0[idx];
    coord  = arg0->extra->field_8;
    Mem_Set(work, 0, 0xF0);
    arg0->field_1C = (Actor503500Work*)work;
    work->field_EC = idx;

    coord->sub                   = &((TmdObject*)parent->extra)->field_8[D_actor_503500_8016F0E8[idx]];
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18      = coord;
    enemy->node.field_4 |= 9;
    enemy->field_1C.vx   = D_actor_503500_8016F0F0[idx].vx;
    enemy->field_1C.vy   = D_actor_503500_8016F0F0[idx].vy;
    enemy->field_1C.vz   = D_actor_503500_8016F0F0[idx].vz;
    rec                  = &work->rec;
    enemy->field_50      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54      = (s32)rec;
    enemy->field_40      = enemy->field_50->field_4;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = D_actor_503500_8016F0F0[idx].vx;
    work->obj.field_12 = D_actor_503500_8016F0F0[idx].vy;
    work->obj.field_14 = D_actor_503500_8016F0F0[idx].vz;
    work->obj.field_18 = 0x30023;
    work->obj.field_1C = 0x5DC;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E4   = 0x600;
    work->field_E0   = coord;
    work->field_E6   = 3;
    work->obj.flags |= 0x8000;
    MoveImage(&D_actor_503500_8016F100, 0, 0x105);
    arg0->exitCallback = (TaskFunc)func_actor_503500_8013BC54;
    arg0->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80131FF0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013AF60);

s16  func_actor_503500_80136134(Actor503500* arg0);
void func_actor_503500_8013B60C(Actor503500* arg0, s32 side, s32 arg2);
void func_actor_503500_8013BE48(Actor503500* arg0, s32 arg1);

void func_actor_503500_8013B460(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              angle;
    s32              offset;
    s32              side;

    work = arg0->field_1C;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013BE48(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_EE) {
        case 0:
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, 9, 0x10);
            work->field_EE++;
            break;
        case 1:
            work->field_EA++;
            if (work->field_EA >= 0x80) {
                offset = -0x12C;
                angle  = func_actor_503500_80136134((Actor503500*)arg0->parent);
                side   = work->field_EC;
                if (side != 0) {
                    offset = 0x12C;
                }
                if (angle > -0x400 - offset && angle < 0x400 - offset) {
                    func_actor_503500_8013B60C(arg0, side, 0);
                }
                if (angle < offset - 0x400 || offset + 0x400 < angle) {
                    func_actor_503500_8013B60C(arg0, side, 1);
                }
                work->field_EE++;
            }
        case 2:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 9) != 0) {
                func_actor_503500_8013BE48(arg0, 0);
            }
            break;
    }
}

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

void func_actor_503500_8013BCB4(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    if (enemy->field_4C & 1) {
        enemy->field_4C &= ~1;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= ~2;
    }
    if (enemy->field_4C & 0xC) {
        enemy->field_4C &= ~0xC;
    }
}

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
void func_actor_503500_8013BEE4(Actor503500* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = arg0->extra->field_8;
    enemy  = arg0->field_20;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_801776A0, 0, 0xF4);
    arg0->field_1C = &D_actor_503500_801776A0;

    parts                        = ((TmdObject*)parent->extra)->field_8;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[16];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx  = D_actor_503500_8016F1B0.vx;
    enemy->field_1C.vy  = D_actor_503500_8016F1B0.vy;
    enemy->field_1C.vz  = D_actor_503500_8016F1B0.vz;
    rec                 = &D_actor_503500_801776A0.rec;
    enemy->field_50     = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54     = (s32)rec;
    enemy->field_40     = enemy->field_50->field_4;

    D_actor_503500_801776A0.obj.field_8  = coord;
    D_actor_503500_801776A0.obj.field_C  = rec;
    D_actor_503500_801776A0.obj.field_18 = 0x30023;
    D_actor_503500_801776A0.obj.field_1C = 0x3E8;
    D_actor_503500_801776A0.obj.flags    = 1;
    D_actor_503500_801776A0.obj.field_10 = D_actor_503500_8016F1B0.vx;
    D_actor_503500_801776A0.obj.field_12 = D_actor_503500_8016F1B0.vy;
    D_actor_503500_801776A0.obj.field_14 = D_actor_503500_8016F1B0.vz;
    Gp_LinkObj(2, &D_actor_503500_801776A0.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_801776A0.field_E4   = 0x600;
    D_actor_503500_801776A0.field_E0   = coord;
    D_actor_503500_801776A0.field_E6   = 3;
    D_actor_503500_801776A0.obj.flags |= 0x8000;
    func_actor_503500_8013CA74(arg0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_8013C900;
    arg0->state       += 1;
}
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132028);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C088);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013C558);

/// Per-frame tick of the first 0xF4 block, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013C878(Actor503500* arg0)
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
        func_actor_503500_8013C9DC(arg0);
    }
    func_actor_503500_8013C960(arg0);
    func_actor_503500_8013CA34(arg0);
}
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

void func_actor_503500_8013C960(Actor503500* arg0)
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
        func_actor_503500_8013C088(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013C9DC(Actor503500* arg0)
{
    GpEnemy* enemy = arg0->field_20;

    if (enemy->field_4C & 1) {
        enemy->field_4C &= ~1;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= ~2;
    }
    if (enemy->field_4C & 0xC) {
        enemy->field_4C &= ~0xC;
    }
}

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

/// State-0 init of the 0xF4 enemy in slot `spawnArg1` of
/// `D_actor_503500_801770E8`, the same shape as `func_actor_503500_8013BEE4`
/// but without linking the enemy node.
void func_actor_503500_8013CAE4(Actor503500* arg0)
{
    GpEnemy*              enemy;
    Task*                 parent;
    GsCOORDINATE2*        coord;
    MATRIX*               mtx;
    GsCOORDINATE2*        parts;
    GpRec18*              rec;
    Actor503500Work770E8* work;
    SVECTOR*              pos;
    s32                   idx;

    idx    = arg0->spawnArg1;
    enemy  = arg0->field_20;
    work   = &D_actor_503500_801770E8[idx];
    pos    = &D_actor_503500_8016F210[idx];
    coord  = arg0->extra->field_8;
    parent = arg0->parent;
    Mem_Set(work, 0, 0xF4);
    arg0->field_1C = (Actor503500Work*)work;

    parts                        = ((TmdObject*)parent->extra)->field_8;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[1];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    enemy->field_18              = coord;
    enemy->node.field_4          = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx           = pos->vx;
    enemy->field_1C.vy           = pos->vy;
    enemy->field_1C.vz           = pos->vz;
    rec                          = work->rec;
    enemy->field_50              = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54              = (s32)rec;
    enemy->field_40              = enemy->field_50->field_4;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = pos->vx;
    work->obj.field_12 = pos->vy;
    work->obj.field_14 = pos->vz;
    work->obj.field_18 = 0x30023;
    work->obj.field_1C = 0x190;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E4   = 0x600;
    work->field_E0   = coord;
    work->field_E6   = 3;
    work->obj.flags &= 0x7FFF;
    func_actor_503500_8013DBA8(arg0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_8013D85C;
    arg0->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132060);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013CCBC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D1CC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013D558);

/// Per-frame tick of the second 0xF4 block, the same shape as
/// `func_actor_503500_8013BBCC`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013D7D4(Actor503500* arg0)
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
        func_actor_503500_8013D8BC(arg0);
    }
    func_actor_503500_8013D914(arg0);
    func_actor_503500_8013D990(arg0);
}
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
void func_actor_503500_8013D8BC(Actor503500* arg0)
{
    GpEnemy* enemy = arg0->field_20;

    if (enemy->field_4C & 1) {
        enemy->field_4C &= ~1;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= ~2;
    }
    if (enemy->field_4C & 0xC) {
        enemy->field_4C &= ~0xC;
    }
}

void func_actor_503500_8013D914(Actor503500* arg0)
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
        func_actor_503500_8013CCBC(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}
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
void func_actor_503500_8013DD10(Actor503500* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = arg0->extra->field_8;
    enemy  = arg0->field_20;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_8017797C, 0, 0xF0);
    arg0->field_1C = &D_actor_503500_8017797C;

    parts                        = ((TmdObject*)parent->extra)->field_8;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[1];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18     = coord;
    enemy->node.field_4 = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx  = D_actor_503500_8016F2D8.vx;
    enemy->field_1C.vy  = D_actor_503500_8016F2D8.vy;
    enemy->field_1C.vz  = D_actor_503500_8016F2D8.vz;
    rec                 = &D_actor_503500_8017797C.rec;
    enemy->field_50     = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54     = (s32)rec;
    enemy->field_40     = enemy->field_50->field_4;

    D_actor_503500_8017797C.obj.field_8  = coord;
    D_actor_503500_8017797C.obj.field_C  = rec;
    D_actor_503500_8017797C.obj.field_18 = 0x30023;
    D_actor_503500_8017797C.obj.field_1C = 0x3E8;
    D_actor_503500_8017797C.obj.flags    = 1;
    D_actor_503500_8017797C.obj.field_10 = D_actor_503500_8016F2D8.vx;
    D_actor_503500_8017797C.obj.field_12 = D_actor_503500_8016F2D8.vy;
    D_actor_503500_8017797C.obj.field_14 = D_actor_503500_8016F2D8.vz;
    Gp_LinkObj(2, &D_actor_503500_8017797C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_8017797C.field_E4   = 0x600;
    D_actor_503500_8017797C.field_E0   = coord;
    D_actor_503500_8017797C.field_E6   = 3;
    D_actor_503500_8017797C.obj.flags |= 0x8000;
    func_actor_503500_8013EC20(arg0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_8013EA2C;
    arg0->state       += 1;
}
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_80132098);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013DEB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013E384);

/// Sub-state 2 of the third 0xF0 block, stepped by `field_ED`: the same
/// death sequence as `func_actor_503500_8013F4A4`, except the effects come
/// from `D_actor_503500_8016F31C` - the odd-frame one at a random row - and
/// the VRAM rect is restored on frame 8 rather than at the end.
void func_actor_503500_8013E740(Actor503500* arg0)
{
    GpEnemy*         enemy;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    s32              pan;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_ED) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->field_54  = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            enemy->field_4C &= 0xF0;
            pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
            work->field_ED++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001C00,
                            &D_actor_503500_8016F31C[(s16)(work->field_EA % 9)]);
                if (work->field_EA & 1) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6018C, coord, 0x04404600,
                                &D_actor_503500_8016F31C[(u16)((Gp_LcgState >> 16) % 9)]);
                }
            }
            if (work->field_EA == 8) {
                MoveImage(&D_actor_503500_8016F364, 0x141, 0x12A);
            }
            if (work->field_EA++ >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_ED++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The third 0xF0 block's per-frame tick, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013E9A4(Actor503500* arg0)
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
        func_actor_503500_8013EA8C(arg0);
    }
    func_actor_503500_8013EAE4(arg0);
    func_actor_503500_8013EB60(arg0);
}
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

void func_actor_503500_8013EA8C(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    if (enemy->field_4C & 1) {
        enemy->field_4C &= ~1;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= ~2;
    }
    if (enemy->field_4C & 0xC) {
        enemy->field_4C &= ~0xC;
    }
}

void func_actor_503500_8013EAE4(Actor503500* arg0)
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
        func_actor_503500_8013DEB4(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}
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
void func_actor_503500_8013ECBC(Actor503500* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = arg0->extra->field_8;
    enemy  = arg0->field_20;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_80177A6C, 0, 0xF4);
    arg0->field_1C = &D_actor_503500_80177A6C;

    parts                        = ((TmdObject*)parent->extra)->field_8;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[8];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18      = coord;
    enemy->node.field_4 |= 9;
    enemy->field_1C.vx   = D_actor_503500_8016F36C.vx;
    enemy->field_1C.vy   = D_actor_503500_8016F36C.vy;
    enemy->field_1C.vz   = D_actor_503500_8016F36C.vz;
    rec                  = &D_actor_503500_80177A6C.rec;
    enemy->field_50      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54      = (s32)rec;
    enemy->field_40      = enemy->field_50->field_4;

    D_actor_503500_80177A6C.obj.field_8  = coord;
    D_actor_503500_80177A6C.obj.field_C  = rec;
    D_actor_503500_80177A6C.obj.field_10 = D_actor_503500_8016F36C.vx;
    D_actor_503500_80177A6C.obj.field_12 = D_actor_503500_8016F36C.vy;
    D_actor_503500_80177A6C.obj.field_14 = D_actor_503500_8016F36C.vz;
    D_actor_503500_80177A6C.obj.field_18 = 0x30023;
    D_actor_503500_80177A6C.obj.field_1C = 0x320;
    D_actor_503500_80177A6C.obj.flags    = 1;
    Gp_LinkObj(2, &D_actor_503500_80177A6C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_80177A6C.field_E4   = 0x600;
    D_actor_503500_80177A6C.field_E0   = coord;
    D_actor_503500_80177A6C.field_E6   = 3;
    D_actor_503500_80177A6C.obj.flags &= 0x7FFF;
    func_actor_503500_8013F9D4(arg0, 3);
    arg0->exitCallback = (TaskFunc)func_actor_503500_8013F778;
    arg0->state       += 1;
}
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_6", D_actor_503500_801320D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_6", func_actor_503500_8013EE5C);

/// A sub-state of the 0xF4 block, stepped by `field_F1`: applies the boss's
/// animation preset 0xC, counts 0x6F frames (spawning table entry 3 at frame
/// 0x1E with its coordinate parented to this task's), applies preset 0xD once
/// the boss reports sub-state 0xC done, and leaves when 0xD is done too.
void func_actor_503500_8013F328(Actor503500* arg0)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->field_1C;
    Task*              task;
    GsCOORDINATE2*     coord;

    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013F9D4(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, 0xC, 8);
            work->field_F1++;
            break;
        case 1:
            work->field_EA++;
            if ((s16)work->field_EA >= 0x6F) {
                work->field_EA = 0;
                work->field_F1++;
            } else if ((s16)work->field_EA == 0x1E) {
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 3, 0, 0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->field_8;
                    coord->sub        = arg0->extra->field_8;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                }
            }
            break;
        case 2:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 0xC) != 0) {
                func_actor_503500_80135FB4((Actor503500*)arg0->parent, 0xD, 0x10);
                work->field_F1++;
            }
            break;
        case 3:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 0xD) != 0) {
                func_actor_503500_8013F9D4(arg0, 0);
            }
            break;
    }
}

/// Sub-state 2 of the 0xF4 block, stepped by `field_F1`: phase 0 hides the
/// display node, unlinks the enemy node, releases its state-F0 reference and
/// plays the death sound; phase 1 spawns effects from `D_actor_503500_8016F374`
/// for 0x1F frames, then restores the VRAM rect and moves on.
void func_actor_503500_8013F4A4(Actor503500* arg0)
{
    GpEnemy*           enemy;
    Actor503500WorkF4* work;
    GsCOORDINATE2*     coord;
    s32                pan;

    enemy = arg0->field_20;
    work  = (Actor503500WorkF4*)arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_F1) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->field_54  = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            enemy->field_4C &= 0xF0;
            pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
            work->field_EC = 2;
            work->field_F1++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001A00,
                            &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 6)]);
                if (work->field_EA & 1) {
                    Gp_SpawnEff(0x6018C, coord, 0x04404600,
                                &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 3) + 3]);
                }
            }
            work->field_EA++;
            if ((s16)work->field_EA >= 0x1F) {
                MoveImage(&D_actor_503500_8016F3A4, 0x140, 0x100);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                func_actor_503500_8013611C(arg0->spawnArg1);
                work->field_F1++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The 0xF4 block's per-frame tick, the same shape as
/// `func_actor_503500_8013E9A4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013F6F0(Actor503500* arg0)
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
        func_actor_503500_8013F7D8(arg0);
    }
    func_actor_503500_8013F830(arg0);
    func_actor_503500_8013F8AC(arg0);
}
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
void func_actor_503500_8013F7D8(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    if (enemy->field_4C & 1) {
        enemy->field_4C &= ~1;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= ~2;
    }
    if (enemy->field_4C & 0xC) {
        enemy->field_4C &= ~0xC;
    }
}

void func_actor_503500_8013F830(Actor503500* arg0)
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
        func_actor_503500_8013EE5C(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}
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
void func_actor_503500_8013F984(Actor503500* arg0)
{
    Actor503500Work* work;

    if (arg0->killCountdown == 8) {
        func_actor_503500_8013F9D4(arg0, 0);
        work             = arg0->field_1C;
        work->obj.flags |= 0x8000;
    }
}
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

void func_actor_503500_80140BE8(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              dmg;
    u8               flags;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if ((func_actor_503500_80136208() == 0) && (Game_Session->field_1 == 0)) {
        flags = enemy->field_4C;
        if (flags & 1) {
            enemy->field_4C = flags & 0xFE;
            func_actor_503500_80142310(arg0, 0);
            work->field_3A6 = 5;
            work->field_3AA = 8;
        }
        if (enemy->field_4C & 2) {
            enemy->field_4C &= 0xFD;
        }
        if (enemy->field_4C & 0xC) {
            func_actor_503500_80142310(arg0, 4);
            if (Gp_ObjFlag4Expired((GpObj5C*)arg0->field_20) != 0) {
                enemy->field_4C &= 0xF3;
                func_actor_503500_80142310(arg0, 0);
            } else {
                dmg = Gp_TickObjFlag4((GpObj5C*)enemy);
                if (dmg != 0) {
                    enemy->field_40 -= dmg;
                    func_800DA6E8(&enemy->node, dmg, 0);
                    work->field_3AA = 8;
                    if (enemy->field_40 <= 0) {
                        enemy->field_4C &= 0xF3;
                        func_actor_503500_80142310(arg0, 5);
                    } else {
                        func_actor_503500_80142310(arg0, 0);
                    }
                }
            }
        }
    }
}

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
