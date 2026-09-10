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
#include <psyq/inline_c.h>

/// `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

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
extern s32                  D_80070F70;
void                        func_actor_503500_8013BC54(Actor503500* arg0);
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
/// Row of `Gp_PackPair` arguments, one slot per effect task in this file
/// (`func_actor_503500_801448E8` takes the slot before this one). Declared as
/// an array because the read has to alias the struct stores around it: GCC
/// 2.8.1's `fixed_scalar_and_varying_struct_p` lets a scalar global at a fixed
/// address float above them, and hoists the load out of the call sequence.
extern GpU16Pair* D_actor_503500_8016E7D4[];
/// `func_actor_503500_80144300`'s `Gp_PackPair` slot, two before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7CC[];
extern Actor503500UVec D_actor_503500_801715AC;
/// `func_actor_503500_801448E8`'s `Gp_PackPair` slot, the one before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7D0[];
extern Actor503500UVec D_actor_503500_801715B4;
/// Local offset of the display node `func_actor_503500_80144E8C` links, and the
/// offsets it seeds its `GpActorD4Rec` with.
extern Actor503500UVec D_actor_503500_801715C4;
extern Actor503500UVec D_actor_503500_801715CC;
/// Local offset of the display node `func_actor_503500_801455A4` links.
extern Actor503500UVec D_actor_503500_801715D4;
/// `func_actor_503500_80145A2C`'s `Gp_PackPair` slot (the one after
/// `D_actor_503500_8016E7D4[1]`), which the original reads by its own name, and
/// its display-node and `GpActorD4Rec` offsets.
extern GpU16Pair*      D_actor_503500_8016E7DC[];
extern Actor503500UVec D_actor_503500_801715DC;
extern Actor503500UVec D_actor_503500_801715E4;
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
/// libgte routine right after `RotMatrixX` in the main executable (likely
/// `RotMatrixY`); rotates `m` in place by `angle`.
void func_8004BFF8(s32 angle, MATRIX* m);
/// Task table `func_actor_503500_801437D0` spawns entry 0 from.
extern TaskDesc D_actor_503500_8017146C;
/// Message 0x3FF payloads, indexed by side (see `Actor503500Msg3FF`).
extern Actor503500Msg3FF D_actor_503500_801714E0[];
/// Payload of the 0x3F8 query `func_actor_503500_801437D0` sends the player.
extern s32 D_actor_503500_80171544;
void       func_actor_503500_8013B460(Actor503500* arg0);
void       func_actor_503500_8013B8D0(Actor503500* arg0);
void       func_actor_503500_8013BE0C(Actor503500* arg0);
void       func_actor_503500_8013BCB4(Actor503500* arg0);
void       func_actor_503500_8013C900(Actor503500* arg0);
void       func_actor_503500_8013C9DC(Actor503500* arg0);
void       func_actor_503500_8013C960(Actor503500* arg0);
void       func_actor_503500_8013CA34(Actor503500* arg0);
void       func_actor_503500_8013CA74(Actor503500* arg0, s8 arg1);
void       func_actor_503500_8013D8BC(Actor503500* arg0);
void       func_actor_503500_8013D914(Actor503500* arg0);
void       func_actor_503500_8013D990(Actor503500* arg0);
void       func_actor_503500_8013BD0C(Actor503500* arg0);
void       func_actor_503500_8013BD88(Actor503500* arg0);
void       func_actor_503500_8013C558(Actor503500* arg0);
void       func_actor_503500_8013E384(Actor503500* arg0);
void       func_actor_503500_8013E740(Actor503500* arg0);
void       func_actor_503500_8013EBE4(Actor503500* arg0);
void       func_actor_503500_8013EA8C(Actor503500* arg0);
void       func_actor_503500_8013EAE4(Actor503500* arg0);
void       func_actor_503500_8013EB60(Actor503500* arg0);
void       func_actor_503500_8013F778(Actor503500* arg0);
void       func_actor_503500_8013F7D8(Actor503500* arg0);
void       func_actor_503500_8013F830(Actor503500* arg0);
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
void      func_actor_503500_8014473C(Task* arg0);
void      func_actor_503500_80137290(s32 arg0);
void      func_actor_503500_80144B40(Actor503500* arg0);
void      func_actor_503500_80144DA8(Task* arg0);
void      func_actor_503500_80144E10(Task* arg0);
void      func_actor_503500_80145480(Task* arg0);
void      func_actor_503500_801450A0(Actor503500* arg0);
void      func_actor_503500_801454E0(Actor503500* arg0);
void      func_actor_503500_80145754(Actor503500* arg0);
void      func_actor_503500_80145950(Task* arg0);
void      func_actor_503500_801459B0(Task* arg0);
void      func_actor_503500_80145C50(Actor503500* arg0);
void      func_actor_503500_80145E98(Task* arg0);
void      func_actor_503500_80145F18(Actor503500* arg0);
void      func_actor_503500_8014618C(Task* arg0);
void      func_actor_503500_80146524(void);
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

