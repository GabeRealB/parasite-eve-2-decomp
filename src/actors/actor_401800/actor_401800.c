#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor.h"
#include "actors/actors_shared_80133eb8.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// XZ patrol point in `Actor401800Work.field_C`. Same shape as
/// `Actor01900Waypoint`.
typedef struct Actor401800Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
} Actor401800Waypoint;

/// 0xC-byte row of `D_actor_401800_8013E700`; the init body copies the four
/// halfwords of the row picked by the spawn argument's low nibble into
/// `Actor401800Work.field_C08..field_C0E`. Same shape as
/// `Actor01900TintRow`.
typedef struct Actor401800TintRow {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ s16  field_4;
    /* 0x6 */ s16  field_6;
    /* 0x8 */ byte pad_8[4];
} Actor401800TintRow;
STATIC_ASSERT_SIZEOF(Actor401800TintRow, 0xC);

/// Private work block of the actor 401800 task, hanging off `Task::work`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The offsets match
/// the same-shaped teardown of actor 01900 (`Actor01900Work`), whose display
/// nodes sit at the same three addresses; only the child-task pair differs.
/// `field_4` is the live-actor flag `func_actor_401800_8013E138` tests, and
/// `field_A08.flags` / `field_B48.flags` are the two masks it writes. The
/// halfwords at 0x898..0x8A2 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes; `func_actor_401800_8013E194` is that body.
typedef struct Actor401800Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Step counter `func_actor_401800_8013E4F0` resets to 0 and bumps once a
    /// frame; the same slot `Actor104000Work.field_6` counts in.
    /* 0x006 */ s16 field_6;
    /// Stride counter the walking body bumps once a frame alongside `field_6`
    /// and clears whenever `func_actor_401800_80133918` reports the actor is on
    /// target; the walking body picks `field_0 = 0x1B` once it reaches 0x5B.
    /// Same slot `Actor01900Work.field_8` counts in.
    /* 0x008 */ s16  field_8;
    /* 0x00A */ byte pad_A[2];
    /// XZ patrol points: the spawn position and one step along its facing.
    /* 0x00C */ Actor401800Waypoint field_C[2];
    /* 0x014 */ s16                 field_14;
    /// Heading of the root coordinate, `ratan2` of its Z axis, stored by the
    /// placement handler `func_actor_401800_8013DE3C` after it rotates the
    /// model into place.
    /* 0x016 */ s16  field_16;
    /* 0x018 */ byte pad_18[0x42];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x43A];
    /* 0x4A4 */ u16  field_4A4;
    /* 0x4A6 */ byte pad_4A6[0x3EE];
    /* 0x894 */ s32  field_894;
    /* 0x898 */ s16  field_898;
    /* 0x89A */ s16  field_89A;
    /* 0x89C */ s16  field_89C;
    /* 0x89E */ s16  field_89E;
    /* 0x8A0 */ u16  field_8A0;
    /* 0x8A2 */ s16  field_8A2;
    /* 0x8A4 */ s16  field_8A4;
    /* 0x8A6 */ s16  field_8A6;
    /* 0x8A8 */ s16  field_8A8;
    /* 0x8AA */ u16  field_8AA;
    /* 0x8AC */ s16  field_8AC;
    /* 0x8AE */ s16  field_8AE;
    /* 0x8B0 */ s16  field_8B0;
    /* 0x8B2 */ byte pad_8B2[2];
    /// State the `0x3FF` handler last ran for: `func_actor_401800_8013A034`
    /// sends the actor's 0x200 effect when `field_5A & 0x3FF` is 4 and differs
    /// from this, then stores the mask back. Same slot `Actor01900Work.field_894`
    /// compares against.
    /* 0x8B4 */ s32 field_8B4;
    /// `func_800FDB18` argument record: the coordinate handed to it plus the
    /// effect scale / count pair. Same slot `Actor01900Work.field_8B8` keeps.
    /* 0x8B8 */ GpEffArg field_8B8;
    /* 0x8C0 */ byte     pad_8C0[2];
    /// Frame counter the aim-and-rescale body decrements once per frame while
    /// it is non-zero. Same role `Actor01900Work.field_C37` plays in the
    /// matching chase body.
    /* 0x8C2 */ u8      field_8C2;
    /* 0x8C3 */ byte    pad_8C3[5];
    /* 0x8C8 */ GpObj   field_8C8;
    /* 0x8E8 */ GpRec18 field_8E8;
    /* 0x900 */ byte    pad_900[0x108];
    /* 0xA08 */ GpObj   field_A08;
    /* 0xA28 */ GpRec18 field_A28;
    /* 0xA40 */ byte    pad_A40[0x108];
    /* 0xB48 */ GpObj   field_B48;
    /* 0xB68 */ GpRec18 field_B68;
    /// Light and color matrices the init body binds onto the model object
    /// (`TmdObject.lightMtx` / `field_20`). Same pair `Actor01900Work` keeps
    /// at `field_BB0` / `field_BD0`.
    /* 0xB80 */ MATRIX field_B80;
    /// Root-coordinate matrix the live-actor block of the walking body copies
    /// from `field_BC0` back over itself, then re-writes from the pose scratch.
    /* 0xBA0 */ MATRIX field_BA0;
    /// Home of the root-coordinate matrix the live-actor block restores.
    /// Same pair `Actor00100Work.field_BA0` keeps.
    /* 0xBC0 */ MATRIX field_BC0;
    /// Cleared by the init body right after `field_A08` is linked; same slot
    /// `Actor01900Work.field_C10` / `Actor401300Work.field_C88` clears.
    /* 0xBE0 */ s16  field_BE0;
    /* 0xBE2 */ u16  field_BE2;
    /* 0xBE4 */ u16  field_BE4;
    /* 0xBE6 */ byte pad_BE6[2];
    /// Direction the swing body of `func_actor_401800_80137DDC` rebuilds from
    /// the yaw in the scratch angle with `Gfx_MatrixCol2` / `VectorNormalSS`,
    /// then GPF-scales by `field_C02` into the offset added to the root
    /// coordinate. Same slot `Actor01900Work.field_C18` keeps.
    /* 0xBE8 */ SVECTOR field_BE8;
    /* 0xBF0 */ s16     field_BF0;
    /* 0xBF2 */ s16     field_BF2;
    /* 0xBF4 */ s16     field_BF4;
    /* 0xBF6 */ byte    pad_BF6[2];
    /// Bearing the chase body of `func_actor_401800_80137714` steers 0x89 a
    /// frame toward `field_BFA` and hands to `Gfx_RotMatrixY`. Same slot
    /// `Actor401300Work.field_C94` / `Actor01900Work.field_C20` keep.
    /* 0xBF8 */ s16 field_BF8;
    /// Target bearing the same body stores as the actor's facing plus twice
    /// the wrapped turn toward the player. Same slot
    /// `Actor401300Work.field_C96` / `Actor01900Work.field_C22` keep.
    /* 0xBFA */ s16 field_BFA;
    /// Step the aim-and-rescale body walks the actor along its own local Z
    /// axis while `func_actor_401800_80133558` says the path is clear, and
    /// reloads `field_0 = 9` once it has counted down to zero. Same slot
    /// `Actor401300Work.field_C98` keeps.
    /* 0xBFC */ s16 field_BFC;
    /// Countdown that walks the chase body of `func_actor_401800_80136EAC`
    /// through its step ramp: 8 until `field_8A2` reaches 0x18, then -1 until
    /// it lands on 0x12, then 0 — the state that ticks `field_6` and re-aims
    /// the actor at the player. Added to `field_8A2` every frame. Same slot
    /// `Actor01900Work.field_C26` keeps.
    /* 0xBFE */ s16 field_BFE;
    /// Side the live-actor swing of `func_actor_401800_80137DDC` takes from
    /// the LCG, then flips every frame it runs: 1 adds 0x171 to the yaw and
    /// -1 subtracts it. Same slot `Actor01900Work.field_C28`.
    /* 0xC00 */ s16 field_C00;
    /// The 0xDE the swing body resets its GPF scale to and halves once the
    /// `field_A28` contact test fires. Same slot `Actor01900Work.field_C2A`.
    /* 0xC02 */ s16 field_C02;
    /// Step the actor walks along its local Z axis: `func_actor_401800_80139118`
    /// seeds it with -0x78, hands it to the step helper while the 0x10 clip is
    /// playing, and halves it each time the `field_A28` contact test fires.
    /// Same role `Actor401300Work.field_C98` plays.
    /* 0xC04 */ s16  field_C04;
    /* 0xC06 */ byte pad_C06[2];
    /// Per-variant reload the LCG spreads over the idle step countdown: the
    /// high half of a fresh `Gp_LcgState` draw masked to 3 bits (`& 7`) is
    /// added to it and stored into `field_6`. Same slot `Actor401300Work`
    /// keeps as `field_CA0`, whose counterpart loads it with `& 0xF`.
    /* 0xC08 */ u16 field_C08;
    /// Second and third halfwords of the same `D_actor_401800_8013E700` row the
    /// init body copies; `field_C0A` / `field_C0C` pair with `field_C08` and
    /// `field_C0E` as the row's four halfwords.
    /* 0xC0A */ u16 field_C0A;
    /* 0xC0C */ u16 field_C0C;
    /// Radius `func_actor_401800_8013A034` hands its scratch distance test:
    /// the player is close enough to arm the actor once the squared XZ offset
    /// fits inside it. Same role `Actor01900Work.field_C32` plays.
    /* 0xC0E */ u16 field_C0E;
    /// The three bytes `func_actor_401800_8013DF80` copies out of the room
    /// request record it is handed; same slot as `Actor01900Work.field_C34`.
    /* 0xC10 */ u8   field_C10[3];
    /* 0xC13 */ byte pad_C13[1];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC14 */ Task* field_C14;
    /* 0xC18 */ Task* field_C18;
    /// Contact range the chase body of `func_actor_401800_80137714` tests the
    /// player offset against: under 2 it takes the 8 state outright. Same slot
    /// `Actor401300Work.field_D1C` / `Actor01900Work.field_C40` keep.
    /* 0xC1C */ s16 field_C1C;
    /// Frames the live-actor swing of `func_actor_401800_80137DDC` has run:
    /// it picks the side `field_C00` flips to only while this is zero, and
    /// bumps it once per frame. Same slot `Actor01900Work.field_C42`.
    /* 0xC1E */ s16 field_C1E;
    /// Set to 1 by the actors that own the `0x3F1` message and cleared once
    /// `func_actor_401800_80139118` has sent it. Same slot `Actor01900Work`
    /// keeps at 0xC20.
    /* 0xC20 */ s16     field_C20;
    /* 0xC22 */ byte    pad_C22[2];
    /* 0xC24 */ SVECTOR field_C24[7];
    /* 0xC5C */ byte    pad_C5C[0x18];
    /// Cleared by the init body once the root coordinate has been rescaled;
    /// same slot `Actor01900Work.field_C98` / `Actor401300Work.field_C98`
    /// clears at the same point.
    /* 0xC74 */ s16 field_C74;
} Actor401800Work;

/// Animation view of the same task work block: the pose context at 0x1C and
/// its slot array, then the blend context the actor keeps beside it. The
/// arrays cover the slot indices the blended tick `func_actor_401800_801337EC`
/// walks; the offsets all match `Actor01900AnimWork`, and the tail overlays
/// the work block's `field_8A2` / `field_8A4` (the state the slot writes step
/// down by 3).
typedef struct Actor401800AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[19];
    /* 0x328 */ byte       pad_328[0x130];
    /* 0x458 */ GpAnimCtx  blendAnim;
    /* 0x46C */ GpAnimSlot blendSlots[19];
    /* 0x764 */ byte       pad_764[0x13E];
    /* 0x8A2 */ s16        field_8A2;
    /* 0x8A4 */ s16        field_8A4;
    /* 0x8A6 */ byte       pad_8A6[4];
    /* 0x8AA */ s16        field_8AA;
    /* 0x8AC */ s16        field_8AC;
} Actor401800AnimWork;

/// 0x20 scratch block the bisector push's coordinate walk takes from
/// `G_SCRATCH_HEAD`. `coord` is the frame the walk is currently standing on
/// (it climbs the `GsCOORDINATE2::sub` parent chain until NULL), `vec` is the
/// vector being carried up into world space, and `out` receives the `MVMVA`
/// result (`MAC1..3`) that is fed back into `vec` each step. `flag` takes the
/// GTE flag register so the block matches what the code stores. Same layout as
/// `RoomsShared80182078Walk`.
typedef struct Actor401800BisectorWalk {
    /* 0x00 */ GsCOORDINATE2* coord;
    /* 0x04 */ SVECTOR        vec;
    /* 0x0C */ s32            out[3];
    /* 0x18 */ s32            pad_18;
    /* 0x1C */ s32            flag;
} Actor401800BisectorWalk;
STATIC_ASSERT_SIZEOF(Actor401800BisectorWalk, 0x20);

