#ifndef ACTOR_356100_H
#define ACTOR_356100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// One XZ pair of `Actor356100Work::field_C`; same shape as
/// `Actor01900Waypoint`.
typedef struct Actor356100Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor356100Waypoint;

/// 8-byte row of the `D_actor_356100_8016A994` table `func_actor_356100_8016382C`
/// picks the re-entry pair from on the spawn argument; the same role
/// `Actor01900TintRow` has for `Actor01900_D0AC64`.
typedef struct Actor356100TintRow {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor356100TintRow;
STATIC_ASSERT_SIZEOF(Actor356100TintRow, 0x8);

/// Head of the work block this overlay hangs off `Task::idMap`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_0` / `field_5A` / `field_68` are the
/// same state, clip-id and flag halfwords `Actor01900Work` keeps at those
/// offsets; `field_974` is the `field_5A & 0x3FF` snapshot
/// `func_actor_356100_8016A468` stores (same role as `Actor01900Work.field_8B4`).
/// The halfwords at 0x978..0x982 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes at 0x898..0x8A2; `field_984` is the halfword
/// `func_actor_356100_8016A5DC` snapshots into `field_982`, where
/// `Actor01900Work.field_8A4` sits, and `field_98E` / `field_990` the zero-pair
/// that function clears at +0x8AE / +0x8B0. `field_9BC` holds the 0x180 it
/// writes; `Actor01900_Fn0AA78` puts the same constant in the neighbouring
/// `GpObj` field it names `field_8C8.field_1C` (+0x8E4), not this one.
/// `field_B5C` / `field_B60` are the two helper tasks the exit callback
/// kills; same pair as `Actor01900Work` at +0xC38 / +0xC3C, without the three
/// `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Countdown `func_actor_356100_8016A668` decrements every frame and
    /// tests with `(s16)` — the 0x0F / 0x10 state it picks when the counter
    /// wraps is the transition into the state 0xB / 0xC clip it is running.
    /// Signed, like `Actor01900Work.field_6` / `Actor401300Work.field_6`;
    /// `func_actor_356100_80167584` reads the same slot as a `u16` when it
    /// increments it, so it casts there.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[4];
    /// The two XZ pairs `func_actor_356100_8016382C` seeds on entering state
    /// 0x10: the model root's X/Z, then the same pair pushed one normalised
    /// unit along the facing. Same role as `Actor01900Work.field_C`.
    /* 0x00C */ Actor356100Waypoint field_C[2];
    /// Zeroed on the same entry, next to the pair above; same slot as
    /// `Actor01900Work.field_14`.
    /* 0x014 */ s16  field_14;
    /* 0x016 */ byte pad_16[0x44];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x90A];
    /* 0x974 */ s32  field_974;
    /* 0x978 */ s16  field_978;
    /* 0x97A */ s16  field_97A;
    /* 0x97C */ byte pad_97C[2];
    /* 0x97E */ s16  field_97E;
    /* 0x980 */ byte pad_980[2];
    /* 0x982 */ s16  field_982;
    /* 0x984 */ s16  field_984;
    /* 0x986 */ byte pad_986[8];
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[2];
    /// Snapshot of `field_5A & 0x3FF` `func_actor_356100_80167818` tests
    /// before re-storing it, so the clip-4 branch fires once per change.
    /// Distinct from `field_974`, which the state 0xE tick snapshots.
    /* 0x994 */ s32  field_994;
    /* 0x998 */ byte pad_998[0x24];
    /* 0x9BC */ s16  field_9BC;
    /* 0x9BE */ byte pad_9BE[2];
    /// First `GpRec18` of the body-part record table, the address
    /// `func_actor_356100_8016382C` publishes in the enemy's `field_54` slot
    /// and the exit callback drops. Same slot as `Actor01900Work.field_8E8`.
    /* 0x9C0 */ GpRec18 field_9C0;
    /* 0x9D8 */ byte    pad_9D8[0x100];
    /// Light matrix `func_actor_356100_8016382C` binds to the model's
    /// `TmdObject::field_1C` (the color matrix is `field_AF8`).
    /* 0xAD8 */ MATRIX field_AD8;
    /// Color matrix bound to the model's `TmdObject::field_20`.
    /* 0xAF8 */ MATRIX field_AF8;
    /// Copy `func_actor_356100_80167584` saves `field_AF8` into when it enters
    /// its state; same role as `Actor401300Work.field_C68` has for `field_C48`.
    /* 0xB18 */ MATRIX field_B18;
    /* 0xB38 */ byte   pad_B38[2];
    /// Threshold `func_actor_356100_8016A834` tests once the enemy is still
    /// alive (`field_40 > 0`) to choose clip 4 or 0x11; the analogue of the
    /// `Actor00100Ctx.field_4C & 2` bit `Actor00100_Fn0BB2C` tests there.
    /* 0xB3A */ s16  field_B3A;
    /* 0xB3C */ byte pad_B3C[0x18];
    /// Random reload `func_actor_356100_8016A668` adds a 4-bit `Gp_LcgState`
    /// draw to when the work block's `field_4` flag is set.
    /* 0xB54 */ u16 field_B54;
    /// Second half of the pair: the re-entry delay `func_actor_356100_8016382C`
    /// copies off `D_actor_356100_8016A994` next to `field_B54`.
    /* 0xB56 */ u16 field_B56;
    /// Copy of the first three bytes of the last event
    /// `func_actor_356100_8016A0B8` handled.
    /* 0xB58 */ u8    field_B58[3];
    /* 0xB5B */ byte  pad_B5B;
    /* 0xB5C */ Task* field_B5C;
    /* 0xB60 */ Task* field_B60;
    /* 0xB64 */ byte  pad_B64[0x58];
    /// Zeroed on the state-0x10 entry below the matrices; same tail slot as
    /// `Actor01900Work.field_C98`.
    /* 0xBBC */ s16  field_BBC;
    /* 0xBBE */ byte pad_BBE[2];
} Actor356100Work;
STATIC_ASSERT_SIZEOF(Actor356100Work, 0xBC0);