void func_actor_503500_80141D7C(Actor503500* arg0)
{
    Actor503500Work* work;

    work = arg0->field_1C;
    switch (work->field_3A4) {
        case 0:
            func_actor_503500_80141E64(arg0);
            break;
        case 1:
            func_actor_503500_801400A4(arg0);
            break;
        case 2:
            work->field_3A6--;
            if (work->field_3A6 < 0) {
                func_actor_503500_80142310(arg0, 0);
            }
            break;
        case 5:
            func_actor_503500_80140654(arg0);
            break;
        case 6:
            func_actor_503500_80141F48(arg0);
            break;
        case 7:
            func_actor_503500_80141FC8(arg0);
            break;
    }
    work->field_3AA--;
    if (work->field_3AA < 0) {
        work->field_3AA = 0;
    }
}

/// Per-slot local offset `func_actor_503500_80141E64` copies into
/// `Actor503500Work::field_368`, indexed by `spawnArg1`.
extern SVECTOR D_actor_503500_8016F3AC[];

void func_actor_503500_80141E64(Actor503500* arg0)
{
    Actor503500Work* work;
    u16              level;

    work = arg0->field_1C;
    if (arg0->killCountdown == 2) {
        func_actor_503500_80142310(arg0, 1);
        arg0->killCountdown = 0;
    }
    level           = work->field_3B6 + 0x20;
    work->field_3B6 = level;
    if ((s16)level >= 0x1001) {
        work->field_3B6 = 0x1000;
    }
    level           = work->field_3CC - 0x111;
    work->field_3CC = level;
    if ((s16)level < 0) {
        work->field_3CC = 0;
    }
    work->field_368.vx = D_actor_503500_8016F3AC[arg0->spawnArg1].vx;
    work->field_368.vy = D_actor_503500_8016F3AC[arg0->spawnArg1].vy;
    work->field_368.vz = D_actor_503500_8016F3AC[arg0->spawnArg1].vz;
}

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

/// Identity rotation, written two halfwords per word store. Being inline is
/// what matches: the argument is expanded as an address sum, so the caller's
/// `&mats[i]` is recomputed each iteration instead of strength-reduced.
static inline void func_actor_503500_SetRotIdentity(MATRIX* m)
{
    *(s32*)&m->m[0][0] = 0x1000;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = 0x1000;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = 0x1000;
}

/// Sub-state 0 resets the matrix table to identity; sub-state 1 raises the
/// fade level by 0x20 a frame and, once it passes 0x1000, relinks the display
/// node and moves on like `func_actor_503500_80141F48`.
void func_actor_503500_80141FC8(Actor503500* arg0)
{
    Actor503500Work* work;
    u16              level;
    s32              i;
    long*            t;

    work = arg0->field_1C;
    switch (work->field_3D0) {
        case 0:
            for (i = 1; i < 9; i++) {
                func_actor_503500_SetRotIdentity(&((Actor503500Work3D8Mtx*)work)->mats[i]);
                // The view shifted by i matrices puts mats[i] at mats[0]; this
                // `(work + i) + offset` association is what lets the pointer
                // derive from the giv the indexed store below uses.
                t                                            = ((Actor503500Work3D8Mtx*)((MATRIX*)work + i))->mats[0].t;
                ((Actor503500Work3D8Mtx*)work)->mats[i].t[0] = 0;
                t[1]                                         = 0;
                t[2]                                         = 0;
            }
            work->field_3B2 = 0;
            work->field_3D0++;
        case 1:
            level           = work->field_3B2 + 0x20;
            work->field_3B2 = level;
            if ((s16)level >= 0x1001) {
                Gp_LinkNode(&arg0->field_20->node);
                work->field_3B2     = 0x1000;
                work->obj160.flags |= 0x8000;
                func_actor_503500_80142310(arg0, 0);
            }
            break;
    }
}