/// 0xE4 scratch block `func_actor_401800_80132E0C` takes from
/// `G_SCRATCH_HEAD` while it nudges a coordinate frame away from the
/// obstacles recorded in a `GpRec18` table. `m` is the working matrix handed
/// to `Gfx_RotMatrixY` / `Gfx_MatrixCol2`. `eye` is the frame's own world
/// position and `aim` the world point one unit (0x1000) in front of it, both
/// produced by walking the parent chain; `delta` is the scratch difference fed
/// to `ratan2` and later the GPF-scaled push applied to `coord.t[0]` /
/// `coord.t[2]`. `kind` is the record's `key` high halfword, `angle[]` the
/// per-record bearing relative to the facing direction (0x7FFE marks "no more
/// records", 0x7FFF "record does not count"), `i` / `j` the two loop counters,
/// `diff` the wrapped bearing difference between a pair of records and `hit`
/// the value the function returns. Same layout as
/// `RoomsShared80182078Scratch`.
typedef struct Actor401800BisectorScratch {
    /* 0x00 */ MATRIX  m;
    /* 0x20 */ byte    pad_20[0x80];
    /* 0xA0 */ SVECTOR delta;
    /* 0xA8 */ SVECTOR eye;
    /* 0xB0 */ SVECTOR aim;
    /* 0xB8 */ s32     kind;
    /* 0xBC */ s16     angle[0x10];
    /* 0xDC */ s16     i;
    /* 0xDE */ s16     j;
    /* 0xE0 */ s16     diff;
    /* 0xE2 */ s16     hit;
} Actor401800BisectorScratch;
STATIC_ASSERT_SIZEOF(Actor401800BisectorScratch, 0xE4);

/// Carries `v` from the local frame `coord` up the `GsCOORDINATE2::sub` parent
/// chain into world space, using a 0x20 scratch block from `G_SCRATCH_HEAD`.
static __inline__ void Actor401800_BisectorToWorld(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor401800BisectorWalk* blk;

    {
        register GsCOORDINATE2* parent asm("v0");
        parent                                                                                              = coord;
        ((Actor401800BisectorWalk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk)))->coord = parent;
    }
    {
        register u8* tmp asm("v0");
        tmp = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk);
        blk = (Actor401800BisectorWalk*)tmp;
    }
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor401800BisectorWalk);
}

/// Same as `Actor401800_BisectorToWorld`, but the walk starts at `coord`
/// itself rather than its parent, so the vector is not carried through a frame
/// until the loop has run at least once.
static __inline__ void Actor401800_BisectorToWorld2(GsCOORDINATE2* coord, SVECTOR* v)
{
    Actor401800BisectorWalk* blk;

    blk         = (Actor401800BisectorWalk*)((u8*)*(void**)G_SCRATCH_HEAD - sizeof(Actor401800BisectorWalk));
    blk->coord  = coord;
    blk->vec.vx = v->vx;
    blk->vec.vy = v->vy;
    blk->vec.vz = v->vz;

    *(void**)G_SCRATCH_HEAD = blk;
    while (blk->coord != NULL) {
        gte_SetTransMatrix(&blk->coord->coord);
        gte_SetRotMatrix(&blk->coord->coord);
        gte_ldv0(&blk->vec);
        gte_rtv0tr();
        gte_stlvnl(blk->out);
        gte_stflg(&blk->flag);
        blk->vec.vx = *(u16*)&blk->out[0];
        blk->vec.vy = *(u16*)&blk->out[1];
        blk->vec.vz = *(u16*)&blk->out[2];
        blk->coord  = blk->coord->sub;
    }
    v->vx = blk->vec.vx;
    v->vy = blk->vec.vy;
    v->vz = blk->vec.vz;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor401800BisectorWalk);
}

/// The 34 state handlers copied to the frame before the per-frame dispatch.
typedef struct Actor401800StateTable {
    TaskFunc fn[34];
} Actor401800StateTable;
STATIC_ASSERT_SIZEOF(Actor401800StateTable, 0x88);

extern const Actor401800StateTable D_actor_401800_80131FDC;

/// Payload of the `0x3FF` message `func_actor_401800_80138F5C` sends: the same
/// 0x14-byte animation record other actors keep as `GpAnimArg` data
/// (`D_actor_356100_80173244` and friends); `field_4` is the animation id.
extern GpAnimArg D_actor_401800_80155A0C;

extern GpAnimSet* D_actor_401800_801559F8[];
extern GpAnimSet* D_actor_401800_801559F0[];
extern s8         D_8007218A;

extern GpDelayArg D_actor_401800_80155AF8;

/// Twelve `SVECTOR` hit positions `func_actor_401800_801348A8` picks from by
/// damage magnitude: the low four when the hit is light, the high two when it
/// is heavy, and the last four on the `arg1 > 0` / `arg1 <= 0` split in
/// between. The fourth halfword (`pad`, unused by the effect itself) is the
/// model part index `func_800FDB18` anchors the spawned effect to. Same role
/// `Actor00100_D1B9F4` plays for `Actor00100_Fn03340`.
extern SVECTOR D_actor_401800_80155A20[12];

/// Animation bank both `func_800B3F84` contexts are initialised from. Same
/// role `Actor01900_D17174` plays for actor 01900.
extern s32 D_actor_401800_80155938;

/// Enemy description record the init body copies `hpMax` out of into
/// `GpEnemy.hp` and points `GpEnemy.param` at. Same role
/// `D_actor_401300_80141FA0` plays for actor 401300.
extern GpPairSrcE D_actor_401800_8013E6F0;

/// The three `Actor401800TintRow` variants the init body picks from by the
/// spawn argument's low nibble: `[0]` when it is 2, `[2]` when it is 1, `[1]`
/// otherwise. Same table shape as `Actor01900_D0AC64`.
extern Actor401800TintRow D_actor_401800_8013E700[];

/// Handler table the actor's task receives in `Task::msgTable`; same role
/// `Actor01900_D1728C` plays for actor 01900.
extern void* D_actor_401800_80155A80;

/// Payload `func_actor_401800_80138C28` fills and sends with message 0x3E9.
extern GpXformArg D_actor_401800_80155AD8;

/// Frame counter the chase body of `func_actor_80136EAC` accumulates its step
/// `field_BFC` into and the init body clears; the aim-and-rescale body reads it
/// back as the phase of the step it walks. Same role `Actor01900_D172FC` plays
/// for actor 01900.
extern u16 D_actor_401800_80155AC0;

/// Camera-target matrix `func_actor_401800_8013A034` measures the actor's root
/// coordinate against for its proximity test. Same global `Actor401300` reads.
extern MATRIX* D_80073B8C;

/// The block `func_actor_401800_8013A034` posts into `D_actor_401800_80155978`
/// when the actor's live flag is set, taking over the animation the actor had
/// been running. Same pair `Actor401300` keeps as `D_actor_401300_80158878` /
/// `D_actor_401300_80152BB8`.
extern s32  D_actor_401800_80155124;
extern s32* D_actor_401800_80155978;

/// Gameplay slot `Gp_SpawnEff` effects read their model data from; set before
/// each spawn in `func_actor_401800_8013BB10`.
extern void* D_80114B78[1];

/// Overlay effect model data `func_actor_401800_8013BB10` points `D_80114B78`
/// at before spawning: the 0x60030 debris burst, then the 0xA0005 fan the step
/// counter trips at 3 and 5 and the two 0xA0005 bursts at 7 and 9.
extern char D_actor_401800_80143E9C;
extern char D_actor_401800_80144434;
extern char D_actor_401800_80144F24;

/// Movement is frozen while this is 1. Same flag `actorMoveForwardNonzero`
/// and the other families' step helpers test.
extern u8 D_80072729;