/// Event record `func_actor_356100_8016A0B8` dispatches on: the first three
/// bytes are copied raw into `Actor356100Work::field_B58`, `w[0]` is the
/// event kind and `w[1]` its sub-code. Same shape as `Actor401300Event`.
typedef union Actor356100Event {
    u8  b[3];
    u16 w[2];
} Actor356100Event;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401000`. The sibling teardown
/// `func_actor_356100_8016A158` reaches those same slots as a `Task*`.
typedef struct Actor356100 {
    /* 0x00 */ byte pad_0[0x18];
    /// Teardown `func_actor_356100_8016382C` installs; same slot as
    /// `Task::exitCallback` / `Actor01900.field_18`.
    /* 0x18 */ TaskFunc         field_18;
    /* 0x1C */ Actor356100Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /// Event-handler table installed on entry; same slot as
    /// `Task::field_24` / `Actor01900.field_24`.
    /* 0x24 */ void*      field_24;
    /* 0x28 */ byte       pad_28[4];
    /* 0x2C */ TmdObject* field_2C;
    /// State index `func_actor_356100_8016382C` advances once its entry has
    /// run; same slot as `Task::state` / `Actor01900.field_30`.
    /* 0x30 */ s32  field_30;
    /* 0x34 */ byte pad_34[2];
    /// Spawn sub-type `func_actor_356100_8016382C` switches on to pick the
    /// re-entry pair; same slot and role as `Actor01900.field_36`, the high
    /// half of `Task::spawnArg1`.
    /* 0x36 */ s16 field_36;
} Actor356100;

/// Binds the model's light and colour matrices to the pair kept in the work
/// block. Same body as `Actor01900_BindMatrices`.
static __inline__ void Actor356100_BindMatrices(Actor356100* actor)
{
    Actor356100Work* work;
    TmdObject*       obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->field_1C = &work->field_AD8;
    obj->field_20 = &work->field_AF8;
}

/// Animation view of the work block above, as `func_actor_356100_801633DC`
/// reads it: the `Actor01900AnimWork` layout 0xE0 bytes later, so the two
/// `GpAnimCtx` blocks at 0x1C / 0x4C8 each sit 0x14 bytes before their
/// 0x28-byte slot array. `field_982` is the clip id the slot loop copies
/// minus 3 into `slots[i].field_9`, `field_98A` the clip written whole into
/// `blendSlots[i].field_9`, and `field_98C` the blend weight, the same three
/// roles `Actor01900AnimWork.field_8A2` / `field_8AA` / `field_8AC` have.
typedef struct Actor356100AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[24];
    /* 0x3F0 */ byte       pad_3F0[0xD8];
    /* 0x4C8 */ GpAnimCtx  blendAnim;
    /* 0x4DC */ GpAnimSlot blendSlots[24];
    /* 0x89C */ byte       pad_89C[0xE6];
    /* 0x982 */ s16        field_982;
    /* 0x984 */ byte       pad_984[6];
    /* 0x98A */ s16        field_98A;
    /* 0x98C */ s16        field_98C;
} Actor356100AnimWork;

/// Blends pose slots 1..0x14: the first eleven copy the two clip ids into
/// their slot records and are written from both animation contexts with
/// `0x1000 - field_98C` as the blend weight, the rest only tick. Same body as
/// `Actor01900_Fn01950` / `func_actor_403000_801336B4` with this overlay's
/// slot count.
void func_actor_356100_801633DC(Actor356100* arg0);

void func_actor_356100_80163508(Actor356100* arg0);

/// The player's coordinate, as `Actor401300` / `Actor01900` name it. The
/// overlay keeps its own copy like those two do.
extern MATRIX* D_80073B8C;

extern u32 Gp_LcgState;

/// Player-to-`coord` vector, in the 16-bit `SVECTOR` view of both matrices.
static __inline__ void Actor356100_PositionDelta(GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = D_80073B8C->t[0] - coord->coord.t[0];
    pos->vy = D_80073B8C->t[1] - coord->coord.t[1];
    pos->vz = D_80073B8C->t[2] - coord->coord.t[2];
}

/// 0xC-byte 3D squared-radius block walked off `G_SCRATCH_HEAD`, same shape
/// `Actor401300RangeScratch` has. Handed out whole by `Actor356100_OutOfRange`.
typedef struct Actor356100RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor356100RangeScratch;
STATIC_ASSERT_SIZEOF(Actor356100RangeScratch, 0xC);

/// Whether `d` is further than `r` from the origin, compared on squared
/// lengths. Takes the block off `G_SCRATCH_HEAD`, publishes it for the
/// duration of the multiply chain and gives it back, then adds `dx*dx` to
/// `dz*dz` and tests `>= r*r`. Same body as `Actor401300_OutOfRange`; the two
/// `G_SCRATCH_HEAD` stores bracketing the chain are what the scratch block is
/// reserved against (`func_actor_356100_80167818` and the tick above it pass
/// 3000).
static __inline__ s32 Actor356100_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor356100RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor356100RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor356100RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor356100RangeScratch*)(head - 0xC))->dx *= ((Actor356100RangeScratch*)(head - 0xC))->dx;
    *(Actor356100RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor356100RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// 0x10-byte `G_SCRATCH_HEAD` block `func_actor_356100_80168E44` takes: the
/// offset from the actor to the player, then the facing yaw it settles on.
/// Same shape as `Actor401300AimScratch` / `Actor01900AimScratch`.
typedef struct Actor356100AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor356100AimScratch;
STATIC_ASSERT_SIZEOF(Actor356100AimScratch, 0x10);

/// 0x34-byte `G_SCRATCH_HEAD` block `Actor356100_RescaleYaw` builds its scaled
/// Y rotation in. Same shape as `Actor401300RotScratch` / `Actor01900RotScratch`.
typedef struct Actor356100RotScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
    /* 0x32 */ s16    pad_32;
} Actor356100RotScratch;
STATIC_ASSERT_SIZEOF(Actor356100RotScratch, 0x34);

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor356100_NormalizeYaw(s16 input)
{
    s16 value = input;

    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// `Actor356100_PositionDelta` against an explicit config block rather than
/// the overlay's own `D_80073B8C` copy of the player coordinate.
static __inline__ void Actor356100_ConfigPositionDelta(WipSysConfig* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->field_4->t[0] - coord->coord.t[0];
    pos->vy = config->field_4->t[1] - coord->coord.t[1];
    pos->vz = config->field_4->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the
/// offset. Same body as `Actor401300_PositionYaw` / `Actor01900_PositionYaw`.
static __inline__ s16 Actor356100_PositionYaw(Actor356100* actor, SVECTOR* pos, WipSysConfig* config)
{
    GsCOORDINATE2* coord;
    s32            angle;

    Actor356100_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor356100_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Rebuilds `coord`'s Y rotation from the yaw it already faces, uniformly
/// scaled by `scale`. Same body as `Actor401300_RescaleYaw` / `Actor01900_RescaleYaw`.
static __inline__ void Actor356100_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor356100RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor356100RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor356100RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Payload of message 0x3E9 `func_actor_356100_801666B4` sends the player:
/// the player's own position, then the heading away from this actor, so the
/// player ends up moved one normalised unit along that direction. Same shape
/// and roles as `Actor403000Msg3E9`, whose `field_10` / `field_12` are the
/// flag and angle pair its sender writes there.
typedef struct Actor356100Msg3E9 {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  field_10;
    /// Heading `ratan2` produces from the normalised player-to-actor vector,
    /// where `Actor356100Work::field_5A` holds this actor's own facing.
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[0x2];
} Actor356100Msg3E9;
STATIC_ASSERT_SIZEOF(Actor356100Msg3E9, 0x18);

/// The overlay's only message-0x3E9 instance; all eight words are zero in the
/// image, so it is a work area rather than a table.
extern Actor356100Msg3E9 D_actor_356100_801732B0;

/// Zeroed word `func_actor_356100_80167818` clears when the actor goes live.
/// The 0x74 bytes after it are zero in the image too, so the whole run is a
/// work area rather than a table.
extern s32 D_actor_356100_801731B0;

/// Effect record `func_actor_356100_80167818` fills for `func_800FDB18`:
/// coordinate index 5 of the model, scale 0x100 and count 2. Same shape and
/// roles as `Actor401300Work.field_910`.
extern GpEffArg D_actor_356100_801732A8;

/// Animation bank `func_800B3F84` seeds both of the work block's slot arrays
/// from. Same role as `Actor01900_D17174`.
extern u8 D_actor_356100_801730B8[];

/// Enemy descriptor `func_actor_356100_8016382C` publishes in the enemy's
/// `field_50` slot and takes `field_40` off. Same role as `Actor01900_D0AC54`.
extern GpPairSrcE D_actor_356100_8016A984;

/// The three rows `func_actor_356100_8016382C` picks its re-entry pair from on
/// the spawn sub-type. Same role as `Actor01900_D0AC64`.
extern Actor356100TintRow D_actor_356100_8016A994[];

/// Event-handler table `func_actor_356100_8016382C` hands the task as
/// `Task::field_24`. Same shape and role as `Actor01900_D1728C`.
extern void* D_actor_356100_80173258;

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The spawn
/// sub-type picks the clip and re-entry pair, and the finished entry advances
/// the state.
void func_actor_356100_8016382C(GpEnemy* enemy, Actor356100* actor);

/// Separation tick: when the work block's `field_4` flag is set, pushes this
/// actor one normalised unit away from the player along the player-to-actor
/// direction in XZ (recentring it on the player first), clears the model's
/// root `flg`, and sends the player message 0x3E9 with its own position and
/// the resulting heading. Bit 0 of `field_68` then forces `field_0` to 0xD.
void func_actor_356100_801666B4(Actor356100* arg0);

/// Approach tick, and the sibling of `func_actor_356100_80167584` above it. Going
/// live clears `D_actor_356100_801731B0` and re-seeds the animation slots at
/// clip 2 / speed 0x10 with the enemy's link node cleared; otherwise a single
/// sound 0x51030008 is queued the first time through, keyed on the enemy's
/// `field_8 >> 12` bank. Each frame then snapshots `field_5A & 0x3FF` into
/// `field_994`, and the frame that first lands on clip 4 spawns the
/// `D_actor_356100_801732A8` effect at model coordinate 5. Once the player is
/// further than 3000 away it plays 0x51030008 as a type-7 event and enters
/// state 6. Same shape as `func_actor_401300_801397F8`.
void func_actor_356100_80167818(Actor356100* arg0);

/// Event handler: copies the event's first three bytes into the work block's
/// `field_B58`, then dispatches on `w[0] == 0xB05` and `w[1]` — sub-code 1
/// puts the actor in state 0x1E, 0 and 2 in state 0. Anything else returns 0.
s32 func_actor_356100_8016A0B8(Actor356100* arg0, s32 arg1, Actor356100Event* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `field_54` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the 0x978..0x982 animation
/// slots; otherwise clears the model's root `flg`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `GpObj` flag masks.
void func_actor_356100_8016A21C(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 3`.
void func_actor_356100_8016A2AC(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A340(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A3D4(Actor356100* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern GpAnimArg D_actor_356100_80173244;

/// When the work block's `field_4` flag is set, writes the 0x978..0x982
/// animation slots, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `field_974`. Bit 1 of `field_68` forces `field_0`
/// to 0xE.
void func_actor_356100_8016A468(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the model's `field_C`,
/// clears the enemy's link node and writes the 0x978..0x982 animation slots
/// with `field_9BC` forced to 0x180. Bit 0 of `field_68` forces `field_0` to 7.
/// Same shape as `Actor01900_Fn0AA78` without its two `GpObj` flag masks.
void func_actor_356100_8016A5DC(Actor356100* arg0);

#endif
