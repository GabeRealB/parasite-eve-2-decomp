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
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>

/// `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// `mvmva 1, 0, 3, 3, 0` (`rtir`), spelled out for the same reason.
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

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
/// Eighteen effect offsets `func_actor_503500_8013C558` picks from at random.
extern SVECTOR D_actor_503500_8016F1B8[];
/// Rest pitch of each link of the 0x3D8 enemy's chain, scaled by
/// `Actor503500Work3D8Chain::field_3CC` in `func_actor_503500_80141448`.
extern s16 D_actor_503500_8016F434[];
void       func_actor_503500_8014176C(SVECTOR* pts, GsCOORDINATE2* coords);
void       func_actor_503500_80141A44(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
void       func_actor_503500_80142220(SVECTOR* angles, GsCOORDINATE2* nodes);
/// The same pair for the 0xF0 enemy at `D_actor_503500_8017797C`.
extern SVECTOR         D_actor_503500_8016F2D8;
extern Actor503500Work D_actor_503500_8017797C;
/// Per-slot parent part index and local offset of the two 0xF0 enemies in
/// `D_actor_503500_801774C0`, indexed by `spawnArg1 - 4`.
extern s32                  D_actor_503500_8016F0E8[];
extern SVECTOR              D_actor_503500_8016F0F0[];
extern Actor503500Work774C0 D_actor_503500_801774C0[];
extern RECT                 D_actor_503500_8016F100;
/// `func_actor_503500_8013B8D0`'s nine effect offsets, one per frame, and the
/// per-side pair of rects it moves on frame 0x14, indexed by `field_EC`.
extern SVECTOR D_actor_503500_8016F168[];
/// Declared in `gameplay/gameplay.h`, which conflicts with this TU's headers.
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
/// Effect offsets `func_actor_503500_80139014` cycles through, three entries.
extern SVECTOR D_actor_503500_8016F0D0[];
/// The same three-entry cycle for `func_actor_503500_80140654`.
extern SVECTOR D_actor_503500_8016F448[];
/// Main-executable counter; tested here for multiples of 12 (actor_503500_7.c
/// reads its parity).
extern s32  D_80070F70;
extern RECT D_actor_503500_8016F148[][2];
/// Offsets `func_actor_503500_8013F4A4` spawns its two effects at: rows 0-5
/// for the per-frame effect, rows 3-5 for the odd-frame one.
extern SVECTOR D_actor_503500_8016F374[];
extern RECT    D_actor_503500_8016F3A4;
/// `func_actor_503500_8013E384`'s per-shot tables, indexed by shot 0..5: a
/// spawn-arg term (`<< 12`) and the rotation each spawned child is turned by.
extern s16     D_actor_503500_8016F2E0[];
extern SVECTOR D_actor_503500_8016F2EC[];
/// `func_actor_503500_8013D1CC`'s spawn offset and rotation for its one child.
extern SVECTOR D_actor_503500_8016F258;
extern SVECTOR D_actor_503500_8016F260;
extern MATRIX* D_80073B8C;
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
/// The two three-row effect offset tables `func_actor_503500_8013D558`
/// spawns from: the first for spawn slot 7, the second for every other slot.
extern SVECTOR D_actor_503500_8016F278[];
extern SVECTOR D_actor_503500_8016F290[];
/// The same pair for the 0x160 enemy at `D_actor_503500_80176D88`: a world
/// translation seeded into the task's own coordinate and the local offset its
/// `GpEnemy::field_1C` and display node share.
extern SVECTOR         D_actor_503500_8016F060;
extern SVECTOR         D_actor_503500_8016F068;
extern Actor503500Work D_actor_503500_80176D88;
/// Per-slot tables of the 0x2EC enemies in `D_actor_503500_80176EE8`: world
/// translation and rotation of the task's coordinate (indexed by
/// `spawnArg1 - 2`), the local offset its `GpEnemy::field_1C` and display node
/// share, and the vector seeded into `field_294` / `field_29C` (indexed by
/// `spawnArg1`).
extern SVECTOR            D_actor_503500_8016F090[];
extern SVECTOR            D_actor_503500_8016F0A0[];
extern SVECTOR            D_actor_503500_8016F0B0;
extern Actor503500UVec    D_actor_503500_8016F0A8[];
extern Actor503500Work2EC D_actor_503500_80176EE8[];
/// Rest offset of the same enemies' `field_29C`, indexed by `spawnArg1 - 2`
/// (the bytes of `D_actor_503500_8016F0A8[2..]`, named on their own because the
/// code indexes from here), and the local vector `func_actor_503500_80138A30`
/// rotates by `field_2A4` and adds to it every frame.
extern Actor503500UVec D_actor_503500_8016F0B8[];
extern SVECTOR         D_actor_503500_8016F0C8;
void                   func_actor_503500_8013A900(Actor503500* arg0);
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
/// Spawns slot enemy `arg1` as a child of `arg0`; returns it, or NULL.
GpEnemy* func_actor_503500_80135D00(Actor503500* arg0, s32 arg1);
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
void func_actor_503500_80135E20(Actor503500* arg0, s32 arg1, SVECTOR* arg2);
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
void      func_actor_503500_8013A470(SVECTOR* pts, GsCOORDINATE2* coords, s32 phase);
void      func_actor_503500_8013A7B0(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
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
/// are passed by every caller but the body ignores them. Always returns 0.
s32 func_actor_503500_80135950(Actor503500* arg0, s32 arg1,
                               Actor503500AnimPreset* arg2, s32 arg3);
/// The animation preset `func_actor_503500_80136D30` re-applies when the boss
/// finishes the clip it was gating on.
extern Actor503500AnimPreset D_actor_503500_8016EAD4;

/// Per-slot parent part index and local offset of the 0x224 enemy in
/// `D_actor_503500_80178AC0`, indexed by `spawnArg1 - 0xA`, and the
/// translation it gives the task's own coordinate.
extern s32                D_actor_503500_80171464[];
extern SVECTOR            D_actor_503500_80171480[];
extern SVECTOR            D_actor_503500_80171478;
extern Actor503500Work224 D_actor_503500_80178AC0[];
/// `func_actor_503500_80144300`'s `Gp_PackPair` slot, two before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7CC[];
extern Actor503500UVec D_actor_503500_801715AC;
/// `func_actor_503500_801448E8`'s `Gp_PackPair` slot, the one before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7D0[];
extern Actor503500UVec D_actor_503500_801715B4;
/// Frame count `func_actor_503500_80144B40` holds its sub-state 1 for, indexed
/// by `Task::spawnArg1`.
extern s32 D_actor_503500_801715BC[];
/// libgte routine right after `RotMatrixX` in the main executable (likely
/// `RotMatrixY`); rotates `m` in place by `angle`.
void func_8004BFF8(s32 angle, MATRIX* m);
/// Task table `func_actor_503500_801437D0` spawns entry 0 from.
extern TaskDesc D_actor_503500_8017146C;
/// Message 0x3FF payloads, indexed by side (see `Actor503500Msg3FF`).
extern Actor503500Msg3FF D_actor_503500_801714E0[];
/// Payload of the 0x3F8 query `func_actor_503500_801437D0` sends the player.
extern s32 D_actor_503500_80171544;
void       func_actor_503500_80143F78(Actor503500* arg0);
void       func_actor_503500_8014473C(Task* arg0);
void       func_actor_503500_80144DA8(Task* arg0);

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy - the same body as `func_actor_503500_80137C90` on this block's
/// fields: each attack id is taken once, only type-2 ids land while the
/// `field_E8` countdown is clear, and a hit that empties `field_40` starts the
/// death sub-state. The hit effect is pulled to 1400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
void func_actor_503500_8013C088(Actor503500* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->field_40 -= dmg;
        if (enemy->field_40 <= 0) {
            func_actor_503500_8013CA74(arg0, 1);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].field_8 - coord->workm.t[0];
        pos.vy = arg2[i].field_A - coord->workm.t[1];
        pos.vz = arg2[i].field_C - coord->workm.t[2];
        scale  = 0x578000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F1B0.vx;
        pos.vy += D_actor_503500_8016F1B0.vy;
        pos.vz += D_actor_503500_8016F1B0.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, (GpEffArg*)&work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Death sub-state of the first 0xF4 block, stepped by `field_F1`: phase 0
/// is the death setup shared with `func_actor_503500_8013D558` and seeds the
/// shrinking scale; phase 1 spawns three effects at random offsets for up to
/// 2000 frames while the scale shrinks by an ever smaller step, and leaves
/// once that step runs out.
void func_actor_503500_8013C558(Actor503500* arg0)
{
    Actor503500Work776A0* work;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    s32                   pan;

    work  = (Actor503500Work776A0*)arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_F1) {
        case 0:
            work->obj.flags         &= 0x7FFF;
            arg0->field_20->field_54 = 0;
            Gp_UnlinkNode(&arg0->field_20->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            arg0->field_20->field_4C &= 0xF0;
            pan                       = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
            work->field_EC = 0x1000;
            work->field_EE = 0x80;
            work->field_F1++;
            break;
        case 1:
            if (++work->field_EA <= 2000) {
                if (func_actor_503500_801360BC(arg0->spawnArg1, 5) != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60055, coord, 0x01001900,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60055, coord, 0x01001700,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6018C, coord, 0x01404600,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                }
            }
            vec.vx = 0x1000;
            vec.vy = 0x1000;
            vec.vz = work->field_EC;
            func_actor_503500_80135E20((Actor503500*)arg0->parent, 0x10, &vec);
            work->field_EC -= work->field_EE;
            work->field_EE -= 4;
            if (work->field_EE <= 0) {
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

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_80132060);

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// this form - the same pass as `func_actor_503500_8013C088`: each attack id
/// is taken once, only type-2 ids land while the `field_E8` countdown is
/// clear, and a hit that empties `field_40` starts death sub-state 3. The hit
/// effect is pulled to 200 units along the contact offset and shifted by this
/// slot's `D_actor_503500_8016F210` entry. `arg1` is passed but unused.
void func_actor_503500_8013CCBC(Actor503500* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->field_40 -= dmg;
        if (enemy->field_40 <= 0) {
            func_actor_503500_8013DBA8(arg0, 3);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].field_8 - coord->workm.t[0];
        pos.vy = arg2[i].field_A - coord->workm.t[1];
        pos.vz = arg2[i].field_C - coord->workm.t[2];
        scale  = 0xC8000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F210[arg0->spawnArg1].vx;
        pos.vy += D_actor_503500_8016F210[arg0->spawnArg1].vy;
        pos.vz += D_actor_503500_8016F210[arg0->spawnArg1].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, (GpEffArg*)&work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 2 of the second 0xF4 block, stepped by `field_F1`: phase 0
/// applies the boss's preset 0xA; from frame 0x14 phase 1 spawns one child
/// from `D_actor_503500_8016E9F0`, placed at the offset
/// `D_actor_503500_8016F258` in the actor's frame and turned by
/// `D_actor_503500_8016F260` (both mirrored for slot 8); phase 2 hands off
/// once `func_actor_503500_80136014` reports preset 0xA flagged.
void func_actor_503500_8013D1CC(Actor503500* arg0)
{
    SVECTOR               pos;
    SVECTOR               ofs;
    MATRIX                m;
    Actor503500Work770E8* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        dst;
    Task*                 task;
    s32*                  src;
    s32*                  out;
    s32                   i;
    s32                   t;

    work  = (Actor503500Work770E8*)arg0->field_1C;
    coord = arg0->extra->field_8;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013DBA8(arg0, 1);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, 0xA, 0x10);
            work->field_F1++;
            break;
        case 1:
            if ((s16)++work->field_EC < 0x14) {
                break;
            }
            task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 1, 1, 0xC00000);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
                dst    = ((TmdObject*)task->extra)->field_8;
                ofs.vx = D_actor_503500_8016F258.vx;
                ofs.vy = D_actor_503500_8016F258.vy;
                ofs.vz = D_actor_503500_8016F258.vz;
                if (arg0->spawnArg1 == 8) {
                    t      = ofs.vx;
                    ofs.vx = -t;
                }
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0_real();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                out             = (s32*)&dst->coord;
                src             = (s32*)&m;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                pos.vx             = D_actor_503500_8016F260.vx;
                pos.vy             = D_actor_503500_8016F260.vy;
                pos.vz             = D_actor_503500_8016F260.vz;
                if (arg0->spawnArg1 == 8) {
                    t      = pos.vy;
                    pos.vy = -t;
                }
                RotMatrixZYX(&pos, &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir_real();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir_real();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir_real();
                gte_stclmv((char*)&dst->coord + 4);
            }
            work->field_F1++;
            break;
        case 2:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 0xA) != 0) {
                func_actor_503500_8013DBA8(arg0, 1);
            }
            break;
    }
}

/// Sub-state 3 of the second 0xF4 block, stepped by `field_F1`: phase 0 is
/// the death setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns an
/// effect per frame from a slot-dependent offset table for 6 frames, then a
/// final burst of three; the last phase keeps counting to 0x1F and leaves.
void func_actor_503500_8013D558(Actor503500* arg0)
{
    Actor503500Work770E8* work;
    GsCOORDINATE2*        coord;
    SVECTOR*              vec;
    s32                   pan;

    work  = (Actor503500Work770E8*)arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_F1) {
        case 0:
            work->obj.flags         &= 0x7FFF;
            arg0->field_20->field_54 = 0;
            Gp_UnlinkNode(&arg0->field_20->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            arg0->field_20->field_4C &= 0xF0;
            pan                       = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
            work->field_F1++;
            break;
        case 1:
            if (arg0->spawnArg1 == 7) {
                vec = D_actor_503500_8016F278;
            } else {
                vec = D_actor_503500_8016F290;
            }
            if (func_actor_503500_801360BC(arg0->spawnArg1, 2) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001C00, &vec[(s16)((s16)work->field_EC % 3)]);
            }
            if ((s16)work->field_EC++ >= 5) {
                if (func_actor_503500_801360BC(arg0->spawnArg1, 6) != 0) {
                    Gp_SpawnEff(0x6018C, coord, 0x04404600, &vec[0]);
                    Gp_SpawnEff(0x6018C, coord, 0x05404600, &vec[1]);
                    Gp_SpawnEff(0x6018C, coord, 0x06404600, &vec[2]);
                }
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_F1++;
            }
            break;
        default:
            work->field_EC++;
            if ((s16)work->field_EC >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1);
                arg0->state++;
            }
            break;
    }
}

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