s32  func_actor_401800_8013271C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos);
s32  func_actor_401800_80132C68(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2);
s32  func_actor_401800_80132E0C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push);
s32  func_actor_401800_8013629C(Task* arg0, GpRec18* recs, s16 count);
s32  func_actor_401800_80133558(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
s32  func_actor_401800_80133918(Task* arg0);
void func_actor_401800_801348A8(Task* arg0, s16 arg1, s32 arg2);
s32  func_actor_401800_8013DCBC(Task* arg0, s32 arg1, GpAnimArg* arg2);
void func_actor_401800_80133EB8(Task* arg0);
void func_actor_401800_801320C8(GsCOORDINATE2* coord, s16 yaw);
void func_actor_401800_801337EC(Task* arg0);
s32  func_actor_401800_80133B78(Actor401800Work* work);
void func_actor_401800_8013423C(GpEnemy* enemy, Task* actor);
void func_actor_401800_8013E0A0(Task* task);
void func_actor_401800_8013E138(Task* arg0);
void func_actor_401800_8013E194(Task* arg0);
void func_actor_401800_8013E23C(Task* arg0);
void func_actor_401800_8013E2E8(Task* arg0);
void func_actor_401800_8013E394(Task* arg0);
void func_actor_401800_8013E44C(Task* arg0);
void func_actor_401800_8013E4F0(Task* arg0);
void func_actor_401800_8013E5A4(Task* arg0);
void func_actor_401800_801381E4(Task* arg0);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Integer part of the last movement step `func_actor_401800_80132C68`
/// applied, nudged one unit outward where the step had a fractional part.
extern SVECTOR D_actor_401800_80155AD0;

/// Clip-transition table the cross-fade reads: one byte per (previous clip,
/// requested clip) pair, rows of 0x2D, handed to `func_800B4114` as the
/// transition argument.
extern s8 D_actor_401800_8015514C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it. The animation driver turns two joints of the
/// actor's chain with it, by two thirds and one half of the same clamped head
/// yaw.
void func_actor_401800_801320C8(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

/// Refreshes `coord` and computes the push-out of the last kind 0x10000 /
/// 0x30000 record in `recs`, walking until `count` records or a zero `key`;
/// returns nonzero when any record hit. The push is left in the scratch block,
/// clamped to length 0x100. Nothing is done while movement is frozen or the
/// view is not ready.
s32 func_actor_401800_801323D4(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->flg                           = 0;
    head                                 = *(ActorRepelScratch**)G_SCRATCH_HEAD;
    blk                                  = head - 1;
    *(ActorRepelScratch**)G_SCRATCH_HEAD = blk;
    s                                    = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->flg                            = 0;
    *(ActorRepelScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `pos` and to the coordinate's
/// translation.
s32 func_actor_401800_8013271C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* pos)
{
    u8*                head;
    ActorAvoidScratch* s;
    s16                diff;
    s16                t;
    s32                mag;

    if (gGameSession->viewReady == 1 || D_80072729 == 1) {
        return 0;
    }

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(ActorAvoidScratch);
    s                     = (ActorAvoidScratch*)*(u8**)G_SCRATCH_HEAD;
    s->blocked            = 0;
    pos->vz               = 0;
    pos->vy               = 0;
    pos->vx               = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&coord->workm.t[0];
    s->eye.vy = *(u16*)&coord->workm.t[1];
    s->eye.vz = *(u16*)&coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = actorBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = actorBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + sizeof(ActorAvoidScratch);
    return s->blocked != 0;
}

/// Moves `coord` by the step the first `arg2` records of `movement` resolve to
/// (`func_800E0C10`) and latches that step's integer part into
/// `D_actor_401800_80155AD0`. Where the X or Z step has a fractional part,
/// the coordinate and the latched step go one unit further from zero. Returns
/// 1 when the X or Z step is nonzero.
s32 func_actor_401800_80132C68(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**          scratch;
    u8*             head;
    ActorDeltaFlag* s;
    register void*  p asm("v1");
    s32             val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((ActorDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_401800_80155AD0.vx = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_401800_80155AD0.vy = s->delta.vy.w >> 16;
        D_actor_401800_80155AD0.vz = s->delta.vz.w >> 16;
        val                        = ((ActorDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_401800_80155AD0.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_401800_80155AD0.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_401800_80155AD0.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_401800_80155AD0.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Nudges `coord` away from the obstacles in `recs`: takes the frame's world
/// position and the point one unit in front of it, gives every kind 0x10000 /
/// 0x30000 record a bearing relative to that facing, and at the first record
/// with no other record within 0x400 of its bearing pushes the frame `push`
/// units away from it. Returns 1 when a push was applied, and does nothing
/// while the view is not ready.
s32 func_actor_401800_80132E0C(GsCOORDINATE2* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                      scratch;
    void**                      tail;
    u8*                         head;
    Actor401800BisectorScratch* st;
    u16                         vz;
    s16                         d;
    s16                         dz;
    s32                         t;
    s32                         hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(Actor401800BisectorScratch);
        st  = (Actor401800BisectorScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    Actor401800_BisectorToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    Actor401800_BisectorToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail  = (void**)G_SCRATCH_HEAD;
    hit   = st->hit;
    *tail = (u8*)*tail + sizeof(Actor401800BisectorScratch);
    return hit;
}

/// Returns nonzero while the actor at `coord` should keep closing on the
/// slot-3 player. With `arg2 >= 0` that is when the player is more than 0x400
/// off the actor's facing; with `arg2 < 0`, when the player is within 0x400
/// of it. Otherwise the point `arg2` units along the facing is taken and the
/// result is whether it is still at least `arg1 + 0x96` from the player.
s32 func_actor_401800_80133558(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (arg2 >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(arg2);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= arg1 + 0x96;
}

/// Per-frame blended animation tick: for pose slots 1..10 it sets both
/// contexts' slot rates (the blend context's from `field_8AA`, the pose
/// context's three below `field_8A2`), samples each context's pose and writes
/// their mix weighted by `field_8AC` against its 0x1000 complement. Slots 11
/// and up only take the pose rate and are advanced unblended.
void func_actor_401800_801337EC(Task* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor401800AnimWork* work;

    work   = (Actor401800AnimWork*)arg0->work;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AA;
            work->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Line-of-sight test between the slot-3 player and this actor: both root
/// positions, raised by 1000, are rotated into world space and handed to
/// `func_800E0308`, whose result is returned.
s32 func_actor_401800_80133918(Task* arg0)
{
    Task*              player;
    u8*                head;
    ActorSightScratch* s;
    SVECTOR*           local;
    SVECTOR*           v;
    SVECTOR*           out;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (ActorSightScratch*)(head - 0x1C);
    s->local.vx           = ((TmdObject*)player->extra)->coords->coord.t[0];
    s->local.vy           = ((TmdObject*)player->extra)->coords->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((TmdObject*)player->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->local.vy = ((TmdObject*)arg0->extra)->coords->coord.t[1] - 1000;
    s->local.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx           += gGfxViewCoord.workm.t[0];
    s->from.vy           += gGfxViewCoord.workm.t[1];
    s->from.vz           += gGfxViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}

/// Animation-event dispatcher: picks the `0x400A00xx` event the state in
/// `field_89E` and the animation index in `field_5A & 0x3FF` queue, one event
/// per index change because `field_8B4` latches the index the last one fired
/// for — a repeat of that index just stores it back. A state/index pair with
/// no event falls through to the shared tail, which latches the index, or
/// clears `field_8B4` outright in the states that need it. Same dispatcher
/// shape as `Actor01900_Fn01A7C`.
s32 func_actor_401800_80133B78(Actor401800Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x1A) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
                work->field_8B4 = id;
            } else if (id == 0x13) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0003;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
            id = work->field_5A & 0x3FF;
            if (id == 0x10) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x16) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 12:
            id = work->field_5A & 0x3FF;
            if (id == 7 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xA && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0004;
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x12 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0002;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 5:
            id = work->field_5A & 0x3FF;
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0x16 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0003;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 6:
            id = work->field_5A & 0x3FF;
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x13 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

/// The actor's per-frame animation driver. A pending clip change in
/// `field_898` either cross-fades every pose slot from the previous clip to the
/// requested one (1) or restarts them on it (2); a blend request in `field_8A6`
/// restarts the blend context on `field_8A8`. The slots are then advanced,
/// blended while `field_89A` is set (cleared once the pose context reports its
/// end), the head yaw in `field_8B0` eases toward `field_8AE` by at most 0x100 a
/// frame and turns two joints of the model by it, and the animation event for
/// the current state and frame is played at the model's pan and depth.
void func_actor_401800_80133EB8(Task* arg0)
{
    ActorsShared80133eb8Work* seekWork;
    ActorsShared80133eb8Work* resetWork;
    ActorsShared80133eb8Work* secondaryWork;
    ActorsShared80133eb8Work* tickWork;
    ActorsShared80133eb8Work* work;
    GpEnemy*                  enemy;
    s32                       animation;
    s32                       index;
    u32                       table;
    s16                       state;
    s32                       seekIndex;
    s32                       resetIndex;
    s32                       secondaryIndex;
    s32                       tickIndex;
    s32                       seekSlotIndex;
    s32                       resetSlotIndex;
    s32                       secondarySlotIndex;
    s32                       tickSlotIndex;
    s32                       targetAngle;
    s32                       currentAngle;
    s32                       targetAngleBits;
    s32                       currentAngleBits;
    s16                       angle;
    s32                       clampedAngle;
    s16                       signedTurn;
    s32                       sound;
    s32                       soundId;
    s32                       pan;
    s8*                       seekSlot;
    s8*                       resetSlot;
    s8*                       secondarySlot;
    s8*                       tickSlot;

    work  = (ActorsShared80133eb8Work*)arg0->work;
    enemy = arg0->spawnArg2;
    state = work->field_898;
    if (state == 1) {
        // Keep the copy before the comparison so it fills the branch delay slot.
        seekWork = (ActorsShared80133eb8Work*)arg0->work;
        if (work->field_89C != (s16)work->field_89E) {
            seekIndex = 1;
            table     = (u32)&D_actor_401800_8015514C;
            // Slot i has stride 0x28; its rate is at work + 0x39 + i * 0x28.
            seekSlot = ((s8*)work + 0x28);
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_8A2;
                animation      = (s16)seekWork->field_89E;
                seekSlot      += 0x28;
                index          = seekWork->field_89C * 0x2D;
                func_800B4114(&seekWork->anim, seekSlotIndex, animation, 0,
                              (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x13);
            seekWork->field_89C = (s16)seekWork->field_89E;
        }
        work->field_898 = 3;
        work->field_8A0 = 0;
        work->field_8B4 = 0;
    } else if (state == 2) {
        resetWork = work;
        // Preserve the separate work pointer for the reset loop.
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = ((s8*)work + 0x28);
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_8A2;
            resetSlot      += 0x28;
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex,
                             (s32)(s16)resetWork->field_89E);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_89C = (s16)resetWork->field_89E;
        work->field_898      = 3;
        work->field_8A0      = 0;
        work->field_8B4      = 0;
    }
    if (work->field_8A6 == 2) {
        secondaryWork            = (ActorsShared80133eb8Work*)arg0->work;
        secondaryIndex           = 1;
        secondarySlot            = ((s8*)secondaryWork + 0x28);
        secondaryWork->field_8AA = 0x30;
        secondaryWork->field_8AC = 0x800;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_8AA;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex,
                             (s32)secondaryWork->field_8A8);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x13);
        work->field_8A6 = 3;
    }
    work->field_8A0 = (u16)(work->field_8A0 + 1);
    if ((s16)work->field_89A == 0) {
        tickWork  = (ActorsShared80133eb8Work*)arg0->work;
        tickIndex = 1;
        tickSlot  = ((s8*)tickWork + 0x28);
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_8A2;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        func_actor_401800_801337EC(arg0);
        if (work->field_4A4 & 1) {
            work->field_89A = 0;
        }
    }
    targetAngle      = (s16)work->field_8AE;
    currentAngle     = (s16)work->field_8B0;
    targetAngleBits  = (u16)work->field_8AE;
    currentAngleBits = (u16)work->field_8B0;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->field_8B0 = currentAngleBits + 0x100;
        } else {
            goto atTargetAngle;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->field_8B0 = currentAngleBits - 0x100;
    } else {
    atTargetAngle:
        work->field_8B0 = targetAngleBits;
    }
    angle        = (s16)work->field_8B0;
    clampedAngle = (u16)work->field_8B0;
    if (angle != 0) {
        if (angle >= 0x401) {
            clampedAngle = 0x400;
        }
        if (angle < -0x400) {
            clampedAngle = -0x400;
        }
        signedTurn = (s16)clampedAngle * 2 / 3;
        func_actor_401800_801320C8(&((TmdObject*)arg0->extra)->coords[5], signedTurn);
        func_actor_401800_801320C8(&((TmdObject*)arg0->extra)->coords[2], (s16)clampedAngle / 2);
        ((TmdObject*)arg0->extra)->coords[5].flg = 0;
        ((TmdObject*)arg0->extra)->coords[4].flg = 0;
        ((TmdObject*)arg0->extra)->coords[3].flg = 0;
        ((TmdObject*)arg0->extra)->coords[2].flg = 0;
    }
    sound = func_actor_401800_80133B78((Actor401800Work*)work);
    if (sound != 0) {
        soundId = sound | (((u16)enemy->placeKey >> 0xC) << 8);
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan,
                            (s32)(s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
}

/// Binds the work block's light and color matrices onto the model object.
/// Same body as `Actor01900_BindMatrices` / `Actor401300_BindMatrices`.
static __inline__ void Actor401800_BindMatrices(Task* actor)
{
    Actor401800Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra;
    obj->lightMtx = &work->field_B80;
    obj->colorMtx = &work->field_BA0;
}

/// Enemy init: allocates the work block, binds the model matrices, sets up both
/// animation contexts and the three hit/body `GpObj` nodes, then picks the
/// starting state and tint row from the spawn flags and rescales the model.
/// Same body as `Actor01900_Fn02018` / `func_actor_401300_80134454`.
void func_actor_401800_8013423C(GpEnemy* enemy, Task* actor)
{
    SVECTOR          dir;
    VECTOR           pos;
    SVECTOR*         v;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor401800Work* work;
    GpObj*           body;
    GpObj*           head;
    s32              kind;

    root        = ((TmdObject*)actor->extra)->coords;
    obj         = actor->extra;
    work        = memCalloc(0xC78, 0);
    actor->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    actor->exitCallback = func_actor_401800_8013E0A0;
    Actor401800_BindMatrices(actor);
    enemy->field_4    = &((TmdObject*)actor->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)actor->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->reactionFlags = 0;
    enemy->hp            = (s16)D_actor_401800_8013E6F0.hpMax;
    enemy->param         = &D_actor_401800_8013E6F0;
    enemy->recs          = &work->field_8E8;
    func_800B3F84(&((Actor401800AnimWork*)work)->anim, &D_actor_401800_80155938, obj,
                  ((Actor401800AnimWork*)work)->pad_328, ((Actor401800AnimWork*)work)->slots);
    func_800B3F84(&((Actor401800AnimWork*)work)->blendAnim, &D_actor_401800_80155938, obj,
                  ((Actor401800AnimWork*)work)->pad_764, ((Actor401800AnimWork*)work)->blendSlots);
    work->field_898 = 2;
    work->field_89E = 2;
    work->field_89A = 0;
    work->field_8B0 = 0;
    work->field_8AE = 0;
    work->field_8A4 = 0x10;
    work->field_8A2 = 0x10;
    if ((s16)((enemy->placeKey >> 12) & 1) == 1) {
        work->field_8A4++;
    } else {
        work->field_8A4--;
    }
    func_actor_401800_80133EB8(actor);

    work->field_A08.ctx.recs = &work->field_A28;
    work->field_A08.coord    = root;
    work->field_A08.pos.vx   = 0;
    work->field_A08.pos.vy   = -0xAC;
    work->field_A08.pos.vz   = 0;
    work->field_A08.key      = 0x30012;
    work->field_A08.radius   = 0x12C;
    work->field_A08.flags    = 1;
    Gp_LinkObj(2, &work->field_A08);
    work->field_BE0        = 0;
    work->field_A08.flags |= 0x4000;
    Gp_InitRec18Table(work->field_A08.ctx.recs, 0xC, 0);

    body           = &work->field_8C8;
    body->coord    = &((TmdObject*)actor->extra)->coords[2];
    body->ctx.recs = &work->field_8E8;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->key      = 0x30000;
    body->radius   = 0x12C;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_B48;
    head->coord    = &((TmdObject*)actor->extra)->coords[6];
    head->ctx.recs = &work->field_B68;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x180;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);

    work->field_14     = 0;
    work->field_C[0].x = ((TmdObject*)actor->extra)->coords->coord.t[0];
    work->field_C[0].z = ((TmdObject*)actor->extra)->coords->coord.t[2];
    Gfx_MatrixCol2(&((TmdObject*)actor->extra)->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->field_C[1].x = ((TmdObject*)actor->extra)->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = ((TmdObject*)actor->extra)->coords->coord.t[2] + dir.vz;

    actor->msgTable = &D_actor_401800_80155A80;
    root->sub       = &gGfxViewCoord;
    root->flg       = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_8B8.coord      = &((TmdObject*)actor->extra)->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    kind                       = (actor->spawnArg1 >> 16);
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (actor->spawnArg1 & 0xF) {
        case 2:
            work->field_C08 = D_actor_401800_8013E700[0].field_0;
            work->field_C0A = D_actor_401800_8013E700[0].field_2;
            work->field_C0C = D_actor_401800_8013E700[0].field_4;
            work->field_C0E = D_actor_401800_8013E700[0].field_6;
            break;
        case 1:
            work->field_C08 = D_actor_401800_8013E700[2].field_0;
            work->field_C0A = D_actor_401800_8013E700[2].field_2;
            work->field_C0C = D_actor_401800_8013E700[2].field_4;
            work->field_C0E = D_actor_401800_8013E700[2].field_6;
            break;
        case 0:
        default:
            work->field_C08 = D_actor_401800_8013E700[1].field_0;
            work->field_C0A = D_actor_401800_8013E700[1].field_2;
            work->field_C0C = D_actor_401800_8013E700[1].field_4;
            work->field_C0E = D_actor_401800_8013E700[1].field_6;
            break;
    }

    actorRescaleYaw(((TmdObject*)actor->extra)->coords, 0x1194);
    work->field_C74 = 0;
    actor->state++;
}

/// Picks one of twelve hit positions out of `D_actor_401800_80155A20` by damage
/// magnitude `arg1`, copies it to an 8-byte scratch vector, then arms the
/// `field_8B8` spawn record with the actor's part-1 coordinate as its anchor
/// and hands it to `func_800FDB18` to spawn effect `Gp_GetIdParam1(arg2)`.
/// Scale 0x300 and count 2 are the effect's; the record's coordinate comes from
/// `TmdObject.coords[sc->pad]`, so the effect follows the part the table entry
/// names. Same body as `Actor00100_Fn03340` (actors/lib/actor_400100_damage.c).
void func_actor_401800_801348A8(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401800Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = (Actor401800Work*)arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401800_80155A20[0];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[1];
                break;
            case 2:
                *sc = D_actor_401800_80155A20[2];
                break;
            case 3:
                *sc = D_actor_401800_80155A20[3];
                break;
            default:
                *sc = D_actor_401800_80155A20[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401800_80155A20[5];
                break;
            case 1:
                *sc = D_actor_401800_80155A20[6];
                break;
            default:
                *sc = D_actor_401800_80155A20[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[8];
        } else {
            *sc = D_actor_401800_80155A20[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401800_80155A20[10];
        } else {
            *sc = D_actor_401800_80155A20[11];
        }
    }
    work->field_8B8.coord      = &((TmdObject*)arg0->extra)->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[sc->pad], sc, &work->field_8B8);
    *(u32*)G_SCRATCH_HEAD += 8;
}

void func_actor_401800_80134C94(Task* arg0)
{
    PlayerStatus*    config = &Player_Status;
    Actor401800Work* work;
    GpEnemy*         enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GsCOORDINATE2*   coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s32              value;
    s16              state;
    s16              animState;
    s16              next;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head  = *(ActorHitScratch**)G_SCRATCH_HEAD;
        s     = (*(ActorHitScratch**)G_SCRATCH_HEAD = head - 1);
        s->id = actorFindHit(&head[-1].hitPos, &work->field_8E8);
        if (s->id == 0) {
            s->id = actorFindHit(&s->hitPos, &work->field_A28);
        }
        if (s->id != 0) {
            if (s->id & 0x8000) {
                player       = gameGetPtrSlot(3);
                s->hitPos.vx = ((TmdObject*)player->extra)->coords->workm.t[0];
                s->hitPos.vy = ((TmdObject*)player->extra)->coords->workm.t[1];
                s->hitPos.vz = ((TmdObject*)player->extra)->coords->workm.t[2];
            }
            if (work->field_C20 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                work->field_C20 = 0;
                if ((u16)(work->field_0 - 0xB) < 4) {
                    work->field_0 = 0x13;
                }
            }
            work->field_C1C                        = 0;
            work->field_C1E                        = 0;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
            s->dir.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
            s->dir.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
            s->dir.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
            s->dir.vx = s->hitPos.vx - ((TmdObject*)arg0->extra)->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - ((TmdObject*)arg0->extra)->coords->workm.t[1];
            z         = s->hitPos.vz - ((TmdObject*)arg0->extra)->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = ((TmdObject*)arg0->extra)->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = actorNormalizeYaw(s->yaw);
            func_actor_401800_801348A8(arg0, s->yaw, s->id);
            work->field_8B0 = 0;
            work->field_8AE = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 &&
                state != 4) {
                s->m = ((TmdObject*)arg0->extra)->coords->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if ((s16)work->field_BE4 > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->dir.vx;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] += s->dir.vy;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->dir.vz;
                ((TmdObject*)arg0->extra)->coords->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x1F && state != 0x20 && state != 0xF && state != 0x10 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78(enemy, s->id, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->field_BE2 += s->damage;
            effect           = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[2], effect, NULL);
            }
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((u16)(work->field_0 - 0xC) < 3 && config->hp > 0 && work->field_C20 == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0008;
                deathPan   = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            } else {
                hitSound = ((enemy->placeKey >> 0xC) << 8) | 0x400A0007;
                hitPan   = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            }
            work->field_BE0 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                    if (work->field_0 == 0x18 || work->field_0 == 0x16 || work->field_0 == 0x17) {
                        work->field_0 = 6;
                    }
                    state = work->field_0;
                    if (state == 0x13 || state == 0x14 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                        if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                            work->field_89A = 1;
                            work->field_8A8 = 0xB;
                        } else {
                            work->field_89A = 1;
                            work->field_8A8 = 0x19;
                        }
                        work->field_8A6 = 2;
                    } else if ((s16)work->field_BE2 >= 0x38 || s->crit == 1) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    } else {
                        work->field_8A8 = 0xD;
                        work->field_89A = 1;
                        work->field_8A6 = 2;
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 4) {
                        work->field_0 = 4;
                    } else if (state == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                    }
                    break;
                case 3:
                    if (work->field_0 == 0x18 || work->field_0 == 0x16 || work->field_0 == 0x17) {
                        work->field_0 = 6;
                    }
                    Gp_SetObjFlag4(enemy, s->id, 0);
                    break;
                case 1:
                    enemy->reactionFlags &= 0xFE;
                    state                 = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
                case 8:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 0x11 && state != 4) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == 0x10 && work->field_6 < 0x21) {
                                work->field_0 = 0x20;
                            } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                                work->field_0 = 0x1F;
                            } else {
                                mag = s->yaw;
                                if (mag < 0) {
                                    mag = -mag;
                                }
                                work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                            }
                        }
                    }
                    break;
                case 9:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x1F && state != 0x20 && state != 4 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                    break;
            }
            next = 5;
        } else if ((s16)work->field_BE4 <= 0) {
            work->field_BE2 = 0;
            goto be4_done;
        } else {
            next = work->field_BE4 - 1;
        }
        work->field_BE4 = next;
    be4_done:
        if (enemy->reactionFlags & 0xC) {
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                state = work->field_0;
                value = (u16)work->field_0;
                if (state == 7 || state == 0x1E || state == 0xB || state == 0x1B) {
                    work->field_0 = 5;
                } else if ((u16)(value - 0x13) < 2 || state == 0xF || state == 0x10 || state == 4 || state == 0x11) {
                    if (work->field_89E == 0xB || work->field_89E == 0x17 || work->field_89E == 8 || work->field_89E == 0xA) {
                        work->field_89A = 1;
                        work->field_8A8 = 0xB;
                    } else {
                        work->field_89A = 1;
                        work->field_8A8 = 0x19;
                    }
                    work->field_8A6 = 2;
                } else {
                    work->field_89A = 1;
                    work->field_8A8 = 0xD;
                    work->field_8A6 = 2;
                }
            }
        }
        if (enemy->hp <= 0) {
            value = s->id;
            if (value != 0) {
                if ((Gp_GetIdParam0(value) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->id) & 0xFFFF) == 6) {
                    animState = work->field_89E;
                    if (animState == 2 || animState == 3) {
                        work->field_0 = 0x21;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x11) {
                        if (state == 0x10 && work->field_6 < 0x21) {
                            work->field_0 = 0x20;
                        } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x1F;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            work->field_0 = (mag < 0x400) ? 0x13 : 0x14;
                        }
                    }
                }
            } else {
                if (work->field_C20 == 1) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                    work->field_C20 = 0;
                }
                state = work->field_0;
                if ((u16)(state - 0x13) >= 3 && state != 0x1D && state != 0x21 && state != 0 && state != 0x1F && state != 0x20 && state != 0x11) {
                    if (state == 0x10 && work->field_6 < 0x21) {
                        work->field_0 = 0x20;
                    } else if (work->field_0 == 0xF && work->field_6 < 0xC) {
                        work->field_0 = 0x1F;
                    } else {
                        work->field_0 = 0x14;
                    }
                }
            }
        }
        *(ActorHitScratch**)G_SCRATCH_HEAD += 1;
    }
}