/// Steps the 0x3D8 block's countdown at 0x3A8 down to zero, then, unless the
/// global freeze is on, runs both display nodes through their record tables
/// before releasing the tables. Same shape as `func_actor_503500_80144004`.
void func_actor_503500_801420C4(Actor503500* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->field_1C;
    if (work->field_3A8 != 0) {
        timer           = (u16)work->field_3A8 - 1;
        work->field_3A8 = timer;
        if (timer < 0) {
            work->field_3A8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_80140D38(arg0, &work->obj160, work->rec180, 8);
        func_actor_503500_8014215C(arg0, &work->obj240, work->rec260, 4);
    }
    Gp_ClearRec18Occupied(work->rec180);
    Gp_ClearRec18Occupied(work->rec260);
}
/// Scans `count` `GpRec18` slots and clears bit 0x8000 of `obj->flags` for
/// every slot whose `field_4` high half is 1.
void func_actor_503500_8014215C(Actor503500* arg0, GpObj* obj, GpRec18* rec, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++, rec++) {
        if ((rec->field_4 & 0xFFFF0000) == 0x10000) {
            obj->flags &= 0x7FFF;
        }
    }
}

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
void func_actor_503500_80142220(SVECTOR* angles, GsCOORDINATE2* nodes)
{
    SVECTOR ang;
    MATRIX* m;
    s32     i;

    for (i = 2; i < 8; i++) {
        m = &nodes[i].coord;
        Gp_ExtractEuler(&ang, m);
        ang.vx                 = angles[i].vx;
        *(s32*)&nodes[i].coord = ONE;
        *(s32*)&m->m[0][2]     = 0;
        *(s32*)&m->m[1][1]     = ONE;
        *(s32*)&m->m[2][0]     = 0;
        m->m[2][2]             = ONE;
        RotMatrixZYX(&ang, m);
    }
}
/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
void func_actor_503500_801422B8(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}
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

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_801423C8);

