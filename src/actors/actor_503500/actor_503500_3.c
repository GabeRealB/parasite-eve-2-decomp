#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_503500.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Element of `D_actor_503500_80176EE8`, the two 0x2EC blocks
/// `func_actor_503500_8013852C` clears for spawn slots 2 and 3. The shared
/// `Actor503500Work` cannot be indexed at this stride, and this block puts a
/// spawn record at 0x240 where the shared view names the
/// 0x3D8 block's `obj240`, so the array gets its own type; the task's
/// `field_1C` still points at the block through the shared view, whose
/// 0x2D4..0x2EB fields agree with the ones below. It opens with the light and
/// colour matrices the init republishes on `TmdObject::lightMtx` / `field_20`,
/// then a private copy of model parts 1..8's `coord` matrices.
typedef struct Actor503500Work2EC {
    /* 0x000 */ MATRIX   light;
    /* 0x020 */ MATRIX   color;
    /* 0x040 */ MATRIX   mats[9];
    /* 0x160 */ GpObj    obj;
    /* 0x180 */ GpRec18  rec[8];    // Gp_InitRec18Table(rec, 8, 0)
    /* 0x240 */ GpEffArg field_240; // record the 0x2EC block's effects are spawned with
                                    /// Cleared by `func_actor_503500_801395BC` once `field_2E2` has faded
                                    /// to 0; the same offset as the shared view's `obj240.field_8`.
    /* 0x248 */ void* field_248;
    /// Bezier-sampled chain polyline, root first, that
    /// `func_actor_503500_8013A0D0` re-aims the model's links along.
    /* 0x24C */ SVECTOR pts[9];
    /* 0x294 */ SVECTOR field_294;   // both seeded from D_actor_503500_8016F0A8
    /* 0x29C */ SVECTOR field_29C;
    /* 0x2A4 */ SVECTOR field_2A4;   // rotation, spun by func_actor_503500_80138A30
                                     /// Part 0's `coord` matrix, saved by `func_actor_503500_80139014` once
                                     /// the body has risen and restored every frame before scaling.
    /* 0x2AC */ MATRIX field_2AC;
    /* 0x2CC */ s32    field_2CC;    // approach speed, see func_actor_503500_80139EFC
                                     /// Top approach speed, seeded to 0x800000; its integer half doubles as
                                     /// the arrival distance `func_actor_503500_80139EFC` tests against.
    /* 0x2D0 */ GpFixed16 field_2D0;
    /* 0x2D4 */ s16       field_2D4; // sub-state index
    /* 0x2D6 */ s16       field_2D6;
    /* 0x2D8 */ s16       field_2D8; // per-frame countdown
    /* 0x2DA */ s16       field_2DA;
    /* 0x2DC */ s16       field_2DC; // vertical scale, 0x1000 down to 0x200
    /* 0x2DE */ s16       field_2DE; // sub-state frame counter
    /* 0x2E0 */ s16       phase;     // chain pulse phase, stepped by 0x80
    /* 0x2E2 */ s16       field_2E2;
    /* 0x2E4 */ s8        field_2E4; // sub-state phase, cleared with field_2D4
    /* 0x2E5 */ s8        field_2E5;
    /* 0x2E6 */ byte      pad_2E6[0x2];
    /* 0x2E8 */ s8        field_2E8; // cleared, then awaited, by func_actor_503500_80138C08
    /* 0x2E9 */ s8        field_2E9;
    /* 0x2EA */ s8        field_2EA;
    /* 0x2EB */ s8        field_2EB; // TMD buffer countdown
} Actor503500Work2EC;
STATIC_ASSERT_SIZEOF(Actor503500Work2EC, 0x2EC);

/// Animation head of the boss block (`D_actor_503500_80176574`,
/// `Mem_Set(_, 0x7E8)`), viewed through its own type rather than the shared
/// `Actor503500Work`: the boss fronts its allocation with a `GpAnimCtx` --
/// `func_actor_503500_80136D30` passes the block itself to `Gp_AnimTickIndex`
/// -- whose slot array is inline at 0x14 and whose pose buffer starts
/// at 0x334, the two addresses `func_actor_503500_80135950` hands to
/// `func_800B3F84`. Twenty 0x28-byte slots fit exactly between them, and both
/// tick loops walk indices 1..0x13. That run covers 0x40..0x60, where the
/// shared view names the 0x160 block's display node, so the two blocks
/// genuinely disagree about it: the halfword the shared view calls
/// `Actor503500Slot40::boss.flags_4C` is `slots[1].field_10`, that animation
/// slot's flags word. This type stops at the pose buffer.
typedef struct Actor503500WorkBoss {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
} Actor503500WorkBoss;
STATIC_ASSERT_SIZEOF(Actor503500WorkBoss, 0x334);

/// Twelve-byte record of `Actor503500VecSet::field_C`, copied whole.
typedef struct Actor503500Rec12 {
    /* 0x00 */ s16 field_0[6];
} Actor503500Rec12;
STATIC_ASSERT_SIZEOF(Actor503500Rec12, 0xC);

/// Vector set that `func_actor_503500_80136B64` rotates from the template at
/// `D_actor_503500_8016F03C` into the live copy at `D_80183EEC`: the four
/// `field_4` vectors are only rotated, the eight `field_8` ones are also
/// offset by the attach coordinate's translation.
typedef struct Actor503500VecSet {
    /* 0x00 */ s32               field_0;
    /* 0x04 */ SVECTOR*          field_4;
    /* 0x08 */ SVECTOR*          field_8;
    /* 0x0C */ Actor503500Rec12* field_C;
} Actor503500VecSet;
STATIC_ASSERT_SIZEOF(Actor503500VecSet, 0x10);