/// Walk-state body, split on the live flag. Live: hand the model back to
/// `Tmd_AllocBuffers`, restart the 0x898 slot, ramp `field_8A2` to 0x10, remap
/// the state at 0x89E (11 -> 0x17, 12/25 -> 0x18, anything else -> 0x17) and
/// hold the two `field_5A` countdowns open until the step helper has run its
/// course, then drop `field_8A2` to 0x20. Dead: clear the model's coordinate
/// flag, halve `field_8A2` with the 1 / -1 wrap, and once `Gp_TickObjFlag2`
/// reports 1 clear the enemy's node bit 1 and move to state 0x11.
/// Same body as `func_actor_401300_80135DDC`.
void func_actor_401800_80135DAC(Task* arg0)
{
    Actor401800Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd               = arg0->extra;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A08.flags |= 0x4000;
        if (work->field_89E == 11) {
            work->field_89E = 0x17;
        } else if (work->field_89E == 12 || work->field_89E == 25) {
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            func_actor_401800_80133EB8(arg0);
        } while (!(work->field_89E == 0x17 && (work->field_5A & 0x3FF) >= 6) &&
                 !(work->field_89E == 0x18 && (work->field_5A & 0x3FF) >= 9));
        work->field_8A2 = 0x20;
        return;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    work->field_8A2                        = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    func_actor_401800_80133EB8(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~2;
        work->field_0         = 0x11;
    }
}

/// Aim the actor at the player and rescale its root coordinate. On the live
/// flag it resets the model buffers and hands back the pose the actor was
/// running; otherwise it takes a 0x10 scratch for the player offset and the
/// clamped turn, folds the turn into the coordinate's Y rotation and rebuilds
/// the matrix from the new yaw at scale 0x1194.
/// Same body as `Actor01900_Fn080A8`, with the aim and rescale helpers inlined.
void func_actor_401800_80135F58(Task* arg0)
{
    Actor401800Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_8C8.radius = 0x12C;
        Gp_ArmStateF0(1);
        return;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD    -= 1;
    aim                                    = *(ActorAimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

/// Push the root coordinate out of a `GpRec18` table: take a 0x34 scratch, seed
/// its position from the second coordinate, then walk the records until `count`
/// or a zero `key`. Each kind 0x10000 / 0x30000 record contributes half its
/// offset along X and Z, normalised to length 0x96 first when it is longer than
/// that; `hit` reports whether one was seen.
/// Same body as `Actor01900_Fn03FF8` / `func_actor_401300_80132910`, with the
/// coordinate update written out in both arms of the length test.
s32 func_actor_401800_8013629C(Task* arg0, GpRec18* recs, s16 count)
{
    ActorPushScratch* head;
    ActorPushScratch* s;
    ActorPushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    ((TmdObject*)arg0->extra)->coords[1].flg = 0;
    head                                     = *(ActorPushScratch**)G_SCRATCH_HEAD;
    blk                                      = head - 1;
    *(ActorPushScratch**)G_SCRATCH_HEAD      = blk;
    s                                        = blk;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
    s->pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    s->pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    s->pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x96) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x96);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx / 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                ((TmdObject*)arg0->extra)->coords->coord.t[0] += s->offset.vx / 2;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] += s->offset.vz / 2;
            }
            ((TmdObject*)arg0->extra)->coords->flg = 0;
        }
    }
    *(ActorPushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401800_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                head;
    ActorRangeScratch* blk;
    s32                ret;

    head                                    = *(u8**)G_SCRATCH_HEAD;
    blk                                     = (ActorRangeScratch*)(head - 0xC);
    ((ActorRangeScratch*)(head - 0xC))->dx  = d->vx;
    *(ActorRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                 = d->vz;
    blk->r                                  = r;
    ((ActorRangeScratch*)(head - 0xC))->dx *= ((ActorRangeScratch*)(head - 0xC))->dx;
    blk->dz                                *= blk->dz;
    blk->r                                 *= blk->r;
    *(u8**)G_SCRATCH_HEAD                   = head;
    ret                                     = ((ActorRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

static __inline__ s32 Actor401800_ChaseOutOfRange(SVECTOR* d, s16 r)
{
    u8*                head;
    ActorRangeScratch* blk;
    s32                ret;

    head                                    = *(u8**)G_SCRATCH_HEAD;
    ((ActorRangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                     = (ActorRangeScratch*)(head - 0xC);
    blk->dz                                 = d->vz;
    blk->r                                  = r;
    ((ActorRangeScratch*)(head - 0xC))->dx *= ((ActorRangeScratch*)(head - 0xC))->dx;
    *(ActorRangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                *= blk->dz;
    blk->r                                 *= blk->r;
    *(u8**)G_SCRATCH_HEAD                   = head;
    ret                                     = ((ActorRangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_401800_80136560(Task* arg0)
{
    Actor401800Work*   work;
    TmdObject*         obj;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     turnCoord;
    GsCOORDINATE2*     facing;
    void**             scratch;
    u8*                head;
    u8*                block;
    s16                animRate;
    ActorChaseScratch* s;
    s32                kind;

    kind = (arg0->spawnArg1 >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_B48.flags &= 0x7FFF;
        animRate               = work->field_8A4;
        TOUCH_REG(work);
        work->field_89A        = 0;
        work->field_8A2        = animRate;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_C1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        return;
    }
    work->field_6++;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = head - 0x10;
    *scratch = block;
    s        = (ActorChaseScratch*)block;
    if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    coord                                         = ((TmdObject*)arg0->extra)->coords;
    ((ActorChaseScratch*)(head - 0x10))->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                   = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                   = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    ((TmdObject*)arg0->extra)->coords->flg        = 0;
    func_actor_401800_80133EB8(arg0);
    s->playerYaw                                  = ratan2(-((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][0],
                                                           ((TmdObject*)gameGetPtrSlot(3)->extra)->coords->coord.m[2][2]);
    coord                                         = ((TmdObject*)arg0->extra)->coords;
    ((ActorChaseScratch*)(head - 0x10))->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy                                   = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz                                   = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    s->yaw                                        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw                                        = actorNormalizeYaw(s->yaw);
    turnCoord                                     = ((TmdObject*)arg0->extra)->coords;
    s->turn                                       = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]));
    work->field_8AE                               = s->turn;
    if (abs(s->yaw - s->playerYaw) < 0x44) {
        if (((s16)work->field_C0C + work->field_C1E / 2) < work->field_6) {
            if (abs(s->turn) < 0x80) {
                if (Actor401800_ChaseOutOfRange(&s->delta, 0x708) && func_actor_401800_80133918(arg0) != 1) {
                    work->field_0 = 0xA;
                }
            }
        }
    }
    if (s->turn < 0x200) {
        if (!Actor401800_ChaseOutOfRange(&s->delta, 0x44C) && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
            work->field_0 = 0xB;
        }
    }
    if (s->turn > 0x30) {
        s->turn = 0x30;
    }
    if (s->turn < -0x30) {
        s->turn = -0x30;
    }
    facing   = ((TmdObject*)arg0->extra)->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->turn, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 3) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, ((work->field_8A4 + 2) * 0x42) / 18) != 0) {
                actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, ((work->field_8A4 + 2) * 0x42) / 18);
            }
        } else if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, (((work->field_8A4 + 2) * 0x42) / 18) >> 2) != 0) {
            actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, (((work->field_8A4 + 2) * 0x42) / 18) >> 2);
        }
    } else if (work->field_68 & 1) {
        work->field_89E = 3;
        work->field_898 = 1;
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(u8**)G_SCRATCH_HEAD += 0x10;
}