/// Sub-state 1 of the second 0xF4 block: counts `field_EA` up to `arg1`
/// frames, then - if slot `kind` is free - respawns the two slot enemies,
/// each primed to die in 9 frames with a fifth of this enemy's HP (at least
/// 1), and drops back to sub-state 0 with the node unlinked. A pending
/// kill (`killCountdown == 2`) moves it to sub-state 2 instead.
void func_actor_503500_8013DA2C(Actor503500* arg0, s32 arg1)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    GpEnemy*         child;
    s32              kind;
    s32              slotA;
    s32              slotB;
    s32              hp;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (arg1 < ++work->field_EA) {
        kind = 3;
        if (arg0->spawnArg1 == 7) {
            kind  = 2;
            slotA = 0xD;
            slotB = 0xE;
        } else {
            slotA = 0xF;
            slotB = 0x10;
        }
        if (func_actor_503500_80135E04(arg0->parent, kind) != 0) {
            child = func_actor_503500_80135D00((Actor503500*)arg0->parent, slotA);
            hp    = (s16)(enemy->field_40 / 5);
            if (hp <= 0) {
                hp = 1;
            }
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->field_40            = hp;
            }
            child = func_actor_503500_80135D00((Actor503500*)arg0->parent, slotB);
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->field_40            = hp;
            }
            work->field_EA = 0;
        }
        func_actor_503500_8013DBA8(arg0, 0);
        work->obj.flags &= 0x7FFF;
        Gp_UnlinkNode(&enemy->node);
        work->field_E8 = 0;
        return;
    }
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013DBA8(arg0, 2);
        arg0->killCountdown = 0;
    }
}

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
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_80132098);