void func_actor_503500_8013AC6C(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

/// The boss work block, cleared by `func_actor_503500_80132F64`.
extern Actor503500Work D_actor_503500_80176574;
extern s8              D_actor_503500_80176D5A;
extern s16             D_actor_503500_80176D2E;
/// 18-entry table of per-slot u16 counters, indexed by slot in
/// `func_actor_503500_801360A4` / `_801360BC` / `_8013611C`.
extern u16 D_actor_503500_80176D64[];
extern u16 D_actor_503500_80176D24;
/// Main-executable globals with no module header yet: `D_80071075` gates the
/// "everything is dead" message, `D_80073BA0` is the remaining-enemy count and
/// `D_80114C12` the cutscene/among-us mode flag.
extern u8  D_80071075;
extern s16 D_80073BA0;
extern s8  D_80114C12;
/// Main-executable flag byte cleared when the boss enters state 2; also written
/// by `mist_r18`, which has no module header for it either. Declared as an
/// array: `func_actor_503500_801345F4` needs the in-struct store, which keeps
/// the preceding `field_79C` store ordered before it.
extern s8 D_80071090[];
s32       func_actor_503500_80133684(Actor503500* arg0);
void      func_actor_503500_80137074(Actor503500* arg0, s8 arg1, s16 arg2);
void      func_actor_503500_801338E8(Actor503500* arg0);
void      func_actor_503500_80134408(Actor503500* arg0);
void      func_actor_503500_801345F4(Actor503500* arg0);
void      func_actor_503500_80134A24(Actor503500* arg0);
void      func_actor_503500_80134C68(Actor503500* arg0);
s32       func_actor_503500_80136FA8(Actor503500Work* work, s32 slot);
s32       func_actor_503500_80133D40(Actor503500* arg0, Actor503500Work* work);
s32       func_actor_503500_80133FD8(Actor503500* arg0, Actor503500Work* work);
s32       func_actor_503500_80136FDC(Actor503500Work* work, s32 slot);
/// Asks slot `slot` to die: `arg2` becomes its `field_7E0`/`field_2A` flag
/// and `arg3` its `field_752` countdown.
void func_actor_503500_80136F40(Actor503500Work* work, s32 slot, s32 arg2, s32 arg3);
/// Animation-preset table indexed by preset id; `func_actor_503500_80135FB4`
/// and `func_actor_503500_80132F64` hand entry pointers to
/// `func_actor_503500_80135950`.
extern Actor503500AnimPreset D_actor_503500_8016EAC0[];
/// Applies preset `arg2` to the boss block's animation slots; `arg1` and `arg3`
/// are passed by every caller but the body ignores them. Always returns 0.
s32 func_actor_503500_80135950(Actor503500* arg0, s32 arg1,
                               Actor503500AnimPreset* arg2, s32 arg3);
/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
/// Animation-bank table indexed by `Actor503500AnimPreset::field_0`.
extern void* D_actor_503500_8016EAB8[];
void         func_actor_503500_80136450(Actor503500* arg0);
void         func_actor_503500_801369E4(Actor503500* arg0);
void         func_actor_503500_80136A80(Actor503500* arg0);
/// Republishes the boss's four cached matrices (`arg1`) and/or re-seeds its
/// display state (`arg2`).
void func_actor_503500_80136B64(Actor503500* arg0, s32 arg1, s32 arg2);
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1);
void func_actor_503500_801374BC(Actor503500* arg0);
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_801398D0(Actor503500* arg0);
void      func_actor_503500_8013A0D0(Actor503500* arg0);
void      func_actor_503500_8013A96C(Actor503500* arg0);
void      func_actor_503500_8013AA44(Actor503500* arg0);
void      func_actor_503500_8013AAC0(Actor503500* arg0);
void      func_actor_503500_801334CC(Actor503500* arg0);
void      func_actor_503500_80135178(Actor503500* arg0);
void      func_actor_503500_801353F0(Actor503500* arg0);
void      func_actor_503500_80135644(Actor503500* arg0);
void      func_actor_503500_80136280(Actor503500* arg0);
void      func_actor_503500_80136304(Actor503500* arg0);
void      func_actor_503500_80136A88(Actor503500* arg0);
void      func_actor_503500_80136AEC(Actor503500* arg0);
void      func_actor_503500_80136D30(Actor503500* arg0);
void      func_actor_503500_80136DDC(Actor503500* arg0);
/// `TaskDesc` table `func_actor_503500_80132F64` and
/// `func_actor_503500_80135D00` spawn slot enemies from.
extern TaskDesc D_actor_503500_8016E924;
/// Initial position of the boss's collision node, copied into both the
/// enemy's `field_1C` and `field_5D4`'s `field_10/12/14`.
extern SVECTOR  D_actor_503500_8016EC50;
extern SVECTOR  D_actor_503500_8016EF58[];
extern TaskDesc D_actor_503500_8016E9F0;
/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_503500_80132F64`.
extern GpMsgEntry D_actor_503500_8016EA2C[];
/// Per-spawn enemy parameter table indexed by `Task::spawnArg1`;
/// `func_actor_503500_8013BEE4` and `func_actor_503500_8013ECBC` park the row
/// in `GpEnemy::param` and seed the enemy's HP from its `hpMax`.
extern GpPairSrcE D_actor_503500_8016E7EC[];
void              func_actor_503500_80136228(Actor503500* arg0);
/// Main-executable matrix the scene tracks (`acropolis_forked_road.h` calls it
/// the camera target); no module header owns it yet.
extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;
/// Attack lists for `func_actor_503500_801338E8`, indexed by the phase bit
/// (`field_774` bit 3), the height band `field_7DC` and the yaw bucket
/// `field_7DE`; each entry points at a NULL-terminated `Actor503500Step` run.
extern Actor503500Step** D_actor_503500_8016EF10[2][3];
/// Yaw-bucket thresholds, same `[phase][band]` indexing: ascending `|yaw|`
/// limits scanned until one is not below the current yaw offset.
extern s16* D_actor_503500_8016EF28[2][3];
/// Slot table walked by `func_actor_503500_80133D40`: four slot ids, tried in
/// order, and the per-entry value handed to `func_actor_503500_80136F40` as
/// its last argument when that slot is picked.
extern s16 D_actor_503500_8016EF40[];
extern s16 D_actor_503500_8016EF48[];
/// Second slot table for `func_actor_503500_80133FD8`; shares the per-entry
/// values in `D_actor_503500_8016EF40`.
extern s16 D_actor_503500_8016EF50[];
/// Per-sub-state speed class: 1 and 2 pick the slower yaw-tracking rates.
extern s8 D_actor_503500_8016E8FC[];
/// Main-executable yaw rotation: rotates `m` about Y by `angle`.
void func_8004BFF8(s32 angle, MATRIX* m);
/// Per-slot camera masks: bits 0x08/0x10/0x20/0x40 are yaw sectors around the
/// boss, bits 1/2/4 camera height bands (see `func_actor_503500_80135644`).
extern u8 D_actor_503500_8016E910[];
/// Declared in `gameplay/gameplay.h`, which conflicts with this TU's headers.
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
/// Effect offsets `func_actor_503500_80137678` cycles through, three entries.
extern SVECTOR D_actor_503500_8016F078[];
/// Effect offsets `func_actor_503500_80139014` cycles through, three entries.
extern SVECTOR D_actor_503500_8016F0D0[];
/// Main-executable counter the actor paces periodic effects by (its value
/// modulo 6 or 12, its low bits).
extern s32 D_80070F70;
/// The same pair for the 0x160 enemy at `D_actor_503500_80176D88`: a world
/// translation seeded into the task's own coordinate and the local offset its
/// `GpEnemy::bodyPos` and display node share.
extern SVECTOR         D_actor_503500_8016F060;
extern SVECTOR         D_actor_503500_8016F068;
extern Actor503500Work D_actor_503500_80176D88;
/// Per-slot tables of the 0x2EC enemies in `D_actor_503500_80176EE8`: world
/// translation and rotation of the task's coordinate (indexed by
/// `spawnArg1 - 2`), the local offset its `GpEnemy::bodyPos` and display node
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
/// Re-places a display node and its record table: `arg2` is the node's
/// `GpRec18` table and `arg3` the record count.
void func_actor_503500_80134EAC(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void func_actor_503500_80138288(Actor503500* arg0);
void func_actor_503500_801382F4(Actor503500* arg0);
void func_actor_503500_801382FC(Actor503500* arg0);
void func_actor_503500_80138378(Actor503500* arg0);
void func_actor_503500_801383D0(Actor503500* arg0);
void func_actor_503500_8013ACC4(Actor503500* arg0, s32 arg1);
void func_actor_503500_80138A30(Actor503500* arg0);
void func_actor_503500_80138C08(Actor503500* arg0);
void func_actor_503500_801395BC(Actor503500* arg0);
void func_actor_503500_8013A470(SVECTOR* pts, GsCOORDINATE2* coords, s32 phase);
void func_actor_503500_80137048(Actor503500* arg0, s32 rate);
/// The animation preset `func_actor_503500_80136D30` re-applies when the boss
/// finishes the clip it was gating on.
extern Actor503500AnimPreset D_actor_503500_8016EAD4;
extern Actor503500VecSet     D_80183EEC;
extern Actor503500VecSet     D_actor_503500_8016F03C;
void                         func_actor_503500_80132F64(Actor503500* arg0);
void                         func_actor_503500_80133270(Actor503500* arg0);
void                         func_actor_503500_801372C8(Actor503500* arg0);
void                         func_actor_503500_8013815C(Actor503500* arg0);
void                         func_actor_503500_8013852C(Actor503500* arg0);
void                         func_actor_503500_80138898(Actor503500* arg0);

/// `Task::state` handlers `func_actor_503500_80137238` dispatches through.
const TaskFuncTable3 D_actor_503500_80131E44 = {
    {
        func_actor_503500_80132F64,
        func_actor_503500_80133270,
        func_actor_503500_80136228,
    },
};

/// State-0 init of the boss: clears and seeds its work block, links the
/// second body part's display node, spawns slot enemies 1..11 from
/// `D_actor_503500_8016E924` (tinting each from the current area record, as
/// `func_actor_503500_80135D00` does) and applies preset 0x7D3.
void func_actor_503500_80132F64(Actor503500* arg0)
{
    GpAreaKey      key;
    GpAreaKey*     sessionKey;
    u8             areaByte0;
    GpAreaRec*     rec;
    GpAreaPlace*   entry;
    GpEnemy*       child;
    TmdObject*     model;
    u32            raw;
    s32            idx;
    s32            i;
    TmdObject*     tmd;
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;
    GpRec18*       recs;
    /* Kept in a register across the spawn loop: the ROM stores enemies[0]
       through the same base rather than rebuilding the address. */
    Actor503500Work* work = &D_actor_503500_80176574;

    tmd   = arg0->extra;
    enemy = arg0->field_20;
    coord = tmd->coords;
    Mem_Set(work, 0, 0x7E8);
    arg0->field_1C     = work;
    work->field_7D5    = -1;
    work->field_7D6    = -1;
    work->field_7D9    = -1;
    work->field_7A4    = 0x80000;
    work->field_6C4.vx = coord->coord.t[0] << 16;
    work->field_6C4.vy = coord->coord.t[1] << 16;
    work->field_6C4.vz = coord->coord.t[2] << 16;
    work->field_7D8    = 1;
    work->field_7CA    = 0x5A;
    tmd->lightMtx      = &work->lightMtx;
    tmd->colorMtx      = &work->colorMtx;
    tmd->otOffset      = 0x14;
    coord->flg         = 0;

    enemy->field_4     = &coord->coord;
    part               = &coord[3];
    enemy->field_48    = 0;
    enemy->coord       = part;
    enemy->node.flags |= 9;
    enemy->bodyPos.vx  = D_actor_503500_8016EC50.vx;
    enemy->bodyPos.vy  = D_actor_503500_8016EC50.vy;
    enemy->bodyPos.vz  = D_actor_503500_8016EC50.vz;
    recs               = work->rec5F4;
    enemy->param       = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs        = recs;
    enemy->hp          = enemy->param->hpMax;

    work->field_5D4.coord    = part;
    work->field_5D4.ctx.recs = recs;
    work->field_5D4.key      = 0x30023;
    work->field_5D4.radius   = 0x258;
    work->field_5D4.flags    = 1;
    work->field_5D4.pos.vx   = D_actor_503500_8016EC50.vx;
    work->field_5D4.pos.vy   = D_actor_503500_8016EC50.vy;
    work->field_5D4.pos.vz   = D_actor_503500_8016EC50.vz;
    Gp_LinkObj(2, &work->field_5D4);
    Gp_InitRec18Table(recs, 8, 0);
    work->field_6E4.spawnArgLo = 0x600;
    work->field_6E4.coord      = part;
    work->field_6E4.spawnArgHi = 3;
    work->field_5D4.flags     &= 0x7FFF;

    for (i = 1; i < 12; i++) {
        child = Gp_SpawnEnemyFromTable(&D_actor_503500_8016E924, i, i, enemy);
        if (child != NULL) {
            sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
            raw        = arg0->field_20->placeKey;
            model      = (TmdObject*)child->task->extra;
            key.stage  = sessionKey->stage;
            key.area   = sessionKey->area;
            key.room   = sessionKey->room;
            areaByte0  = sessionKey->view;
            idx        = raw >> 12;
            key.view   = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            rec          = Gp_GetNestedAreaRec(&key);
            entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
            model->tpage = entry->tpage;
            model->clut  = entry->clut;
            if (model->buffer != NULL) {
                tmdProcessStream(model);
                tmdProcessStream(model);
            }
            work->enemies[i] = child;
        }
    }
    work->enemies[0] = enemy;
    ((void (*)(s32))Gp_IncStateF0Ref)(0x23);
    func_actor_503500_80136B64(arg0, 1, 0);
    for (i = 17; i >= 0; i--) {
        D_actor_503500_80176D64[i] = 0;
    }
    func_actor_503500_80135950(arg0, 0x7D3, D_actor_503500_8016EAC0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_80136228;
    arg0->field_24     = D_actor_503500_8016EA2C;
    arg0->state       += 1;
}

/// Per-frame update. `D_801153F4` 1 pauses the boss (buffers kept, only
/// `func_actor_503500_80136AEC` runs), 2 hides it; anything else runs the
/// normal chain. `field_7D9` counts down to the frame the TMD buffers are freed.
void func_actor_503500_80133270(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s32              mode;

    enemy = arg0->field_20;
    tmd   = arg0->extra;
    mode  = D_801153F4;
    work  = arg0->field_1C;

    switch (mode) {
        case 1:
            if (work->field_7E4 == 0) {
                SndEvt_EnqueueType8(0x40000000);
                Tmd_AllocBuffers(tmd);
                tmd->flags     &= 0xFF7B;
                work->field_7E4 = mode;
                work->field_7E5 = 0;
            }
            func_actor_503500_80136AEC(arg0);
            return;
        case 2:
            if (work->field_7D9 >= 0) {
                if (work->field_7D9 == 0) {
                    Tmd_FreeBuffers(tmd);
                }
                work->field_7D9--;
            }
            if (work->field_7E5 == 0) {
                SndEvt_EnqueueType8(0x40000000);
                tmd->flags     |= 0x84;
                work->field_7D9 = 1;
                work->field_7E4 = 0;
                work->field_7E5 = 1;
            }
            return;
        default:
            if (work->field_7E4 == 1 || work->field_7E5 == 1 || work->field_7E7 != 0) {
                SndEvt_EnqueueType9(0x40000000);
                work->field_7E4 = 0;
                work->field_7E5 = 0;
                work->field_7E7 = 0;
            }
            if ((D_80071075 & 0xF0) == 0x40) {
                SndEvt_EnqueueType8(0x40000000);
                work->field_7E7 = 1;
            }
            if (gGameSession->eventState == 0) {
                tmd->flags &= 0xFF7F;
            }
            if (work->field_7D9 >= 0) {
                if (work->field_7D9 == 0) {
                    Tmd_FreeBuffers(tmd);
                }
                work->field_7D9--;
            }
            if (enemy->reactionFlags != 0) {
                func_actor_503500_80136280(arg0);
            }
            func_actor_503500_80136A88(arg0);
            func_actor_503500_80136AEC(arg0);
            func_actor_503500_80136B64(arg0, 0, 0);
            func_actor_503500_801334CC(arg0);
            func_actor_503500_80136304(arg0);
            func_actor_503500_80135178(arg0);
            func_actor_503500_801353F0(arg0);
            func_actor_503500_80136D30(arg0);
            func_actor_503500_80135644(arg0);
            func_actor_503500_80136DDC(arg0);
            break;
    }
}

/// Per-frame upkeep: ticks the `field_752` slot counters down to 0 while the
/// boss is in state 0, rolls `field_7C8` from `Gp_LcgState`, stores the yaw to
/// `D_80073B8C` (offset by `field_7D2`, wrapped into [-0x800, 0x800)) in
/// `field_7B8`, and when `field_7CC` runs out links or unlinks `field_20`'s
/// node per `field_7E2`.
void func_actor_503500_801334CC(Actor503500* arg0)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GpEnemy*         enemy;
    SVECTOR          vec;
    s16              angle;
    s16              count;
    s16*             p;
    s32              i;

    work = arg0->field_1C;
    p    = work->field_752;
    if (work->field_7B0 == 0) {
        for (i = 0; i < 0x11; i++, p++) {
            if (--*p < 0) {
                *p = 0;
            }
        }
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_7C8 = Gp_LcgState >> 16;
    coord           = arg0->extra->coords;
    vec.vx          = D_80073B8C->t[0] - coord->coord.t[0];
    vec.vy          = 0;
    vec.vz          = D_80073B8C->t[2] - coord->coord.t[2];
    angle           = work->field_7D2 + ratan2(vec.vx, vec.vz);
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    work->field_7B8 = angle;
    enemy           = arg0->field_20;
    count           = --work->field_7CC;
    if (count < 0) {
        work->field_7CC = 0;
    } else if (count == 0) {
        if (work->field_7E2 != 0) {
            work->field_5D4.flags |= 0x8000;
            Gp_LinkNode(&enemy->node);
        } else {
            work->field_5D4.flags &= 0x7FFF;
            Gp_UnlinkNode(&enemy->node);
        }
    }
}

s32 func_actor_503500_80133684(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy**        slots;
    GpEnemy*         slot1;
    s32              ret;

    ret   = 0;
    work  = arg0->field_1C;
    slots = work->enemies;

    if (work->field_7B0 != 2) {
        if (!(work->field_774 & 1) && (slots[4] == NULL)) {
            slots[10]->task->killCountdown = 8;
            ret                            = 1;
            work->field_774               |= 1;
        }
        if (!((work->field_774 >> 1) & 1) && (slots[5] == NULL)) {
            slots[11]->task->killCountdown = 8;
            ret                            = 1;
            work->field_774               |= 2;
        }
        if (!((work->field_774 >> 2) & 1) && (slots[1] == NULL)) {
            if (slots[12] != NULL) {
                slots[12]->task->killCountdown = 8;
                ret                            = 1;
                work->field_774               |= 4;
            }
        }
    }

    if (!((work->field_774 >> 3) & 1) &&
        ((((slot1 = slots[1], slot1 == NULL)) && (slots[12] == NULL)) ||
         (slots[7] == NULL) || (slots[8] == NULL) || (slots[10] == NULL) ||
         (slots[11] == NULL) || ((slots[9] == NULL) && (slot1 == NULL)) ||
         ((slots[4]->hp == 0) && (slots[5]->hp == 0)))) {
        if ((((GameActor*)(gameGetPtrSlot(3))->work)->field_954 != 2) &&
            (D_80073BA0 > 0) && (D_80114C12 != 1)) {
            ret = 1;
            if (D_80071075 == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
                work->field_774 |= 8;
                /* `ret` has to be dead across the call for GCC to keep it in
                 * $a1: it is re-set on the way out of both arms. */
                goto done;
            }
        } else {
        done:
            ret = 1;
        }
    }
    return ret;
}

/// Boss attack picker. Every frame it stores the camera target's yaw
/// relative to the boss (wrapped into [-0x800, 0x800)) in `field_7BA`. Step 0
/// updates the height band `field_7DC` from the target's Y with hysteresis,
/// buckets `|yaw|` into `field_7DE`, then walks that bucket's weighted list
/// until the running weight reaches the random byte in `field_7C8`, stores
/// the chosen step in `field_778` and runs it at once. Step 1 keeps running
/// it. A non-zero result, an empty list (0x1E) or running off the end of the
/// list (0xF) goes to `field_7CA` after `func_actor_503500_80136EFC(arg0, 0)`.
void func_actor_503500_801338E8(Actor503500* arg0)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    Actor503500Step* step;
    s32              bit;
    s16*             thr;
    s32              angle;
    s32              absAngle;
    u32              sum;
    u32              r;
    s32              ret;

    work  = arg0->field_1C;
    coord = arg0->extra->coords;
    angle = ratan2(D_80073B8C->t[0] - coord->coord.t[0], D_80073B8C->t[2] - coord->coord.t[2]) - work->field_7B6;
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    work->field_7BA = angle;
    switch ((s8)work->field_7DA) {
        case 0:
            work->field_7DD = work->field_7DC;
            work->field_7DF = work->field_7DE;
            switch (work->field_7DD) {
                case 0:
                    if (D_80073B8C->t[1] >= -0xAEF) {
                        work->field_7DC = 1;
                    }
                    break;
                case 1:
                    if (D_80073B8C->t[1] >= -0x31F) {
                        work->field_7DC = 2;
                    } else if (D_80073B8C->t[1] < -0xC80) {
                        work->field_7DC = 0;
                    }
                    break;
                case 2:
                    if (D_80073B8C->t[1] < -0x4B0) {
                        work->field_7DC = 1;
                    }
                    break;
            }
            thr      = D_actor_503500_8016EF28[(work->field_774 >> 3) & 1][work->field_7DC];
            absAngle = abs(angle);
            for (work->field_7DE = 0; *thr++ < absAngle; work->field_7DE++) {
            }
            bit  = (work->field_774 >> 3) & 1;
            step = D_actor_503500_8016EF10[bit][work->field_7DC][work->field_7DE];
            r    = (u8)work->field_7C8;
            if (step->fn != NULL) {
                sum = step->weight;
                while (sum < r) {
                    if (step[1].fn == NULL) {
                        func_actor_503500_80136EFC(arg0, 0);
                        work->field_7CA = 0xF;
                        return;
                    }
                    step++;
                    sum += step->weight;
                }
                work->field_778 = step->fn;
                work->field_7DA++;
            } else {
                func_actor_503500_80136EFC(arg0, 0);
                work->field_7CA = 0x1E;
                return;
            }
        case 1:
            ret = work->field_778(arg0, work);
            if (ret != 0) {
                func_actor_503500_80136EFC(arg0, 0);
                work->field_7CA = ret;
            }
            break;
        default:
            func_actor_503500_80136EFC(arg0, 0);
            break;
    }
}

/// Script step that dismisses one of the boss's slot-10/11 helpers. State 0
/// picks the slot: with `field_7DE` clear it tries the slot chosen by the low
/// bit of `field_7C8` and then the other one, and always advances; otherwise
/// the sign of `field_7BA` picks slot 10 or 11 and it only advances once that
/// slot is ready. State 1 waits for the chosen slot (`field_7C2`) to finish
/// dying. Returns 1 while no slot is ready, 0 the frame the request is issued
/// or while waiting, and 1 once the slot has gone quiet. `arg0` is passed
/// through the step table and ignored here.
s32 func_actor_503500_80133BF4(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;
    s32 odd;
    s32 slot;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (work->field_7DE == 0) {
                odd  = work->field_7C8 & 1;
                slot = odd + 0xA;
                if (func_actor_503500_80136FDC(work, slot) != 0 ||
                    (slot = 0xB - odd, func_actor_503500_80136FDC(work, slot) != 0)) {
                    func_actor_503500_80136F40(work, slot, 2, 0x78);
                    work->field_7C2 = slot;
                }
                ret             = 0;
                work->field_7D2 = 0;
                work->field_7DB = 1;
            } else if (work->field_7BA > 0) {
                if (func_actor_503500_80136FDC(work, 0xA) != 0) {
                    func_actor_503500_80136F40(work, 0xA, 2, 0x78);
                    ret             = 0;
                    work->field_7C2 = 0xA;
                    work->field_7D2 = 0;
                    work->field_7DB = 1;
                }
            } else if (func_actor_503500_80136FDC(work, 0xB) != 0) {
                func_actor_503500_80136F40(work, 0xB, 2, 0x78);
                ret             = 0;
                work->field_7C2 = 0xB;
                work->field_7D2 = 0;
                work->field_7DB = 1;
            }
            break;
        case 1:
            if (func_actor_503500_80136FA8(work, work->field_7C2) == 0) {
                ret = 0;
            }
            break;
    }
    return ret;
}

/// Script step that dismisses the first ready slot of four (slots 2, 7, 13,
/// 14 are tested) whose `field_7BA` range test passes; 13 and 14 also depend on
/// whether the other one's `enemies` entry is empty. State 0 returns 0 the
/// frame a slot is issued, else 1. State 1 bumps `field_7BE` unless the slot
/// is 7, and once the slot is done or 0x1E frames have passed returns 0x3C for
/// slot 7 or 0x1E for 2/13/14; until then 0. Any other state returns 1.
s32 func_actor_503500_80133D40(Actor503500* arg0, Actor503500Work* work)
{
    s32  ret;
    s32  i;
    s16* slots;
    s16  dir;
    s16  slot;

    switch ((s8)work->field_7DB) {
        case 0:
            ret   = 1;
            i     = 0;
            slots = D_actor_503500_8016EF48;
            dir   = work->field_7BA;
            for (; i < 4; i++) {
                slot = *slots;
                if (func_actor_503500_80136FDC(work, slot) != 0) {
                    switch (slot) {
                        case 2:
                            if (!(dir >= -0x6A4 && dir <= 0x3E8)) {
                                ret = 0;
                            }
                            break;
                        case 13:
                            if (func_actor_503500_80135E04((Task*)arg0, 0xE) != 0) {
                                if (!(dir >= -0x7D0 && dir <= 0x3E8)) {
                                    ret = 0;
                                }
                            } else if (dir >= 0x3E9 && dir <= 0x6A3) {
                                ret = 0;
                            }
                            break;
                        case 14:
                            if (func_actor_503500_80135E04((Task*)arg0, 0xD) != 0) {
                                if (!(dir >= -0x7D0 && dir <= 0x3E8)) {
                                    ret = 0;
                                }
                            } else if (!(dir >= -0x7D0 && dir <= 0x6A3)) {
                                ret = 0;
                            }
                            break;
                        case 7:
                            if (dir >= 0x5DD && dir <= 0x76B) {
                                ret = 0;
                            }
                            break;
                    }
                    if (ret == 0) {
                        func_actor_503500_80136F40(work, slot, 2, D_actor_503500_8016EF40[i]);
                        work->field_7C2 = slot;
                        work->field_7DB = 1;
                        break;
                    }
                }
                slots++;
            }
            break;
        case 1:
            ret = 0;
            if (work->field_7C2 != 7) {
                work->field_7BE++;
            }
            if (func_actor_503500_80136FA8(work, work->field_7C2) != 0 || work->field_7BE > 0x1E) {
                switch (work->field_7C2) {
                    case 2:
                        ret = 0x1E;
                        break;
                    case 13:
                        ret = 0x1E;
                        break;
                    case 14:
                        ret = 0x1E;
                        break;
                    case 7:
                        ret = 0x3C;
                        break;
                }
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Mirror of `func_actor_503500_80133D40` for the other side: tries slots 3,
/// 15, 16 and 8 with the `field_7BA` range tests reflected. State 0 returns 0
/// the frame a slot is issued, else 1. State 1 bumps `field_7BE` unless the
/// slot is 8, and once the slot is done or 0x14 frames have passed returns 0x3C
/// for slot 8 or 0x1E for 3/15/16; until then 0. Any other state returns 1.
s32 func_actor_503500_80133FD8(Actor503500* arg0, Actor503500Work* work)
{
    s32  ret;
    s32  i;
    s16* slots;
    s16  dir;
    s16  slot;

    switch ((s8)work->field_7DB) {
        case 0:
            ret   = 1;
            i     = 0;
            slots = D_actor_503500_8016EF50;
            dir   = work->field_7BA;
            for (; i < 4; i++) {
                slot = *slots;
                if (func_actor_503500_80136FDC(work, slot) != 0) {
                    switch (slot) {
                        case 3:
                            if (!(dir >= -0x3E8 && dir <= 0x6A4)) {
                                ret = 0;
                            }
                            break;
                        case 15:
                            if (func_actor_503500_80135E04((Task*)arg0, 0x10) != 0) {
                                if (!(dir >= -0x3E8 && dir <= 0x7D0)) {
                                    ret = 0;
                                }
                            } else if (!(dir >= -0x6A3 && dir <= 0x7D0)) {
                                ret = 0;
                            }
                            break;
                        case 16:
                            if (func_actor_503500_80135E04((Task*)arg0, 0xF) != 0) {
                                if (!(dir >= -0x3E8 && dir <= 0x7D0)) {
                                    ret = 0;
                                }
                            } else if (dir < -0x3E8) {
                                if (dir >= -0x6A3) {
                                    ret = 0;
                                }
                            }
                            break;
                        case 8:
                            if (dir < -0x5DC) {
                                if (dir >= -0x76B) {
                                    ret = 0;
                                }
                            }
                            break;
                    }
                    if (ret == 0) {
                        func_actor_503500_80136F40(work, slot, 2, D_actor_503500_8016EF40[i]);
                        work->field_7C2 = slot;
                        work->field_7DB = 1;
                        break;
                    }
                }
                slots++;
            }
            break;
        case 1:
            ret = 0;
            if (work->field_7C2 != 8) {
                work->field_7BE++;
            }
            if (func_actor_503500_80136FA8(work, work->field_7C2) != 0 || work->field_7BE > 0x14) {
                switch (work->field_7C2) {
                    case 3:
                        ret = 0x1E;
                        break;
                    case 15:
                        ret = 0x1E;
                        break;
                    case 16:
                        ret = 0x1E;
                        break;
                    case 8:
                        ret = 0x3C;
                        break;
                }
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Script step pairing `func_actor_503500_80133D40` and `_80133FD8`. State 0
/// runs the one the sign of `field_7BA` picks; the fallback to the other one
/// needs `field_7BA` beyond +/-0x76C on the opposite side, which that sign
/// rules out, so it never fires. `field_7D2` becomes 0x7D0 when the result
/// is 0, else 0. State 1 bumps `field_7BE`
/// unless `field_7C2` is slot 7 or 8, and once that slot is done or 0x5B
/// frames have passed returns a per-slot delay (0xF, 0x3C, 0x5A or 0x1E);
/// until then it returns 0. Any other state returns 1.
s32 func_actor_503500_80134284(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;
    s32 dir;

    dir = work->field_7BA;
    switch ((s8)work->field_7DB) {
        case 0:
            if (dir > 0) {
                ret = func_actor_503500_80133D40(arg0, work);
                if (ret == 1 && dir < -0x76C) {
                    ret = func_actor_503500_80133FD8(arg0, work);
                }
            } else {
                ret = func_actor_503500_80133FD8(arg0, work);
                if (ret == 1 && dir > 0x76C) {
                    ret = func_actor_503500_80133D40(arg0, work);
                }
            }
            if (ret == 0) {
                work->field_7D2 = 0x7D0;
            } else {
                work->field_7D2 = 0;
            }
            break;
        case 1:
            ret = 0;
            if ((u16)work->field_7C2 - 7 >= 2U) {
                work->field_7BE++;
            }
            if (func_actor_503500_80136FA8(work, work->field_7C2) != 0 || work->field_7BE > 0x5A) {
                switch (work->field_7C2) {
                    case 2:
                    case 3:
                        ret = 0xF;
                        break;
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                        ret = 0x3C;
                        break;
                    case 7:
                    case 8:
                        ret = 0x5A;
                        break;
                    default:
                        ret = 0x1E;
                        break;
                }
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Two-step state of the boss block. Step 0 hides the second body part,
/// unlinks the enemy node, stores the summed `field_40` of occupied slots
/// 1..16 in `gGameSession->bossPartsHpSum` and plays sound 0x40230010 at the
/// part's position. Step 1 counts 0x1F frames, then posts message 0x13F4
/// under the same gates as `func_actor_503500_80133684`.
void func_actor_503500_80134408(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              i;
    s16              sum;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch ((s8)work->field_7DA) {
        case 0:
            work->field_5D4.flags &= 0x7FFF;
            func_actor_503500_8013611C(arg0->spawnArg1);
            Gp_UnlinkNode(&enemy->node);
            enemy->recs     = 0;
            work->field_7B4 = 0;
            Gp_PulseState1C();
            sum = 0;
            for (i = 1; i < 0x11; i++) {
                if (work->enemies[i] != NULL) {
                    sum += work->enemies[i]->hp;
                }
            }
            gGameSession->bossPartsHpSum = sum;
            work->field_7E0              = 0;
            func_actor_503500_80135FB4(arg0, 0xE, 0x20);
            pan = (s8)Gp_GetObjPan(&arg0->extra->coords[3]);
            SndEvt_EnqueueType6(0x40230010, pan,
                                (s8)(gpGetObjDepth(&arg0->extra->coords[3]) / 2));
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (++work->field_7BC >= 0x1F &&
                ((GameActor*)(gameGetPtrSlot(3))->work)->field_954 != 2 &&
                D_80073BA0 > 0 && D_80114C12 != 1 && D_80071075 == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_7DA = work->field_7DA + 1;
            }
            break;
    }
}

/// Seven-step state of the boss block. Step 0 applies preset 0x13 and clears
/// `field_79C`; step 1 sprays 0x60055 effects for 0x78 frames (one from the
/// frame count, one from `Gp_LcgState`), plays 0x40230012, and at frame 0x97
/// spawns the attached effect task into `field_79C`. Step 2 waits for
/// `field_7E3`, kills that task and spawns a fresh one; steps 3..6 walk
/// presets 6, 7 and 8 and finally return the boss to state 0.
void func_actor_503500_801345F4(Actor503500* arg0)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    Task*            task;
    s32              pan;

    work = arg0->field_1C;
    switch ((s8)work->field_7DA) {
        case 0:
            func_actor_503500_80135FB4(arg0, 0x13, 0x10);
            func_actor_503500_8013611C(arg0->spawnArg1);
            work->field_79C = NULL;
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (work->field_7BC < 0x78) {
                Gp_SpawnEff(0x60055, &arg0->extra->coords[3], 0x01001800,
                            &D_actor_503500_8016EF58[(s16)(work->field_7BC % 7)]);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60055, &arg0->extra->coords[3], 0x01001800,
                            &D_actor_503500_8016EF58[(u16)((Gp_LcgState >> 16) % 7)]);
            }
            if (work->field_7BC == 0x78) {
                SndEvt_EnqueueType7(0x40230012, 0x3C);
            }
            if (work->field_7BC == 2) {
                coord = &arg0->extra->coords[3];
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x40230012, pan, (s8)(gpGetObjDepth(coord) / 2));
            }
            if (++work->field_7BC >= 0x97) {
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 4, 0x64, (s32)arg0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->coords;
                    coord->sub        = &arg0->extra->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                work->field_79C = task;
                D_80071090[0]   = 1;
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 2:
            if (work->field_7E3 != 0) {
                task = work->field_79C;
                if (task != NULL) {
                    task->exitCallback(task);
                }
                func_actor_503500_80135FB4(arg0, 5, 0x10);
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 4, 0x5A, (s32)arg0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->coords;
                    coord->sub        = &arg0->extra->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                func_actor_503500_80137074(arg0, 1, 1);
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 3:
            if (func_actor_503500_80136014(arg0, 5) != 0) {
                func_actor_503500_80135FB4(arg0, 6, 0x10);
                work->field_7BC = 0;
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 4:
            if (++work->field_7BC >= 0x33) {
                func_actor_503500_80135FB4(arg0, 7, 0);
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 5:
            if (func_actor_503500_80136014(arg0, 7) != 0) {
                func_actor_503500_80135FB4(arg0, 8, 0);
                func_actor_503500_80137074(arg0, 0, 0xE);
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 6:
            if (func_actor_503500_80136014(arg0, 8) != 0) {
                D_80071090[0] = 0;
                func_actor_503500_80135FB4(arg0, 0, 0);
                work->field_7D2 = 0;
                func_actor_503500_80136EFC(arg0, 0);
            }
            break;
    }
}

/// Three-step state of the boss block. Step 0 saves the coordinate's rotation
/// and seeds the Y scale to 0x1000; step 1 counts 0x1F frames, then applies
/// animation preset 0xE; step 2 restores the rotation every frame, squashes it
/// in Y down to 0x200 and fires the light / effect cues at frames 0x3C, 0x46
/// and 0x64. From step 2 on, preset 0xE is reapplied whenever
/// `func_actor_503500_80136014` reports it.
void func_actor_503500_80134A24(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32*             src;
    s32*             dst;
    s32              i;
    VECTOR           scale;

    coord = arg0->extra->coords;
    obj   = arg0->extra;
    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch ((s8)work->field_7DA) {
        case 0:
            work->field_7BC = 0;
            work->field_7CE = 0x1000;
            dst             = work->field_77C;
            src             = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->field_78C = coord->coord.m[2][2];
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (++work->field_7BC >= 0x1F) {
                func_actor_503500_80135FB4(arg0, 0xE, 0x20);
                work->field_7BC = 0;
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 2:
            if (work->field_7CE > 0x200) {
                work->field_7CE -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = work->field_77C;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->field_78C;
            scale.vx             = 0x1000;
            scale.vy             = work->field_7CE;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            switch (++work->field_7BC) {
                case 0x3C:
                    obj->flags |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    break;
                case 0x46:
                    Gp_SpawnEff(0x600A5, coord, 1, NULL);
                    break;
                case 0x64:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
            }
            break;
    }
    if ((s8)work->field_7DA >= 2 && func_actor_503500_80136014(arg0, 0xE) != 0) {
        func_actor_503500_80135FB4(arg0, 0xE, 0x20);
    }
}

void func_actor_503500_80134C68(Actor503500* arg0)
{
    Actor503500Work* work;
    Task*            task;
    GsCOORDINATE2*   coord;

    work = arg0->field_1C;
    if ((u16)(work->field_7B0 - 2) < 3U) {
        func_actor_503500_80136F40(work, 0, 0, 0);
        return;
    }
    switch (work->field_7E1) {
        case 0:
            func_actor_503500_80135FB4(arg0, 4, 0x10);
            func_actor_503500_80137074(arg0, 1, 0x4B);
            work->field_7E1++;
            break;
        case 1:
            if (func_actor_503500_80136014(arg0, 4) != 0) {
                func_actor_503500_80135FB4(arg0, 5, 0x10);
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 4, 0x5A, (s32)arg0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->coords;
                    coord->sub        = &arg0->extra->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                D_80071090[0]   = 1;
                work->field_7C0 = 0;
                work->field_7E1++;
            }
            break;
        case 2:
            if (++work->field_7C0 >= 0x5B) {
                func_actor_503500_80135FB4(arg0, 6, 0x10);
                work->field_7C0 = 0;
                work->field_7E1++;
            }
            break;
        case 3:
            if (++work->field_7C0 >= 0x33) {
                func_actor_503500_80135FB4(arg0, 7, 0);
                work->field_7E1++;
            }
            break;
        case 4:
            if (func_actor_503500_80136014(arg0, 7) != 0) {
                func_actor_503500_80135FB4(arg0, 8, 0);
                func_actor_503500_80137074(arg0, 0, 0xE);
                work->field_7E1++;
            }
            break;
        case 5:
            if (func_actor_503500_80136014(arg0, 8) != 0) {
                work->field_7E0 = 0;
                D_80071090[0]   = 0;
                func_actor_503500_80135F9C((Task*)arg0, 0, 0);
                func_actor_503500_80135FB4(arg0, 0, 0);
            }
            break;
    }
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the boss. Each attack id is taken once, and only type-2 ids land while the
/// `field_7B4` countdown is clear: the damage scales with the attacker's
/// distance, `Gp_RollEnemyChance` can quadruple it, and a hit that empties
/// `field_40` starts the death state instead of the id's status effect. `arg1`
/// is passed by the caller but unused.
void func_actor_503500_80134EAC(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_7B4 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), 0, 0);
        if (Gp_RollEnemyChance(enemy, id, 0) != 0) {
            dmg *= 4;
            Gp_SpawnEff(0x6009C, coord, 0, NULL);
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        enemy->hp -= dmg;
        func_800DA6E8(&enemy->node, dmg, 0);
        if (enemy->hp <= 0) {
            func_actor_503500_80136EFC(arg0, 4);
            work->field_7E6 = 1;
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
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, NULL, &work->field_6E4);
        stun = Gp_GetIdParam2(id);
        if (work->field_7B4 < stun) {
            work->field_7B4 = stun;
        }
    next:;
    }
}

void func_actor_503500_80135178(Actor503500* arg0)
{
    SVECTOR          rot;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GpEnemy**        enemies;
    s32              speed;
    s32              turn;
    s32              diff;
    s32              absDiff;
    s16              yaw;
    s16              newYaw;

    work    = arg0->field_1C;
    coord   = arg0->extra->coords;
    enemies = work->enemies;
    if (work->field_7B0 < 2) {
        if (work->field_7B0 >= 0) {
            switch (D_actor_503500_8016E8FC[work->field_7D5]) {
                case 1:
                    speed = 0x60000;
                    break;
                case 2:
                    speed = 0x10000;
                    break;
                default:
                    speed = 0x70000;
                    break;
            }
            turn = 0;
            if (enemies[10] == NULL) {
                turn = -(speed / 25);
            }
            if (enemies[11] == NULL) {
                turn -= speed / 25;
            }
            if (work->enemies[6] == NULL) {
                turn -= (speed * 12) / 100;
            }
            if (work->enemies[9] == NULL) {
                turn -= speed / 4;
            }
            speed  += turn;
            yaw     = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
            turn    = work->field_7A8;
            diff    = work->field_7B8 - yaw;
            absDiff = ABS(diff);
            if ((speed * 8) >> 16 >= absDiff) {
                if (turn < 0) {
                    turn += speed / 16;
                    if (turn > 0) {
                        turn = 0;
                    }
                } else {
                    turn -= speed / 16;
                    if (turn < 0) {
                        turn = 0;
                    }
                }
            } else {
                if (absDiff > 0x800) {
                    if (diff < 0) {
                        diff += 0x1000;
                    } else {
                        diff -= 0x1000;
                    }
                }
                if (diff > 0) {
                    turn += speed / 32;
                    if (turn > speed) {
                        turn = speed;
                    }
                } else {
                    turn -= speed / 32;
                    if (turn < -speed) {
                        turn = -speed;
                    }
                }
            }
            newYaw          = yaw + (turn >> 16);
            work->field_7A8 = turn;
            rot.vx          = 0;
            rot.vy          = newYaw;
            rot.vz          = 0;
            RotMatrix(&rot, &coord->coord);
            coord->flg      = 0;
            work->field_7B6 = newYaw;
        }
    }
}

/// Steps the boss along its yaw: ramps the walk speed `field_7A0` by
/// `field_7A4` / 32 (reversed at a quarter rate when `field_7B8` is more than
/// 0x258 off `field_7B6`), moves `field_6C4` by it inside a fixed box and drops
/// the integer part into the coordinate. States 4, 5 and 7 (and 6 before step
/// 3) only record the previous translation in `field_6D4`.
void func_actor_503500_801353F0(Actor503500* arg0)
{
    MATRIX           mat;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    s32              step;
    s32              speed;
    s32              scale;
    MATRIX*          m;
    s32*             px;
    s32*             pz;

    work               = arg0->field_1C;
    coord              = arg0->extra->coords;
    work->field_6D4.vx = coord->coord.t[0];
    work->field_6D4.vy = coord->coord.t[1];
    work->field_6D4.vz = coord->coord.t[2];
    switch (work->field_7B0) {
        case 4:
        case 5:
        case 7:
            return;
        case 6:
            if ((s8)work->field_7DA < 3) {
                return;
            }
        case 0:
            work->field_7D8 = 1;
            break;
        default:
            work->field_7D8 = 0;
            break;
    }
    step = work->field_7A4;
    if (ABS(work->field_7B8 - work->field_7B6) > 0x258) {
        step = -step >> 2;
    }
    if (work->field_7D8 != 0) {
        speed = work->field_7A0 + step / 32;
        if (speed > 0) {
            if (step < speed) {
                speed = step;
            }
        } else if (speed < -step) {
            speed = -step;
        }
    } else {
        speed = work->field_7A0 - step / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->field_7A0    = speed;
    m                  = &mat;
    *(s32*)&m->m[0][0] = 0x1000;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = 0x1000;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = 0x1000;
    func_8004BFF8(work->field_7B6, m);
    scale               = speed >> 12;
    work->field_6B4     = mat.m[0][2] * scale;
    work->field_6BC     = mat.m[2][2] * scale;
    work->field_6C4.vx += work->field_6B4;
    work->field_6C4.vz += work->field_6BC;
    px                  = &work->field_6C4.vx;
    pz                  = &work->field_6C4.vz;
    if (*px > 0x23280000) {
        *px = 0x23280000;
    } else if (*px < 0x1B580000) {
        *px = 0x1B580000;
    }
    if (*pz > 0x1F400000) {
        *pz = 0x1F400000;
    } else if (*pz < 0x17700000) {
        *pz = 0x17700000;
    }
    coord->coord.t[0] = *px >> 16;
    coord->coord.t[2] = *pz >> 16;
    coord->flg        = 0;
}

/// Clears each slot enemy's `node.flags` bit 1 only when its
/// `D_actor_503500_8016E910` entry covers both the camera's yaw sector
/// (relative to `field_7B6`) and its height band and `gGameSession->eventState`
/// is 0; otherwise sets it. Bit 4 is set on a height-only miss and cleared on
/// a yaw miss.
void func_actor_503500_80135644(Actor503500* arg0)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GpEnemy*         enemy;
    s16              angle;
    s32              y;
    s16              dirMask;
    s16              heightMask;
    s32              i;
    s16              bits;

    work  = arg0->field_1C;
    coord = arg0->extra->coords;
    angle = ratan2(D_80073B8C->t[0] - coord->coord.t[0], D_80073B8C->t[2] - coord->coord.t[2]) - work->field_7B6;
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    dirMask = 0x10;
    if (ABS(angle) < 0x400) {
        dirMask = 8;
    }
    if (angle > 0x300 && angle < 0x480) {
        dirMask = 0x20;
    } else if (angle < -0x300 && angle > -0x480) {
        dirMask = 0x40;
    }
    y          = D_80073B8C->t[1];
    heightMask = 4;
    if (y < -999) {
        heightMask = 2;
        if (y < -3000) {
            heightMask = 1;
        }
    }
    for (i = 0; i < 0x11; i++) {
        enemy = work->enemies[i];
        if (enemy != NULL) {
            bits = D_actor_503500_8016E910[i];
            if ((bits & dirMask) != dirMask || gGameSession->eventState != 0) {
                enemy->node.flags            |= 1;
                work->enemies[i]->node.flags &= ~4;
            } else if ((bits & heightMask) != heightMask) {
                enemy->node.flags            |= 1;
                work->enemies[i]->node.flags |= 4;
            } else {
                enemy->node.flags &= ~1;
            }
        }
    }
}

/// Copies bits 0x80, 2 and 4 of the parent task's model `field_C` onto
/// `arg0`'s model, unless that model is attached to `gGfxViewCoord`.
/// Clearing bit 4 reallocates the model's buffers; setting it writes 2 to
/// `*arg1`.
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1)
{
    TmdObject* obj;
    TmdObject* pobj;
    u16        flags;
    u16        flags2;

    obj = arg0->extra;
    if (obj->coords->sub != &gGfxViewCoord) {
        flags = obj->flags;
        pobj  = arg0->parent->extra;
        if (flags & 0x80) {
            if (!(pobj->flags & 0x80)) {
                obj->flags = flags & ~0x80;
            }
        } else if (pobj->flags & 0x80) {
            obj->flags = flags | 0x80;
        }
        flags2 = obj->flags;
        if (flags2 & 2) {
            if (!(pobj->flags & 2)) {
                obj->flags = flags2 & ~2;
            }
        } else if (pobj->flags & 2) {
            obj->flags = flags2 | 2;
        }
        flags2 = obj->flags;
        if (flags2 & 4) {
            if (!(pobj->flags & 4)) {
                obj->flags = flags2 & ~4;
                Tmd_AllocBuffers(obj);
            }
        } else if (pobj->flags & 4) {
            obj->flags = flags2 | 4;
            *arg1      = 2;
        }
    }
}

/// Applies preset `arg2`: re-seeds the slot array from bank `field_0` when it
/// changes, then sets every slot 1..0x13 to clip `field_4` (blended over
/// `field_C` frames by `func_800B4114` when `field_8` is set and the array was
/// already seeded) and ticks it once, before re-applying the part scales.
s32 func_actor_503500_80135950(Actor503500* arg0, s32 arg1, Actor503500AnimPreset* arg2, s32 arg3)
{
    Actor503500Work* work;
    Actor503500Work* work2;
    TmdObject*       ext;
    s32              i;

    work = arg0->field_1C;
    ext  = arg0->extra;
    if (arg2->field_0 != work->field_7D6) {
        work->field_7D6 = arg2->field_0;
        func_800B3F84((GpAnimCtx*)work, D_actor_503500_8016EAB8[work->field_7D6], ext,
                      work->field_334, (GpAnimSlot*)&work->obj.pos.vz);
        work->field_7D4 = 0;
    }
    work->field_7D5 = arg2->field_4;
    if (arg2->field_8 != 0 && work->field_7D4 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114((GpAnimCtx*)work, i, work->field_7D5, 0, arg2->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_7D5);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    work->field_7D4 = 1;
    work2           = arg0->field_1C;
    if (work2->field_7AC & 0x20) {
        work2->coord504 = arg0->extra->coords[4];
        ScaleMatrix(&work2->coord504.coord, &work2->field_5A4);
    }
    if (work2->field_7AC & 0x800) {
        work2->coord554 = arg0->extra->coords[10];
        ScaleMatrix(&work2->coord554.coord, &work2->field_5B4);
    }
    if (work2->field_7AC & 0x10000) {
        ScaleMatrix(&arg0->extra->coords[16].coord, &work2->field_5C4);
    }
    return 0;
}

/// Enters boss state `state` the way `func_actor_503500_80136048` enters
/// state 2: clears the per-state counters, asks for sub-state 3 and drops the
/// main-executable flag.
static inline void func_actor_503500_SetBossState(Actor503500* arg0, s16 state)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7B0 = state;
    work->field_7DA = 0;
    work->field_7DB = 0;
    work->field_7BC = 0;
    work->field_7BE = 0;
    func_actor_503500_80137074(arg0, 0, 3);
    D_80071090[0] = 0;
}

/// Boss message handler. Modes 0/1/2 enter states 0/5/7, mode 3 advances the
/// task state, mode 4 saves model part 0's coordinate and `field_7B6` before
/// entering state 6, and mode 5 restores both.
s32 func_actor_503500_80135B74(Actor503500* arg0, s32 arg1, Actor503500ModeMsg* msg)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;

    switch (msg->mode) {
        case 0:
            func_actor_503500_SetBossState(arg0, 0);
            break;
        case 1:
            func_actor_503500_SetBossState(arg0, 5);
            break;
        case 2:
            func_actor_503500_SetBossState(arg0, 7);
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            work            = arg0->field_1C;
            coord           = arg0->extra->coords;
            work->field_7D0 = work->field_7B6;
            work->coord4B4  = *coord;
            coord->flg      = 0;
            func_actor_503500_SetBossState(arg0, 6);
            break;
        case 5:
            work            = arg0->field_1C;
            coord           = arg0->extra->coords;
            work->field_7B6 = work->field_7D0;
            *coord          = work->coord4B4;
            coord->flg      = 0;
            work->field_7E3 = 1;
            break;
    }
    return 0;
}

/// Clears slot `arg1` of the boss work block's `enemies` array. `arg0` is
/// loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135CE8(Task* arg0, s32 arg1)
{
    D_actor_503500_80176574.enemies[arg1] = NULL;
}

/// Spawns table entry `arg1` as a child of `arg0`'s enemy, tints its model
/// from the current area's record and parks it in slot `arg1` of the boss
/// work block's `enemies` array. Returns the new enemy, or NULL.
GpEnemy* func_actor_503500_80135D00(Actor503500* arg0, s32 arg1)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    GpEnemy*     enemy;
    TmdObject*   model;
    s32          idx;
    u32          raw;
    /* Taken before the spawn call: the ROM keeps the address in s4 across
       every call rather than rebuilding it at the store. */
    Actor503500Work* work = &D_actor_503500_80176574;

    enemy = Gp_SpawnEnemyFromTable(&D_actor_503500_8016E924, arg1, arg1, arg0->field_20);
    if (enemy != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = arg0->field_20->placeKey;
        model      = (TmdObject*)enemy->task->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = sessionKey->view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
        work->enemies[arg1] = enemy;
    }
    return enemy;
}

/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32 func_actor_503500_80135E04(Task* arg0, s32 arg1)
{
    return D_actor_503500_80176574.enemies[arg1] == NULL;
}

/// Sets the scale of boss part `arg1` (5, 11 or 16) from `arg2` and marks it
/// in `field_7AC` for `func_actor_503500_80136DDC`. Parts 5 and 11 also seed
/// the private copy of model part 4 / 10 and link it from the next part's
/// `sub`; any other `arg1` only sets its bit.
void func_actor_503500_80135E20(Actor503500* arg0, s32 arg1, SVECTOR* arg2)
{
    Actor503500Work* work = &D_actor_503500_80176574;

    switch (arg1) {
        case 5:
            work->field_5A4.vx         = arg2->vx;
            work->field_5A4.vy         = arg2->vy;
            work->field_5A4.vz         = arg2->vz;
            work->coord504             = arg0->extra->coords[4];
            arg0->extra->coords[5].sub = &work->coord504;
            break;
        case 11:
            work->field_5B4.vx          = arg2->vx;
            work->field_5B4.vy          = arg2->vy;
            work->field_5B4.vz          = arg2->vz;
            work->coord554              = arg0->extra->coords[10];
            arg0->extra->coords[11].sub = &work->coord554;
            break;
        case 16:
            work->field_5C4.vx = arg2->vx;
            work->field_5C4.vy = arg2->vy;
            work->field_5C4.vz = arg2->vz;
            break;
    }
    work->field_7AC |= 1 << arg1;
}

/// Records the per-slot halfword for slot `arg1` of the boss work block.
/// `arg0` is loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2)
{
    D_actor_503500_80176574.field_730[arg1] = arg2;
}

/// Sets the per-slot playback rate `GpAnimSlot.rate` on animation slots 1..16 of the
/// boss block -- `rate` of 0 meaning `Gp_AnimResetSlot`'s own 0x10 default,
/// exactly as `func_actor_503500_80137048` does -- then applies preset `arg1`.
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 rate)
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
        slot->rate = rate;
        slot++;
    }
    func_actor_503500_80135950(arg0, 0x7D3, &D_actor_503500_8016EAC0[arg1], 0);
}

/// Reports whether the boss is in sub-state `arg1` and, if so, whether either
/// of the 0x102 bits of its state flag halfword is set. Returns -1 for any
/// other sub-state. `arg0` is loaded by every caller but the body ignores it,
/// the same way `func_actor_503500_80135E04` does.
s32 func_actor_503500_80136014(Actor503500* arg0, s32 arg1)
{
    if (arg1 != D_actor_503500_80176574.field_7D5) {
        return -1;
    }
    return (D_actor_503500_80176574.slot40.boss.flags_4C & 0x102) != 0;
}

/// Puts the boss into state 2: clears the state's step counters and the two
/// per-state halfwords, asks `func_actor_503500_80137074` for sub-state 3 and
/// drops the main-executable flag.
void func_actor_503500_80136048(Actor503500* arg0)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7B0 = 2;
    work->field_7DA = 0;
    work->field_7DB = 0;
    work->field_7BC = 0;
    work->field_7BE = 0;
    func_actor_503500_80137074(arg0, 0, 3);
    D_80071090[0] = 0;
}

/// Reports whether the boss-wide gate is open; the body ignores its
/// argument, and callers pass unrelated pointers they already hold.
s32 func_actor_503500_8013608C(void* arg0)
{
    return (u32)(D_actor_503500_80176D24 - 2) < 3U;
}

void func_actor_503500_801360A4(s32 arg0, s16 arg1)
{
    D_actor_503500_80176D64[arg0] = arg1;
}

/// Tries to claim `arg1` counts for slot `arg0`: sums every *other* slot's
/// counter plus the requested amount and, if the total stays under 9, writes
/// the request into the slot. Returns whether it was granted.
s32 func_actor_503500_801360BC(s32 arg0, s32 arg1)
{
    s32  accepted;
    s32  total;
    s32  i;
    u16* slot;

    accepted = 0;
    total    = arg1;
    i        = 0;
    slot     = D_actor_503500_80176D64;
    do {
        if (i != arg0) {
            total += (s16)*slot;
        }
        i++;
        slot++;
    } while (i < 0x12);

    if (total < 9) {
        D_actor_503500_80176D64[arg0] = (u16)arg1;
        accepted                      = 1;
    }
    return accepted;
}

void func_actor_503500_8013611C(s32 arg0)
{
    D_actor_503500_80176D64[arg0] = 0;
}

/// Yaw from the actor's first part to `D_80073B8C`'s translation, relative to
/// the part's own heading, wrapped into [-0x800, 0x800).
s16 func_actor_503500_80136134(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s16            angle;

    coord  = arg0->extra->coords;
    vec.vx = D_80073B8C->t[0] - coord->coord.t[0];
    vec.vy = 0;
    vec.vz = D_80073B8C->t[2] - coord->coord.t[2];
    angle  = ratan2(vec.vx, vec.vz) - ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    return angle;
}

s32 func_actor_503500_80136208(void)
{
    return D_actor_503500_80176D5A;
}

s16 func_actor_503500_80136218(void)
{
    return D_actor_503500_80176D2E;
}

/// Exit callback of the boss task: tears down the second body part's display
/// node, clears the enemy's `recs` back-pointer slot and destroys it.
void func_actor_503500_80136228(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_80136B64(arg0, 0, 1);
    Gp_UnlinkObj(&arg0->field_1C->field_5D4);
    enemy->recs    = 0;
    arg0->field_1C = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

void func_actor_503500_80136280(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->reactionFlags;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->reactionFlags = flags & 0xFE;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        func_actor_503500_80136EFC(arg0, 3);
        work->field_7B2 = 3;
    }
    flags = enemy->reactionFlags;
    if (flags & 0xC) {
        enemy->reactionFlags = flags & 0xF3;
    }
}

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

    vec.vx = arg0->extra->coords->workm.t[0];
    vec.vy = arg0->extra->coords->workm.t[1];
    vec.vz = arg0->extra->coords->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

/// Rebuilds the live vector set `D_80183EEC` from its template in the world
/// frame of the actor's second attach coordinate. `arg1` also recopies the
/// four `field_C` records; `arg2` raises the offset by 0x1F40 in Y.
void func_actor_503500_80136B64(Actor503500* arg0, s32 arg1, s32 arg2)
{
    MATRIX             mtx;
    SVECTOR            ofs;
    s32                i;
    SVECTOR*           src;
    SVECTOR*           dst;
    Actor503500VecSet* out = &D_80183EEC;
    Actor503500VecSet* in  = &D_actor_503500_8016F03C;

    if (arg1 != 0) {
        for (i = 0; i < 4; i++) {
            out->field_C[i] = in->field_C[i];
        }
    }
    Gp_ComposeParentWorld(&arg0->extra->coords[1], &mtx, &ofs);
    if (arg2 != 0) {
        ofs.vy += 0x1F40;
    }
    gte_SetRotMatrix(&mtx);
    dst = out->field_4;
    src = in->field_4;
    for (i = 0; i < 4; i++, dst++, src++) {
        gte_ldv0(src);
        gte_rtv0();
        gte_stsv(dst);
    }
    dst = out->field_8;
    src = in->field_8;
    for (i = 0; i < 8; i++, dst++, src++) {
        gte_ldv0(src);
        gte_rtv0();
        gte_stsv(dst);
        dst->vx += ofs.vx;
        dst->vy += ofs.vy;
        dst->vz += ofs.vz;
    }
}

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

/// Re-applies the boss's per-part scales: for each enabled bit of `field_7AC`,
/// refreshes the private copy of model part 4 or 10 and scales it, and for
/// 0x10000 scales model part 16 in place.
void func_actor_503500_80136DDC(Actor503500* arg0)
{
    Actor503500Work* work;

    work = arg0->field_1C;
    if (work->field_7AC & 0x20) {
        work->coord504 = arg0->extra->coords[4];
        ScaleMatrix(&work->coord504.coord, &work->field_5A4);
    }
    if (work->field_7AC & 0x800) {
        work->coord554 = arg0->extra->coords[10];
        ScaleMatrix(&work->coord554.coord, &work->field_5B4);
    }
    if (work->field_7AC & 0x10000) {
        ScaleMatrix(&arg0->extra->coords[16].coord, &work->field_5C4);
    }
}

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
    D_80071090[0] = 0;
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

/// Sets `GpAnimSlot.rate` -- the per-slot value `Gp_AnimResetSlot` seeds
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
        slot->rate = rate;
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
/// `func_actor_503500_80132508` does. It then keeps two derived copies in the work
/// block -- the yaw recovered from the matrix it just built, and the same
/// translation in 16.16 fixed point. Clearing `flg` makes `_gpUpdateCoordTree`
/// recompute the world matrix from the new local one.
s32 func_actor_503500_80137088(Actor503500* arg0, s32 arg1, Actor503500PlaceArgs* args)
{
    Actor503500Work*  work;
    Actor503500Coord* coord;

    work              = arg0->field_1C;
    coord             = (Actor503500Coord*)arg0->extra->coords;
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
            ext->flags = (ext->flags | 0x80) & ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags               |= 0x80;
            arg0->field_1C->field_7D9 = mode;
            ext->flags               |= 4;
            break;
        case 3:
            ext->flags = (ext->flags & ~0x80) | 4;
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

/// `Task::state` handlers `func_actor_503500_801384D4` dispatches through.
const TaskFuncTable3 D_actor_503500_80131F4C = {
    {
        func_actor_503500_801372C8,
        func_actor_503500_8013815C,
        func_actor_503500_80138288,
    },
};

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
    coord     = tmd->coords;
    parentTmd = parent->extra;
    Mem_Set(&D_actor_503500_80176D88, 0, 0x160);
    arg0->field_1C = &D_actor_503500_80176D88;

    coord->sub        = &((TmdObject*)parent->extra)->coords[8];
    coord->coord.t[0] = D_actor_503500_8016F060.vx;
    coord->coord.t[1] = D_actor_503500_8016F060.vy;
    coord->coord.t[2] = D_actor_503500_8016F060.vz;
    tmd->lightMtx     = parentTmd->lightMtx;
    tmd->colorMtx     = parentTmd->colorMtx;
    tmd->otOffset     = 0x13;
    coord->flg        = 0;

    D_actor_503500_80176D88.field_15E = -1;
    enemy->field_4                    = &coord->coord;
    enemy->field_48                   = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord      = coord;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = D_actor_503500_8016F068.vx;
    enemy->bodyPos.vy = D_actor_503500_8016F068.vy;
    enemy->bodyPos.vz = D_actor_503500_8016F068.vz;
    rec               = D_actor_503500_80176D88.rec60;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;
    enemy->hp         = enemy->param->hpMax;

    D_actor_503500_80176D88.slot40.obj.coord    = coord;
    D_actor_503500_80176D88.slot40.obj.ctx.recs = rec;
    D_actor_503500_80176D88.slot40.obj.key      = 0x30023;
    D_actor_503500_80176D88.slot40.obj.radius   = 0x320;
    D_actor_503500_80176D88.slot40.obj.flags    = 1;
    D_actor_503500_80176D88.slot40.obj.pos.vx   = D_actor_503500_8016F068.vx;
    D_actor_503500_80176D88.slot40.obj.pos.vy   = D_actor_503500_8016F068.vy;
    D_actor_503500_80176D88.slot40.obj.pos.vz   = D_actor_503500_8016F068.vz;
    Gp_LinkObj(2, &D_actor_503500_80176D88.slot40.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_80176D88.field_120.spawnArgLo = 0x400;
    D_actor_503500_80176D88.field_120.coord      = coord;
    D_actor_503500_80176D88.field_120.spawnArgHi = 3;
    D_actor_503500_80176D88.slot40.obj.flags    |= 0x8000;
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
                        coord             = ((TmdObject*)task->extra)->coords;
                        coord->sub        = arg0->extra->coords;
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

/// Death state of the 0x160 enemy at `D_actor_503500_80176D88`: phase 0
/// unlinks the enemy node and clears the 16.16 `rot` / `vel` / `pos`; phase 2
/// steps `rot.vx` down for 31 frames, then re-parents the coordinate onto the
/// view in world space, points `vel` along its Z axis and plays 0x40230004;
/// phases 3/4 accelerate `vel.vy`, and phase 4 fires the light and sound cues
/// on frames 10/30 and leaves on frame 40. Every frame the angles and position
/// are applied to the coordinate, and every even frame sprays an effect from
/// `D_actor_503500_8016F078`.
void func_actor_503500_80137678(Actor503500* arg0)
{
    SVECTOR              rot;
    Actor503500IdentMat  m;
    Actor503500MatWords* ident;
    Actor503500Work*     work;
    GpEnemy*             enemy;
    GsCOORDINATE2*       coord;
    s32*                 src;
    s32*                 out;
    s32                  i;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    coord = arg0->extra->coords;
    switch (work->field_15D) {
        case 0:
            work->slot40.obj.flags &= 0x7FFF;
            enemy->recs             = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_158 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            enemy->reactionFlags &= 0xF0;
            work->rot.vx.w        = 0;
            work->rot.vy.w        = 0;
            work->rot.vz.w        = 0;
            work->vel.vx.w        = 0;
            work->vel.vy.w        = 0;
            work->vel.vz.w        = 0;
            work->pos.vx.w        = 0;
            work->pos.vy.w        = 0;
            work->pos.vz.w        = 0;
            work->field_15D++;
            break;
        case 1:
            work->field_15D++;
            break;
        case 2:
            work->rot.vx.w -= 0x4000;
            if ((s16)++work->field_15A >= 0x1F) {
                src   = (s32*)&m;
                coord = arg0->extra->coords;
                Gp_ComposeParentWorld(coord, &m.mat, &rot);
                out = (s32*)&coord->coord;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                coord->coord.m[2][2] = m.mat.m[2][2];
                coord->coord.t[0]    = rot.vx;
                coord->coord.t[1]    = rot.vy;
                coord->coord.t[2]    = rot.vz;
                coord->sub           = &gGfxViewCoord;
                work->vel.vx.w       = 0;
                work->vel.vy.w       = 0;
                work->vel.vz.w       = 0x100000;
                ApplyMatrixLV(&m.mat, (VECTOR*)&work->vel, (VECTOR*)&work->vel);
                func_actor_503500_80135D00((Actor503500*)arg0->parent, 0xC);
                Gp_UpdateCoord(coord);
                SndEvt_EnqueueType6(0x40230004, (s8)Gp_GetObjPan(coord),
                                    (s8)(gpGetObjDepth(coord) / 2));
                work->field_15A = 0;
                work->field_15D++;
            }
            break;
        case 3:
            work->vel.vy.w += 0x8000;
            if ((s16)++work->field_15A >= 0x1F) {
                work->field_15A = 0;
                work->field_15D++;
            }
            break;
        case 4:
            work->vel.vy.w += 0x8000;
            switch ((s16)work->field_15A) {
                case 10:
                    arg0->extra->flags |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    SndEvt_EnqueueType6(0xD, (s8)Gp_GetObjPan(coord),
                                        (s8)(gpGetObjDepth(coord) / 2));
                    break;
                case 30:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 40:
                    SndEvt_EnqueueType7(0xD, 1);
                    arg0->state++;
                    break;
            }
            work->field_15A++;
            break;
        default:
            arg0->state++;
            break;
    }
    rot.vx          = work->rot.vx.w >> 16;
    rot.vy          = work->rot.vy.w >> 16;
    rot.vz          = work->rot.vz.w >> 16;
    m.ident.m00_m01 = 0x1000;
    m.ident.m02_m10 = 0;
    ident           = &m.ident;
    ident->m11_m12  = 0x1000;
    m.ident.m20_m21 = 0;
    ident->m22      = 0x1000;
    RotMatrix(&rot, &m.mat);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&m.mat);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv((char*)&m.mat + 2);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 2);
    gte_ldclmv((char*)&m.mat + 4);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 4);
    work->pos.vx.w    += work->vel.vx.w;
    work->pos.vy.w    += work->vel.vy.w;
    work->pos.vz.w    += work->vel.vz.w;
    coord->coord.t[0] += work->pos.vx.h.hi;
    coord->coord.t[1] += work->pos.vy.h.hi;
    coord->coord.t[2] += work->pos.vz.h.hi;
    work->pos.vx.w     = (u16)work->pos.vx.w;
    work->pos.vy.w     = (u16)work->pos.vy.w;
    work->pos.vz.w     = (u16)work->pos.vz.w;
    coord->flg         = 0;
    if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
        switch ((u32)D_80070F70 % 6) {
            case 0:
            case 2:
            case 4:
                Gp_SpawnEff(0x60070, coord, 0xB0008600,
                            &D_actor_503500_8016F078[(s8)(work->field_15F++ % 3)]);
                break;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->field_15D >= 3) {
        SndEvt_EnqueueType7(0xD, 1);
        arg0->state = 2;
    }
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy, like `func_actor_503500_8013EE5C`: each attack id is taken once,
/// only type-2 ids land while the `field_158` countdown is clear, and a hit
/// that empties `field_40` starts state 2 but still applies the id's status
/// effect. The hit effect is pulled to 800 units along the contact offset.
/// `arg1` is passed by the caller but unused.
void func_actor_503500_80137C90(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
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
    coord = arg0->extra->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_158 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
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
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_80138490(arg0, 2);
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
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x320000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F068.vx;
        pos.vy += D_actor_503500_8016F068.vy;
        pos.vz += D_actor_503500_8016F068.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_120);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_158 < stun) {
            work->field_158 = stun;
        }
    next:;
    }
}

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
            if (!(tmd->flags & 0x80)) {
                func_actor_503500_801382F4(arg0);
            }
            break;
        case 2:
            tmd->flags        |= 0x80;
            enemy->node.flags |= 1;
            break;
        default:
            if (enemy->reactionFlags != 0) {
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
    ((GsCOORDINATE2*)arg0->extra->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->slot40.obj);
    enemy->recs    = 0;
    arg0->field_1C = NULL;
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
    flags = obj->reactionFlags;
    if (flags & 1) {
        obj->reactionFlags = flags & 0xFE;
    }
    if (obj->reactionFlags & 2) {
        obj->reactionFlags = obj->reactionFlags & 0xFD;
    }
    flags2 = obj->reactionFlags;
    if (flags2 & 0xC) {
        obj->reactionFlags = flags2 & 0xF3;
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

/// `Task::state` handlers `func_actor_503500_8013AD0C` dispatches through.
const TaskFuncTable3 D_actor_503500_80131F9C = {
    {
        func_actor_503500_8013852C,
        func_actor_503500_80138898,
        func_actor_503500_8013A900,
    },
};

/// State-0 init of the 0x2EC enemies in spawn slots 2 and 3: clears the slot's
/// block in `D_actor_503500_80176EE8`, hangs the task's coordinate off part 1
/// of the parent's model at the slot's rotation, snapshots model parts 1..8,
/// publishes the block's light and colour matrices, and links the enemy node
/// and the display node.
///
/// The identity matrix's first word is stored through the union member and
/// the rest through `ident`: the member store is a fixed-address struct
/// reference, which keeps it in the store chain behind the `coord.t[]` writes
/// so sched1 does not spend an idle slot on it. Through a cast pointer it
/// would win that slot, the `RotMatrix` argument would be placed before the
/// table row add, and `idx * 8` would drag the row pointer into `$s0`.
void func_actor_503500_8013852C(Actor503500* arg0)
{
    GpEnemy*             enemy;
    TmdObject*           tmd;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       part;
    Actor503500Work2EC*  work;
    GpRec18*             rec;
    Actor503500IdentMat  m;
    Actor503500MatWords* ident;
    s32                  idx;
    s32                  i;

    idx   = arg0->spawnArg1 - 2;
    enemy = arg0->field_20;
    work  = &D_actor_503500_80176EE8[idx];
    coord = arg0->extra->coords;
    tmd   = arg0->extra;
    Mem_Set(work, 0, 0x2EC);
    arg0->field_1C = (Actor503500Work*)work;

    coord->sub        = &((TmdObject*)arg0->parent->extra)->coords[1];
    part              = &coord[8];
    coord->coord.t[0] = D_actor_503500_8016F090[idx].vx;
    coord->coord.t[1] = D_actor_503500_8016F090[idx].vy;
    coord->coord.t[2] = D_actor_503500_8016F090[idx].vz;
    m.ident.m00_m01   = 0x1000;
    ident             = &m.ident;
    ident->m02_m10    = 0;
    ident->m11_m12    = 0x1000;
    ident->m20_m21    = 0;
    ident->m22        = 0x1000;
    RotMatrix(&D_actor_503500_8016F0A0[idx], &m.mat);
    MulMatrix0(&coord->coord, &m.mat, &coord->coord);
    coord->flg = 0;
    for (i = 1; i < 9; i++) {
        work->mats[i] = coord[i].coord;
    }
    work->field_2E2 = 0x1000;
    tmd->colorMtx   = &work->color;
    tmd->otOffset   = 0x12;
    tmd->lightMtx   = &work->light;
    coord->flg      = 0;

    work->field_2EB = -1;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord      = part;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = D_actor_503500_8016F0B0.vx;
    enemy->bodyPos.vy = D_actor_503500_8016F0B0.vy;
    enemy->bodyPos.vz = D_actor_503500_8016F0B0.vz;
    rec               = work->rec;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;
    enemy->hp         = enemy->param->hpMax;

    work->obj.coord    = part;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = D_actor_503500_8016F0B0.vx;
    work->obj.pos.vy   = D_actor_503500_8016F0B0.vy;
    work->obj.pos.vz   = D_actor_503500_8016F0B0.vz;
    work->obj.key      = 0x30023;
    work->obj.radius   = 0x320;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_240.spawnArgLo = 0x600;
    work->field_240.coord      = part;
    work->field_240.spawnArgHi = 3;
    work->obj.flags           |= 0x8000;
    work->field_29C.vx         = D_actor_503500_8016F0A8[arg0->spawnArg1].vx;
    work->field_29C.vy         = D_actor_503500_8016F0A8[arg0->spawnArg1].vy;
    work->field_29C.vz         = D_actor_503500_8016F0A8[arg0->spawnArg1].vz;
    work->field_294.vx         = D_actor_503500_8016F0A8[arg0->spawnArg1].vx;
    work->field_294.vy         = D_actor_503500_8016F0A8[arg0->spawnArg1].vy;
    work->field_294.vz         = D_actor_503500_8016F0A8[arg0->spawnArg1].vz;
    work->field_2D0.w          = 0x800000;
    work->field_2E9            = 1;
    arg0->exitCallback         = (TaskFunc)func_actor_503500_8013A900;
    arg0->state               += 1;
}

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
    if (gGameSession->eventState != 0) {
        slot = 0xB;
        if (arg0->spawnArg1 < 3) {
            slot = 0xA;
        }
        if (func_actor_503500_80135E04(arg0->parent, slot) == 0) {
            tmd->flags |= 4;
        } else {
            goto tick;
        }
    } else {
    tick:
        func_actor_503500_80135828(arg0, &work->field_2EB);
    }

    switch (D_801153F4) {
        case 1:
            if (!(tmd->flags & 0x80)) {
                func_actor_503500_8013AAC0(arg0);
            }
            break;
        case 2:
            tmd->flags        |= 0x80;
            enemy->node.flags |= 1;
            break;
        default:
            if (enemy->reactionFlags != 0) {
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

/// Sub-state of the 0x2EC enemies: seeds `field_29C` from the slot's rest
/// offset once, hands over to state 6 below half HP or state 1 on the kill
/// countdown, and otherwise swings `field_29C` around the rest offset by
/// `D_actor_503500_8016F0C8` rotated through `field_2A4`, spinning that angle.
void func_actor_503500_80138A30(Actor503500* arg0)
{
    Actor503500Work2EC*  work;
    Actor503500IdentMat  m;
    Actor503500MatWords* ident;
    SVECTOR              v;
    s32                  idx;
    s16                  hp;

    work = (Actor503500Work2EC*)arg0->field_1C;
    idx  = arg0->spawnArg1 - 2;
    if (work->field_2E4 == 0) {
        work->field_29C.vx = D_actor_503500_8016F0B8[idx].vx;
        work->field_29C.vy = D_actor_503500_8016F0B8[idx].vy;
        work->field_29C.vz = D_actor_503500_8016F0B8[idx].vz;
        work->field_2E4++;
    }
    hp = arg0->field_20->hp;
    if (hp < (D_actor_503500_8016E7EC[arg0->spawnArg1].hpMax >> 1) && hp > 0) {
        func_actor_503500_8013ACC4(arg0, 6);
        return;
    }
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013ACC4(arg0, 1);
        return;
    }
    m.ident.m00_m01 = 0x1000;
    ident           = &m.ident;
    ident->m02_m10  = 0;
    ident->m11_m12  = 0x1000;
    ident->m20_m21  = 0;
    ident->m22      = 0x1000;
    RotMatrix(&work->field_2A4, &m.mat);
    gte_SetRotMatrix(&m.mat);
    gte_ldv0(&D_actor_503500_8016F0C8);
    gte_rtv0();
    gte_stsv(&v);
    work->field_29C.vx  = D_actor_503500_8016F0B8[idx].vx + v.vx;
    work->field_29C.vy  = D_actor_503500_8016F0B8[idx].vy + v.vy;
    work->field_29C.vz  = D_actor_503500_8016F0B8[idx].vz + v.vz;
    work->field_2A4.vx += 0x20;
    work->field_2A4.vy += 0x40;
    work->field_2A4.vz += 0x80;
}

/// Sub-state of the 0x2EC enemies: passes (0x11, 0x10) to the parent through
/// `func_actor_503500_80135FB4`, then waits for `field_2E8`, meanwhile (for up
/// to 90 frames) pointing `field_29C` at the camera target raised by 1000, in
/// the frame `Gp_ComposeParentWorld` composes for `coord->sub`. Ten frames
/// later, if the chain tip `pts[8]` is within 3000 of the target on the ground
/// plane, it spawns `D_actor_503500_8016E9F0` 0x640 along `coord[8]`'s Z axis;
/// ten frames after that it hands over to state 0.
void func_actor_503500_80138C08(Actor503500* arg0)
{
    SVECTOR             pos;
    SVECTOR             ofs;
    MATRIX              m;
    MATRIX              rot;
    MATRIX*             mat;
    Actor503500Work2EC* work;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      dst;
    Task*               task;
    s32*                src;
    s32*                out;
    s32                 dist;
    s32                 i;

    work  = (Actor503500Work2EC*)arg0->field_1C;
    coord = arg0->extra->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013ACC4(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_2E4) {
        case 0:
            work->field_2E8 = 0;
            func_actor_503500_80135FB4((Actor503500*)arg0->parent, 0x11, 0x10);
            work->field_2E4++;
        case 1:
            if (work->field_2E8 != 0) {
                work->field_2E4++;
                return;
            }
            if (++work->field_2DE >= 0x5B) {
                func_actor_503500_8013ACC4(arg0, 0);
                return;
            }
            mat = &m;
            Gp_ComposeParentWorld(coord->sub, mat, &ofs);
            pos.vx = D_80073B8C->t[0] - ofs.vx;
            pos.vy = D_80073B8C->t[1] - ofs.vy - 1000;
            pos.vz = D_80073B8C->t[2] - ofs.vz;
            TRANSPOSE_ROT(mat, &rot);
            gte_SetRotMatrix(&rot);
            gte_ldv0(&pos);
            gte_rtv0();
            gte_stsv(&work->field_29C);
            break;
        case 2:
            if (++work->field_2DE >= 0xB) {
                pos.vx = D_80073B8C->t[0] - work->pts[8].vx;
                pos.vz = D_80073B8C->t[2] - work->pts[8].vz;
                dist   = SquareRoot0(pos.vx * pos.vx + pos.vz * pos.vz);
                if (dist < 3000) {
                    task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 0, 0, dist * 3000);
                    if (task != NULL) {
                        Gp_ComposeParentWorld(&coord[8], &m, &pos);
                        src    = (s32*)&m;
                        dst    = ((TmdObject*)task->extra)->coords;
                        ofs.vx = 0;
                        ofs.vy = 0;
                        ofs.vz = 0x640;
                        gte_SetRotMatrix(src);
                        gte_ldv0(&ofs);
                        gte_rtv0();
                        gte_stsv(&ofs);
                        dst->coord.t[0] = pos.vx + ofs.vx;
                        dst->coord.t[1] = pos.vy + ofs.vy;
                        dst->coord.t[2] = pos.vz + ofs.vz;
                        out             = (s32*)&dst->coord;
                        for (i = 0; i < 4; i++) {
                            *out++ = *src++;
                        }
                        dst->coord.m[2][2] = m.m[2][2];
                    }
                }
                work->field_2DE = 0;
                work->field_2E4++;
            }
            break;
        case 3:
            if (++work->field_2DE >= 0xB) {
                func_actor_503500_8013ACC4(arg0, 0);
            }
            break;
    }
}

/// Death state of the 0x2EC enemies: unlinks the enemy node, waits for
/// `field_2E8`, then re-parents the root coordinate onto the view in world
/// space and plays 0x40230004 at it. Phase 2 eases every part's Euler angles
/// back to rest while the body rises by 10 a frame; past 1000 the pose is saved
/// in `field_2AC` and phase 3 squashes it vertically (`field_2DC`), firing the
/// light, sound and effect cues on frames 10/15/30 and leaving on frame 40.
/// Every twelfth frame of phases 0..2 sprays effects along parts 8..1.
void func_actor_503500_80139014(Actor503500* arg0)
{
    MATRIX              m;
    VECTOR              scale;
    SVECTOR             rot;
    Actor503500Work2EC* work;
    GpEnemy*            enemy;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      part;
    s16*                p;
    s32                 phase;
    s32                 i;
    s32                 j;

    work  = (Actor503500Work2EC*)arg0->field_1C;
    enemy = arg0->field_20;
    phase = work->field_2E4;
    coord = arg0->extra->coords;
    switch (phase) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->recs      = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_2D8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048((Actor503500*)arg0->parent);
            enemy->reactionFlags &= 0xF0;
            work->field_29C.vy    = 0x1388;
            work->field_2D0.w     = 0x300000;
            work->field_2E8       = 0;
            work->field_2E4++;
            break;
        case 1:
            if (work->field_2E8 != 0) {
                Gp_ComposeParentWorld(coord, &m, &rot);
                coord->coord      = m;
                coord->coord.t[0] = rot.vx;
                coord->coord.t[1] = rot.vy;
                coord->coord.t[2] = rot.vz;
                coord->sub        = &gGfxViewCoord;
                coord->flg        = 0;
                work->field_2EA   = phase;
                Gp_UpdateCoord(coord);
                SndEvt_EnqueueType6(0x40230004, (s8)Gp_GetObjPan(coord),
                                    (s8)(gpGetObjDepth(coord) / 2));
                work->field_2E4++;
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
                work->field_2AC = coord->coord;
                work->field_2DC = 0x1000;
                work->field_2E4++;
            }
            break;
        case 3:
            if (work->field_2DC > 0x200) {
                work->field_2DC -= 0x20;
            }
            coord->coord = work->field_2AC;
            scale.vx     = 0x1000;
            scale.vy     = work->field_2DC;
            scale.vz     = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            switch (work->field_2DE) {
                case 10:
                    arg0->extra->flags |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    SndEvt_EnqueueType6(0xD, (s8)Gp_GetObjPan(coord),
                                        (s8)(gpGetObjDepth(coord) / 2));
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
            work->field_2DE++;
            break;
    }
    if (func_actor_503500_801360BC(arg0->spawnArg1, 4) != 0 && work->field_2E4 < 3 &&
        (u32)D_80070F70 % 12 == 0) {
        for (i = 8, j = 0; i > 0; i--) {
            Gp_SpawnEff(0x60070, &arg0->extra->coords[i], 0xB0008600, &D_actor_503500_8016F0D0[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->field_2E4 > 0) {
        SndEvt_EnqueueType7(0xD, 1);
        arg0->state = 2;
    }
}

/// Sub-state of the 0x2EC enemies: unlinks the enemy node and steps
/// `field_2E2` down to 0, spawns a pair of 0x60055 effects on the model
/// parts for 26 frames, gives slots 0xD/0xE (spawn slot 2) or 0xF/0x10 an
/// 8-frame kill countdown and half this enemy's `field_40`, then advances
/// the task state 91 frames later.
void func_actor_503500_801395BC(Actor503500* arg0)
{
    SVECTOR             vec;
    Actor503500Work2EC* work;
    GpEnemy*            enemy;
    GpEnemy*            child;
    GsCOORDINATE2*      coord;
    s32                 phase;
    s32                 a;
    s32                 b;

    work  = (Actor503500Work2EC*)arg0->field_1C;
    phase = work->field_2E4;
    enemy = arg0->field_20;
    switch (phase) {
        case 0:
            work->obj.flags &= 0x7FFF;
            Gp_UnlinkNode(&enemy->node);
            enemy->recs     = 0;
            work->field_2D8 = 0;
            work->field_2E4++;
        case 1:
            work->field_2E2 -= 0x20;
            if (work->field_2E2 <= 0) {
                work->field_2E2 = 0;
                work->field_248 = NULL;
                work->field_2E4++;
            }
            break;
        case 2:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 4) != 0) {
                coord  = &arg0->extra->coords[(s16)(work->field_2DE / 3)];
                vec.vx = 0;
                vec.vy = -700;
                vec.vx = (s16)(work->field_2DE % 3) * 33;
                Gp_SpawnEff(0x60055, coord, 0x11101800, &vec);
                vec.vy = 700;
                Gp_SpawnEff(0x60055, coord, 0x11101800, &vec);
            }
            work->field_2DE++;
            if (work->field_2DE >= 0x1A) {
                a = 0xF;
                if (arg0->spawnArg1 == phase) {
                    a = 0xD;
                    b = 0xE;
                } else {
                    b = 0x10;
                }
                child = func_actor_503500_80135D00((Actor503500*)arg0->parent, a);
                if (child != NULL) {
                    child->task->killCountdown = 8;
                    child->hp                  = enemy->hp / 2;
                }
                child = func_actor_503500_80135D00((Actor503500*)arg0->parent, b);
                if (child != NULL) {
                    child->task->killCountdown = 8;
                    child->hp                  = enemy->hp / 2;
                }
                func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
                work->field_2EB = 3;
                work->field_2DE = 0;
                work->field_2E4++;
            }
            break;
        case 3:
            arg0->extra->flags |= 0x84;
            work->field_2DE++;
            if (work->field_2DE >= 0x5B) {
                func_actor_503500_8013611C(arg0->spawnArg1);
                arg0->state++;
            }
            break;
    }
}

void func_actor_503500_801398D0(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              dmg;
    u8               flags;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if ((func_actor_503500_80136208() == 0) && (gGameSession->eventState == 0)) {
        flags = enemy->reactionFlags;
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
            func_actor_503500_8013ACC4(arg0, 0);
            work->field_2D6 = 5;
            work->field_2DA = 8;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
        }
        if (enemy->reactionFlags & 0xC) {
            func_actor_503500_8013ACC4(arg0, 4);
            if (Gp_ObjFlag4Expired((GpObj5C*)arg0->field_20) != 0) {
                enemy->reactionFlags &= 0xF3;
                func_actor_503500_8013ACC4(arg0, 0);
            } else {
                dmg = Gp_TickObjFlag4((GpObj5C*)enemy);
                if (dmg != 0) {
                    enemy->hp -= dmg;
                    func_800DA6E8(&enemy->node, dmg, 0);
                    work->field_2DA = 8;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= 0xF3;
                        func_actor_503500_8013ACC4(arg0, 5);
                    } else {
                        func_actor_503500_8013ACC4(arg0, 0);
                    }
                }
            }
        }
    }
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy, like `func_actor_503500_80134EAC`: each attack id is taken once,
/// only type-2 ids land while the `field_2D8` countdown is clear, and a hit
/// that empties `field_40` starts state 5. The hit effect is placed at the
/// record's contact point, pulled to 800 units from part 8 along the offset
/// and rotated into its frame. `arg1` is passed by the caller but unused.
void func_actor_503500_80139A20(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    SVECTOR             pos;
    MATRIX              rot;
    MATRIX              mtx;
    VECTOR              d;
    Actor503500Work2EC* work;
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
    work  = (Actor503500Work2EC*)arg0->field_1C;
    coord = &arg0->extra->coords[8];
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_2D8 != 0) {
            continue;
        }
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        src  = Gp_ActorSlots[(id >> 7) & 1]->extra->coords;
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
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013ACC4(arg0, 5);
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
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x320000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F0B0.vx;
        pos.vy += D_actor_503500_8016F0B0.vy;
        pos.vz += D_actor_503500_8016F0B0.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_240);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_2D8 < stun) {
            work->field_2D8 = stun;
        }
    next:;
    }
}

/// Steers `field_294` toward `field_29C`. Inside the arrival distance (the
/// integer half of `field_2D0`) it sets `field_2E8` and stops; otherwise the
/// speed `field_2CC` accelerates toward +/-`field_2D0` while `field_2E9` is
/// set, or decays to 0, and moves `field_294` along the normalized offset
/// (at a quarter speed while `field_2DA` runs).
void func_actor_503500_80139EFC(Actor503500* arg0)
{
    SVECTOR             d;
    SVECTOR             n;
    VECTOR              step;
    Actor503500Work2EC* work;
    s16                 tx;
    s16                 ty;
    s16                 tz;
    s32                 lim;
    s32                 speed;
    s32                 k;

    work = (Actor503500Work2EC*)arg0->field_1C;
    tx   = work->field_29C.vx - work->field_294.vx;
    d.vx = tx;
    ty   = work->field_29C.vy - work->field_294.vy;
    d.vy = ty;
    tz   = work->field_29C.vz - work->field_294.vz;
    d.vz = tz;
    if (ABS(tx) + ABS(ty) + ABS(tz) < work->field_2D0.h.hi) {
        work->field_2E8 = 1;
        return;
    }
    lim             = work->field_2D0.w;
    work->field_2E8 = 0;
    if (work->field_2E9 != 0) {
        speed = work->field_2CC + lim / 32;
        if (speed > 0) {
            if (speed > lim) {
                speed = lim;
            }
        } else if (speed < -lim) {
            speed = -lim;
        }
    } else {
        speed = work->field_2CC - lim / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->field_2CC = speed;
    VectorNormalSS(&d, &n);
    if (work->field_2DA != 0) {
        speed >>= 2;
    }
    k                   = speed >> 12;
    step.vx             = n.vx * k;
    step.vy             = n.vy * k;
    step.vz             = n.vz * k;
    work->field_294.vx += step.vx >> 16;
    work->field_294.vy += step.vy >> 16;
    work->field_294.vz += step.vz >> 16;
}

/// Builds the chain polyline `pts[0..8]` from cubic Bezier segments
/// (`func_actor_503500_8013A7B0`): a first curve runs from the root's world
/// position, through a point 1000 units along its Z axis, to the parent-local
/// `field_294` point raised in Y; `pts[1..5]` and `pts[6..8]` are then sampled
/// from two curves re-seeded from that first one. `func_actor_503500_8013A470`
/// re-aims the links along the result, and `phase` advances by 0x80.
void func_actor_503500_8013A0D0(Actor503500* arg0)
{
    SVECTOR             ctrl[4];
    SVECTOR             ofs;
    SVECTOR             tmp;
    VECTOR              out[9];
    VECTOR              v;
    MATRIX              m;
    GsCOORDINATE2*      coord;
    Actor503500Work2EC* work;
    s32                 i;

    coord = arg0->extra->coords;
    work  = (Actor503500Work2EC*)arg0->field_1C;
    Gp_ComposeParentWorld(coord, &m, &ctrl[0]);
    work->pts[0].vx = ctrl[0].vx;
    work->pts[0].vy = ctrl[0].vy;
    work->pts[0].vz = ctrl[0].vz;
    ofs.vx          = 0;
    ofs.vy          = 0;
    ofs.vz          = 1000;
    gte_SetRotMatrix(&m);
    gte_ldv0(&ofs);
    gte_rtv0();
    gte_stsv(&ctrl[1]);
    ctrl[1].vx += ctrl[0].vx;
    ctrl[1].vy += ctrl[0].vy;
    ctrl[1].vz += ctrl[0].vz;
    Gp_ComposeParentWorld(coord->sub, &m, &tmp);
    gte_SetRotMatrix(&m);
    gte_ldv0(&work->field_294);
    gte_rtv0();
    gte_stsv(&ofs);
    tmp.vx    += ofs.vx;
    tmp.vy    += ofs.vy;
    tmp.vz    += ofs.vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy - 3000;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy - 2000;
    ctrl[3].vz = tmp.vz;
    for (i = 8; i >= 0; i--) {
        func_actor_503500_8013A7B0(ctrl, &ctrl[3], 9, i, &out[i].vx);
    }
    ctrl[0].vx = out[8].vx;
    ctrl[0].vy = out[8].vy;
    ctrl[0].vz = out[8].vz;
    ctrl[1].vx = out[6].vx;
    ctrl[1].vy = out[6].vy + 1000;
    ctrl[1].vz = out[6].vz;
    ctrl[2].vx = out[5].vx;
    ctrl[2].vy = out[5].vy - 2000;
    ctrl[2].vz = out[5].vz;
    ctrl[3].vx = out[4].vx;
    ctrl[3].vy = out[4].vy - 2000;
    ctrl[3].vz = out[4].vz;
    for (i = 4; i >= 0; i--) {
        func_actor_503500_8013A7B0(ctrl, &ctrl[3], 5, i, &v.vx);
        copyVector(&work->pts[5 - i], &v);
    }
    ctrl[0].vx = out[4].vx;
    ctrl[0].vy = out[4].vy - 2000;
    ctrl[0].vz = out[4].vz;
    ctrl[1].vx = out[3].vx;
    ctrl[1].vy = out[3].vy - 2000;
    ctrl[1].vz = out[3].vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy - 1000;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy;
    ctrl[3].vz = tmp.vz;
    for (i = 2; i >= 0; i--) {
        func_actor_503500_8013A7B0(ctrl, &ctrl[3], 16, i + 12, &v.vx);
        copyVector(&work->pts[8 - i], &v);
    }
    func_actor_503500_8013A470(work->pts, arg0->extra->coords, work->phase);
    work->phase = (work->phase + 0x80) & 0xFFF;
}

/// Scaled variant of `func_actor_503500_8014176C`: re-aims the eight child
/// coordinates along `pts[0..8]`, normalising each basis with `MatrixNormal`,
/// and from the second link on sets the translation to the local segment
/// scaled by `0x1000 + rsin(phase) / 64` (a 1/64 pulse).
void func_actor_503500_8013A470(SVECTOR* pts, GsCOORDINATE2* coords, s32 phase)
{
    Actor503500ChainScratch* s;
    SVECTOR*                 dir;
    s32                      scale;
    s32                      i;
    s32                      j;

    s        = (Actor503500ChainScratch*)(SCRATCH_SP -= sizeof(Actor503500ChainScratch));
    s->up.vx = 0;
    s->up.vy = 0x1000;
    s->up.vz = 0;
    Gp_ComposeParentWorld(coords->sub, &s->world, &s->rot);
    scale = ((rsin(phase) << 6) >> 12) + 0x1000;
    for (i = 0, j = 1; i < 8; i++, j++) {
        s->diff.vx = pts[j].vx - pts[i].vx;
        s->diff.vy = pts[j].vy - pts[i].vy;
        s->diff.vz = pts[j].vz - pts[i].vz;
        gte_SetRotMatrix(&s->world);
        dir = &s->dir;
        gte_ldclmv(&coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->world);
        gte_ldclmv((char*)&coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->world + 2);
        gte_ldclmv((char*)&coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->world + 4);
        TRANSPOSE_ROT(&s->world, &s->inv);
        gte_SetRotMatrix(&s->inv);
        gte_ldv0(&s->diff);
        gte_rtv0();
        gte_stlvnl(&s->pos);
        VectorNormalS(&s->pos, dir);
        Gfx_OrthonormalBasis(&s->basis, dir, &s->up);
        MatrixNormal(&s->basis, &coords[j].coord);
        if (i != 0) {
            coords[j].coord.t[0] = (s->pos.vx * scale) >> 12;
            coords[j].coord.t[1] = (s->pos.vy * scale) >> 12;
            coords[j].coord.t[2] = (s->pos.vz * scale) >> 12;
        }
    }
    SCRATCH_SP += sizeof(Actor503500ChainScratch);
}

/// Evaluates a cubic Bezier segment at frame `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_actor_503500_8013A7B0(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
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
        func_actor_503500_8013AC6C(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_actor_503500_8013AC6C(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_actor_503500_8013AC6C(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

void func_actor_503500_8013A900(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)arg0->extra->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&arg0->field_1C->obj160);
    enemy->recs    = 0;
    arg0->field_1C = NULL;
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

    vec.vx = arg0->extra->coords->workm.t[0];
    vec.vy = arg0->extra->coords->workm.t[1];
    vec.vz = arg0->extra->coords->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

/// Blends model parts 1..8 toward the 0x2EC block's private copies in `mats`:
/// while `field_2E2` is below 0x1000, each part's `coord` rotation goes through
/// `Gp_LerpOrthonormal` and its translation keeps a `field_2E2 / 0x1000` share
/// of its offset from the copy.
void func_actor_503500_8013AB38(Actor503500* arg0)
{
    VECTOR              d;
    Actor503500Work2EC* work;
    GsCOORDINATE2*      coord;
    MATRIX*             mat;
    s32                 t;
    s32                 i;

    work  = (Actor503500Work2EC*)arg0->field_1C;
    coord = arg0->extra->coords + 1;
    if (work->field_2E2 < 0x1000) {
        mat = &work->mats[1];
        t   = work->field_2E2;
        for (i = 1; i < 9; i++) {
            Gp_LerpOrthonormal(mat, &coord->coord, &coord->coord, t);
            d.vx              = ((coord->coord.t[0] - mat->t[0]) * t) >> 12;
            d.vy              = ((coord->coord.t[1] - mat->t[1]) * t) >> 12;
            d.vz              = ((coord->coord.t[2] - mat->t[2]) * t) >> 12;
            coord->coord.t[0] = mat->t[0] + d.vx;
            coord->coord.t[1] = mat->t[1] + d.vy;
            coord->coord.t[2] = mat->t[2] + d.vz;
            mat++;
            coord++;
        }
    }
}

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