/// Chase body that steers the actor along its own local Z while the step
/// countdown runs: takes a 0x10 scratch for the player offset and the yaws, and
/// on the live flag resets the model buffers, arms the walk state and seeds
/// `field_BFE` to 8. Otherwise it re-aims the actor at the player once
/// `field_8` has run 7 frames, clamps the turn toward the player into
/// `s->angle`, rebuilds the root coordinate at scale 0x1194 and steps the actor
/// by `field_BFC` (the step countdown, halved while `field_89A` is set, forced
/// to 2 while `field_8` is live) while `func_actor_401800_80133558` reports the
/// path clear. `field_BFE` then walks 8 -> -1 -> 0 against `field_8A2`, and at
/// 0 the fifth `field_6` frame picks `field_0` from the yaw offset to the
/// player. Same step ramp as `Actor01900_Fn04D14`, with the aim and the step
/// helper inlined.
void func_actor_401800_80136EAC(Task* arg0)
{
    Actor401800Work*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* s;
    TmdObject*         obj;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     facing;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x96;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_BFE         = 8;
        work->field_6           = 0;
        work->field_8           = 0;
        D_actor_401800_80155AC0 = 0;
        work->field_C1C++;
        return;
    }
    head                                   = *(ActorChaseScratch**)G_SCRATCH_HEAD;
    *(ActorChaseScratch**)G_SCRATCH_HEAD   = head - 1;
    s                                      = head - 1;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 0) {
        work->field_8++;
    } else {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    if (work->field_8 >= 7) {
        s->playerYaw  = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
        s->yaw        = ratan2(s->delta.vx, s->delta.vz) + 0x800;
        s->yaw        = actorNormalizeYaw(s->yaw);
        work->field_0 = 0x1A;
    }
    coord   = ((TmdObject*)arg0->extra)->coords;
    s->turn = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn    = s->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            s->angle = s->turn - 1000;
        } else if (diffPos > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            s->angle = s->turn + 1000;
        } else if (diffNeg > 0) {
            s->angle = 0x60;
        } else {
            s->angle = -0x60;
        }
    }
    facing    = ((TmdObject*)arg0->extra)->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    coord                                  = ((TmdObject*)arg0->extra)->coords;
    work->field_8AE                        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    work->field_BFC                        = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_BFC = work->field_BFC >> 1;
    }
    if (work->field_8 != 0) {
        work->field_BFC = 2;
    }
    if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_BFC) != 0) {
        actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, work->field_BFC);
    }
    D_actor_401800_80155AC0 += work->field_BFC;
    if (work->field_BFE == 8 && work->field_8A2 >= 0x18) {
        work->field_BFE = -1;
    }
    if (work->field_BFE == -1 && work->field_8A2 == 0x12) {
        work->field_BFE = 0;
        work->field_6   = 0;
    }
    if (work->field_BFE == 0) {
        if (++work->field_6 == 5) {
            s->playerYaw = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                                  ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
            actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
            s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
            yaw    = actorNormalizeYaw(s->yaw);
            s->yaw = yaw;
            yaw    = yaw - s->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw >= 0x401 && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += work->field_BFE;
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(ActorChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Chase body: takes a 0x10 scratch for the player offset and the heading it
/// folds into the root coordinate. On the live flag it resets the model
/// buffers, arms the walk state and stores the facing yaw `field_BF8` along
/// with the target `field_BFA` — the facing plus twice the wrapped turn toward
/// the player. Otherwise it picks `field_0` from the `field_C1C` contact range
/// and the `field_8C2` countdown, steers `field_BF8` 0x89 a frame toward
/// `field_BFA`, rebuilds the root coordinate at scale 0x1194 and steps the
/// actor 0x28 / 0x14 along its local Z while `func_actor_401800_80133558`
/// reports the path clear. Same body as `func_actor_401300_801376E4` /
/// `Actor01900_Fn0551C`, with the step helper's clear-path test added.
void func_actor_401800_80137714(Task* arg0)
{
    Actor401800Work*   work;
    ActorChaseScratch* head;
    ActorChaseScratch* s;
    TmdObject*         obj;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                    = *(ActorChaseScratch**)G_SCRATCH_HEAD;
        obj                                     = arg0->extra;
        *(ActorChaseScratch**)G_SCRATCH_HEAD    = head - 1;
        s                                       = head - 1;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
        coord                                 = ((TmdObject*)arg0->extra)->coords;
        s->turn                               = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                = ((TmdObject*)arg0->extra)->coords;
        s->angle                              = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_BF8                       = s->angle;
        work->field_BFA                       = s->angle + (u16)s->turn * 2;
        *(ActorChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                 = *(ActorChaseScratch**)G_SCRATCH_HEAD;
    *(ActorChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                    = head - 1;
    func_actor_401800_80133EB8(arg0);
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    if (work->field_BF8 == work->field_BFA) {
        if (work->field_C1C < 2 || Actor401800_OutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else if (func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 8;
        }
    }
    if (work->field_BF8 > work->field_BFA) {
        work->field_BF8 -= 0x89;
        if (work->field_BF8 < work->field_BFA) {
            work->field_BF8 = work->field_BFA;
        }
    }
    if (work->field_BF8 < work->field_BFA) {
        work->field_BF8 += 0x89;
        if (work->field_BF8 > work->field_BFA) {
            work->field_BF8 = work->field_BFA;
        }
    }
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, work->field_BF8, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89A == 0) {
        if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 0x28) != 0) {
            actorStepForward(((TmdObject*)arg0->extra)->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 0x14) != 0) {
            actorStepForward(((TmdObject*)arg0->extra)->coords, 0x14);
        }
    }
    if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(ActorChaseScratch**)G_SCRATCH_HEAD += 1;
}

/// Live-actor swing. On the live flag it resets the model buffers, takes the
/// swing side `field_C00` from `Gp_LcgState`, offsets the player bearing in
/// `field_BE8` by +-0x171 on the first frame and stores the 0x15 / 0x14 state
/// the walk body runs. The `field_BE8` yaw is then rebuilt into a direction and
/// GPF-scaled by `field_C02` into the offset added to the root coordinate while
/// `field_6` sits in 0xC..0x15 — halving the scale once the `field_A28` contact
/// test fires. The step counter moves the actor to state 7 at 0x1E; a kind
/// 0x10 actor reloads 0x1E instead. Same body as `Actor01900_Fn05B4C`, with the
/// position delta inlined.
void func_actor_401800_80137DDC(Task* arg0)
{
    Actor401800Work* work;
    ActorAimScratch* head;
    ActorAimScratch* aim;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR*         dir;
    MATRIX           mat;
    u16              angle;
    s32              kind;

    kind = (arg0->spawnArg1 >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                               = *(ActorAimScratch**)G_SCRATCH_HEAD;
    *(ActorAimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                = head - 1;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x96;
        work->field_6          = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C00 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C00 = 1;
            } else {
                work->field_C00 = -1;
            }
        }
        if (work->field_C00 == 1) {
            work->field_89E = 0x15;
            if (work->field_C1E == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_C0A + angle;
            } else {
                aim->angle += work->field_C0A;
            }
            work->field_C00 = -1;
        } else {
            work->field_89E = 0x14;
            if (work->field_C1E == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_C0A;
            } else {
                aim->angle -= work->field_C0A;
            }
            work->field_C00 = 1;
        }
        work->field_898 = 1;
        work->field_8A2 = 0xC;
        work->field_89A = 0;
        func_actor_401800_80133EB8(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_BE8;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C02 = 0xDE;
        work->field_C1E++;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89A == 0) {
        gte_lddp(work->field_C02);
        gte_ldsv(&work->field_BE8);
        gte_gpf12();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C02 >> 1);
        gte_ldsv(&work->field_BE8);
        gte_gpf12();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = ((TmdObject*)arg0->extra)->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) == 1) {
            work->field_C02 >>= 1;
        }
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

static __inline__ void Actor401800_ViewWalk(GsCOORDINATE2* coord, SVECTOR* svp, SVECTOR* dir)
{
    VECTOR         vec;
    SVECTOR*       outp;
    u8*            head;
    VECTOR*        vecp;
    GsCOORDINATE2* p;
    GsCOORDINATE2* view;
    s32            flag;
    s32*           flagp;
    Task*          player;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 8;
    outp                  = (SVECTOR*)(head - 8);
    view                  = &gGfxViewCoord;
    vecp                  = &vec;
    flagp                 = &flag;
    outp->vx              = 0;
    outp->vy              = 0;
    outp->vz              = 0;
    p                     = &((TmdObject*)player->extra)->coords[1];
    svp->vx               = outp->vx;
    svp->vy               = outp->vy;
    svp->vz               = outp->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            svp->vx = vec.vx;
            svp->vy = vec.vy;
            svp->vz = vec.vz;
            p       = p->sub;
            SOFT_TOUCH_REG(p);
            goto loop;
        }
        SOFT_USE_REG(svp);
        outp->vx = svp->vx;
        outp->vy = svp->vy;
        outp->vz = svp->vz;
    }
    dir->vx                 = outp->vx - coord->coord.t[0];
    dir->vy                 = 0;
    dir->vz                 = outp->vz - coord->coord.t[2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

static __inline__ s16 Actor401800_ViewYaw(GsCOORDINATE2* coord, SVECTOR* dir)
{
    s32 angle;

    angle = ratan2(dir->vx, dir->vz);
    return actorNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

static __inline__ void Actor401800_SetGrabAnim(void)
{
    if (D_8007218A == 1) {
        D_actor_401800_80155A0C.animBlock.ptr = D_actor_401800_801559F8;
    } else {
        D_actor_401800_80155A0C.animBlock.ptr = D_actor_401800_801559F0;
    }
}

void func_actor_401800_801381E4(Task* arg0)
{
    GpEnemy*         enemy;
    Task*            player;
    GameActor*       gactor;
    PlayerStatus*    config;
    Actor401800Work* work;
    SVECTOR          dir;
    SVECTOR          sv;
    s32              ang;
    u16              step;

    enemy  = arg0->spawnArg2;
    work   = arg0->work;
    player = gameGetPtrSlot(3);
    gactor = (GameActor*)player->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_8C2        = 0xA;
        work->field_8C8.radius = 0x12C;
        work->field_C1E        = 0;
        work->field_C20        = 0;
        work->field_B48.flags  = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags  = (u16)(work->field_A08.flags | 0x4000);
        enemy->node.flags      = 0;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 4;
        func_actor_401800_80133EB8(arg0);
        func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
        work->field_BF0 = ((TmdObject*)arg0->extra)->coords->coord.t[0];
        work->field_BF2 = ((TmdObject*)arg0->extra)->coords->coord.t[1];
        work->field_BF4 = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        work->field_6   = 0;
        return;
    }
    step          = (u16)work->field_6 + 1;
    work->field_6 = step;
    if ((s16)step == 1) {
        func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
        work->field_BF0 = ((TmdObject*)arg0->extra)->coords->coord.t[0];
        work->field_BF2 = ((TmdObject*)arg0->extra)->coords->coord.t[1];
        work->field_BF4 = ((TmdObject*)arg0->extra)->coords->coord.t[2];
        Actor401800_ViewWalk(((TmdObject*)arg0->extra)->coords, &sv, &dir);
        ang = Actor401800_ViewYaw(((TmdObject*)arg0->extra)->coords, &dir);
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords[0].coord, ang, 0);
        actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
        dir.vx                                 = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy                                 = 0;
        dir.vz                                 = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8AE                        = 0;
        work->field_8B0                        = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        work->field_C1E                        = 0;
        work->field_C20                        = 0;
        work->field_8C2                        = 0xA;
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_5A & 0x3FF) == 0x10 && gactor->field_954 != 2) {
        Actor401800_ViewWalk(((TmdObject*)arg0->extra)->coords, &sv, &dir);
        ang = Actor401800_ViewYaw(((TmdObject*)arg0->extra)->coords, &dir);
        if (ang < 0) {
            ang = -ang;
        }
        if (ang < 0x20) {
            if (!Actor401800_OutOfRange(&dir, 0x5DC)) {
                Actor401800_SetGrabAnim();
                D_actor_401800_80155AF8.field_14 = 8;
                do {
                    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_401800_80155AF8, 0) == 0) {
                        work->field_0                   = 0xC;
                        work->field_C20                 = 1;
                        D_actor_401800_80155A0C.field_4 = 1;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_401800_80155A0C, 0);
                    }
                } while (0);
            }
        }
    }
    if (work->field_89E == 4 && (work->field_68 & 1)) {
        work->field_0 = 7;
    }
    if ((u32)(work->field_5A & 0x3FF) >= 0x11U) {
        dir.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0] - config->coordMtx->t[0];
        dir.vy = 0;
        dir.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2] - config->coordMtx->t[2];
        if (!Actor401800_OutOfRange(&dir, 0x578)) {
            VectorNormalSS(&dir, &dir);
            gte_lddp(0xA);
            gte_ldsv(&dir);
            gte_gpf12();
            gte_stsv(&dir);
            ((TmdObject*)arg0->extra)->coords->coord.t[0] += dir.vx;
            ((TmdObject*)arg0->extra)->coords->coord.t[2] += dir.vz;
            ((TmdObject*)arg0->extra)->coords->flg         = 0;
        }
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    }
}