/// Applies this frame's hits from the collision records `arg2[0..arg3)`, the
/// same pass as `func_actor_503500_80137C90` for this form: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts state 2. The hit effect is pulled
/// to 600 units along the contact offset. `arg1` is passed but unused.
void func_actor_503500_8013DEB4(Actor503500* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->field_40 -= dmg;
        if (enemy->field_40 <= 0) {
            func_actor_503500_8013EC20(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].field_8 - coord->workm.t[0];
        pos.vy = arg2[i].field_A - coord->workm.t[1];
        pos.vz = arg2[i].field_C - coord->workm.t[2];
        scale  = 0x258000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F2D8.vx;
        pos.vy += D_actor_503500_8016F2D8.vy;
        pos.vz += D_actor_503500_8016F2D8.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, (GpEffArg*)&work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 1 of the third 0xF0 block, stepped by `field_ED`: step 0 applies
/// the boss's preset 0xB; from frame 0x42 step 1 spawns six children from
/// `D_actor_503500_8016E9F0`, one a frame, each placed at a fixed offset in the
/// actor's frame and turned by its row of `D_actor_503500_8016F2EC`; step 3
/// hands off once `func_actor_503500_80136014` reports preset 0xB flagged.
void func_actor_503500_8013E384(Actor503500* arg0)
{
    SVECTOR          pos;
    SVECTOR          ofs;
    MATRIX           m;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   dst;
    Task*            task;
    s32*             src;
    s32*             out;
    s32              i;
    s32              arg;
    s16              idx;

    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013EC20(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_ED) {
        case 0:
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, 0xB, 0x10);
            work->field_ED++;
            break;
        case 1:
            if (++work->field_EA < 0x42) {
                break;
            }
        case 2:
            idx  = work->field_EA - 0x42;
            arg  = (D_actor_503500_8016F2E0[idx] << 12) + (-D_80073B8C->t[1] << 24) / 1000;
            task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 0, 1, arg);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
                dst    = ((TmdObject*)task->extra)->field_8;
                ofs.vy = 0x190;
                ofs.vx = 0;
                ofs.vz = 0x960;
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0_real();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                src             = (s32*)&m;
                out             = (s32*)&dst->coord;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                RotMatrix(&D_actor_503500_8016F2EC[idx], &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir_real();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir_real();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir_real();
                gte_stclmv((char*)&dst->coord + 4);
            }
            if (idx >= 5) {
                work->field_ED += 2;
            }
            break;
        case 3:
            if (func_actor_503500_80136014((Actor503500*)arg0->parent, 0xB) != 0) {
                func_actor_503500_8013EC20(arg0, 0);
            }
            break;
    }
}

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

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_801320D0);

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the 0xF4 block's enemy, like `func_actor_503500_80139A20`: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts sub-state 2 but still applies the
/// id's status effect. The hit effect is pulled to 400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
void func_actor_503500_8013EE5C(Actor503500* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->field_40 -= dmg;
        if (enemy->field_40 <= 0) {
            func_actor_503500_8013F9D4(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].field_8 - coord->workm.t[0];
        pos.vy = arg2[i].field_A - coord->workm.t[1];
        pos.vz = arg2[i].field_C - coord->workm.t[2];
        scale  = 0x190000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F36C.vx;
        pos.vy += D_actor_503500_8016F36C.vy;
        pos.vz += D_actor_503500_8016F36C.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, (GpEffArg*)&work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

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

/// Per-slot tables of the 0x3D8 enemies in `D_actor_503500_80177B60`: world
/// translation and rotation of the task's coordinate (indexed by
/// `spawnArg1 - 0xD`), the local offset its `GpEnemy::field_1C` and first
/// display node share, and the second display node's local offset.
extern SVECTOR         D_actor_503500_8016F3AC[];
extern SVECTOR         D_actor_503500_8016F3CC[];
extern SVECTOR         D_actor_503500_8016F3EC;
extern Actor503500UVec D_actor_503500_8016F3F4[];
/// Rest control point seeded into `field_368` / `field_358`, also indexed by
/// `spawnArg1 - 0xD`. The code reaches it as `&table[spawnArg1 - 0xD]`, which
/// folds the bias into the address constant (`D_actor_503500_8016F414 - 0x68`
/// == `D_actor_503500_8016F3AC`); a distinct symbol is what keeps CSE from
/// sharing that address with the `D_actor_503500_8016F3AC` access above.
extern SVECTOR            D_actor_503500_8016F414[];
extern Actor503500Work3D8 D_actor_503500_80177B60[];
void                      func_actor_503500_80141D04(Actor503500* arg0);

void func_actor_503500_8013FA74(Actor503500* arg0)
{
    GpEnemy*             enemy;
    TmdObject*           tmd;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       part;
    Actor503500Work3D8*  work;
    GpRec18*             rec;
    GpRec18*             rec2;
    Actor503500IdentMat  m;
    Actor503500MatWords* ident;
    s32                  idx;
    s32                  i;

    idx   = arg0->spawnArg1 - 0xD;
    enemy = arg0->field_20;
    work  = &D_actor_503500_80177B60[idx];
    coord = arg0->extra->field_8;
    tmd   = arg0->extra;
    Mem_Set(work, 0, 0x3D8);
    arg0->field_1C = (Actor503500Work*)work;

    coord->sub        = &((TmdObject*)arg0->parent->extra)->field_8[1];
    coord->coord.t[0] = D_actor_503500_8016F3AC[idx].vx;
    coord->coord.t[1] = D_actor_503500_8016F3AC[idx].vy;
    coord->coord.t[2] = D_actor_503500_8016F3AC[idx].vz;
    m.ident.m00_m01   = 0x1000;
    ident             = &m.ident;
    ident->m02_m10    = 0;
    ident->m11_m12    = 0x1000;
    ident->m20_m21    = 0;
    ident->m22        = 0x1000;
    RotMatrix(&D_actor_503500_8016F3CC[idx], &m.mat);
    MulMatrix0(&coord->coord, &m.mat, &coord->coord);
    coord->flg      = 0;
    work->field_3D7 = -1;
    tmd->field_1C   = &work->light;
    tmd->field_20   = &work->color;
    tmd->field_E    = 0x12;

    enemy->field_4      = &coord->coord;
    part                = &coord[8];
    enemy->field_48     = 0;
    enemy->field_18     = part;
    enemy->node.field_4 = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx  = D_actor_503500_8016F3EC.vx;
    enemy->field_1C.vy  = D_actor_503500_8016F3EC.vy;
    enemy->field_1C.vz  = D_actor_503500_8016F3EC.vz;
    rec                 = work->rec180;
    enemy->field_50     = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54     = (s32)rec;

    work->obj160.field_8  = part;
    work->obj160.field_C  = rec;
    work->obj160.field_10 = D_actor_503500_8016F3EC.vx;
    work->obj160.field_12 = D_actor_503500_8016F3EC.vy;
    work->obj160.field_14 = D_actor_503500_8016F3EC.vz;
    work->obj160.field_18 = 0x30023;
    work->obj160.field_1C = 0x258;
    work->obj160.flags    = 1;
    Gp_LinkObj(2, &work->obj160);
    Gp_InitRec18Table(rec, 8, 0);
    rec2                  = work->rec260;
    work->obj240.field_8  = part;
    work->obj240.field_C  = rec2;
    work->obj160.flags   &= 0x7FFF;
    work->obj240.field_10 = D_actor_503500_8016F3F4[idx].vx;
    work->obj240.field_12 = D_actor_503500_8016F3F4[idx].vy;
    work->obj240.field_14 = D_actor_503500_8016F3F4[idx].vz;
    work->obj240.field_18 = Gp_PackPair(enemy->field_50->field_0, 0);
    work->obj240.field_1C = 0x1F4;
    work->obj240.flags    = 1;
    Gp_LinkObj(3, &work->obj240);
    Gp_InitRec18Table(rec2, 4, 0);
    work->field_2C0.field_4 = 0x600;
    work->field_2C0.field_0 = part;
    work->field_2C0.field_6 = 3;
    work->obj240.flags     &= 0x7FFF;

    copyVector(&work->field_368, &D_actor_503500_8016F414[arg0->spawnArg1 - 0xD]);
    copyVector(&work->field_358, &D_actor_503500_8016F414[arg0->spawnArg1 - 0xD]);
    work->field_39C.w = 0x600000;
    work->field_3B4   = 0x40;
    work->field_3B6   = 0x1000;
    work->field_3D5   = 1;
    for (i = 1; i < 9; i++) {
        work->phase[i] = (i << 9) & 0xFFF;
        work->mats[i]  = coord[i].coord;
    }
    Gp_UpdateCoord(coord);
    func_actor_503500_801421A8(arg0);
    switch (arg0->killCountdown) {
        case 8:
            func_actor_503500_80142310(arg0, 6);
            break;
        case 9:
            tmd->field_C |= 0x80;
            func_actor_503500_80142310(arg0, 7);
            break;
        default:
            Gp_LinkNode(&enemy->node);
            enemy->field_40     = D_actor_503500_8016E7EC[arg0->spawnArg1].field_4;
            work->field_3B2     = 0x1000;
            work->obj160.flags |= 0x8000;
            func_actor_503500_80142310(arg0, 0);
            break;
    }
    arg0->exitCallback = (TaskFunc)func_actor_503500_80141D04;
    arg0->state       += 1;
}

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

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_80132108);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_8", func_actor_503500_801400A4);

/// Death state of the 0x3D8 enemies, the same body as
/// `func_actor_503500_80139014` at this block's offsets: unlinks the enemy
/// node, waits for `field_3D4`, re-parents the root coordinate onto the view
/// and plays 0x40230004 at it. Phase 2 eases every part back to rest while the
/// body rises; past 1000 the pose is saved in `field_378` and phase 3 squashes
/// it vertically (`field_3AC`), firing the cues on frames 10/15/30/40.
/// Every twelfth frame of phases 0..2 sprays effects along parts 8..1.
void func_actor_503500_80140654(Actor503500* arg0)
{
    MATRIX           m;
    VECTOR           scale;
    SVECTOR          rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    s16*             p;
    s32              phase;
    s32              i;
    s32              j;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    phase = work->field_3D0;
    coord = arg0->extra->field_8;
    switch (phase) {
        case 0:
            work->obj160.flags &= 0x7FFF;
            enemy->field_54     = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_3A8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            enemy->field_4C   &= 0xF0;
            work->field_368.vy = 0x7D0;
            work->field_3D0++;
            break;
        case 1:
            if (work->field_3D4 != 0) {
                Gp_ComposeParentWorld(coord, &m, &rot);
                coord->coord      = m;
                coord->coord.t[0] = rot.vx;
                coord->coord.t[1] = rot.vy;
                coord->coord.t[2] = rot.vz;
                coord->sub        = &Gfx_ViewCoord;
                coord->flg        = 0;
                work->field_3D6   = phase;
                Gp_UpdateCoord(coord);
                SndEvt_EnqueueType6(0x40230004, (s8)Gp_GetObjPan((GpObj38*)coord),
                                    (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
                work->field_3D0++;
            }
            break;
        case 2:
            for (i = 1; i < 9; i++) {
                part = &coord[i];
                Gp_ExtractEuler(&rot, &part->coord);
                p = &rot.vx;
                j = 1;
                do {
                    if (*p > 0) {
                        *p -= 2;
                        if (*p < 0) {
                            *p = 0;
                        }
                    } else {
                        *p += 2;
                        if (*p > 0) {
                            *p = 0;
                        }
                    }
                    p++;
                } while (j++ < 3);
                func_actor_503500_SetRotIdentity(&coord[i].coord);
                RotMatrix(&rot, &part->coord);
                part->flg = 0;
            }
            coord->flg         = 0;
            coord->coord.t[1] += 10;
            if (coord->coord.t[1] > 1000) {
                work->field_378 = coord->coord;
                work->field_3AC = 0x1000;
                work->field_3D0++;
            }
            break;
        case 3:
            if (work->field_3AC > 0x200) {
                work->field_3AC -= 0x20;
            }
            coord->coord = work->field_378;
            scale.vx     = 0x1000;
            scale.vy     = work->field_3AC;
            scale.vz     = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            switch (work->field_3AE) {
                case 10:
                    arg0->extra->field_C |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    SndEvt_EnqueueType6(0xD, (s8)Gp_GetObjPan((GpObj38*)coord),
                                        (s8)(Gp_GetObjDepth((GpObj38*)coord) / 2));
                    break;
                case 15:
                    Gp_SpawnEff(0x600A5, coord, 1, NULL);
                    break;
                case 30:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 40:
                    SndEvt_EnqueueType7(0xD, 1);
                    arg0->state++;
                    break;
            }
            work->field_3AE++;
            break;
    }
    if (func_actor_503500_801360BC(arg0->spawnArg1, 4) != 0 && work->field_3D0 < 3 &&
        (u32)D_80070F70 % 12 == 0) {
        for (i = 8, j = 0; i > 0; i--) {
            Gp_SpawnEff(0x60070, &arg0->extra->field_8[i], 0xB0008600, &D_actor_503500_8016F448[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (Game_Session->field_1 != 0 && Game_Session->field_4D != 0 && work->field_3D0 > 0) {
        SndEvt_EnqueueType7(0xD, 1);
        arg0->state = 2;
    }
}

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

/// Applies this frame's hits from `obj160`'s collision records `arg2[0..arg3)`
/// to the 0x3D8 block's enemy, like `func_actor_503500_8013EE5C`, but at model
/// part 8: each attack id is taken once, only type-2 ids land while the
/// `field_3A8` countdown is clear, and a hit that empties `field_40` enters
/// state 5 (unless already past it) instead of applying the id's status effect.
/// The hit effect is pulled to 500 units along the contact offset, and a hit
/// landing in state 1 drops back to state 0. `arg1` is passed but unused.
void func_actor_503500_80140D38(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    SVECTOR             pos;
    MATRIX              rot;
    MATRIX              mtx;
    VECTOR              d;
    Actor503500Work3D8* work;
    GpEnemy*            enemy;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      src;
    s16                 stun;
    u32                 id;
    s32                 dmg;
    s32                 crit;
    s32                 scale;
    s32                 i;
    s32                 j;

    enemy = arg0->field_20;
    work  = (Actor503500Work3D8*)arg0->field_1C;
    coord = &arg0->extra->field_8[8];
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_3A8 != 0) {
            continue;
        }
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        src  = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->field_40 -= dmg;
        if (enemy->field_40 <= 0) {
            if (work->field_3A4 < 6) {
                func_actor_503500_80142310(arg0, 5);
            }
        } else {
            switch (Gp_GetIdParam0(id) & 0xFFFF) {
                case 0:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    break;
                case 1:
                    Gp_SetObjFlag1((GpObj4C*)enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                    break;
            }
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].field_8 - coord->workm.t[0];
        pos.vy = arg2[i].field_A - coord->workm.t[1];
        pos.vz = arg2[i].field_C - coord->workm.t[2];
        scale  = 0x1F4000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F3EC.vx;
        pos.vy += D_actor_503500_8016F3EC.vy;
        pos.vz += D_actor_503500_8016F3EC.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_2C0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_3A8 < stun) {
            work->field_3A8 = stun;
        }
        if (work->field_3A4 == 1) {
            func_actor_503500_80142310(arg0, 0);
        }
    next:;
    }
}

/// The 0x3D8 block's copy of `func_actor_503500_80139EFC`: saves `field_358`
/// into `field_360`, then steers it toward `field_368`. Inside the arrival
/// distance (the integer half of `field_39C`) it snaps onto the target and sets
/// `field_3D4`; otherwise the speed `field_398` accelerates toward
/// +/-`field_39C` while `field_3D5` is set, or decays to 0, and moves
/// `field_358` along the normalized offset (at a quarter speed while
/// `field_3AA` runs).
void func_actor_503500_80141248(Actor503500* arg0)
{
    SVECTOR             d;
    SVECTOR             n;
    VECTOR              step;
    Actor503500Work3D8* work;
    s32                 lim;
    s32                 speed;

    work               = (Actor503500Work3D8*)arg0->field_1C;
    work->field_360.vx = work->field_358.vx;
    work->field_360.vy = work->field_358.vy;
    work->field_360.vz = work->field_358.vz;
    d.vx               = work->field_368.vx - work->field_358.vx;
    d.vy               = work->field_368.vy - work->field_358.vy;
    d.vz               = work->field_368.vz - work->field_358.vz;
    if (ABS(d.vx) + ABS(d.vy) + ABS(d.vz) < work->field_39C.h.hi) {
        work->field_3D4    = 1;
        work->field_358.vx = work->field_368.vx;
        work->field_358.vy = work->field_368.vy;
        work->field_358.vz = work->field_368.vz;
        return;
    }
    lim             = work->field_39C.w;
    work->field_3D4 = 0;
    if (work->field_3D5 != 0) {
        speed = work->field_398 + lim / 32;
        if (speed > 0) {
            if (speed > lim) {
                speed = lim;
            }
        } else if (speed < -lim) {
            speed = -lim;
        }
    } else {
        speed = work->field_398 - lim / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->field_398 = speed;
    VectorNormalSS(&d, &n);
    if (work->field_3AA != 0) {
        speed >>= 2;
    }
    step.vx             = n.vx * (speed >> 12);
    step.vy             = n.vy * (speed >> 12);
    step.vz             = n.vz * (speed >> 12);
    work->field_358.vx += step.vx >> 16;
    work->field_358.vy += step.vy >> 16;
    work->field_358.vz += step.vz >> 16;
}

/// Lays the 0x3D8 enemy's nine-point chain along a cubic Bezier from the root
/// coordinate's world position to its parent's, with the near control point
/// offset 1000 units in the root's frame (X mirrored for spawn slots 15 / 16)
/// and both far control points at the parent plus its rotated `field_358`. The
/// samples land in `pts`, `func_actor_503500_8014176C` re-aims the links along
/// them, and links 2..8 get a pitch of a fading sine sway plus the scaled rest
/// pitch before `func_actor_503500_80142220` applies it.
void func_actor_503500_80141448(Actor503500* arg0)
{
    SVECTOR                  ctrl[4];
    SVECTOR                  ofs;
    SVECTOR                  tmp;
    VECTOR                   out[9];
    MATRIX                   m;
    GsCOORDINATE2*           coord;
    Actor503500Work3D8Chain* work;
    s32                      i;
    s32                      v;

    coord = arg0->extra->field_8;
    work  = (Actor503500Work3D8Chain*)arg0->field_1C;
    Gp_ComposeParentWorld(coord, &m, &ctrl[0]);
    work->pts[0].vx = ctrl[0].vx;
    work->pts[0].vy = ctrl[0].vy;
    work->pts[0].vz = ctrl[0].vz;
    if ((u32)(arg0->spawnArg1 - 15) < 2) {
        ofs.vx = -1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    } else {
        ofs.vx = 1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    }
    gte_SetRotMatrix(&m);
    gte_ldv0(&ofs);
    gte_rtv0_real();
    gte_stsv(&ctrl[1]);
    ctrl[1].vx += ctrl[0].vx;
    ctrl[1].vy += ctrl[0].vy;
    ctrl[1].vz += ctrl[0].vz;
    Gp_ComposeParentWorld(coord->sub, &m, &tmp);
    gte_SetRotMatrix(&m);
    gte_ldv0(&work->field_358);
    gte_rtv0_real();
    gte_stsv(&ofs);
    tmp.vx    += ofs.vx;
    tmp.vy    += ofs.vy;
    tmp.vz    += ofs.vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy;
    ctrl[3].vz = tmp.vz;
    for (i = 8; i >= 0; i--) {
        func_actor_503500_80141A44(ctrl, &ctrl[3], 9, i, &out[i].vx);
        copyVector(&work->pts[8 - i], &out[i]);
    }
    func_actor_503500_8014176C(work->pts, arg0->extra->field_8);
    for (i = 8; i >= 2; i--) {
        v                   = ((work->field_3B4 * work->field_3B6 >> 12) * rsin(work->phase[i])) >> 12;
        work->angles[i].vx  = v;
        work->angles[i].vx += D_actor_503500_8016F434[i] * work->field_3CC >> 12;
        work->angles[i].vy  = 0;
        work->angles[i].vz  = 0;
        work->phase[i]      = (work->phase[i] + 0x80) & 0xFFF;
    }
    func_actor_503500_80142220(work->angles, arg0->extra->field_8);
}

/// Re-aims a chain of eight child coordinates along the polyline `pts[0..8]`.
/// `world` starts as the chain root's world rotation and accumulates each
/// link's local rotation; the segment `pts[i + 1] - pts[i]` is taken into that
/// frame, and the resulting direction becomes the next link's basis
/// (`Gfx_OrthonormalBasis`, up hint +Y) with the local segment as its
/// translation. Works in an `Actor503500ChainScratch` on the scratchpad stack.
void func_actor_503500_8014176C(SVECTOR* pts, GsCOORDINATE2* coords)
{
    Actor503500ChainScratch* s;
    MATRIX*                  inv;
    SVECTOR*                 dir;
    s32                      i;
    s32                      j;

    s        = (Actor503500ChainScratch*)(SCRATCH_SP -= sizeof(Actor503500ChainScratch));
    s->up.vx = 0;
    s->up.vy = 0x1000;
    s->up.vz = 0;
    Gp_ComposeParentWorld(coords->sub, &s->world, &s->rot);
    for (i = 0, j = 1; i < 8; i++, j++) {
        s->diff.vx = pts[j].vx - pts[i].vx;
        s->diff.vy = pts[j].vy - pts[i].vy;
        s->diff.vz = pts[j].vz - pts[i].vz;
        gte_SetRotMatrix(&s->world);
        inv = &s->inv;
        dir = &s->dir;
        gte_ldclmv(&coords[i].coord);
        gte_rtir_real();
        gte_stclmv(&s->world);
        gte_ldclmv((char*)&coords[i].coord + 2);
        gte_rtir_real();
        gte_stclmv((char*)&s->world + 2);
        gte_ldclmv((char*)&coords[i].coord + 4);
        gte_rtir_real();
        gte_stclmv((char*)&s->world + 4);
        TRANSPOSE_ROT(&s->world, inv);
        gte_SetRotMatrix(inv);
        gte_ldv0(&s->diff);
        gte_rtv0_real();
        gte_stlvnl(&s->pos);
        VectorNormalS(&s->pos, dir);
        Gfx_OrthonormalBasis(&coords[j].coord, dir, &s->up);
        coords[j].coord.t[0] = s->pos.vx;
        coords[j].coord.t[1] = s->pos.vy;
        coords[j].coord.t[2] = s->pos.vz;
    }
    SCRATCH_SP += sizeof(Actor503500ChainScratch);
}

/// Same cubic Bezier evaluation as `func_actor_503500_8013A7B0`: control points
/// `pts[0..2]` and `p3`, `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_actor_503500_80141A44(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_actor_503500_801422B8(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_actor_503500_801422B8(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_actor_503500_801422B8(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

/// The fade-level counterpart of `func_actor_503500_8013AB38`: while
/// `field_3B2` is below 0x1000, blends parts 1..8 toward the 0x3D8 block's
/// matrix table, copying the lerped rotation back word-wise and keeping a
/// `field_3B2 / 0x1000` share of each part's offset from its table entry.
void func_actor_503500_80141B94(Actor503500* arg0)
{
    MATRIX           m;
    VECTOR           d;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          mat;
    s32*             src;
    s32*             dst;
    s32              t;
    s32              i;
    s32              j;

    work  = arg0->field_1C;
    coord = arg0->extra->field_8 + 1;
    if ((s16)work->field_3B2 < 0x1000) {
        mat = &((Actor503500Work3D8Mtx*)work)->mats[1];
        t   = (s16)work->field_3B2;
        for (i = 1; i < 9; i++) {
            Gp_LerpOrthonormal(mat, &coord->coord, &m, t);
            dst = (s32*)&coord->coord;
            src = (s32*)&m;
            for (j = 0; j < 4; j++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = m.m[2][2];
            d.vx                 = ((coord->coord.t[0] - mat->t[0]) * t) >> 12;
            d.vy                 = ((coord->coord.t[1] - mat->t[1]) * t) >> 12;
            d.vz                 = ((coord->coord.t[2] - mat->t[2]) * t) >> 12;
            coord->coord.t[0]    = mat->t[0] + d.vx;
            coord->coord.t[1]    = mat->t[1] + d.vy;
            coord->coord.t[2]    = mat->t[2] + d.vz;
            mat++;
            coord++;
        }
    }
}

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

/// State-0 init of the 0x224 enemy at `D_actor_503500_80178AC0[spawnArg1 - 0xA]`:
/// clears the block, hangs the task's coordinate off the parent part picked by
/// `D_actor_503500_80171464`, republishes the parent's light and colour
/// matrices, and links three display nodes - `obj0` on the task's own
/// coordinate, `obj1` / `obj2` on parent parts 6 / 7 (slot 1) or 12 / 13
/// (slot 0) sharing `rec1` - before starting sub-state 0.
void func_actor_503500_801423C8(Actor503500* arg0)
{
    GpEnemy*            enemy;
    TmdObject*          tmd;
    Task*               parent;
    s32                 slot;
    Actor503500Work224* work;
    GsCOORDINATE2*      coord;
    TmdObject*          parentTmd;
    SVECTOR*            ofs;
    GpRec18*            rec;
    GsCOORDINATE2*      parts;
    GsCOORDINATE2*      parts2;

    enemy     = arg0->field_20;
    tmd       = arg0->extra;
    parent    = arg0->parent;
    slot      = arg0->spawnArg1 - 0xA;
    work      = &D_actor_503500_80178AC0[slot];
    coord     = tmd->field_8;
    parentTmd = parent->extra;
    Mem_Set(work, 0, 0x224);
    arg0->field_1C = (Actor503500Work*)work;

    coord->sub        = &((TmdObject*)parent->extra)->field_8[D_actor_503500_80171464[slot]];
    coord->coord.t[0] = D_actor_503500_80171478.vx;
    coord->coord.t[1] = D_actor_503500_80171478.vy;
    coord->coord.t[2] = D_actor_503500_80171478.vz;
    work->field_220   = arg0->spawnArg1 - 0xA;
    tmd->field_1C     = parentTmd->field_1C;
    tmd->field_20     = parentTmd->field_20;
    tmd->field_E      = 0x13;
    tmd->field_C     |= 0x84;
    coord->flg        = 0;

    enemy->field_4      = &coord->coord;
    ofs                 = &D_actor_503500_80171480[slot];
    enemy->field_48     = 0;
    enemy->field_18     = coord;
    enemy->node.field_4 = (enemy->node.field_4 | 8) & 0xFE;
    enemy->field_1C.vx  = ofs->vx;
    enemy->field_1C.vy  = ofs->vy;
    enemy->field_1C.vz  = ofs->vz;
    rec                 = work->rec0;
    enemy->field_50     = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54     = (s32)rec;
    enemy->field_40     = enemy->field_50->field_4;

    work->obj0.field_8  = coord;
    work->obj0.field_C  = rec;
    work->obj0.field_10 = ofs->vx;
    work->obj0.field_12 = ofs->vy;
    work->obj0.field_14 = ofs->vz;
    work->obj0.field_18 = 0x30023;
    work->obj0.field_1C = 0x5DC;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(rec, 8, 0);
    work->obj0.flags &= 0x7FFF;

    parts = ((TmdObject*)parent->extra)->field_8;
    if (slot != 0) {
        work->obj1.field_8 = &parts[6];
    } else {
        work->obj1.field_8 = &parts[12];
    }
    work->obj1.field_C  = work->rec1;
    work->obj1.field_10 = 0;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0;
    work->obj1.field_18 = 0x30023;
    work->obj1.field_1C = 0x320;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 4, 0);
    work->obj1.flags &= 0x7FFF;

    parts2 = ((TmdObject*)parent->extra)->field_8;
    if (slot != 0) {
        work->obj2.field_8 = &parts2[7];
    } else {
        work->obj2.field_8 = &parts2[13];
    }
    work->obj2.field_C  = work->rec1;
    work->obj2.field_10 = 0;
    work->obj2.field_12 = 0x190;
    work->obj2.field_14 = 0;
    work->obj2.field_18 = 0x30023;
    work->obj2.field_1C = 0x4B0;
    work->obj2.flags    = 1;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 4, 0);
    work->field_1E4   = 0x600;
    work->field_1E0   = coord;
    work->field_1E6   = 3;
    work->obj2.flags &= 0x7FFF;
    func_actor_503500_80144238(arg0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_80143F78;
    arg0->state       += 1;
}

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

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_80132178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_8", func_actor_503500_80142980);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_8", func_actor_503500_801431EC);

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

/// Knock-back work block of `func_actor_503500_80143AC0`.
extern Actor503500Work38 D_actor_503500_80178F10;
/// Rows 2 and 3 of `D_actor_503500_801714E0` (the 0x3FF payload after the push,
/// indexed by side) and row 4 (the one sent once the player's weapon animation
/// word has been parked in `D_actor_503500_801714DC`).
extern Actor503500Msg3FF D_actor_503500_80171508[];
extern Actor503500Msg3FF D_actor_503500_80171530;
extern s32               D_actor_503500_801714DC;
/// Script pair handed to `Gp_SpawnScript18` when the push starts.
extern u8         D_actor_503500_8017159C[];
extern u8         D_actor_503500_801715A4[];
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

/// Knock-back task spawned from `D_actor_503500_8017146C` by
/// `func_actor_503500_801437D0`, with the hit side in `spawnArg1` and the
/// enemy's turned rotation in `spawnArg2`. State 0 copies that rotation, starts
/// the push at speed 0x1000000 and shakes the camera for 8 frames; state 1 moves
/// the player by the rotated speed through message 0x3FE, decaying it by
/// 0x30000 a frame, and after 20 frames at rest moves on (or ends at -1 when the
/// player has no HP left). States 2-4 wait out message 0x3ED between the two
/// 0x3FF payloads and the closing 0x3F1. Frame 0x11 plays sound 0x54300002 at
/// the player.
void func_actor_503500_80143AC0(Task* arg0)
{
    VECTOR             vec;
    Actor503500Msg3FE  msg;
    Actor503500Work38* work;
    Task*              player;
    GsCOORDINATE2*     coord;
    s32*               src;
    s32*               dst;
    s32                i;
    s32                pan;
    s32                next;
    s32                shake;

    work   = &D_actor_503500_80178F10;
    player = Game_GetPtrSlot(3);
    if (D_801153F4 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (D_80073BA0 <= 0) {
                Task_Kill(arg0);
                return;
            }
            Mem_Set(work, 0, sizeof(Actor503500Work38));
            work->speed    = 0x1000000;
            work->pos.vx.w = 0;
            work->pos.vy.w = 0;
            work->pos.vz.w = 0;
            src            = (s32*)arg0->spawnArg2;
            dst            = (s32*)&work->rot;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->rot.m[2][2] = ((MATRIX*)arg0->spawnArg2)->m[2][2];
            Gp_SpawnScript18((s32)D_actor_503500_8017159C, (s32)D_actor_503500_801715A4);
            work->field_36 = 8;
            // An s32 temp: passed straight to the s8 parameter, the masked
            // expression is shortened into a byte load of the frame counter.
            shake = (D_80070F70 ^ 1) & 1;
            Display_ClampField126(shake);
            arg0->state++;
        case 1:
            vec.vx = 0;
            vec.vy = 0;
            vec.vz = work->speed;
            ApplyMatrixLV(&work->rot, &vec, &vec);
            work->pos.vx.w += vec.vx;
            work->pos.vy.w += vec.vy;
            work->pos.vz.w += vec.vz;
            msg.field_10    = 1;
            msg.field_12    = 1;
            msg.x           = work->pos.vx.h.hi;
            msg.y           = work->pos.vy.h.hi;
            msg.z           = work->pos.vz.h.hi;
            if (Gp_DispatchMsg(player, 0x3FE, (s32)&msg, 0) != 0) {
                work->speed = 0;
            }
            work->pos.vx.w = (u16)work->pos.vx.w;
            work->pos.vy.w = (u16)work->pos.vy.w;
            work->pos.vz.w = (u16)work->pos.vz.w;
            work->speed   -= 0x30000;
            if (work->speed < 0) {
                work->speed = 0;
                if (++work->field_34 > 20) {
                    // The -1 arm first: reorg inverts the branch around it and
                    // leaves the `li` in the delay slot, sharing $v0 with the load.
                    if (D_80073BA0 <= 0) {
                        next = -1;
                    } else {
                        next = arg0->state + 1;
                    }
                    arg0->state = next;
                }
            }
            if (work->field_36 > 0) {
                shake = (D_80070F70 ^ 1) & 1;
                Display_ClampField126(shake);
                work->field_36--;
            } else {
                Display_ClampField126(0);
            }
            break;
        case 2:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_80171508[arg0->spawnArg1], 0);
                arg0->state++;
            }
            break;
        case 3:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                D_actor_503500_801714DC =
                    Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21]
                        ->field_1C;
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_80171530, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 2, 0);
                Task_Kill(arg0);
            }
            break;
    }
    if (++arg0->killCountdown == 0x11) {
        coord = ((TmdObject*)player->extra)->field_8;
        pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(0x54300002, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_801321DC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_801321E8);

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

void func_actor_503500_80144B40(Actor503500* arg0)
{
    Actor503500WorkB4* work;
    GsCOORDINATE2*     coord;
    VECTOR             v;

    work  = (Actor503500WorkB4*)arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_B0) {
        case 0:
            work->field_A8 -= 0x80000;
            if (work->field_A8 < 0x80000) {
                work->field_80->spawnArg1 = 2;
                work->field_B0++;
            }
            break;
        case 1:
            work->field_AE++;
            if (D_actor_503500_801715BC[arg0->spawnArg1] < work->field_AE) {
                work->field_AE   = 0;
                work->obj.flags &= 0x7FFF;
                work->field_B0++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    v.vx = 0;
    v.vy = 0;
    v.vz = work->field_A8;
    ApplyMatrixLV(&coord->coord, &v, &v);
    work->field_84.vx += v.vx;
    work->field_84.vy += v.vy - 0x10000;
    work->field_84.vz += v.vz;
    work->field_84.vx += v.vx;
    work->field_84.vy += v.vy;
    work->field_84.vz += v.vz;
    if (work->field_84.vx > 0x36B00000) {
        work->field_84.vx = 0x36B00000;
    } else if (work->field_84.vx < 0x7D00000) {
        work->field_84.vx = 0x7D00000;
    }
    if (work->field_84.vz > 0x32C80000) {
        work->field_84.vz = 0x32C80000;
    } else if (work->field_84.vz < 0x3E80000) {
        work->field_84.vz = 0x3E80000;
    }
    coord->coord.t[0] = work->field_84.vx >> 16;
    coord->coord.t[1] = work->field_84.vy >> 16;
    coord->coord.t[2] = work->field_84.vz >> 16;
}

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
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_8", D_actor_503500_801321F4);
