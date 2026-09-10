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
/// Effect offsets `func_actor_503500_80137678` cycles through, three entries.
extern SVECTOR D_actor_503500_8016F078[];
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

void func_actor_503500_8013BE48(Actor503500* arg0, s32 arg1);

/// Applies this frame's hits from the collision records `rec[0..count)`,
/// like `func_actor_503500_80137C90`: each attack id is taken once, only
/// type-2 ids land while the `field_E8` countdown is clear, and a hit that
/// empties `field_40` starts state 2. The hit effect is pulled to 1600 units
/// along the contact offset and placed at the `field_EC` sub-state's offset.
/// `arg1` is passed by the caller but unused.
void func_actor_503500_8013AF60(Actor503500* arg0, Actor503500Work* arg1, GpRec18* rec, s32 count)
{
    MATRIX           mtx;
    MATRIX           rot;
    VECTOR           d;
    SVECTOR          pos;
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
    for (i = 0; i < count; i++) {
        id = rec[i].field_4;
        for (j = 0; j < i; j++) {
            if (rec[j].field_4 == id) {
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
            func_actor_503500_8013BE48(arg0, 2);
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
        pos.vx = rec[i].field_8 - coord->workm.t[0];
        pos.vy = rec[i].field_A - coord->workm.t[1];
        pos.vz = rec[i].field_C - coord->workm.t[2];
        scale  = 0x640000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0_real();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F0F0[work->field_EC].vx;
        pos.vy += D_actor_503500_8016F0F0[work->field_EC].vy;
        pos.vz += D_actor_503500_8016F0F0[work->field_EC].vz;
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

/// Per-slot local offset and Z-Y-X angles of the effects
/// `func_actor_503500_8013B60C` spawns; `side` mirrors the offset's X and the
/// angle's Y.
extern Actor503500UVec D_actor_503500_8016F108[];
extern Actor503500UVec D_actor_503500_8016F128[][3];

/// Spawns effect slot `arg2` of `D_actor_503500_8016E9F0` on the task's own
/// coordinate: the child's translation is the parent world position plus the
/// slot offset rotated into that frame, and its rotation is the parent's world
/// rotation times `RotMatrixZYX` of the slot angles. The negations go through
/// an `s32` so the sign extension of the `u16` component survives.
void func_actor_503500_8013B60C(Actor503500* arg0, s32 side, s32 arg2)
{
    SVECTOR        pos;
    SVECTOR        ofs;
    MATRIX         m;
    GsCOORDINATE2* src;
    GsCOORDINATE2* coord;
    Task*          task;
    s32*           dst;
    s32*           p;
    s32            i;
    s32            t;
    u16            vx;
    u16            vy;

    src  = arg0->extra->field_8;
    task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 1, 0, 0xA00000);
    if (task != NULL) {
        Gp_ComposeParentWorld(src, &m, &pos);
        coord  = ((TmdObject*)task->extra)->field_8;
        ofs.vx = vx = D_actor_503500_8016F108[arg2].vx;
        ofs.vy      = D_actor_503500_8016F108[arg2].vy;
        ofs.vz      = D_actor_503500_8016F108[arg2].vz;
        if (side != 0) {
            t      = -(s16)vx;
            ofs.vx = t;
        }
        gte_SetRotMatrix(&m);
        gte_ldv0(&ofs);
        gte_rtv0_real();
        gte_stsv(&ofs);
        coord->coord.t[0] = pos.vx + ofs.vx;
        coord->coord.t[1] = pos.vy + ofs.vy;
        coord->coord.t[2] = pos.vz + ofs.vz;
        dst               = (s32*)coord->coord.m;
        p                 = (s32*)m.m;
        for (i = 0; i < 4; i++) {
            *dst++ = *p++;
        }
        coord->coord.m[2][2] = m.m[2][2];
        pos.vx               = D_actor_503500_8016F128[arg2][0].vx;
        pos.vy = vy = D_actor_503500_8016F128[arg2][0].vy;
        pos.vz      = D_actor_503500_8016F128[arg2][0].vz;
        if (side != 0) {
            t      = -(s16)vy;
            pos.vy = t;
        }
        RotMatrixZYX(&pos, &m);
        gte_SetRotMatrix(&coord->coord);
        gte_ldclmv(&m);
        gte_rtir_real();
        gte_stclmv(&coord->coord);
        gte_ldclmv((char*)&m + 2);
        gte_rtir_real();
        gte_stclmv((char*)&coord->coord + 2);
        gte_ldclmv((char*)&m + 4);
        gte_rtir_real();
        gte_stclmv((char*)&coord->coord + 4);
    }
}

/// Death state of the 0xF0 block, stepped by `field_EE`: phase 0 is the death
/// setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns a mirrored
/// pair of effects per frame from `D_actor_503500_8016F168`, drifting with the
/// frame count, moves the side's VRAM rects on frame 0x14 and leaves at 0x1F.
void func_actor_503500_8013B8D0(Actor503500* arg0)
{
    GpEnemy*         enemy;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              pan;
    s32              side;
    s32              n;
    s32              i;
    s32              t;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    switch (work->field_EE) {
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
            work->field_EE++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                i       = (s16)(work->field_EA % 9);
                vec.vx  = D_actor_503500_8016F168[i].vx;
                vec.vy  = D_actor_503500_8016F168[i].vy;
                vec.vz  = D_actor_503500_8016F168[i].vz;
                vec.vx -= work->field_EA * 10;
                vec.vy -= work->field_EA * 20;
                if (work->field_EC != 0) {
                    t      = vec.vx;
                    vec.vx = -t;
                }
                Gp_SpawnEff(0x60055, coord, 0x1800, &vec);
                Gp_SpawnEff(0x60070, coord, 0x80008600, &vec);
                t      = vec.vz;
                vec.vz = -t;
                Gp_SpawnEff(0x60055, coord, 0x1800, &vec);
                Gp_SpawnEff(0x60070, coord, 0x80008600, &vec);
            }
            work->field_EA++;
            if (work->field_EA >= 0x1F) {
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_EE++;
            } else if (work->field_EA == 0x14) {
                side = work->field_EC;
                n    = 2;
                if (side != 0) {
                    n = 4;
                }
                MoveImage(&D_actor_503500_8016F148[side][0], (n << 6) + 0x140, 0x100);
                MoveImage(&D_actor_503500_8016F148[side][1], 0, n + 0xF7);
            }
            break;
        default:
            func_actor_503500_8013611C(arg0->spawnArg1);
            arg0->state++;
            break;
    }
}

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
INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_7", D_actor_503500_80132028);