/// Live-actor body: arms the animation slots and the two `field_8C8` /
/// `field_A08` nodes, then aims the actor at the `gameGetPtrSlot(3)` task's
/// root position — the XZ offset normalized by `VectorNormalSS` and GPF-scaled
/// by 0x3E8, the heading taken through `ratan2` — sends it as message 0x3E9
/// and spawns the 0xC/8/0x8F pad-lerp. On work flag bit 0 while `field_89E` is
/// 5, restarts the actor's model (`field_0 = 0xD`, the 0x8B8 effect record for
/// the second coordinate). Same shape as `func_actor_401300_80138800`.
void func_actor_401800_80138C28(Task* arg0)
{
    SVECTOR          dir;
    Actor401800Work* work  = arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                   = gameGetPtrSlot(3);
        work->field_8C8.radius                   = 0x12C;
        work->field_B48.flags                   &= 0x7FFF;
        work->field_A08.flags                   |= 0x4000;
        enemy->node.flags                        = 0;
        work->field_898                          = 1;
        work->field_8A2                          = 0x10;
        work->field_89E                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        D_actor_401800_80155AD8.pos.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        D_actor_401800_80155AD8.pos.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
        D_actor_401800_80155AD8.pos.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir                           = &dir;
        dir.vx                         = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy                         = 0;
        dir.vz                         = ((GpCoordXZ*)((TmdObject*)arg0->extra)->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        ((TmdObject*)arg0->extra)->coords->flg        = 0;
        D_actor_401800_80155AD8.rot.vx                = 0;
        D_actor_401800_80155AD8.rot.vy                = ratan2(dir.vx, dir.vz);
        D_actor_401800_80155AD8.rot.vz                = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_401800_80155AD8, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    if (work->field_89E == 5 && (work->field_68 & 1)) {
        work->field_0              = 0xD;
        work->field_8B8.coord      = &((TmdObject*)arg0->extra)->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[5], 0, &work->field_8B8);
    }
}

/// On the live-actor flag, raises the three animation slots, sends the `0x3FF`
/// animation record and the `0x3F9` object pair to the `gameGetPtrSlot(3)`
/// task, then spawns the 5/0xFF/8 pad-lerp. On work flag bit 0, restarts the
/// actor's model (`field_0 = 0xE`, the 0x8B8 effect record for the second
/// coordinate) and finally copies the `field_5A` clip id into `field_894` and
/// rebuilds the four coordinate parts the actor draws from.
void func_actor_401800_80138F5C(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    void*            player;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401800_80155A0C;
        msg->field_4    = 2;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = gameGetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair(enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_0              = 0xE;
        work->field_8B8.coord      = &((TmdObject*)arg0->extra)->coords[1];
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[5], 0, &work->field_8B8);
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401800_80133EB8(arg0);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[2].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[3]);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[3].coord, -0x80, 0);
    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[2]);
}

/// Per-frame body of the live actor while it walks: on work flag bit 0 it
/// raises the `0x10`/7/2 render slots, re-sends the `0x3FF` animation record
/// with clip 3 to the `gameGetPtrSlot(3)` task and seeds the walk step
/// `field_C04` to -0x78; otherwise, while the `field_5A` clip is one of
/// `0x10..0x16`, it advances the actor along its own local Z by `field_C04`
/// once `func_actor_401800_80133558` says the path is still clear and halves
/// that step each time the `field_A28` contact fires. Both paths then tick the
/// animation, and — on work bit 0 — pick `field_0` from the enemy's state byte
/// (`6`, or `0xA` when the enemy is not the one `func_actor_401800_80133918`
/// reports) and release the `0x3F1` message once.
void func_actor_401800_80139118(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    PlayerStatus*    config;
    u8               kind;

    work   = arg0->work;
    enemy  = arg0->spawnArg2;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
        D_actor_401800_80155A0C.field_4 = 3;
        if (config->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&D_actor_401800_80155A0C, 0);
        }
        work->field_C04        = -0x78;
        work->field_6          = 0;
        work->field_A08.flags |= 0x4000;
        return;
    }
    if ((Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) && (config->hp > 0) && (work->field_C20 == 1)) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C20 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7) {
        if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_C04) != 0) {
            actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, work->field_C04);
        }
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) == 1) {
            work->field_C04 = work->field_C04 / 2;
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        kind = enemy->node.targeted;
        if (kind == 1) {
            if (func_actor_401800_80133918(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if ((config->hp > 0) && (work->field_C20 == 1)) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C20 = 0;
        }
    }
}

/// Per-frame body of the live actor armed into state 1: raises the same
/// animation slots as `func_actor_401800_8013971C` but leaves `field_89E = 0xA`
/// (with `field_898 = 1` and `field_89A` cleared), then, while that slot is
/// still `0xA`, advances the actor along its own local Z by a fixed `-0x57`
/// once `func_actor_401800_80133558` says the path is clear. The `0xA` branch
/// then flips the slots to `0xB`/2 and ticks the animation a second time before
/// the two contact records are rebuilt, after which work bit 0 picks `field_0`
/// from the enemy's HP sign and its `field_4C` bit 1.
void func_actor_401800_8013945C(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 1;
        work->field_89E                  = 0xA;
        work->field_89A                  = 0;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    if ((work->field_89E == 0xA) && ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, -0x57) != 0)) {
        actorStepForward(((TmdObject*)arg0->extra)->coords, -0x57);
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) && (work->field_89E == 0xA)) {
        work->field_89E = 0xB;
        work->field_898 = 2;
        func_actor_401800_80133EB8(arg0);
    }
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if ((work->field_68 & 1) && (work->field_89E == 0xB)) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Per-frame body of the live actor: arms the animation slots and the two
/// `field_8C8` / `field_A08` nodes, re-seeds the 0x8E8 and 0xA28 contact
/// records, then — while work bit 0x100 is set — picks `field_0` from the
/// enemy's HP sign and its `field_4C` bit 1. Same body as `Actor01900_Fn09BE8`.
void func_actor_401800_8013971C(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0xB;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Second per-frame body of the live actor: as `func_actor_401800_8013971C`,
/// but it arms the animation slots with `field_89E = 0x19` and skips the
/// `field_5A` clip rebuild.
void func_actor_401800_80139870(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0x19;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Third per-frame body of the live actor: `func_actor_401800_8013971C` with
/// the animation slots armed at 1 / 0xC, and its `field_0` selector driven by
/// work bit 0 instead of bit 8. Same body as `func_actor_401800_8013971C`
/// apart from those three constants.
void func_actor_401800_801399C4(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 1;
        work->field_89E                  = 0xC;
        work->field_8A2                  = 0x10;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8C8.flags |= 0x4000;
    }
    func_actor_401800_80133EB8(arg0);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC);
    func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_68 & 1) {
        work->field_8C8.flags &= 0xBFFF;
        if (enemy->hp <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->reactionFlags & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Grow-and-settle body of the live actor: while its flag is set it clears the
/// `field_C` overlay, drops the two `GpObj` flag bits the previous body raised,
/// marks the enemy node live and restarts the step counter. The counter then
/// runs to 0x401, firing the light-mode and `0x600A5` effect cues as it crosses
/// steps 0x18, 0x1D, 0x29, 0x2F and 0x3F, and from step 0x1A on rebuilds the
/// root coordinate's Y rotation from its current yaw at scale 0x1194 with the
/// Y component shedding 0xB a step. Same body as `Actor01900_Fn06904` with the
/// `Gp_ReleaseStateF0Add` argument and the actor types changed.
void func_actor_401800_80139B18(Task* arg0)
{
    Actor401800Work*      work;
    GpEnemy*              enemy;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    ActorScaleRotScratch* blk;
    u8*                   head;
    u8*                   tail;
    void*                 scratch_base;
    s16                   temp_v0;
    s16                   ang;
    s16                   cur;
    s32                   k;
    s32                   sy;
    u16                   temp_v1;
    u16                   m22;

    work  = arg0->work;
    obj   = arg0->extra;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj->flags            = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.flags     = 1;
        work->field_6         = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add(arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, ((TmdObject*)arg0->extra)->coords + 2, 3, NULL);
                break;
            case 23:
                ((TmdObject*)arg0->extra)->flags = 2;
                break;
            case 17:
                Gp_SetLightMode(enemy, 2);
                break;
            case 39:
                ((TmdObject*)arg0->extra)->flags = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                       = 0x1194;
            head                                    = scratch_base;
            head                                    = *(u8**)(head + 0x3FC);
            coord                                   = ((TmdObject*)arg0->extra)->coords;
            blk                                     = (ActorScaleRotScratch*)(head - 0x34);
            sy                                      = k - (cur - 0x14) * 0xB;
            *(ActorScaleRotScratch**)G_SCRATCH_HEAD = blk;
            ang                                     = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                              = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Countdown body: on the live-actor flag re-allocates the model's buffers,
/// copies the root coordinate over its `field_BC0` home and restarts the step
/// counter in state 0xE. The counter then runs to 0x961, rerolling the LCG each
/// frame past it and bailing for that frame on every 0xF-th draw; the surviving
/// frames re-test the squared XZ offset to the camera target against
/// `field_C0E` and arm `Gp_StateF0` state 6 on a miss — bit 0x50000 there arms
/// it the same way. After the shared per-frame tick the body flips between
/// states 0xE and 0xF, one LCG draw per attempt, on the two `field_68` mask
/// bits. Same shape as `func_actor_401800_8013A034`.
void func_actor_401800_80139D60(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    u16              step;
    u32              lcg;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj        = arg0->extra;
        enemy      = arg0->spawnArg2;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        enemy->node.flags      = 0;
        work->field_6          = 0;
        work->field_BC0        = work->field_BA0;
        work->field_89E        = 0xE;
        work->field_898        = 1;
        work->field_8A2        = work->field_8A4;
    }
    step = (u16)work->field_6;
    if (work->field_6 >= 0x961) {
        lcg         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = lcg;
        if (!((lcg >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)(step + 1);
    }
    coord    = ((TmdObject*)arg0->extra)->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C0E)) {
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_89E == 0xE) {
        if (work->field_68 & 2) {
            lcg         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = lcg;
            if ((lcg >> 16) & 1) {
                work->field_89E = 0xF;
                work->field_898 = 1;
                func_actor_401800_80133EB8(arg0);
            }
        }
    }
    if (work->field_89E == 0xF && (work->field_68 & 1)) {
        work->field_89E = 0xE;
        work->field_898 = 1;
        func_actor_401800_80133EB8(arg0);
    }
}

/// Walking body: on the live-actor flag re-allocates the model's buffers,
/// hands the actor the `D_actor_401800_80155124` animation block and zeroes the
/// step counter and the 0x8A2..0x8B0 pose slots, otherwise plays the actor's
/// 0x51030008 spawn sound once on the first frame. After the shared per-frame
/// tick, a `field_5A` state of 4 that differs from the last handled one
/// (`field_8B4`) sends the 0x200-scale effect for the second coordinate part.
/// Then, if the squared XZ offset to the camera target fits inside
/// `field_C0E`, the actor plays 0x51030008 and arms `Gp_StateF0` in state 6 —
/// bit 0x50000 of `Gp_StateF0` arms it the same way. Same shape as
/// `Actor01900_Fn06B4C` and `func_actor_401300_801397F8`.
void func_actor_401800_8013A034(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                     = arg0->extra;
        D_actor_401800_80155978 = &D_actor_401800_80155124;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x10;
        work->field_8AE        = 0;
        work->field_6          = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->placeKey >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_6 = 1;
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.coord      = ((TmdObject*)arg0->extra)->coords + 1;
        work->field_8B8.spawnArgLo = 0x200;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), ((TmdObject*)arg0->extra)->coords + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = ((TmdObject*)arg0->extra)->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401800_OutOfRange(d, work->field_C0E)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        work->field_0 = 6;
    }
}