/// Sub-state 1 of the 0x224 enemy. Phase 0 hands the parent 0xC, or 0x12 when
/// `func_actor_503500_80135E04` accepts slot 4/5, and keeps the pick in
/// `field_223`. Phase 1 counts frames in `field_21A`: on frame 0x7A (0xC) or
/// 0x51 (0x12) it sets bit 0x8000 on `obj1` / `obj2` and plays 0x40230009 at
/// parent coordinate 6 or 12 (by `field_220`); on 0x90 / 0x60 it clears them,
/// and it moves on once `func_actor_503500_80136014` reports done.
void func_actor_503500_8014271C(Actor503500* arg0)
{
    Actor503500Work224* work;
    GsCOORDINATE2*      coords;
    GsCOORDINATE2*      coord;
    s32                 side;
    s32                 anim;
    s16                 frame;

    work = (Actor503500Work224*)arg0->field_1C;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80144238(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    side = 3;
    if (work->field_220 != 0) {
        side = 2;
    }
    switch (work->field_222) {
        case 0:
            anim = 0xC;
            if (func_actor_503500_80135E04(arg0->parent, side == 2 ? 5 : 4) != 0) {
                anim = 0x12;
            }
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, side, anim);
            work->field_223 = anim;
            work->field_222++;
            break;
        case 1:
            frame = ++work->field_21A;
            if (work->field_223 == 0xC) {
                switch (frame) {
                    case 0x7A:
                        work->obj1.flags |= 0x8000;
                        work->obj2.flags |= 0x8000;
                        coords            = ((TmdObject*)arg0->parent->extra)->field_8;
                        if (work->field_220 != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(0x40230009, (s8)Gp_GetObjPan((GpObj38*)coord),
                                            (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
                        break;
                    case 0x90:
                        work->obj1.flags &= 0x7FFF;
                        work->obj2.flags &= 0x7FFF;
                        break;
                }
            } else {
                switch (frame) {
                    case 0x51:
                        work->obj1.flags |= 0x8000;
                        work->obj2.flags |= 0x8000;
                        coords            = ((TmdObject*)arg0->parent->extra)->field_8;
                        if (work->field_220 != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(0x40230009, (s8)Gp_GetObjPan((GpObj38*)coord),
                                            (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
                        break;
                    case 0x60:
                        work->obj1.flags &= 0x7FFF;
                        work->obj2.flags &= 0x7FFF;
                        break;
                }
            }
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, side) != 0) {
                work->obj1.flags &= 0x7FFF;
                work->obj2.flags &= 0x7FFF;
                work->field_222++;
            }
            break;
        default:
            func_actor_503500_80144238(arg0, 0);
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_80132178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_80142980);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_801431EC);

/// Scans the 0x224 enemy's shared record table. For each record whose
/// `field_4` high half is 1 - unless the player task (`Game_GetPtrSlot(3)`)
/// is in mode 2 or answers message 0x3F8 - copies the parent's root rotation
/// into `field_40` and turns it by +/-0x5DC with `func_8004BFF8` (sign from
/// `field_220`), then takes the world position of parent coordinate 5 or 11
/// into the player's frame. The sign of its z picks the 0x3FF payload and is
/// passed to the task spawned from `D_actor_503500_8017146C`; message 0x3F9
/// carries the enemy's packed pair, and sound 7 plays at the player.
void func_actor_503500_801437D0(Actor503500* arg0, GpRec18* rec, s32 count)
{
    SVECTOR             vec;
    MATRIX              world;
    MATRIX              rot;
    GpEnemy*            enemy;
    Actor503500Work224* work;
    GsCOORDINATE2*      coord;
    Task*               player;
    GsCOORDINATE2*      pcoord;
    s32*                src;
    s32*                dst;
    s32                 i;
    s32                 j;
    s32                 side;
    s32                 pan;

    enemy = arg0->field_20;
    work  = (Actor503500Work224*)arg0->field_1C;
    for (i = 0; i < count; i++) {
        if ((rec[i].field_4 & 0xFFFF0000) == 0x10000) {
            player = Game_GetPtrSlot(3);
            pcoord = ((TmdObject*)player->extra)->field_8;
            if (((GameActor*)player->idMap)->field_954 != 2 &&
                Gp_DispatchMsg(player, 0x3F8, (s32)&D_actor_503500_80171544, 0) == 0) {
                coord = ((TmdObject*)arg0->parent->extra)->field_8;
                src   = (s32*)&coord->coord;
                dst   = (s32*)&work->field_40;
                for (j = 0; j < 4; j++) {
                    *dst++ = *src++;
                }
                work->field_40.m[2][2] = coord->coord.m[2][2];
                if (work->field_220 != 0) {
                    func_8004BFF8(0x5DC, &work->field_40);
                    coord = &((TmdObject*)arg0->parent->extra)->field_8[5];
                } else {
                    func_8004BFF8(-0x5DC, &work->field_40);
                    coord = &((TmdObject*)arg0->parent->extra)->field_8[11];
                }
                Gp_ComposeParentWorld(coord, &world, &vec);
                TRANSPOSE_ROT(&pcoord->coord, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&vec);
                gte_rtv0_real();
                gte_stsv(&vec);
                side = vec.vz >= 0;
                Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_801714E0[side], 0);
                Task_SpawnFromTable(&D_actor_503500_8017146C, 0, side, (s32)&work->field_40);
                Gp_StateC08.field_6 |= 1;
                pan                  = (s8)Gp_GetObjPan((GpObj38*)pcoord);
                SndEvt_EnqueueType6(7, pan, (s8)(Gp_GetObjDepth((GpObj38*)pcoord) / 2));
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_80143AC0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_801321DC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_801321E8);

void func_actor_503500_80143EB4(Actor503500* arg0)
{
    GpEnemy*   enemy;
    TmdObject* tmd;
    s32        mode;

    enemy = arg0->field_20;
    mode  = D_801153F4;
    tmd   = arg0->extra;
    switch (mode) {
        case 1:
            if (!(tmd->field_C & 0x80)) {
                func_actor_503500_80143FFC(arg0);
            }
            break;
        case 2:
            tmd->field_C        |= 0x80;
            enemy->node.field_4 |= 1;
            break;
        default:
            if (enemy->field_4C != 0) {
                func_actor_503500_80144098(arg0, mode, enemy);
            }
            func_actor_503500_80143FFC(arg0);
            func_actor_503500_80144004(arg0);
            func_actor_503500_801440F0(arg0);
            break;
    }
}
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
void func_actor_503500_80144098(Actor503500* arg0, s32 arg1, GpEnemy* arg2)
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

void func_actor_503500_8014418C(Actor503500* arg0)
{
    switch (arg0->killCountdown) {
        case 2:
            func_actor_503500_80144238(arg0, 1);
            arg0->killCountdown = 0;
            break;
        case 8:
            func_actor_503500_80144238(arg0, 3);
            arg0->killCountdown = 0;
            break;
    }
}
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

void func_actor_503500_80144300(Task* arg0)
{
    Actor503500WorkC0* work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    VECTOR             v;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = (TaskIdMap*)work;

    work->field_84.vx.w = coord->coord.t[0] << 16;
    work->field_84.vy.w = coord->coord.t[1] << 16;
    work->field_84.vz.w = coord->coord.t[2] << 16;
    work->field_94.vx   = work->field_84.vx.w;
    work->field_B8      = 0x1000;
    work->field_94.vy   = work->field_84.vy.w;
    work->field_94.vz   = work->field_84.vz.w;

    if (arg0->spawnArg2 != NULL) {
        v.vx = 0;
        v.vy = 0;
        v.vz = (s32)arg0->spawnArg2;
        ApplyMatrixLV(&coord->coord, &v, &work->field_A4);
    } else {
        m     = (GpMtxWords*)&coord->coord;
        m->w0 = 0x1000;
        m->w1 = 0;
        m->w2 = 0x1000;
        m->w3 = 0;
        m->h4 = 0x1000;
    }
    rec = work->rec;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = D_actor_503500_801715AC.vx;
    work->obj.field_12 = D_actor_503500_801715AC.vy;
    work->obj.field_14 = D_actor_503500_801715AC.vz;
    work->obj.field_18 = Gp_PackPair(D_actor_503500_8016E7CC[0], arg0->spawnArg1);
    work->obj.field_1C = 0x12C;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->obj.flags |= 0xC000;

    eff = Gp_SpawnEff(0x60189, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_8014473C(arg0);
        return;
    }
    child          = eff->field_0;
    work->field_80 = child;
    Task_Reparent(arg0, child);
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(0x40230005, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    func_actor_503500_80137290(1);
    arg0->exitCallback = func_actor_503500_8014473C;
    arg0->state       += 1;
}

void func_actor_503500_80144520(Actor503500* arg0)
{
    Actor503500WorkC0* work;
    GsCOORDINATE2*     coord;

    work  = (Actor503500WorkC0*)arg0->field_1C;
    coord = arg0->extra->field_8;
    if (work->field_BE != 0) {
        work->field_80->spawnArg1 = 2;
        work->field_BC            = -1;
    }
    switch (work->field_BC) {
        case 0:
            work->field_A4.vy += 9.8 * 0x10000;
            if (work->field_B4 != 0) {
                work->obj.field_1C = 0x258;
                work->field_A4.vx  = 0;
                work->field_A4.vy  = 0;
                work->field_A4.vz  = 0;
                work->field_BF     = 1;
                work->field_B4     = 0;
                work->field_BA     = 0;
                work->field_BC++;
            } else {
                work->field_BA++;
                if ((s16)work->field_BA >= 0x3D) {
                    work->field_BC = -1;
                }
            }
            break;
        case 1:
            work->field_BA++;
            if ((s16)work->field_BA >= 6) {
                work->field_80->spawnArg1 = 2;
                work->field_BA            = 0;
                work->obj.flags          &= 0x7FFF;
                work->field_BC++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    work->field_84.vx.w += work->field_A4.vx;
    work->field_84.vy.w += work->field_A4.vy;
    work->field_84.vz.w += work->field_A4.vz;
    coord->coord.t[0]    = work->field_84.vx.h.hi;
    coord->coord.t[1]    = work->field_84.vy.h.hi;
    coord->coord.t[2]    = work->field_84.vz.h.hi;
}

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

void func_actor_503500_80144778(Actor503500* arg0)
{
    GpDeltaScratch     delta;
    Actor503500WorkC0* work;
    GpRec18*           rec;
    s32                result;
    s32                i;

    work = (Actor503500WorkC0*)arg0->field_1C;
    rec  = work->rec;
    if (work->field_BF == 0) {
        result         = func_800E0C10(rec, &delta, 4, NULL);
        work->field_B4 = result;
        switch (result) {
            case 0:
                break;
            case 1:
                work->field_84.vx.w += delta.vx.w;
                work->field_84.vy.w += delta.vy.w;
                work->field_84.vz.w += delta.vz.w;
                break;
            case 2:
                work->field_84.vx.w = work->field_94.vx;
                work->field_84.vy.w = work->field_94.vy;
                work->field_84.vz.w = work->field_94.vz;
                break;
        }
    }
    for (i = 0; i < 4; i++) {
        if ((rec[i].field_4 & 0xFFFF0000) == 0x10000) {
            work->field_BE = 1;
        }
    }
    Gp_ClearRec18Occupied(work->rec);
}

void func_actor_503500_80144890(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321DC;
    sp.funcs[task->state](task);
}

void func_actor_503500_801448E8(Task* arg0)
{
    Actor503500WorkB4* work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    s32                pan;
    s32                pan2;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = (TaskIdMap*)work;

    work->field_84.vx = coord->coord.t[0] << 16;
    work->field_84.vy = coord->coord.t[1] << 16;
    work->field_84.vz = coord->coord.t[2] << 16;
    work->field_94.vx = work->field_84.vx;
    work->field_AC    = 0x1000;
    work->field_94.vy = work->field_84.vy;
    work->field_94.vz = work->field_84.vz;

    if (arg0->spawnArg2 != NULL) {
        work->field_A8 = (s32)arg0->spawnArg2;
    } else {
        m              = (GpMtxWords*)&coord->coord;
        m->w0          = 0x1000;
        m->w1          = 0;
        m->w2          = 0x1000;
        m->w3          = 0;
        m->h4          = 0x1000;
        work->field_A8 = 0x100000;
    }
    rec = work->rec;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = D_actor_503500_801715B4.vx;
    work->obj.field_12 = D_actor_503500_801715B4.vy;
    work->obj.field_14 = D_actor_503500_801715B4.vz;
    work->obj.field_18 = Gp_PackPair(D_actor_503500_8016E7D0[0], arg0->spawnArg1);
    work->obj.field_1C = 0x898;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->obj.flags |= 0x8000;

    if (arg0->spawnArg1 == 0) {
        eff = Gp_SpawnEff(0x6018E, coord, 0, NULL);
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0x40230008, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    } else {
        eff  = Gp_SpawnEff(0x6018D, coord, 0, NULL);
        pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0x40230007, pan2, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    }
    if (eff == NULL) {
        func_actor_503500_80144DA8(arg0);
        return;
    }
    child          = eff->field_0;
    work->field_80 = child;
    Task_Reparent(arg0, child);
    func_actor_503500_80137290(3);
    arg0->exitCallback = func_actor_503500_80144DA8;
    arg0->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_80144B40);

void func_actor_503500_80144D50(Actor503500* arg0)
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
    func_actor_503500_80144E10((Task*)arg0);
    func_actor_503500_80144B40(arg0);
}
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

void func_actor_503500_80144E8C(Task* arg0)
{
    Actor503500WorkD0* work;
    GsCOORDINATE2*     coord;
    GpActorD4Rec*      d4;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m1;
    GpMtxWords*        m2;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap    = (TaskIdMap*)work;
    work->field_C4 = 0x1000;

    m1     = (GpMtxWords*)&coord->coord;
    m1->w0 = 0x1000;
    m1->w1 = 0;
    m1->w2 = 0x1000;
    m1->w3 = 0;
    m1->h4 = 0x1000;

    m2     = (GpMtxWords*)&work->field_9C;
    m2->w0 = 0x1000;
    m2->w1 = 0;
    m2->w2 = 0x1000;
    m2->w3 = 0;
    m2->h4 = 0x1000;

    d4  = &work->head.d4;
    rec = work->head.rec;

    work->head.obj.field_8  = coord;
    work->head.obj.field_C  = (GpRec18*)d4;
    work->head.obj.field_10 = D_actor_503500_801715C4.vx;
    work->head.obj.field_12 = D_actor_503500_801715C4.vy;
    work->head.obj.field_14 = D_actor_503500_801715C4.vz;
    work->head.obj.field_18 = Gp_PackPair(D_actor_503500_8016E7D4[0], 0);
    work->head.obj.flags    = 3;
    work->head.obj.field_1C = 0;

    d4->field_14 = rec;
    d4->field_8  = 0;
    d4->field_A  = 0;
    d4->field_C  = 0;
    d4->field_0  = D_actor_503500_801715CC.vx;
    d4->field_2  = D_actor_503500_801715CC.vy;
    d4->field_4  = D_actor_503500_801715CC.vz;
    d4->field_12 = 0x3E8;
    d4->field_10 = 0x7D0;

    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->head.obj.flags &= 0x7FFF;

    if (arg0->spawnArg1 == 0) {
        eff = Gp_SpawnEff(0x60195, coord, 0, NULL);
        if (eff == NULL) {
            func_actor_503500_80145480(arg0);
            return;
        }
        child          = eff->field_0;
        work->field_98 = child;
        Task_Reparent(arg0, child);
    }
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(0x4023000A, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145480;
    arg0->state       += 1;
}
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_7", func_actor_503500_801450A0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_80132218);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_80132224);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_80132230);

void func_actor_503500_80145428(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord      = arg0->extra->field_8;
    coord->flg = 0;
    func_actor_503500_801454E0(arg0);
    func_actor_503500_801450A0(arg0);
}
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

void func_actor_503500_801455A4(Task* arg0)
{
    Actor503500Work44* work;
    GsCOORDINATE2*     coord;
    GpMtxWords*        m;
    GpEffWork*         eff;
    Task*              child;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = (TaskIdMap*)work;

    m     = (GpMtxWords*)&coord->coord;
    m->w0 = 0x1000;
    m->w1 = 0;
    m->w2 = 0x1000;
    m->w3 = 0;
    m->h4 = 0x1000;

    work->head.obj.field_8  = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    work->head.obj.field_C  = &work->head.rec;
    work->head.obj.field_10 = D_actor_503500_801715D4.vx;
    work->head.obj.field_12 = D_actor_503500_801715D4.vy;
    work->head.obj.field_14 = D_actor_503500_801715D4.vz;
    work->head.obj.field_18 = Gp_PackPair(D_actor_503500_8016E7D4[1], 0);
    work->head.obj.field_1C = 0x12C;
    work->head.obj.flags    = 1;
    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(&work->head.rec, 1, 0);
    work->head.obj.flags &= 0x7FFF;

    eff = Gp_SpawnEff(0x6018A, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_80145950(arg0);
        return;
    }
    child          = eff->field_0;
    work->field_38 = child;
    Task_Reparent(arg0, child);
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(0x4023000C, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    func_actor_503500_80137290(6);
    arg0->exitCallback = func_actor_503500_80145950;
    arg0->state       += 1;
}

extern MATRIX* D_80073B8C;

void func_actor_503500_80145754(Actor503500* arg0)
{
    Actor503500Work44* work;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     coord2;
    s32                pan;
    s32                pan2;

    work = (Actor503500Work44*)arg0->field_1C;
    if (func_actor_503500_8013608C(arg0) == 0) {
        switch (work->field_40) {
            case 0:
                work->field_3C++;
                if (work->field_3C >= 0x5F) {
                    if (D_80073B8C->t[1] < -1000) {
                        work->head.obj.flags |= 0x8000;
                    }
                    work->field_3C = 0;
                    work->field_40++;
                } else if (work->field_3C == 0x3E) {
                    coord = arg0->extra->field_8;
                    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(0x40230014, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
                } else if (work->field_3C == 0x5A) {
                    coord2 = arg0->extra->field_8;
                    pan2   = (s8)Gp_GetObjPan((GpObj38*)coord2);
                    SndEvt_EnqueueType6(0x4023000D, pan2, (s8)(Gp_GetObjDepth((GpObj38*)coord2) / 2));
                }
                return;
            case 1:
                work->field_3C++;
                if (work->field_3C >= 4) {
                    work->field_3C        = 0;
                    work->head.obj.flags &= 0x7FFF;
                    work->field_40++;
                }
                return;
        }
    }
    arg0->state += 1;
}

void func_actor_503500_801458F8(Actor503500* arg0)
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
    func_actor_503500_801459B0((Task*)arg0);
    func_actor_503500_80145754(arg0);
}
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

void func_actor_503500_80145A2C(Task* arg0)
{
    Actor503500WorkAC* work;
    GsCOORDINATE2*     coord;
    GpActorD4Rec*      d4;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    s32                pan;
    s32                pan2;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = (TaskIdMap*)work;

    m     = (GpMtxWords*)&coord->coord;
    m->w0 = 0x1000;
    m->w1 = 0;
    m->w2 = 0x1000;
    m->w3 = 0;
    m->h4 = 0x1000;

    d4  = &work->head.d4;
    rec = work->head.rec;

    work->head.obj.field_8  = coord;
    work->head.obj.field_C  = (GpRec18*)d4;
    work->head.obj.field_10 = D_actor_503500_801715DC.vx;
    work->head.obj.field_12 = D_actor_503500_801715DC.vy;
    work->head.obj.field_14 = D_actor_503500_801715DC.vz;
    work->head.obj.field_18 = Gp_PackPair(D_actor_503500_8016E7DC[0], 0);
    work->head.obj.flags    = 3;
    work->head.obj.field_1C = 0;

    d4->field_14 = rec;
    d4->field_8  = 0;
    d4->field_A  = 0;
    d4->field_C  = 0;
    d4->field_0  = D_actor_503500_801715E4.vx;
    d4->field_2  = D_actor_503500_801715E4.vy;
    d4->field_4  = D_actor_503500_801715E4.vz;
    d4->field_12 = 0x7D0;
    d4->field_10 = 0xBB8;

    Gp_LinkObj(3, &work->head.obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->head.obj.flags &= 0x7FFF;

    eff = Gp_SpawnEff(0x60190, coord, arg0->spawnArg1, NULL);
    if (eff == NULL) {
        func_actor_503500_80145E98(arg0);
        return;
    }
    child          = eff->field_0;
    work->field_98 = child;
    Task_Reparent(arg0, child);
    if (Game_Session->field_1 != 0) {
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0x40230013, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    } else {
        pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0x4023000E, pan2, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
    }
    func_actor_503500_80137290(8);
    arg0->exitCallback = func_actor_503500_80145E98;
    arg0->state       += 1;
}

void func_actor_503500_80145C50(Actor503500* arg0)
{
    Actor503500WorkAC* work;
    GsCOORDINATE2*     coord;
    s32                pan;

    work = (Actor503500WorkAC*)arg0->field_1C;
    switch (work->field_A8) {
        case 0:
            if (D_80070F70 & 1) {
                Gp_SpawnPadLerp(1, 0x96, 0x96);
            }
            if (++work->field_A4 < 0x5B) {
                return;
            }
            if (Game_Session->field_1 == 0) {
                work->head.obj.flags |= 0x8000;
                coord                 = arg0->extra->field_8;
                pan                   = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(0x4023000F, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
            }
            goto next;
        case 1:
            if (Game_Session->field_1 == 0) {
                Gp_SpawnPadLerp(1, 0xFF, 0xFF);
            }
            if (++work->field_A4 < 0x38) {
                return;
            }
            work->head.obj.flags &= 0x7FFF;
            SndEvt_EnqueueType7(0x4023000F, 1);
        next:
            work->field_A4 = 0;
            work->field_A8++;
            return;
        case 2:
            if (++work->field_A4 < 0x24) {
                return;
            }
        default:
            arg0->state += 1;
            return;
    }
}

void func_actor_503500_80145E1C(Actor503500* arg0)
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
    func_actor_503500_80145F18(arg0);
    func_actor_503500_80145C50(arg0);
    if (func_actor_503500_8013608C(arg0->field_20)) {
        arg0->exitCallback((Task*)arg0);
    }
}
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

void func_actor_503500_80145F18(Actor503500* arg0)
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
void func_actor_503500_80145F84(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132224;
    sp.funcs[task->state](task);
}

/// Per-frame tick of the `Actor503500Effect4CC` effect: runs the motion
/// handler `field_4C0` selects, adds the 16.16 velocity `field_4B0` onto the
/// accumulator `field_4A0`, moves the coordinate by the integer part and keeps
/// only the fraction, then ticks the animation slots and the actor colour.
void func_actor_503500_80145FDC(Actor503500* task)
{
    VECTOR                pos;
    TmdObject*            ext      = task->extra;
    Actor503500Effect4CC* work     = (Actor503500Effect4CC*)task->field_1C;
    TaskFunc              funcs[2] = { (TaskFunc)func_actor_503500_80146524, func_actor_503500_8014618C };
    GsCOORDINATE2*        coord;
    s32                   i;

    funcs[work->field_4C0]((Task*)task);
    coord              = task->extra->field_8;
    work->field_4A0   += work->field_4B0;
    work->field_4A4   += work->field_4B4;
    work->field_4A8   += work->field_4B8;
    coord->coord.t[0] += (s16)(work->field_4A0 >> 16);
    coord->coord.t[1] += (s16)(work->field_4A4 >> 16);
    coord->coord.t[2] += (s16)(work->field_4A8 >> 16);
    coord->flg         = 0;
    work->field_4A0    = (u16)work->field_4A0;
    work->field_4A4    = (u16)work->field_4A4;
    work->field_4A8    = (u16)work->field_4A8;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->field_20, &pos, 0, 0);
    }
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4C8--;
    }
}

/// Motion handler 1 of `Actor503500Effect4CC` (`field_4C0`), stepped by
/// `field_4C2`: state 0 saves the coordinate's rotation words into
/// `field_480`/`field_490`, state 1 waits 31 frames, and state 2 restores that
/// rotation every frame while squashing its Y scale `field_4C6` from 0x1000 down
/// to 0x200, firing the light and spark cues on the way before advancing the
/// task at frame 150.
void func_actor_503500_8014618C(Task* arg0)
{
    VECTOR                scale;
    GsCOORDINATE2*        coord;
    Actor503500Effect4CC* work;
    TmdObject*            ext;
    void*                 enemy;
    s32*                  src;
    s32*                  dst;
    s32                   i;

    // `extra` is read twice on purpose: the second read is what leaves the
    // target's `move s2, v0` copy.
    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor503500Effect4CC*)arg0->idMap;
    enemy = arg0->spawnArg2;
    ext   = arg0->extra;
    switch (work->field_4C2) {
        case 0:
            work->field_4C4 = 0;
            work->field_4C6 = 0x1000;
            dst             = work->field_480;
            src             = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->field_490 = coord->coord.m[2][2];
            work->field_4C2++;
            break;
        case 1:
            work->field_4C4++;
            if (work->field_4C4 >= 0x1F) {
                work->field_4C4 = 0;
                work->field_4C2++;
            }
            break;
        case 2:
            if (work->field_4C6 > 0x200) {
                work->field_4C6 -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = work->field_480;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->field_490;
            scale.vx             = 0x1000;
            scale.vy             = work->field_4C6;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            work->field_4C4++;
            switch (work->field_4C4) {
                case 0x14:
                    ext->field_C |= 2;
                    Gp_SetLightMode(enemy, 1);
                    break;
                case 0x1E:
                    Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    break;
                case 0x64:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 0x96:
                    arg0->state++;
                    break;
            }
            break;
    }
}

void func_actor_503500_801463C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132230;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_503500_8014642C(Actor503500* arg0)
{
    Actor503500Effect4CC* work;
    GsCOORDINATE2*        coord;
    GpEnemy*              enemy;

    enemy = arg0->field_20;
    coord = arg0->extra->field_8;

    work = Mem_Calloc(sizeof(Actor503500Effect4CC), false);
    if (work == NULL) {
        Gp_EnemyTaskExit((Task*)arg0);
        return;
    }

    arg0->field_1C  = (Actor503500Work*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C8 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->field_54 = 0;

    func_actor_503500_80146508((Task*)arg0);
    TOUCH_REG(enemy);

    arg0->field_24     = D_actor_503500_80176530;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state++;
}