/// Patrol state: walks toward the waypoint `field_14` selects, turning at most
/// 0x18 per step and swapping waypoints once the waypoint is inside 0xA0 or
/// `field_6` has run 0x15 frames. The turn is folded into the root coordinate,
/// which is rebuilt at scale 0x1194, and the actor steps 7 units along its own
/// local Z while `func_actor_401800_80133558` reports the path clear. The
/// `field_A28` / `field_8E8` contact records then decide whether `field_6`
/// counts up or `func_actor_401800_8013629C` re-seeds them. In the tail the
/// `Player_Status` offset arms state 6 within `field_C0E`, or within 0xFA0 when
/// the aim toward the player is under 0x300. Same body as
/// `Actor01900_Fn06F40` / `func_actor_401300_80139AB0`, with the aim and step
/// helpers inlined. The waypoint delta is written twice; the retail build keeps
/// both sets of stores.
void func_actor_401800_8013A2E8(Task* arg0)
{
    Actor401800Work*  work;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    facing;
    ActorTurnScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
        return;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD -= 1;
    s                                    = *(ActorTurnScratch**)G_SCRATCH_HEAD;
    s->delta.vx                          = work->field_C[work->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->delta.vy                          = 0;
    s->delta.vz                          = work->field_C[work->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    s->delta.vx                          = work->field_C[work->field_14].x - ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->delta.vy                          = 0;
    s->delta.vz                          = work->field_C[work->field_14].z - ((TmdObject*)arg0->extra)->coords->coord.t[2];
    if (!Actor401800_OutOfRange(&s->delta, 0xA0) || work->field_6 >= 0x15) {
        if (work->field_14 == 0) {
            work->field_14 = 1;
        } else {
            work->field_14 = 0;
        }
        work->field_6 = 0;
    }
    func_actor_401800_80133EB8(arg0);
    coord           = ((TmdObject*)arg0->extra)->coords;
    s->angle        = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->angle;
    if (s->angle > 0x18) {
        s->angle = 0x18;
    }
    if (s->angle < -0x18) {
        s->angle = -0x18;
    }
    facing    = ((TmdObject*)arg0->extra)->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    if (work->field_89A == 0 && (s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 7) != 0) {
        actorStepForward(((TmdObject*)arg0->extra)->coords, 7);
    }
    if ((arg0->spawnArg1 >> 16) != 0x10) {
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) == 1 &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    } else {
        if ((func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) == 1 ||
             func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC) == 1) &&
            ABS(work->field_8AE) < 0x80) {
            work->field_6++;
        } else {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (func_actor_401800_80133918(arg0) != 1) {
        actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
        if (!Actor401800_OutOfRange(&s->delta, work->field_C0E)) {
            work->field_0 = 6;
        } else if (!Actor401800_OutOfRange(&s->delta, 0xFA0)) {
            coord    = ((TmdObject*)arg0->extra)->coords;
            s->angle = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(s->angle) < 0x300) {
                work->field_0 = 6;
            }
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player, fold the clamped turn into the root
/// coordinate's Y rotation, then step it along its own local Z while
/// `func_actor_401800_80133558` says the path is clear — reloading
/// `field_0 = 9` once the `field_BFC` step countdown runs out. Same body as
/// `func_actor_401300_8013A208`, with the aim and step helpers inlined.
void func_actor_401800_8013AB64(Task* arg0)
{
    Actor401800Work*  work;
    GpEnemy*          enemy;
    TmdObject*        obj;
    GsCOORDINATE2*    coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2;
        obj             = arg0->extra;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B0        = 0;
        work->field_8A2        = 0x1E;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                 = *(ActorTurnScratch**)G_SCRATCH_HEAD;
    turn->angle                          = actorPositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8AE                      = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = ((TmdObject*)arg0->extra)->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, turn->angle, 1);
    if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
        func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
    }
    if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, work->field_BFC) != 0) {
        actorMoveForwardNonzero(((TmdObject*)arg0->extra)->coords, work->field_BFC);
    }
    if (work->field_BFC > 0) {
        next            = work->field_BFC - 0xA;
        work->field_BFC = next;
        if ((s16)next < 0) {
            work->field_BFC = 0;
        }
    }
    func_actor_401800_80133EB8(arg0);
    if ((work->field_68 & 1) || work->field_BFC == 0) {
        work->field_0 = 9;
    }
    *(ActorTurnScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player and rebuild its root coordinate from the new
/// yaw at scale 0x1194, clamping the turn it adds to +-0x80 and halving it
/// when it is not below -0x80. Once the aim state reaches 0x11 it counts
/// frames in `field_6`, steps the actor along its own local Z while
/// `func_actor_401800_80133558` says the path is clear, re-seeds the
/// `field_A28` contact record and past 0x13 frames turns the actor away from
/// the side the player is on by +-0x4B0. On the live flag it resets the model
/// buffers and arms the state 2 the aim test promotes to 0x11 within 0x80.
/// Same body as `Actor01900_Fn080A8` / `func_actor_401300_8013A5C0`, with the
/// aim, rescale and step helpers inlined.
void func_actor_401800_8013AF1C(Task* arg0)
{
    Actor401800Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        func_actor_401800_80133EB8(arg0);
        return;
    }
    func_actor_401800_80133EB8(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    aim->angle                          = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE                     = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        func_actor_401800_80133EB8(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, -0x10) != 0) {
            actorStepForward(((TmdObject*)arg0->extra)->coords, -0x10);
        }
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401800_8013B444(Task* arg0)
{
    Actor401800Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 9;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(ActorAimScratch**)G_SCRATCH_HEAD    -= 1;
    aim                                    = *(ActorAimScratch**)G_SCRATCH_HEAD;
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8AE = aim->angle;
    if (aim->angle > 0) {
        aim->angle = 0;
    }
    if (aim->angle < 0) {
        aim->angle = 0;
    }
    coord       = ((TmdObject*)arg0->extra)->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    func_actor_401800_80133EB8(arg0);
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

/// Aim the actor at the player and rescale its root coordinate, turning the
/// stored yaw toward the target by at most 0x28 a frame instead of the hard
/// clamp `func_actor_401800_80135F58` uses. On the live flag it resets the
/// model buffers and re-arms the step countdown; otherwise it hands the
/// player offset and the new yaw to the actor's state body and rebuilds the
/// matrix at scale 0x1194.
void func_actor_401800_8013B784(Task* arg0)
{
    Actor401800Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    ActorAimScratch* aim;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 1;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_89E        = 0x13;
        work->field_89A        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags &= 0xBFFF;
        func_actor_401800_80133EB8(arg0);
        func_actor_401800_80133EB8(arg0);
        work->field_6   = 0;
        work->field_8B0 = 0;
        return;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                 = *(ActorAimScratch**)G_SCRATCH_HEAD;
    aim->angle                          = actorPositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8AE < aim->angle) {
        if (aim->angle - work->field_8AE >= 0x29) {
            work->field_8AE = (u16)work->field_8AE + 0x28;
        } else {
            work->field_8AE = aim->angle;
        }
    } else if (work->field_8AE - aim->angle >= 0x29) {
        work->field_8AE = (u16)work->field_8AE - 0x28;
    } else {
        work->field_8AE = aim->angle;
    }
    if (work->field_8AE == aim->angle && func_actor_401800_80133918(arg0) != 1 && work->field_8C2 == 0) {
        work->field_0 = 0xB;
    }
    coord      = ((TmdObject*)arg0->extra)->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, aim->angle, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    work->field_898 = 2;
    func_actor_401800_80133EB8(arg0);
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(ActorAimScratch**)G_SCRATCH_HEAD += 1;
}

/// Step-driven effect spawner for the actor's live ramp: while the spawn flag
/// is set the actor crouches (0x8C8 node pitched to 0x12C, 0xA08 flags bit
/// 0x4000 cleared), plays the 0x60030 debris burst and hands the task to the
/// state-F0 list; the step counter then fires the 0xA0005 effects at 3, 5, 7
/// and 9, each tinted from the enemy's area record, and parks the actor at 0x3D.
void func_actor_401800_8013BB10(Task* arg0)
{
    SVECTOR          vec;
    Actor401800Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0x80;
        work->field_8C8.radius           = 0x12C;
        work->field_A08.flags            = (u16)(work->field_A08.flags & 0xBFFF);
        enemy->node.flags                = 1;
        work->field_8AE                  = 0;
        work->field_6                    = 0U;
        vec.vx                           = 0x64;
        vec.vz                           = 0;
        vec.vy                           = 0;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401800_80144434;
        vec.vy        = 0;
        vec.vx        = 0;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401800_80143E9C;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 9) {
        D_80114B78[0] = &D_actor_401800_80144F24;
        actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

/// Step-driven aim-and-rescale body: while the spawn flag is set the actor
/// pitches its 0x8C8 node to the 0x12C walk animation, sets the 0xA08 flags bit
/// 0x4000, plays the 0x60030 burst and arms the 2/0x1A state pair; the step
/// counter then steps the actor along its facing while
/// `func_actor_401800_80133558` reports the path clear, re-seeds the
/// `field_A28` contact and fires the 0xA0005 effects at 3, 5 and 6. The 0x1A
/// state runs its light/state table and folds the countdown into the root
/// coordinate's Y scale; the nine body coordinates from +2 to +10 are then
/// rebuilt at unit scale.
void func_actor_401800_8013BF48(Task* arg0)
{
    SVECTOR          vec;
    Actor401800Work* work;
    GpEnemy*         enemy;
    u16              next;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_8C8.radius = 0x12C;
        work->field_A08.flags  = (u16)(work->field_A08.flags | 0x4000);
        enemy->node.flags      = 1;
        work->field_8AE        = 0;
        work->field_6          = 0U;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        work->field_89E        = 2;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords + 1, 0x10300, &vec);
        work->field_6 = 0U;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->field_68 & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 7) != 0) {
                actorStepForward(((TmdObject*)arg0->extra)->coords, 7);
            }
            func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &D_actor_401800_80143E9C;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401800_80143E9C;
                actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 1, 0x200, NULL), enemy);
            }
            if (work->field_6 == 6) {
                D_80114B78[0] = &D_actor_401800_80144F24;
                actorTintEffect(Gp_SpawnEff(0xA0005, ((TmdObject*)arg0->extra)->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->field_68 & 0x100)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, 1);
                    Gp_SpawnEff(0x600A5, ((TmdObject*)arg0->extra)->coords + 2, 2, NULL);
                    break;
                case 23:
                    ((TmdObject*)arg0->extra)->flags = 2;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, 2);
                    break;
                case 39:
                    ((TmdObject*)arg0->extra)->flags = 0x80;
                    work->field_0                    = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                actorRescaleYawY(((TmdObject*)arg0->extra)->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    func_actor_401800_80133EB8(arg0);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 2);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 3);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 4);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 5);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 6);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 7);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 8);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 9);
    actorResetYaw(((TmdObject*)arg0->extra)->coords + 10);
}

/// Walk body: takes a 0x10 scratch for the player offset, the facing yaws and
/// the wrapped turn toward the player. On the live flag it resets the model
/// buffers, arms the walk state (`0x12C` animation, step 0x30) and zeroes the
/// step counters; otherwise it counts both step slots, re-seeds the `field_A28`
/// / `field_8E8` contacts, folds the turn into the root coordinate's Y rotation
/// at scale 0x1194 and steps the actor 0x15 / 5 along its own local Z while
/// `func_actor_401800_80133558` says the path is clear. On the `func_actor_
/// 401800_80133918` hit it clears the stride, picks a side from the LCG and
/// flips it every 0xF1 frames instead. Same body as `Actor01900_Fn042BC`.
void func_actor_401800_8013CD98(Task* arg0)
{
    Actor401800Work*   work;
    TmdObject*         obj;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     facing;
    ActorChaseScratch* s;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_8C8.radius = 0x12C;
        work->field_898        = 1;
        work->field_8A2        = 0x30;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C1C        = 0;
        work->field_B48.flags &= 0x7FFF;
        work->field_A08.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1 >> 16) == 0x10) {
            work->field_8C8.flags |= 0x4000;
        }
    }
    work->field_6++;
    work->field_8++;
    *(ActorChaseScratch**)G_SCRATCH_HEAD -= 1;
    s                                     = *(ActorChaseScratch**)G_SCRATCH_HEAD;
    if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_A28, 0xC) != 1) {
        if (func_actor_401800_80132C68(((TmdObject*)arg0->extra)->coords, &work->field_8E8, 0xC) != 1) {
            func_actor_401800_8013629C(arg0, &work->field_8E8, 0xC);
        }
    }
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    s->playerYaw = ratan2(-((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][0],
                          ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    actorConfigPositionDelta(&Player_Status, ((TmdObject*)arg0->extra)->coords, &s->delta);
    s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw = actorNormalizeYaw(s->yaw);
    if (func_actor_401800_80133918(arg0) != 1) {
        work->field_6   = 0;
        coord           = ((TmdObject*)arg0->extra)->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                            ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!Actor401800_OutOfRange(&s->delta, 0x44C) && work->field_8C2 == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8 = 0;
        work->field_6++;
        coord           = ((TmdObject*)arg0->extra)->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) -
                                            ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C00 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C00 = -1;
            } else {
                work->field_C00 = 1;
            }
        }
        if (work->field_C00 == 1) {
            s->turn += 0x400;
        } else {
            s->turn -= 0x400;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C00 = -work->field_C00;
        }
    }
    if (s->turn > 0x20) {
        s->turn = 0x20;
    }
    if (s->turn < -0x20) {
        s->turn = -0x20;
    }
    facing   = ((TmdObject*)arg0->extra)->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->turn, 1);
    actorRescaleYaw(((TmdObject*)arg0->extra)->coords, 0x1194);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 0x15) != 0) {
                actorStepForward(((TmdObject*)arg0->extra)->coords, 0x15);
            }
        } else if ((s16)func_actor_401800_80133558(((TmdObject*)arg0->extra)->coords, 0x12C, 5) != 0) {
            actorStepForward(((TmdObject*)arg0->extra)->coords, 5);
        }
    } else if (work->field_68 & 1) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->field_8C2 != 0) {
        work->field_8C2--;
    }
    *(ActorChaseScratch**)G_SCRATCH_HEAD += 1;
}

const Actor401800StateTable D_actor_401800_80131FDC = { {
    func_actor_401800_8013E138,
    func_actor_401800_8013E194,
    func_actor_401800_8013E23C,
    func_actor_401800_8013E2E8,
    func_actor_401800_80135DAC,
    func_actor_401800_8013E394,
    func_actor_401800_80135F58,
    func_actor_401800_80136560,
    func_actor_401800_80136EAC,
    func_actor_401800_80137714,
    func_actor_401800_80137DDC,
    func_actor_401800_801381E4,
    func_actor_401800_80138C28,
    func_actor_401800_80138F5C,
    func_actor_401800_80139118,
    func_actor_401800_8013E44C,
    func_actor_401800_8013E4F0,
    func_actor_401800_8013E5A4,
    NULL,
    func_actor_401800_8013945C,
    func_actor_401800_801399C4,
    func_actor_401800_80139B18,
    func_actor_401800_80139D60,
    func_actor_401800_8013A034,
    func_actor_401800_8013A2E8,
    func_actor_401800_8013AF1C,
    func_actor_401800_8013AB64,
    func_actor_401800_8013B444,
    func_actor_401800_8013B784,
    func_actor_401800_8013BB10,
    func_actor_401800_8013CD98,
    func_actor_401800_8013971C,
    func_actor_401800_80139870,
    func_actor_401800_8013BF48,
} };

void func_actor_401800_8013D64C(GpEnemy* arg0, Task* arg1)
{
    VECTOR                pos;
    Actor401800StateTable states;
    Actor401800Work*      work;
    ActorViewScratch*     scratch;
    ActorViewScratch*     head;
    s32                   state;
    s32                   stop;
    s32                   index;

    work   = arg1->work;
    states = D_actor_401800_80131FDC;

    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    pos.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);

    switch (Gp_StateF0.field_4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            Gp_ClearRec18Occupied(&work->field_A28);
            Gp_ClearRec18Occupied(&work->field_8E8);
            return;
    }

    head                                = *(ActorViewScratch**)G_SCRATCH_HEAD;
    *(ActorViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                             = head - 1;

    if (work->field_BE0 > 0) {
        work->field_BE0 = (s16)((u16)work->field_BE0 - 1);
    } else {
        func_actor_401800_80134C94(arg1);
    }
    if (work->field_2 != work->field_0) {
        if ((work->field_2 == 0xB) || (work->field_2 == 0xD)) {
            ((TmdObject*)arg1->extra)->coords->coord.t[0] = work->field_BF0;
            ((TmdObject*)arg1->extra)->coords->coord.t[1] = work->field_BF2;
            ((TmdObject*)arg1->extra)->coords->coord.t[2] = work->field_BF4;
            ((TmdObject*)arg1->extra)->coords->flg        = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
        }
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    SCHED_BARRIER();
    index = work->field_0;
    SCHED_BARRIER();
    stop = 0x15;
    TOUCH_REG(stop);
    states.fn[index](arg1);
    state = work->field_0;
    if ((state == 0x1C) || (state == stop) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8C8.flags &= 0x7FFF;
        work->field_A08.flags &= 0x7FFF;
    } else {
        work->field_8C8.flags |= 0x8000;
        work->field_A08.flags |= 0x8000;
    }
    Gp_ClearRec18Occupied(&work->field_A28);
    Gp_ClearRec18Occupied(&work->field_8E8);
    if ((Gp_StateF0.field_3 == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(((TmdObject*)arg1->extra)->coords + 2, &scratch->pos);

    work->field_C24[work->field_C74].vx = scratch->pos.vx;
    work->field_C24[work->field_C74].vy = scratch->pos.vy;
    work->field_C24[work->field_C74].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_C74        = (u16)work->field_C74 + 1;
    if (work->field_C74 == 7) {
        work->field_C74 = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        arg0->bodyPos.vx = work->field_C24[work->field_C74].vx;
        arg0->bodyPos.vy = work->field_C24[work->field_C74].vy;
        arg0->bodyPos.vz = work->field_C24[work->field_C74].vz;
    } else {
        arg0->bodyPos.vx = scratch->pos.vx;
        arg0->bodyPos.vy = scratch->pos.vy;
        arg0->bodyPos.vz = scratch->pos.vz;
    }
    arg0->coord = &gGfxViewCoord;
    TOUCH_REG(stop);
}

void func_actor_401800_8013DCB4(void)
{
}

/// The enemy task's three handlers, indexed by `Task::state`: the first
/// initialises the actor, the second runs it every frame, and the third tears
/// the enemy down.
const GpEnemyTaskFuncTable3 D_actor_401800_80132064 = { {
    func_actor_401800_8013423C,
    func_actor_401800_8013D64C,
    Gp_DestroyEnemy,
} };

/// The `0x7D3` handler of the `D_actor_401800_80155A80` table: maps the
/// requested state onto the work block's `field_89E` animation slot (5 selects
/// nothing), then resets the actor to state `0x11` with `field_2` cleared.
s32 func_actor_401800_8013DCBC(Task* arg0, s32 arg1, GpAnimArg* arg2)
{
    Actor401800Work* work = arg0->work;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Applies one of four model settings chosen by `arg2`: 0 sets the model's
/// flags to 0x80 and reallocates its buffers, 1 clears the flags and
/// reallocates them, 2 and 3 set bit 2 (3 clearing the rest first). Case 1
/// moves the actor to state 0x18, the others to state 0. Always returns 0.
s32 func_actor_401800_8013DD2C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor401800Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor401800Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

/// Returns 1 while the actor's enemy still has HP. Once it is down, returns 0
/// if the model carries bit 0x80 or lacks bit 2, and 1 otherwise.
s32 func_actor_401800_8013DDEC(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Places the model from `placement`: copies the translation into the root
/// coordinate, applies the X, Y and Z rotations in that order (re-fetching the
/// coordinate for every field), then stores the resulting heading - `ratan2`
/// of the rotation matrix's Z axis - in the work block's `field_16`. Always
/// returns 1.
s32 func_actor_401800_8013DE3C(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor401800Work* work;

    work                                          = (Actor401800Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->field_16                         = ratan2(-mx, mz);
    return 1;
}

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 func_actor_401800_8013DF3C(Task* task)
{
    Actor401800Work* work = (Actor401800Work*)task->work;
    PlayerStatus*    cfg  = &Player_Status;

    if (work->field_0 == 0xD) {
        if (cfg->hp > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}

/// Room request handler: copies the request's three leading bytes into the work
/// block, then dispatches on the request's room id and state pair.
///
/// Room `0x301` only accepts state `1` (mount); room `0x1002` accepts `0`
/// (reset) or `2`, which parks the actor's root coordinate at
/// (-0x595, 0, -0x5B1) and rebuilds its Y rotation from -0x400, clearing `flg`
/// so the coordinate tree recomputes. Anything else returns 0, leaving the work
/// block's `field_0` state alone. Same body as `Actor01900_Fn0A5A4`, which
/// takes the same record and dispatches on the same two room ids.
s32 func_actor_401800_8013DF80(Task* arg0, s32 arg1, u16* arg2)
{
    Actor401800Work* work;
    u16              room;
    u16              state;
    u16              state2;

    work = arg0->work;

    work->field_C10[0] = ((u8*)arg2)[0];
    work->field_C10[1] = ((u8*)arg2)[1];
    work->field_C10[2] = ((u8*)arg2)[2];

    room = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                                 = 0x1C;
                ((TmdObject*)arg0->extra)->coords->coord.t[0] = -0x595;
                ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0;
                ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, -0x400, 1);
                ((TmdObject*)arg0->extra)->coords->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

void func_actor_401800_8013E0A0(Task* task)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = (Actor401800Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C14 != NULL) {
            taskKill(work->field_C14);
        }
        if (work->field_C18 != NULL) {
            taskKill(work->field_C18);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401800_8013E138(Task* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 1;
        obj->flags                              = (u16)(obj->flags | 0x80);
        work->field_B48.flags                   = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags                   = (u16)(work->field_A08.flags & 0xBFFF);
    }
}

void func_actor_401800_8013E194(Task* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E23C(Task* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E2E8(Task* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E394(Task* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                     = arg0->extra;
        ((GpEnemy*)arg0->spawnArg2)->node.flags = 0;
        obj->flags                              = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401800_8013E44C(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 8;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        work->field_8A2                  = work->field_8A4;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401800_8013E4F0(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->flags = 0;
        work->field_8C8.radius           = 0x12C;
        work->field_B48.flags           &= 0x7FFF;
        work->field_A08.flags           |= 0x4000;
        enemy->node.flags                = 0;
        work->field_898                  = 2;
        work->field_89E                  = 0x16;
        work->field_8B0                  = 0;
        work->field_8AE                  = 0;
        work->field_6                    = 0;
        work->field_8A2                  = work->field_8A4;
    }
    work->field_6 = (u16)(work->field_6 + 1);
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

/// Idle-step handler: with the live flag set a fresh `Gp_LcgState` draw is
/// spread over the step countdown as 0..7 extra steps, and once the countdown
/// underflows the animation state at `field_89E` picks the actor's next
/// `field_0` (0xF for states 11/23, 0x10 for 12/24/25); a target with no HP
/// left forces 0x15 over that. Same body as `func_actor_401300_80141DF4`,
/// whose counterpart masks the LCG draw with 0xF instead of 7.
void func_actor_401800_8013E5A4(Task* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_C08 + ((Gp_LcgState >> 16) & 7);
    }
    if (--work->field_6 < 0) {
        switch (work->field_89E) {
            case 11:
            case 23:
                work->field_0 = 0xF;
                break;
            case 12:
            case 24:
            case 25:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->field_0 = 0x15;
    }
    func_actor_401800_80133EB8(arg0);
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401800_8013E68C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401800_80132064;
    sp.funcs[task->state](task->spawnArg2, task);
}
