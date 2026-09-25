#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80133cd0.h"
#include "actors/actors_shared_80134810.h"
#include "actors/actors_shared_80136288.h"
#include "actors/actors_shared_80136938.h"
#include "actors/actors_shared_80136c80.h"
#include "actors/actors_shared_8013777c.h"
#include "actors/actors_shared_80137cf4.h"
#include "actors/actors_shared_80137e18.h"
#include "actors/actors_shared_80137ea8.h"
#include "actors/actors_shared_80137f1c.h"
#include "actors/actors_shared_80138570.h"
#include "actors/actors_shared_80138640.h"
#include "actors/actors_shared_801511c8.h"
#include "actors/actors_shared_801673f8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`. It precedes the animation helpers
/// below, which call it.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation work reached through `Task::work`. `field_2B8`/`field_2BA`/
/// `field_2BC` are the same (id, id the three helper slots last saw, frames
/// spent on it) triple as `Actor207200Work`'s `field_28C`/`field_28E`/
/// `field_290`; a non-zero `field_2D2` suppresses the per-frame rebind.
///
/// The second triple, `field_370`/`field_372`/`field_374`, is the same thing
/// over the work's *six* helper slots, mirroring `Actor207200Work`'s
/// `field_48C`/`field_48E`/`field_490`.
///
/// `field_36A`/`field_36E` are the same pair `Actor103800Work` and
/// `ActorShared80137e18Work` carry: the reaction sub-state the damage branch
/// writes (3 here, 5 once the 0x600A5 spawn is armed) and a word cleared
/// alongside it.
///
/// `field_28C`/`field_2CA` are the same pair as `Actor207200Work`'s
/// `field_264`/`field_2A0`: the transform `Actor07000_Fn02BB8` folds onto the
/// model, and the angle it is scaled by.
typedef struct Actor107000Work {
    /* 0x000 */ byte     pad_0[0x11A];
    /* 0x11A */ u16      field_11A; // flag word of the render node at 0xFC, `Actor107000SpawnWork::objFC.flags`
    /* 0x11C */ GpRec18  field_11C; // that node's collision table, `Actor107000SpawnWork::rec11C`
    /* 0x134 */ byte     pad_134[0x1E];
    /* 0x152 */ u16      field_152;
    /* 0x154 */ GpRec18  field_154[4];
    /* 0x1B4 */ byte     pad_1B4[0x1E];
    /* 0x1D2 */ u16      field_1D2;
    /* 0x1D4 */ GpRec18  field_1D4;
    /* 0x1EC */ byte     pad_1EC[0x1E];
    /* 0x20A */ u16      field_20A;
    /* 0x20C */ byte     pad_20C[8];
    /* 0x214 */ GpRec18  field_214; // collision record `Actor07000_Fn046B8` re-rolls
    /* 0x22C */ byte     pad_22C[0x48];
    /* 0x274 */ VECTOR3  field_274; // saved root translation
    /* 0x280 */ byte     pad_280[4];
    /* 0x284 */ GpEffArg field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX   field_28C; // transform folded onto the model part
    /* 0x2AC */ s32      field_2AC; // advanced by 0xC8 a frame while the death flag runs
    /* 0x2B0 */ s16      field_2B0; // heading the specimen is turned toward; stepped 0x20 a frame
    /* 0x2B2 */ s16      field_2B2; // armed to 3 by the hit branch, with the pair below
    /* 0x2B4 */ s16      field_2B4; // cleared on the death branch
    /* 0x2B6 */ s16      field_2B6; // cleared next to `field_2B2`
    /* 0x2B8 */ s16      field_2B8; // animation id the work is playing
    /* 0x2BA */ s16      field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16      field_2BC; // frames spent on the current id
    /* 0x2BE */ s16      field_2BE; // cleared next to the pair above
    /* 0x2C0 */ byte     pad_2C0[0x6];
    /* 0x2C6 */ s16      field_2C6; // armed to 1 with the stage handoff, two bytes before the stage selector
    /* 0x2C8 */ s16      field_2C8; // reaction stage the per-frame handler switches on
    /* 0x2CA */ s16      field_2CA; // angle the transform is scaled by
    /* 0x2CC */ s16      field_2CC; // countdown seeded by the damage branch
    /* 0x2CE */ s16      field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16      field_2D0; // frames until the next sound cue; re-rolled from `Gp_LcgState`
    /* 0x2D2 */ s16      field_2D2; // non-zero: the rebind is suppressed
    /* 0x2D4 */ u16      field_2D4; // frames the reaction has run; the death branch fires at 5
    /* 0x2D6 */ s16      field_2D6; // selects the sound event's high half
    /* 0x2D8 */ s16      field_2D8; // latched copy of `field_2B8`
    /* 0x2DA */ s16      field_2DA;
    /* 0x2DC */ byte     pad_2DC[2];
    /* 0x2DE */ s16      field_2DE;
    /* 0x2E0 */ s16      field_2E0;
    /* 0x2E2 */ s16      field_2E2;
    /* 0x2E4 */ byte     pad_2E4[0x86];
    /* 0x36A */ s16      field_36A; // reaction sub-state, cleared once applied
    /* 0x36C */ s16      field_36C; // cleared next to `field_36A`
    /* 0x36E */ u16      field_36E; // cleared alongside `field_36A`
    /* 0x370 */ s16      field_370; // animation id the work is playing
    /* 0x372 */ u16      field_372; // id the six helper slots last saw
    /* 0x374 */ u16      field_374; // frames spent on the current id
    /* 0x376 */ byte     pad_376[0xC];
    /* 0x382 */ s16      field_382; // reaction branch the hit handler selects
    /* 0x384 */ byte     pad_384[0xC];
    /* 0x390 */ u16      field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16      field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16      field_394; // non-zero: this frame has spent its reaction (see ActorShared80136288Work)
} Actor107000Work;

/// The same 0x2E4-byte work block as its spawn handler builds it, seen from the
/// side that names the render nodes: four `GpObj`s at 0xFC / 0x134 / 0x1B4 /
/// 0x1EC, each followed by the `GpRec18` collision table its `ctx.recs` points
/// at, `node + 0x20` (`&work->rec11C` and friends, handed to
/// `Gp_InitRec18Table`), then the transform node at 0x27C whose coordinate the
/// handler only wires up through `field_284`.
///
/// The record table seeded at 0x20C overlaps `Actor107000Work::field_214`: the
/// handler view starts that run eight bytes later, and the matched
/// `Actor07000_Fn046B8` keeps its own spelling, so the two views cannot
/// be merged without moving one of the two offsets.
typedef struct Actor107000SpawnWork {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[3];
    /* 0x08C */ byte       field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX     field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX     field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ GpObj      objFC;
    /* 0x11C */ GpRec18    rec11C;
    /* 0x134 */ GpObj      obj134;
    /* 0x154 */ GpRec18    rec154[4];
    /* 0x1B4 */ GpObj      obj1B4;
    /* 0x1D4 */ GpRec18    rec1D4;
    /* 0x1EC */ GpObj      obj1EC;
    /* 0x20C */ GpRec18    rec20C;
    /* 0x224 */ byte       pad_224[0x58];
    /* 0x27C */ byte       field_27C[8];
    /* 0x284 */ void*      field_284; // render node at 0x27C: its coordinate
    /* 0x288 */ u16        field_288;
    /* 0x28A */ u16        field_28A;
    /* 0x28C */ byte       pad_28C[0x20];
    /* 0x2AC */ s32        field_2AC;
    /* 0x2B0 */ byte       pad_2B0[4];
    /* 0x2B4 */ s16        field_2B4; // cleared by the spawn handler, as Actor107000Work::field_2B4
    /* 0x2B6 */ byte       pad_2B6[2];
    /* 0x2B8 */ s16        field_2B8;
    /* 0x2BA */ s16        field_2BA;
    /* 0x2BC */ byte       pad_2BC[0x10];
    /* 0x2CC */ s16        field_2CC;
    /* 0x2CE */ s16        field_2CE;
    /* 0x2D0 */ byte       pad_2D0[2];
    /* 0x2D2 */ s16        field_2D2;
    /* 0x2D4 */ u16        field_2D4;
    /* 0x2D6 */ s16        field_2D6;
    /* 0x2D8 */ byte       pad_2D8[2];
    /* 0x2DA */ s16        field_2DA;
    /* 0x2DC */ s16        field_2DC;
    /* 0x2DE */ byte       pad_2DE[4];
    /* 0x2E2 */ s16        field_2E2;
} Actor107000SpawnWork;
STATIC_ASSERT_SIZEOF(Actor107000SpawnWork, 0x2E4);

void ActorsShared801349d8(Task*);

static __inline__ void Actor107000_TickAnim(Task* task)
{
    Actor107000Work* work = (Actor107000Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
    }
}

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// Collision-response scratch, followed by the normalized contact vector and
/// the result word passed to func_800E0C10.
typedef struct Actor107000ContactScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
    /* 0x38 */ VECTOR         normal;
    /* 0x48 */ s32            result;
} Actor107000ContactScratch;
STATIC_ASSERT_SIZEOF(Actor107000ContactScratch, 0x4C);

/// The 0x39C-byte work block the actor's *other* spawn handler
/// (`Actor07000_Fn05068`) allocates, next to `Actor107000SpawnWork`:
/// the same `GpAnimCtx`, seven animation slots instead of three, then three
/// `GpObj` render nodes where that one has four.
///
/// Node 1's `ctx.d4rec` is not a record table but the `GpActorD4Rec` at 0x1FC -
/// the shape `GpActorD4` keeps, where the record's own `recs` points at the
/// `GpRec18` run beside it (here the single record at 0x214). Nodes 2 and 3
/// hold plain tables of four and one, the way `Actor107000SpawnWork`'s do.
///
/// The tail from 0x360 is the same run `Actor107000Work` names from 0x360:
/// `field_370`/`field_372` are its animation id and the id the six helper slots
/// last saw, which is why the reset loop walks slots 1..6 and not 1..2.
typedef struct Actor107000Spawn2Work {
    /* 0x000 */ GpAnimCtx      context;
    /* 0x014 */ GpAnimSlot     slots[7];        // six helper slots + slot 0
    /* 0x12C */ byte           field_12C[0x70]; // pose buffer, func_800B3F84 arg3
    /* 0x19C */ MATRIX         field_19C;       // colour matrix, TmdObject::colorMtx
    /* 0x1BC */ MATRIX         field_1BC;       // light matrix, TmdObject::lightMtx
    /* 0x1DC */ GpObj          obj1;
    /* 0x1FC */ GpActorD4Rec   field_1FC;
    /* 0x214 */ GpRec18        field_214[1]; // the table `field_1FC` names
    /* 0x22C */ GpObj          obj2;
    /* 0x24C */ GpRec18        field_24C[4];
    /* 0x2AC */ GpObj          obj3;
    /* 0x2CC */ GpRec18        field_2CC[1];
    /* 0x2E4 */ SVECTOR        rotation;  // reaction rotation applied to model coordinates 3 and 5
    /* 0x2EC */ byte           pad_2EC[0x50];
    /* 0x33C */ VECTOR3        field_33C; // saved position restored by collision response 2
    /* 0x348 */ byte           pad_348[0x14];
    /* 0x35C */ GsCOORDINATE2* field_35C; // the model's second coordinate
    /* 0x360 */ u16            field_360;
    /* 0x362 */ u16            field_362;
    /* 0x364 */ s16            field_364; // spawn arg's high half
    /* 0x366 */ u16            field_366; // spawn arg's low half
    /* 0x368 */ byte           pad_368[0x2];
    /* 0x36A */ s16            field_36A; // reaction sub-state, as Actor107000Work::field_36A
    /* 0x36C */ s16            field_36C;
    /* 0x36E */ u16            field_36E;
    /* 0x370 */ s16            field_370; // animation id the work is playing
    /* 0x372 */ u16            field_372; // id the six helper slots last saw
    /* 0x374 */ u16            field_374;
    /* 0x376 */ byte           pad_376[2];
    /* 0x378 */ s16            field_378; // seeded to 0xC8 by the reveal arm
    /* 0x37A */ s16            field_37A;
    /* 0x37C */ byte           pad_37C[2];
    /* 0x37E */ s16            field_37E;
    /* 0x380 */ s16            field_380;
    /* 0x382 */ s16            field_382;
    /* 0x384 */ s16            field_384;
    /* 0x386 */ s16            field_386;
    /* 0x388 */ s16            field_388;
    /* 0x38A */ s16            field_38A;
    /* 0x38C */ s16            field_38C;
    /* 0x38E */ s16            field_38E;
    /* 0x390 */ u16            field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16            field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16            field_394; // non-zero: this frame has spent its reaction
    /* 0x396 */ u16            field_396; // armed to 1 by the reveal arm, cleared by the hide
    /* 0x398 */ s16            field_398; // seeded to 0x64 by the reveal arm
    /* 0x39A */ s16            field_39A; // cleared by the reveal arm
} Actor107000Spawn2Work;
STATIC_ASSERT_SIZEOF(Actor107000Spawn2Work, 0x39C);

/// Node 3's pair table, packed by `Gp_PackPair` into `obj1B4`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern GpU16Pair  Actor07000_D06924;
extern GpPairSrcE Actor07000_D06928;

extern u32 Actor07000_D06938[];
extern u32 Actor07000_D06944[];

/// Message dispatch table the caged specimen's spawn parks in `Task::msgTable`.
extern u8 Actor07000_D08030[];

/// The animation data `func_800B3F84` seeds the caged specimen's slots from.
extern u8 Actor07000_D08058[];

extern SVECTOR Actor07000_D08068;

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR Actor07000_D08070;

/// Pair table the second form's node 3 and the projectile's render node pack
/// into their keys.
extern GpU16Pair Actor07000_D08078;

/// Enemy record of the second form; its `hpMax` seeds the enemy's HP.
extern GpPairSrcE Actor07000_D08080;

/// Effect-setup records the spawned task picks up through `D_80062730`, one per
/// random variant the roll selects - the same role `D_actor_207200_801517F8`
/// plays for actor_207200. Only the address reaches the spawn, so the contents
/// stay opaque here.
extern u8 Actor07000_D0AB40[];
extern u8 Actor07000_D0B194[];
extern u8 Actor07000_D0B730[];

/// The animation data `func_800B3F84` seeds the second form's slots from.
extern u8 Actor07000_D0D77C[];

extern SVECTOR Actor07000_D0D7B0;

/// Offset the second form's collapse arms spawn the 0x60080 effect at,
/// `{ 0, -0x12C, 0 }`.
extern SVECTOR Actor07000_D0D7B8;

/// Message dispatch table the second form's spawn parks in `Task::msgTable`.
extern u8 Actor07000_D0D7C0[];

extern TaskDesc Actor07000_D0D7D0;

/// Setup-argument slot in the main executable; a task spawned through
/// `Gp_SpawnEff` reads it as its own setup argument (`D_800626EC[5].arg.model`,
/// see include/actors/actor_207200.h).
extern s32 D_80062730;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Actor07000_Fn000EC(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn00478(Task* arg0);
void Actor07000_Fn00654(Task* arg0);
void Actor07000_Fn00854(Task* arg0);
void Actor07000_Fn00A1C(Task* arg0);
void Actor07000_Fn00F6C(Task* arg0, s32 arg1);
void Actor07000_Fn0107C(Task* arg0);
void Actor07000_Fn011B4(GpEnemy* enemy, Task* task);
void Actor07000_Fn016A8(Task* arg0, u8 arg1);
void Actor07000_Fn01870(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn01BA0(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn01EB0(Task* arg0);
void Actor07000_Fn025A4(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn026BC(Task* arg0);
void Actor07000_Fn027D0(Task* task);
void Actor07000_Fn02860(Task* arg0);
void Actor07000_Fn02914(GpEnemy* arg0, Task* task);
void Actor07000_Fn02984(Task* task);
void Actor07000_Fn029F0(Task* arg0, GsCOORDINATE2* arg1);
void Actor07000_Fn02BB8(Task* arg0);
void Actor07000_Fn02CAC(Task* task);
void Actor07000_Fn02D78(Task* task);
void Actor07000_Fn02E0C(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn03164(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2);
void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2);
void Actor07000_Fn03E08(Task* arg0);
void Actor07000_Fn04274(Task* arg0, s32 arg1);
void Actor07000_Fn04468(GpEnemy* arg0, Task* arg1);
/// Picks the reaction branch the specimen takes on this hit and stores it in
/// `field_382`, then hands back the collision record the caller armed. A
/// countdown of 0xBB8 or more, or a record with no slot matching the 0x10000
/// kind, clears the branch and `field_36E` instead. Otherwise the branch is 3
/// when the first `Gp_LcgState` draw folds to under 11, 2 when the target is
/// 2500 units or further. Closer than that, a second draw is taken: it lands on
/// 1 when that draw folds to 11 or more, and the branch stays 2 when it does
/// not. Either way `field_374`/`field_372` are reset, and the record is released.
void Actor07000_Fn046B8(Task* arg0, s32 arg1);
s32  Actor07000_Fn047F4(GsCOORDINATE2* arg0, u32* arg1);
void Actor07000_Fn049C0(Task* arg0);
void Actor07000_Fn04B18(Task* arg0);
void Actor07000_Fn04E60(Task* arg0);
void Actor07000_Fn05068(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn05400(GpEnemy* arg0, Task* arg1);
void Actor07000_Fn0595C(Task* arg0);
void Actor07000_Fn05ED4(Task* arg0);
void Actor07000_Fn05F84(Task* task);
void Actor07000_Fn05FF8(Task* arg0);
void Actor07000_Fn06088(Task* arg0);
void Actor07000_Fn060FC(Task* arg0);
void Actor07000_Fn062A8(Task* arg0);
void Actor07000_Fn06390(Task* arg0);
void Actor07000_Fn0662C(Task* arg0);
void Actor07000_Fn066FC(Task* dst, Task* src);
void Actor07000_Fn06750(Task* task);
void Actor07000_Fn06820(Task* arg0);
void Actor07000_Fn068B4(Task* arg0);
void Actor07000_Fn068F0(Task* arg0);

/// Spawn handler of the specimen, the `GpEnemyTaskFunc` the task dispatch runs
/// first: it allocates the `Actor107000SpawnWork` block, wires the enemy's four
/// `GpObj` render nodes and their `GpRec18` tables into it and installs
/// `Actor07000_Fn02CAC` as the exit callback. The spawn arg's high halfword is the variant
/// the model was spawned as - when it is 1 the specimen is killed instead, and
/// the same halfword plus the low one seed `field_2DC`/`field_2D6`. Variant 1
/// with a matching `spawnType` is the one that carries a streamed model: its
/// texture page and CLUT row are stepped before the model is re-streamed twice.
void Actor07000_Fn000EC(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    u16                   v;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1 >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord              = part;
    arg0->node.state.b.flags = 0;
    arg0->bodyPos.vx         = 0;
    arg0->bodyPos.vy         = 0;
    arg0->bodyPos.vz         = 0;
    arg0->param              = &Actor07000_D06928;
    arg0->recs               = &work->rec154[0];
    arg0->hp                 = Actor07000_D06928.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D08058, obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.coord    = coord;
    work->objFC.ctx.recs = &work->rec11C;
    work->objFC.pos.vx   = 0;
    work->objFC.pos.vy   = 0;
    work->objFC.pos.vz   = 0;
    work->objFC.key      = 0;
    work->objFC.radius   = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags | 0x8000);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags | 0xC200);
    work->obj1B4.key      = Gp_PackPair(&Actor07000_D06924, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    work->field_2DC    = (s16)(arg1->spawnArg1 >> 16);
    v                  = (u16)arg1->spawnArg1;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->spawnType == (s16)v) {
        obj->tpage = obj->tpage + 1;
        obj->clut  = obj->clut + 1;
        if (obj->buffer != 0) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = Actor07000_Fn02CAC;
    arg1->state       += 1;
}

/// Task states of the caged specimen as `Actor07000_Fn02548` dispatches
/// them: spawn, per-frame update and teardown.
const GpEnemyTaskFuncTable3 Actor07000_D00004 = {
    { Actor07000_Fn000EC, Actor07000_Fn025A4, Actor07000_Fn011B4 },
};

/// Task states of the caged specimen as `Actor07000_Fn02D10` dispatches them:
/// the same update and teardown after a spawn that parks the specimen hidden,
/// and a fourth state for its drop into place.
const GpEnemyTaskFuncTable4 Actor07000_D00010 = {
    { Actor07000_Fn01870, Actor07000_Fn025A4, Actor07000_Fn011B4, Actor07000_Fn01BA0 },
};

/// Per-frame dispatch of the caged specimen, on the reaction state in
/// `field_2B2`: 0 is the dormant arm `Actor07000_Fn00654` and 1 the
/// live handler `Actor07000_Fn00854`. 3 is the arm the reaction
/// dispatch shoots when the enemy's flag byte carries bit 0x2 - it suppresses
/// the rebind, waits out the generic flag-2 helper on the spawn arg and, once
/// that expires, wakes the specimen: the rebind is released and
/// `field_2B2`/`field_2C8` move to 1, the live stage. The arm ends in
/// `Actor07000_Fn027D0` either way.
///
/// 4 and 5 are the two collapse arms. Both drive the model's second coordinate
/// through `Actor07000_Fn029F0`, count `field_2BC` up and spawn the
/// 0x60080 effect on the model's coordinate every 0x10 frames; 5 also counts
/// `field_2D4` and, on the third count, writes the same death sequence the
/// reaction dispatch does - a five-frame countdown, `field_2B4` cleared and the
/// task moved to state 2 - with the spawn arg's `field_40` cleared alongside.
/// Both arms end by re-suppressing the rebind, and the join the compiler builds
/// from their two assignments is what the original binary shows.
void Actor07000_Fn00478(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    u16              frames;

    work = (Actor107000Work*)arg0->work;
    switch (work->field_2B2) {
        case 0:
            Actor07000_Fn00654(arg0);
            return;
        case 1:
            Actor07000_Fn00854(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            Actor07000_Fn027D0(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            Actor07000_Fn029F0(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor07000_D08070);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            Actor07000_Fn029F0(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor07000_D08070);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}

/// Dormant arm of the caged specimen, the `field_2B2 == 0` arm of
/// `Actor07000_Fn00478`. The collision record at `field_11C` is polled for an
/// occupant of kind 0x10000; once one has been seen `field_2D8` stays latched,
/// and a latched specimen moves `field_2B2`/`field_2C8` to the live stage,
/// clears the 0x8000 bit of the render node's flag word and arms the global
/// state through `Gp_ArmStateF0`. The record is released either way.
///
/// While the work plays animation 1 the arm also counts `field_2D0` down and,
/// when it expires, re-rolls it from `Gp_LcgState` to between 0x50 and 0xB3
/// frames and cues a sound event, `field_2D6` choosing between the two ids.
/// `field_2BE` is then set from the frames spent on the animation - 0x14 in
/// the first window, -0x14 in the second, 0 outside both - the frame count
/// wraps at 0x63, and the root part takes one step through
/// `Actor07000_Fn027D0`. Eight bytes of the scratch stack are held across the
/// whole arm.
void Actor07000_Fn00654(Task* arg0)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor107000Work*)arg0->work;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->field_11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2 = 1;
        work->field_2C8 = 1;
        work->field_11A = (u16)(work->field_11A & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->field_11C);
    if (work->field_2B8 == 1) {
        countdown       = work->field_2D0 - 1;
        work->field_2D0 = countdown;
        if ((countdown << 16) <= 0) {
            rng             = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = rng;
            work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
            if (work->field_2D6 != 0) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460009;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x402E0001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
        work->field_2C6 = 1;
        work->field_2BE = 0;
        if ((u32)(work->field_2BC - 1) < 0x29) {
            work->field_2BE = 0x14;
        }
        if ((u32)(work->field_2BC - 0x33) < 0x29) {
            work->field_2BE = -0x14;
        }
        if ((s16)work->field_2BC >= 0x63) {
            work->field_2BC = 0;
        }
        Actor07000_Fn027D0(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}

/// Per-frame handler of the caged specimen, dispatched on the reaction stage in
/// `field_2C8`: 1 is the live specimen, 2 the death throes. Every stage ends in
/// the shared epilogue, so the switch's default is a jump straight there.
///
/// Stage 1 ticks `field_2D0` down and, when it runs out, re-rolls it from
/// `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and 0xB3.
/// The re-roll also cues a sound event: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. As in `Actor07000_Fn00F6C`, the whole assignment *and* its
/// call are written out in both arms - the join the compiler builds from them is
/// what the original binary shows. The stage then re-arms `field_2BE`, switches
/// the animation to 2, runs the frame through `Actor07000_Fn0107C` and
/// `Actor07000_Fn027D0`, and restarts `field_2BC` once it has spent 0x1D
/// frames on the id.
///
/// Stage 2 counts `field_2D4` up and advances `field_2AC` by 0xC8 a frame; on
/// the fifth frame the specimen is killed - `Actor07000_Fn016A8` runs
/// with a zero argument, the kill countdown is armed to 5, the reaction flag
/// `field_2B4` is cleared, the task state latches the stage it just ran, and the
/// enemy's `field_40` HP is zeroed.
void Actor07000_Fn00854(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor107000Work*)arg0->work;
    enemy = arg0->spawnArg2;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            Actor07000_Fn0107C(arg0);
            Actor07000_Fn027D0(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor07000_Fn016A8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}

void Actor07000_Fn00A1C(Task* arg0)
{
    s32                        damageState;
    TmdObject*                 object;
    GpEnemy*                   enemy;
    GpRec18*                   effectRec;
    VECTOR*                    normal;
    VECTOR*                    delta;
    s16                        cooldown;
    s32                        stage;
    s32                        contactStage;
    s32                        effect;
    s32                        pushY;
    s32                        movement;
    s32                        dx;
    s32                        dz;
    s32                        wallDx;
    s32                        wallDz;
    s32                        hitCooldown;
    s32                        boundedDepth;
    s32                        distance;
    s32                        z;
    u32                        id;
    u32                        damage;
    Actor107000Work*           work;
    GsCOORDINATE2*             coord;
    void*                      scratchHead;
    Actor107000ContactScratch* scratch;
    Actor107000Work*           contact;

    work        = (Actor107000Work*)arg0->work;
    scratchHead = (void*)(*(u32*)0x1F8003FC -= 0x4C);
    enemy       = arg0->spawnArg2;
    object      = arg0->extra;
    SOFT_TOUCH_REG_USE(object, scratchHead);
    coord    = object->coords;
    scratch  = scratchHead;
    movement = func_800E0C10(work->field_154, &scratch->delta, 4, &scratch->result);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] = (s32)(coord->coord.t[0] + scratch->delta.vx.h.hi);
            coord->coord.t[1] = (s32)(coord->coord.t[1] + scratch->delta.vy.h.hi);
            z                 = coord->coord.t[2] + scratch->delta.vz.h.hi;
            coord->coord.t[2] = z;
            break;
        case 2:
            coord->coord.t[0] = (s32)work->field_274.vx;
            coord->coord.t[1] = (s32)work->field_274.vy;
            z                 = work->field_274.vz;
            coord->coord.t[2] = z;
            break;
    }
    if (work->field_2CE != 0) {
        cooldown        = (u16)work->field_2CE - 1;
        work->field_2CE = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_2CE = 0;
        }
    }
    dx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vx.w = dx;
    scratch->delta.vy.w = (s32)(Player_Status.coordMtx->t[1] - coord->coord.t[1]);
    dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    scratch->delta.vz.w = dz;
    distance            = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x320) {
        stage = work->field_2C8;
        delta = (VECTOR*)&scratch->delta;
        if (stage == 1) {
            work->field_2D2 = stage;
            work->field_2C8 = 2;
            goto delta_ready;
        }
    } else {
    delta_ready:
        delta = (VECTOR*)&scratch->delta;
    }
    damageState = 2;
    contact     = work;
contact_loop:
    do {
        id = contact->field_154[0].key;
        switch (id & 0xFFFF0000) {
            case 0x10000:
                contactStage = work->field_2C8;
                if (contactStage == 1) {
                    work->field_2D2 = contactStage;
                    work->field_2C8 = 2;
                }
                break;
            case 0x20000:
                if (work->field_2CE == 0) {
                    damage = Gp_ComputeDamage(id, (u32)distance, 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2, contact->field_154[0].key, 0) != 0) {
                        Actor07000_Fn016A8(arg0, 1U);
                        arg0->killCountdown = 5;
                        arg0->state         = damageState;
                        work->field_2B4     = 0;
                        enemy->hp           = -1;
                    } else {
                        func_800E2C78(enemy, (s32)contact->field_154[0].key, (s32)damage, 0);
                        Actor07000_Fn00F6C(arg0, (s32)damage);
                        effect = Gp_GetIdParam0((s32)contact->field_154[0].key) & 0xFFFF;
                        if (effect == damageState)
                            goto effect_flag2;
                        if (effect < 3) {
                            if (effect == 1)
                                goto effect_react;
                        } else {
                            if (effect == 3)
                                goto effect_flag4;
                            if (effect == 9)
                                goto effect_flag2;
                        }
                        goto effect_done;
                    effect_react:
                        work->field_2D2 = effect;
                        work->field_2C8 = damageState;
                        goto effect_done;
                    effect_flag4:
                        Gp_SetObjFlag4(enemy, contact->field_154[0].key, 0);
                        goto effect_done;
                    effect_flag2:
                        Gp_SetObjFlag2(enemy, contact->field_154[0].key, 0);
                    effect_done:
                        if (enemy->hp > 0) {
                            func_800FDB18(Gp_GetIdParam1((s32)contact->field_154[0].key) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 1, NULL, &work->field_284);
                        }
                        hitCooldown = Gp_GetIdParam2((s32)contact->field_154[0].key);
                        if ((hitCooldown << 0x10) > 0) {
                            work->field_2CE = (s16)hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx              = coord->workm.t[0] - contact->field_154[0].point.vx;
                scratch->delta.vy.w = 0;
                scratch->delta.vx.w = wallDx;
                wallDz              = coord->workm.t[2] - contact->field_154[0].point.vz;
                scratch->delta.vz.w = wallDz;
                distance            = contact->field_154[0].depth - SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                boundedDepth        = distance;
                if (distance <= 0) {
                    boundedDepth = 0;
                }
                SOFT_TOUCH_REG_USE(boundedDepth, distance);
                distance            = boundedDepth;
                scratch->delta.vx.w = (s32)(coord->workm.t[0] - contact->field_154[0].point.vx);
                normal              = &scratch->normal;
                scratch->delta.vy.w = (s32)(coord->workm.t[1] - contact->field_154[0].point.vy);
                scratch->delta.vz.w = (s32)(coord->workm.t[2] - contact->field_154[0].point.vz);
                VectorNormal(delta, normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, delta);
                if ((u32)((u16)work->field_2B8 - 1) < 2U) {
                    coord->coord.t[0] = (s32)(coord->coord.t[0] + ((s32)(distance * scratch->delta.vx.w) >> 0xC));
                    pushY             = distance * scratch->delta.vy.w;
                    if (pushY < 0) {
                        coord->coord.t[1] = (s32)(coord->coord.t[1] + (pushY >> 0xC));
                    }
                    coord->coord.t[2] = (s32)(coord->coord.t[2] + ((s32)(distance * scratch->delta.vz.w) >> 0xC));
                }
                break;
        }
        contact = (Actor107000Work*)((u8*)contact + 0x18);
        if ((s32)contact < (s32)((u8*)work + 0x60))
            goto contact_loop;
    } while (0);
    Gp_ClearRec18Occupied(work->field_154);
    effectRec = &work->field_1D4;
    if ((work->field_2C8 != 0) && (Gp_FindRec18(effectRec, 0) != 0)) {
        work->field_1D2 = (u16)((u16)work->field_1D2 & 0x7FFF);
        Gp_ClearRec18Occupied(effectRec);
    }
    *(u32*)0x1F8003FC += 0x4C;
}

/// Damage reaction of the caged specimen. `arg1` is taken off the context's
/// HP, the same amount is pushed through the lock-slot updater, and a depleted
/// specimen switches the task to its death state (2) with a five-frame
/// countdown while `field_2B4` is cleared on the work.
///
/// A live one cues a sound event instead: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. The same call statement is written out in both arms - the join
/// the compiler builds from it is what the original binary shows.
///
/// `field_2CC` is then re-armed and, for the id the animation is playing
/// (`field_2B8 == 1`), latched into `field_2D8`.
void Actor07000_Fn00F6C(Task* arg0, s32 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor107000Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        Actor07000_Fn016A8(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    } else {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}

/// Turns the specimen toward the player by at most 0x20 a frame. The heading
/// `field_2B0` is compared with the XZ direction from the model's coordinate
/// to the player: within 0x20 it is taken outright, otherwise it is stepped
/// 0x20 the short way round the 0x1000 circle. The coordinate's rotation is
/// then rebuilt from that heading alone, in 0x18 bytes taken off the scratch
/// stack.
void Actor07000_Fn0107C(Task* arg0)
{
    Actor107000Work*  work;
    GsCOORDINATE2*    coord;
    ActorFaceScratch* sc;
    s16               cur;
    s32               want;
    s16               diff;
    s32               adiff;
    s16               turn;
    s16               wrap;
    s32               current;

    coord        = ((TmdObject*)arg0->extra)->coords;
    work         = (Actor107000Work*)arg0->work;
    sc           = (ActorFaceScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = 0;
    sc->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want         = ratan2((s16)sc->delta.vx, (s16)sc->delta.vz) & 0xFFF;
    cur          = work->field_2B0 & 0xFFF;
    diff         = want - cur;
    adiff        = diff >= 0 ? diff : -diff;
    turn         = diff;
    if (adiff < 0x21) {
        work->field_2B0 = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->field_2B0;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->field_2B0 = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->field_2B0;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

/// Teardown handler of the caged specimen, run once the task has moved to its
/// death state. `Gp_StateF0.field_4` mode 2 hides the model and mode 1 does nothing;
/// otherwise `field_2B4` steps the death through three phases. Phase 0 shrinks
/// the model and counts the kill countdown down, cueing the death sound,
/// releasing the global state and unlinking the enemy's node and the four
/// render nodes when it runs out (with a final effect if `field_2DA` asked for
/// one). Phase 1 folds the saved transform onto the model through
/// `Actor07000_Fn02BB8` for up to 0x3D frames, and phase 2 destroys the enemy
/// once the same count is spent. Outside the two collapse arms of `field_2B2`
/// the first two phases also tick the animation, scale the model's second
/// part, recompute it and re-colour the actor.
void Actor07000_Fn011B4(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor107000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj   = task->extra;
    work  = (Actor107000Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (Gp_StateF0.field_4) {
        case 1:
            break;
        case 2:
            model->flags             |= 0x80;
            enemy->node.state.b.flags = 1;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->field_20A &= 0x7FFF;
                    work->field_2AC -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = 0x80;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor107000_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, ((TmdObject*)task->extra)->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->objFC);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj134);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1B4);
                        Gp_UnlinkObj(&((Actor107000SpawnWork*)work)->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    Actor07000_Fn02BB8(task);
                    if (work->field_2B6 == 0xA) {
                        ((TmdObject*)task->extra)->flags = 2;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor107000_TickAnim(task);
                Actor07000_Fn029F0(task, &((TmdObject*)task->extra)->coords[1]);
                ((TmdObject*)task->extra)->coords[0].flg = 0;
                ((TmdObject*)task->extra)->coords[1].flg = 0;
                Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
                actorUpdateColor(enemy, &((TmdObject*)task->extra)->coords[1]);
            }
            break;
    }
}

void Actor07000_Fn016A8(Task* arg0, u8 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj         = arg0->extra;
    enemy       = arg0->spawnArg2;
    work        = (Actor107000Work*)arg0->work;
    coord       = obj->coords;
    enemy->hp   = 0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((Gp_LcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000B;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0003;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_1D2 |= 0x8000;
        work->field_20A |= 0x8000;
        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 1, NULL);
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x300, &Actor07000_D08068);
        Gp_SpawnScript18((s32)&Actor07000_D06938, (s32)&Actor07000_D06944);
        work->field_2DA = 1;
    } else {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000C;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0004;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_2B2 = 6;
    }
}

/// Spawn/setup handler of the caged specimen, entry 0 of
/// `Actor07000_D00010`. A task already on this handler (`spawnArg1`'s
/// high halfword reads 1) is torn down instead of spawned.
///
/// Otherwise it allocates the 0x2E4-byte work block and hangs it off the task:
/// the model's coordinate array feeds `field_18` with its second element, the
/// context's `field_4` with that element's matrix, and the block's two
/// `MATRIX`es become the model's colour and light matrices. The work's own
/// collision record is re-rolled from the same coordinate, and the four list
/// nodes are linked into the global object lists with their `GpRec18` tables -
/// the first three leave the 0x8000 last-element bit clear, and the second
/// node's 0x4000 bit is cleared once the third has been built.
///
/// The animation context is then seeded from `func_800B3F84` over the three
/// slots, slots 1 and 2 are reset, and the task moves to handler 3.
void Actor07000_Fn01870(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    TmdObject*            obj;
    s32                   one;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_2DC = (s16)(arg1->spawnArg1 >> 16);
    work->field_2D6 = (u16)arg1->spawnArg1;
    obj->flags      = 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_DC;
    obj->colorMtx   = &work->field_BC;
    arg0->field_4   = &coord[1].coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord              = part;
    arg0->node.state.b.flags = one;
    arg0->bodyPos.vx         = 0;
    arg0->bodyPos.vy         = 0;
    arg0->bodyPos.vz         = 0;
    arg0->param              = &Actor07000_D06928;
    arg0->recs               = &work->rec154[0];
    arg0->hp                 = Actor07000_D06928.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D08058, obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.coord    = coord;
    work->objFC.ctx.recs = &work->rec11C;
    work->objFC.pos.vx   = 0;
    work->objFC.pos.vy   = 0;
    work->objFC.pos.vz   = 0;
    work->objFC.key      = 0;
    work->objFC.radius   = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags & 0x3DFF);
    work->obj1B4.key      = Gp_PackPair(&Actor07000_D06924, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->field_2E2    = 0;
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->msgTable     = Actor07000_D08030;
    arg1->state        = 3;
}

/// Task states of the specimen's second form as `Actor07000_Fn05E6C`
/// dispatches them: spawn, per-frame update, death and destruction.
const GpEnemyTaskFuncTable4 Actor07000_D0003C = {
    { Actor07000_Fn02E0C, Actor07000_Fn03164, Actor07000_Fn04468, Gp_DestroyEnemy },
};

/// Task states of the specimen's second form as `Actor07000_Fn067B4`
/// dispatches them: the same update, death and destruction after a spawn that
/// parks the model hidden, and a fifth state for its drop into place.
const GpEnemyTaskFuncTable5 Actor07000_D0004C = {
    { Actor07000_Fn05068, Actor07000_Fn03164, Actor07000_Fn04468, Gp_DestroyEnemy, Actor07000_Fn05400 },
};

/// Per-frame handler of the specimen while it drops into place, before it
/// lands (the task state that follows `Actor07000_Fn01870`). `Gp_StateF0.field_4`
/// mode 1 only re-colours the actor and mode 2 hides the model; otherwise,
/// once `field_2E2` has armed the drop, the root part is stepped along its
/// facing and by the fall speed `field_2DE`, the collision response is
/// applied, the animation ticks and the root is recomputed, with the step
/// length `field_2BE` decaying by 2 a frame. When the root reaches the floor
/// (Y at or above 0) the landing sound is cued, the root is pinned at 0 and
/// the specimen moves to the live stage with animation 2 and task state 1;
/// until then the fall speed grows by 10 a frame, or 20 once the collision
/// response has latched `field_2E0`.
void Actor07000_Fn01BA0(GpEnemy* arg0, Task* arg1)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;

    work = (Actor107000Work*)arg1->work;
    switch (Gp_StateF0.field_4) {
        case 1:
            actorUpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            break;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.state.b.flags         = 1;
            break;
        case 0:
        default:
            if (work->field_2E2 == 0) {
                return;
            }
            Actor07000_Fn02D78(arg1);
            Actor07000_Fn01EB0(arg1);
            Actor107000_TickAnim(arg1);
            actorUpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            ((TmdObject*)arg1->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            work->field_2BE -= 2;
            if (work->field_2BE < 0) {
                work->field_2BE = 0;
            }
            coord = ((TmdObject*)arg1->extra)->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_2B2                               = 1;
                work->field_2C8                               = 1;
                work->field_2BE                               = 0;
                work->field_2DE                               = 0;
                work->field_2B8                               = 2;
                work->field_2BA                               = 0;
                ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                arg1->state                                   = 1;
            } else if (work->field_2E0 == 0) {
                work->field_2DE += 10;
            } else {
                work->field_2DE += 20;
            }
            break;
    }
}

/// Collision response of the dropping specimen: node 2's collision table is
/// run through `func_800E0C10` with a 0x48-byte scratch. Response 1 adds the
/// returned X and Z offsets to the root; only the first one also adds Y,
/// latches `field_2E0`, sets the fall speed `field_2DE` to -0x64 and takes a
/// quarter off the step length `field_2BE`. Response 2 puts the root back at
/// the translation `Actor07000_Fn02D78` saved. The table is released either
/// way.
void Actor07000_Fn01EB0(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    Actor107000Work*             work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (Actor107000Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274.vx;
            coord->coord.t[1] = work->field_274.vy;
            coord->coord.t[2] = work->field_274.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154[0]);
    SCRATCH_SP += 0x48;
}

s32 Actor07000_Fn01FF8(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    u16              word;
    s16              heading;
    s32              magnitude;
    s32              mode;
    s32              state;
    s32              sound;
    s32              pan;

    obj   = (TmdObject*)arg0->extra;
    enemy = arg0->spawnArg2;
    state = arg0->state;
    work  = (Actor107000Work*)arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = arg2->command;
        if (mode == 4) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor07000_D08070);
            work->field_2B8 = 1;
            Actor107000_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2B2 = 4;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor07000_D08070);
            work->field_2B8 = 1;
            Actor107000_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2D4 = 0;
            work->field_2B2 = 4;
            return 0;
        }
    }
    word = arg2->command & 0xFF;
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->command >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->command >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->command >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->command >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->command >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->command >> 8].z;
            }
            heading         = rot.vy;
            work->field_2B0 = heading;
            magnitude       = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->field_2B0 = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->field_2B0 = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers(arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            enemy->node.state.b.flags         = 0;
            work->field_11A                  |= 0x8000;
            work->field_152                  |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_2BE                        = 0xC8;
            work->field_2E2                        = 1;
            work->field_2DE                        = 0x64;
            work->field_2E0                        = 0;
            work->field_2B2                        = 1;
            work->field_2C8                        = 1;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        enemy->node.state.b.flags         = 1;
        work->field_11A                  &= 0x7FFF;
        work->field_152                  &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 3;
        work->field_2E2 = 0;
        work->field_2B2 = 0;
        work->field_2C8 = 0;
    }
    return 0;
}

/// Task handler of the caged specimen: runs the entry of `Actor07000_D00004`
/// for the task's state - spawn, per-frame update or teardown - with the
/// enemy and the task. The table is copied onto the stack before the call.
void Actor07000_Fn02548(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor07000_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// Per-frame mode handler of the actor, shared with the other enemy actors that
/// gate on `Gp_StateF0.field_4`: mode 1 runs only the tail, mode 2 puts the model in
/// its hidden pose and returns, mode 0 clears both flags before falling into
/// the update, and any other mode updates directly. The update drives the
/// actor's four handlers, clears the display flags of the model's first two
/// coordinate parts and recomputes the second one's world matrix; the tail then
/// colours the actor from that second part and draws its ground shadow.
void Actor07000_Fn025A4(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->node.state.b.flags         = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.state.b.flags         = one;
    return;
default_body:
    Actor07000_Fn00478(arg1);
    Actor07000_Fn026BC(arg1);
    Actor07000_Fn00A1C(arg1);
    Actor07000_Fn02860(arg1);
    Actor07000_Fn029F0(arg1, &((TmdObject*)arg1->extra)->coords[1]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor07000_Fn02914(arg0, arg1);
    Actor07000_Fn02984(arg1);
}

/// Per-frame reaction dispatch, gated on the enemy's flag byte: bit 0x1 runs
/// the death countdown and, once `field_2D4` reaches 5, kills the actor and
/// clears its HP; bit 0x2 arms the reaction sub-state and suppresses the
/// animation rebind; and bits 0x4/0x8 tick the generic flag-4 helper, folding
/// the damage it reports into `Actor07000_Fn00F6C` and clearing the
/// bits once they expire.
void Actor07000_Fn026BC(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = (Actor107000Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            work->field_2D4 += 1;
            work->field_2AC += 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor07000_Fn016A8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = 2;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_2B2       = 3;
            work->field_2B6       = 0;
            work->field_2BE       = 0;
            work->field_2D2       = 1;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4(enemy);
            if (tick != 0) {
                Actor07000_Fn00F6C(arg0, tick);
            }
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}

/// Steps the specimen's root part one frame along its own facing: saves the
/// current translation in `field_274` as the previous position, then advances
/// X and Z along the rotation's Z column scaled by the step length
/// `field_2BE`, and Y by a fixed 0x80.
void Actor07000_Fn027D0(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor107000Work* work;

    coord              = &((TmdObject*)task->extra)->coords[0];
    work               = (Actor107000Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}

/// Rebinds the animation id `field_2B8` to the specimen's two helper slots
/// unless `field_2D2` suppresses the rebind. When the id has changed since the
/// last frame `field_2BA` follows it, the frame count `field_2BC` restarts and
/// both slots are pointed at the new id; otherwise the count ticks and the
/// slots advance by one frame.
void Actor07000_Fn02860(Task* arg0)
{
    Actor107000Work* work;
    s32              i;

    work = (Actor107000Work*)arg0->work;
    if (work->field_2D2 == 0) {
        i = 1;
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            do {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
                i++;
            } while (i < 3);
            return;
        }
        TOUCH_REG(i);
        work->field_2BC = (u16)(work->field_2BC + i);
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 3);
    }
}

/// Colours the caged specimen from the world position of the model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack; `arg0` is the
/// colour target.
void Actor07000_Fn02914(GpEnemy* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Draws the specimen's ground shadow under the model root, at the world
/// translation of the root part, staged in a `VECTOR3` taken off the scratch
/// stack.
void Actor07000_Fn02984(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    coord   = ((TmdObject*)task->extra)->coords;
    vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_SP += 0x18;
}

/// Scales the rotation of `arg1`'s matrix by the specimen's scale factor
/// `field_2AC`, clamped first to 0x1000..0x13E8: each of the matrix's three
/// columns is copied into an `SVECTOR` on the scratch stack, multiplied by the
/// factor on the GTE and written back.
void Actor07000_Fn029F0(Task* arg0, GsCOORDINATE2* arg1)
{
    ActorScaleScratchHead* scratch;
    SVECTOR*               vec;
    MATRIX*                matrix;
    Actor107000Work*       work;

    scratch = (ActorScaleScratchHead*)G_SCRATCH_HEAD;
    vec     = scratch->head;
    work    = arg0->work;
    vec--;
    scratch->head = vec;
    if ((u32)work->field_2AC >= 0x13E8U) {
        work->field_2AC = 0x13E8;
    }
    if ((u32)work->field_2AC < 0x1001U) {
        work->field_2AC = 0x1000;
    }
    matrix = &arg1->coord;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 0, 6, 12);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 2, 8, 14);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 4, 10, 16);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 4, 10, 16);

    scratch->head = (u8*)scratch->head + 8;
}

/// Rebuilds the model's root coordinate from the transform saved in
/// `field_28C`, scaled along Y by `field_2CA`, which decays by 0x50 a frame
/// while it stays above 0x200. The scale matrix and its `VECTOR` live in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void Actor07000_Fn02BB8(Task* arg0)
{
    GsCOORDINATE2*     coord;
    MATRIX*            head;
    ActorScaleScratch* scratch;
    Actor107000Work*   work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorScaleScratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2CA >= 0x201) {
        work->field_2CA = (u16)work->field_2CA - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2CA;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_28C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Exit callback of the caged specimen: takes the enemy's node and the four
/// render nodes of the work block off their lists and runs the common enemy
/// task exit.
void Actor07000_Fn02CAC(Task* task)
{
    Actor107000SpawnWork* work;
    GpEnemy*              enemy;

    enemy = task->spawnArg2;
    work  = (Actor107000SpawnWork*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->objFC);
    Gp_UnlinkObj(&work->obj134);
    Gp_UnlinkObj(&work->obj1B4);
    Gp_UnlinkObj(&work->obj1EC);
    Gp_EnemyTaskExit(task);
}

void Actor07000_Fn02D10(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor07000_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Steps the dropping specimen's root part one frame: saves the current
/// translation in `field_274`, advances X and Z along the rotation's Z column
/// scaled by the step length `field_2BE`, and Y by the fall speed `field_2DE`.
void Actor07000_Fn02D78(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor107000Work* work;

    coord              = ((TmdObject*)task->extra)->coords;
    work               = (Actor107000Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += work->field_2DE;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
}

/// Spawn handler of the specimen's second form, entry 0 of
/// `Actor07000_D0003C`: allocates the 0x39C-byte `Actor107000Spawn2Work`,
/// rebinds the model's light and colour matrices into it, links the enemy
/// node and the three render nodes with their collision tables, seeds the six
/// helper animation slots, draws the two `Gp_LcgState` timers `field_390`/
/// `field_392`, installs `Actor07000_Fn06750` as the exit callback and moves
/// the task on to its per-frame state.
void Actor07000_Fn02E0C(GpEnemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    GpRec18*               table;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord6;
    u32                    rng;
    u32                    rng2;
    s32                    i;

    obj   = arg1->extra;
    coord = obj->coords;
    work  = memCalloc(0x39C, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    coord6         = &coord[6];
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->bodyPos.vy         = -0x64;
    arg0->coord              = coord;
    arg0->node.state.b.flags = 0;
    arg0->bodyPos.vx         = 0;
    arg0->bodyPos.vz         = 0;
    arg0->param              = &Actor07000_D08080;
    arg0->hp                 = Actor07000_D08080.hpMax;
    arg0->recs               = work->field_24C;
    work->field_35C          = &((TmdObject*)arg1->extra)->coords[1];
    work->field_360          = 0x280;
    work->field_362          = 2;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D0D77C, obj,
                  work->field_12C, work->slots);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_370            = 1;
    work->field_372            = 1;
    work->field_388            = 0;
    work->field_384            = 0;
    work->field_386            = 0;
    work->field_38E            = 0;
    work->field_38A            = 0;
    work->field_1FC.end0.vz    = 0xBB8;
    work->field_1FC.end0Radius = 0xFA0;
    work->field_1FC.end1Radius = 0x7D0;
    table                      = work->field_214;
    work->field_1FC.recs       = table;
    work->obj1.ctx.d4rec       = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = 3;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(table, 1, 0);
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = work->field_24C;
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = -0x15E;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x3002A;
    work->obj2.radius   = 0x15E;
    work->obj2.flags    = 1;
    work->obj1.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->field_24C, 4, 0);
    work->obj2.flags   |= 0xC200;
    work->obj3.coord    = coord6;
    work->obj3.ctx.recs = work->field_2CC;
    work->obj3.pos.vx   = -0x154;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj3.key      = Gp_PackPair(&Actor07000_D08078, 0);
    work->obj3.radius   = 0x1F4;
    work->obj3.flags    = 1;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(work->field_2CC, 1, 0);
    work->field_394    = 0;
    work->obj3.flags  &= 0x7FFF;
    rng                = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState        = rng;
    work->field_390    = (u16)((rng >> 16) % 20U + 0x50);
    rng2               = rng * 5 + 0x71357911;
    Gp_LcgState        = rng2;
    work->field_392    = (u16)((rng2 >> 16) % 50U + 0x32);
    arg1->exitCallback = Actor07000_Fn06750;
    arg1->state       += 1;
}

/// Per-frame mode handler of the specimen. The `Gp_StateF0.field_4` switch is the same
/// one `Actor07000_Fn025A4` runs: mode 1 skips to the tail, mode 2 puts
/// the model in its hidden pose and returns, mode 0 clears both flags and falls
/// into the body. The body first dispatches the reaction sub-state `field_36A` -
/// 0 and 1 hand the frame to their own handler, 3 counts `field_36E` out to 0xB
/// before resyncing the animation and dropping back to 0 once the enemy's flag-2
/// fires, 4 does the count alone, and 5 adds the 0x392 spawn counter whose fifth
/// hit re-cues the impact sound, switches the task to its death state and clears
/// the transform angle. 4 and 5 share the `field_390` timer that spawns a
/// 0x60080 effect every 0x10 frames. The tail then twists the model's second
/// coordinate part, runs the five per-frame helpers, and clears the display
/// flags of the model's first two parts before recomputing the second.
void Actor07000_Fn03164(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s32              soundId;

    obj   = (TmdObject*)arg1->extra;
    state = Gp_StateF0.field_4;
    work  = (Actor107000Work*)arg1->work;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags               = 0;
    arg0->node.state.b.flags = 0;
    goto default_body;
case2:
    obj->flags               = 0x80;
    arg0->node.state.b.flags = one;
    return;
default_body:
    switch (work->field_36A) {
        case 0:
            Actor07000_Fn03460(arg1, obj, one);
            break;
        case 1:
            Actor07000_Fn037EC(arg1, obj, one);
            break;
        case 3:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            if (Gp_TickObjFlag2(arg1->spawnArg2) != 0) {
                work->field_36A = 0;
            }
            break;
        case 4:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372 = 2;
                work->field_370 = 5;
                work->field_374 = 0;
                work->field_36E = 0;
            }
            goto block_21;
        case 5:
            work->field_36E += 1;
            if ((s16)work->field_36E >= 0xB) {
                work->field_372  = 2;
                work->field_370  = 5;
                work->field_374  = 0;
                work->field_36E  = 0;
                work->field_392 += 1;
                if ((u32)work->field_392 >= 5U) {
                    SndEvt_EnqueueType7(0x40460003, 0);
                    soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460005;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    work->field_2CA &= 0x7FFF;
                    arg1->state      = 2;
                    work->field_36C  = 0;
                }
            }
        block_21:
            work->field_390 += 1;
            if ((u32)work->field_390 >= 0x10U) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg1->extra)->coords, 0x400, &Actor07000_D0D7B8);
                work->field_390 = 0;
            }
            break;
    }
    coord->coord.t[1] += 0x80;
    Actor07000_Fn0662C(arg1);
    Actor07000_Fn03E08(arg1);
    Actor07000_Fn05ED4(arg1);
    Actor07000_Fn060FC(arg1);
    Actor07000_Fn06390(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor07000_Fn05F84(arg1);
}

void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    s32                    soundId;
    s16                    state;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    state = work->field_370 - 1;
    switch (state) {
        case 0:
            work->field_36E++;
            if ((s16)work->field_36E > work->field_390) {
                work->field_36E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_390 = (Gp_LcgState >> 16) % 20 + 80;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                if ((u16)((Gp_LcgState >> 16) % 100) < 31U) {
                    work->field_370 = 2;
                } else {
                    work->field_370 = 9;
                }
            }
            work->field_380 = 1;
            work->field_378 = 0;
            break;
        case 1:
            work->field_380 = 1;
            work->field_378 = 0;
            if ((s16)work->field_374 == 10) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460003;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_374 == 105) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460006;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_374 >= 110) {
                work->field_370 = 1;
            }
            break;
        case 8:
            work->field_378 = 0;
            if ((s16)work->field_374 >= 18) {
                work->field_38E = 1;
            }
            if ((s16)work->field_374 >= work->field_392 + 18) {
                work->field_38E = 0;
                work->field_370 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_392 = (Gp_LcgState >> 16) % 50 + 50;
            }
            break;
        case 4:
            if ((s16)work->field_374 < 22) {
                return;
            }
            work->field_370 = 2;
            break;
        case 5:
            if ((s16)work->field_374 >= 57) {
                work->field_36A = 1;
                work->field_36E = 0;
                work->field_382 = 0;
            }
            break;
    }
    if (Gp_CountRec18Hi(work->field_214, 0x10000) != 0 && (u16)work->field_38E != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 0;
    }
    if (Gp_CountRec18Hi(work->field_24C, 0x10000) != 0 || work->field_388 != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 2;
    }
    Gp_ClearRec18Occupied(work->field_214);
}

void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2)
{
    u32                    distance;
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    GpEnemy*               enemy;
    s32                    soundId;
    s16                    amount;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_382) {
        case 0:
            work->field_370 = 9;
            if ((s16)work->field_374 >= 18) {
                Gp_SetStateF0Byte3(1);
                Gp_ArmStateF0(1);
            }
            Actor07000_Fn047F4(((TmdObject*)arg0->extra)->coords, &distance);
            if (Gp_CountRec18Hi(work->field_214, 0x10000) == 0 || distance >= 5000U) {
                work->field_36E++;
                if ((s16)work->field_36E > work->field_390) {
                    work->field_36E = 0;
                    work->field_36A = 0;
                    work->field_370 = 2;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_390 = (Gp_LcgState >> 16) % 20 + 80;
                }
            } else {
                work->field_382 = 1;
            }
            Gp_ClearRec18Occupied(work->field_214);
            break;
        case 1:
            Gp_ArmStateF0(1);
            work->field_370 = 4;
            work->field_378 = 0;
            Actor07000_Fn047F4(((TmdObject*)arg0->extra)->coords, &distance);
            if (distance < 900U) {
                work->field_386 = 0;
            } else if (distance > 2700U) {
                work->field_386 = 0x2000;
            } else {
                work->field_386 = ((distance - 900) << 9) / 100;
            }
            if ((s16)work->field_374 == 40) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((u32)(work->field_374 - 32) < 10U) {
                amount = work->field_386;
                if (work->field_384 < amount) {
                    work->field_384 += amount >> 3;
                }
            } else if ((s16)work->field_374 >= 42) {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            }
            if ((u32)(work->field_374 - 32) < 11U) {
                work->obj3.flags |= 0x8000;
            } else {
                work->obj3.flags &= 0x7FFF;
            }
            if ((s16)work->field_374 >= 64) {
                Actor07000_Fn046B8(arg0, 1);
            }
            break;
        case 2:
            Gp_ArmStateF0(1);
            work->field_370 = 3;
            if ((s16)work->field_374 == 48) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->obj3.flags &= 0x7FFF;
                    arg0->state       = 2;
                    work->field_36C   = 0;
                    return;
                }
            }
            if (work->field_388 != 0 && (s16)work->field_374 == 50) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if ((s16)work->field_374 >= 83) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if (work->field_384 >= 0x200) {
                work->field_384 -= 0x250;
            } else {
                work->field_384 = 0;
            }
            work->obj3.flags &= 0x7FFF;
            break;
        case 3:
            work->field_370 = 2;
            if ((s16)work->field_374 == 105) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460006;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_374 >= 110) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
        case 4:
            work->field_370 = 7;
            Actor07000_Fn047F4(((TmdObject*)arg0->extra)->coords, &distance);
            if (distance < 900U) {
                work->field_386 = 0;
            } else if (distance > 2700U) {
                work->field_386 = 0x2000;
            } else {
                work->field_386 = ((distance - 900) << 9) / 100;
            }
            if ((s16)work->field_374 == 30) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((u32)(work->field_374 - 25) < 10U) {
                amount = work->field_386;
                if (work->field_384 < amount) {
                    work->field_384 += amount >> 3;
                }
            } else if ((s16)work->field_374 >= 35) {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            } else {
                if (work->field_384 >= 0x200) {
                    work->field_384 -= 0x250;
                } else {
                    work->field_384 = 0;
                }
            }
            if ((u32)(work->field_374 - 25) < 11U) {
                work->obj3.flags |= 0x8000;
            } else {
                work->obj3.flags &= 0x7FFF;
            }
            if ((s16)work->field_374 == 2) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->obj3.flags &= 0x7FFF;
                    arg0->state       = 2;
                    work->field_36C   = 0;
                    return;
                }
            }
            if ((s16)work->field_374 >= 47) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
    }
}

void Actor07000_Fn03E08(Task* arg0)
{
    s32                          movement;
    s32                          dx;
    s32                          dy;
    s32                          dz;
    s32                          reaction;
    s32                          cooldown;
    u32                          random;
    u32                          kind;
    u32                          damage;
    s32                          i;
    Actor107000Spawn2Work*       work;
    GsCOORDINATE2*               coord;
    GpEnemy*                     enemy;
    void*                        head;
    ActorsShared8013777cScratch* scratch;

    work     = arg0->work;
    head     = (void*)(*(u32*)0x1F8003FC -= 0x38);
    coord    = ((TmdObject*)arg0->extra)->coords;
    enemy    = arg0->spawnArg2;
    scratch  = head;
    movement = func_800E0C10(work->field_24C, &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0]  = (s32)(coord->coord.t[0] + scratch->delta.vx.h.hi);
            coord->coord.t[1]  = (s32)(coord->coord.t[1] + scratch->delta.vy.h.hi);
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = (s32)work->field_33C.vx;
            coord->coord.t[1] = (s32)work->field_33C.vy;
            coord->coord.t[2] = work->field_33C.vz;
            break;
    }
    if (work->field_38A != 0) {
        if (--work->field_38A <= 0) {
            work->field_38A = 0;
        }
    }
    for (i = 0; i < 4; i++) {
        kind = work->field_24C[i].key & 0xFFFF0000;
        switch (kind) {
            case 0x20000:
                if (work->field_38A == 0) {
                    dx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                    scratch->delta.vx.w = dx;
                    dy                  = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                    scratch->delta.vy.w = dy;
                    dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                    scratch->delta.vz.w = dz;
                    damage              = Gp_ComputeDamage(work->field_24C[i].key, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2, work->field_24C[i].key, 0) != 0) {
                        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 0, 0);
                        damage *= 4;
                    }
                    func_800E2C78(enemy, (s32)work->field_24C[i].key, (s32)damage, 0);
                    Actor07000_Fn04274(arg0, (s32)damage);
                    reaction = Gp_GetIdParam0((s32)work->field_24C[i].key) & 0xFFFF;
                    switch (reaction) {
                        case 1:
                        case 7:
                            if (work->field_36A < 2) {
                                work->field_374 = 0;
                                work->field_36A = 1;
                                work->field_36E = 0;
                                work->field_382 = 4;
                            }
                            break;
                        case 3:
                            Gp_SetObjFlag4(enemy, (s32)work->field_24C[i].key, 0);
                            break;
                        case 2:
                        case 8:
                        case 9:
                            Gp_SetObjFlag2(enemy, (s32)work->field_24C[i].key, 0);
                            break;
                        case 4:
                        case 6:
                            if (enemy->hp < 0) {
                                Actor07000_Fn049C0(arg0);
                                work->field_394 = 1;
                            }
                            break;
                    }
                    work->field_38E = 1;
                    func_800FDB18(Gp_GetIdParam1((s32)work->field_24C[i].key) & 0xFFFF, (((TmdObject*)arg0->extra)->coords + 1), &Actor07000_D0D7B0, (GpEffArg*)&work->field_35C);
                    cooldown = Gp_GetIdParam2((s32)work->field_24C[i].key);
                    if ((cooldown << 0x10) > 0) {
                        work->field_38A = (s16)cooldown;
                    }
                    work->field_38C   = 1;
                    random            = (Gp_LcgState * 5) + 0x71357911;
                    Gp_LcgState       = random;
                    work->rotation.vx = (s16)(((random >> 0xB) & 0x60) + 0x100);
                }
                break;
            case 0x10000:
                if (work->field_36A == 0) {
                    work->field_36A = 1;
                    work->field_36E = 0;
                    work->field_382 = 2;
                }
                break;
            case 0x30000:
                if (work->field_37E == 0) {
                    if ((Gp_CountRec18Hi(work->field_24C, 0x30000) != 0) && (work->field_37A == 0)) {
                        work->field_378 = 0;
                        work->field_37A = 1;
                        work->field_370 = 1;
                        work->field_37E = 1;
                    }
                } else if (work->field_37A == 0) {
                    work->field_37E = (s16)((u16)work->field_37E - 1);
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_24C);
    Gp_ClearRec18Occupied(work->field_2CC);
    *(u32*)0x1F8003FC += 0x38;
}

/// Hit reaction of the specimen. `arg1` comes off the context's HP countdown
/// and is pushed through the lock-slot updater by the same amount. A spent
/// countdown switches the task to its death state (2), clears the transform
/// angle and drops the work out of the pose; a live one cues the impact sound
/// - bits 12+ of the context's `field_8` pick the sound bank - and then walks
/// the reaction sub-state `field_36A` through its wind-up.
///
/// The sub-state is only advanced while it sits below 2: `arg1` at or above
/// 0x33 lands on the long recoil (sub-state 1, animation 4) and 0x15 or above
/// on the short one (sub-state 0, animation 6). Below both, an idle sub-state
/// with no branch selected re-measures the coordinate with
/// `Actor07000_Fn047F4` and picks branch 2 once the target is 2500
/// units away, branch 1 otherwise.
void Actor07000_Fn04274(Task* arg0, s32 arg1)
{
    u32              sp10;
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s16              state;

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor107000Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        SndEvt_EnqueueType7(0x40460003, 0);
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460005;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        work->field_2CA &= 0x7FFF;
        arg0->state      = 2;
        work->field_36C  = 0;
        return;
    }
    SndEvt_EnqueueType7(0x40460003, 0);
    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460004;
    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    state = work->field_36A;
    if (state < 2) {
        if ((u32)arg1 >= 0x33) {
            work->field_36A = 1;
            work->field_374 = 0;
            work->field_36E = 0;
            work->field_382 = 4;
            return;
        }
        if ((u32)arg1 >= 0x15) {
            work->field_374 = 0;
            work->field_36A = 0;
            work->field_36E = 0;
            work->field_370 = 6;
            return;
        }
        if (state == 0 || work->field_382 == 0) {
            Actor07000_Fn047F4(((TmdObject*)arg0->extra)->coords, &sp10);
            work->field_36A = 1;
            work->field_36E = 0;
            if (sp10 >= 0x9C4) {
                work->field_382 = 2;
                return;
            }
            work->field_382 = 1;
        }
    }
}

/// Task states of a specimen projectile as `Actor07000_Fn06338` dispatches
/// them: launch, flight and the countdown after impact.
const TaskFuncTable3 Actor07000_D000E0 = {
    { Actor07000_Fn04B18, Actor07000_Fn04E60, Actor07000_Fn068B4 },
};

/// Death handler of the specimen's second form, entry 2 of
/// `Actor07000_D0003C`. `Gp_StateF0.field_4` mode 1 does nothing and mode 2 hides the
/// model. Otherwise `field_36C` steps the death: phase 0 (unless `field_394`
/// has spent the frame's reaction) cues the death sound, sets the model's flag
/// word to 2 and splices a scaling coordinate into the model through
/// `Actor07000_Fn05FF8`, then unlinks the enemy node and the three
/// render nodes, releases the global state and switches the animation to 0xC;
/// phase 1 flattens that coordinate through `Actor07000_Fn06088` for 0x3D
/// frames; phase 2 cues the closing sound,
/// hides the model, re-parents its second coordinate onto the root and moves
/// the task to state 3. The six helper animation slots are then rebound or
/// ticked with the lighting mode set to 1.
void Actor07000_Fn04468(GpEnemy* arg0, Task* arg1)
{
    ActorShared80136288Work* work;
    ActorShared80136288Work* work2;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           part;
    u16                      ticks;
    s32                      state;
    s32                      one;
    s32                      i;

    obj   = (TmdObject*)arg1->extra;
    state = Gp_StateF0.field_4;
    work  = (ActorShared80136288Work*)arg1->work;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case2:
    obj->flags               = obj->flags | 0x80;
    arg0->node.state.b.flags = one;
    return;
default_body:
    switch (work->field_36C) {
        case 0:
            if (work->field_394 == 0) {
                SndEvt_EnqueueType6(0xD, 0, 0);
                obj->flags = 2;
                Actor07000_Fn05FF8(arg1);
            }
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_1DC);
            Gp_UnlinkObj(&work->field_22C);
            Gp_UnlinkObj(&work->field_2AC);
            Gp_ReleaseStateF0Add(arg1, 0x2A);
            work->field_370 = 0xC;
            work->field_36E = 0;
            work->field_36C = 1;
            break;
        case 1:
            if (work->field_394 == 0) {
                Actor07000_Fn06088(arg1);
            } else {
                obj->flags = 0x80;
            }
            ticks           = work->field_36E + 1;
            work->field_36E = ticks;
            if ((s16)ticks >= 0x3D) {
                work->field_36C = 2;
            }
            break;
        case 2:
            SndEvt_EnqueueType7(0xD, 1);
            obj->flags  = 0x80;
            part->sub   = coord;
            arg1->state = 3;
            break;
    }
    Gp_SetLightMode(arg0, 1);
    work2 = (ActorShared80136288Work*)arg1->work;
    i     = 1;
    if (work2->field_370 != (s16)work2->field_372) {
        work2->field_372 = work2->field_370;
        work2->field_374 = 0;
        do {
            func_800B4114((GpAnimCtx*)work2, i, work2->field_370, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work2->field_374 = (u16)(work2->field_374 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work2, i);
        i++;
    } while (i < 7);
case1:
    return;
}

/// Both `Gp_LcgState` draws fold `(state >> 16) % 100` down to under 11 for a
/// hit, and each is taken through its own local so the two divides stay separate
/// objects: the compiler keeps the second draw's quotient alive across the
/// scaling chain, and `SCHED_BARRIER` holds the reaction branch's assignment on
/// the far side of it. The two barriers are load-bearing, not decoration —
/// dropping either one re-schedules the `branch` write to before the chain and
/// the whole block's allocation follows it.
void Actor07000_Fn046B8(Task* arg0, s32 arg1)
{
    u32              sp10;
    Actor107000Work* work;
    u32              rng;
    u32              rng2;
    u32              hi;
    u32              quotient;
    u32              roll;
    s32              hit;
    s16              branch;

    work = (Actor107000Work*)arg0->work;
    Actor07000_Fn047F4(((TmdObject*)arg0->extra)->coords, &sp10);
    if (Gp_CountRec18Hi(&work->field_214, 0x10000) == 0 || sp10 >= 0xBB8U) {
        work->field_382 = 0;
        work->field_36E = 0;
    } else {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if ((u16)((rng >> 0x10) % 100U) < 0xBU) {
            branch = 3;
        } else if (sp10 >= 0x9C4U) {
            branch = 2;
        } else {
            rng2        = rng * 5 + 0x71357911;
            Gp_LcgState = rng2;
            hi          = rng2 >> 0x10;
            quotient    = hi / 100U;
            SCHED_BARRIER();
            roll = (u16)(hi - quotient * 100U);
            SCHED_BARRIER();
            hit    = (roll < 0xBU);
            branch = 2;
            if (!hit) {
                branch = 1;
            }
        }
        work->field_382 = branch;
        work->field_374 = 0;
        work->field_372 = 0;
    }
    Gp_ClearRec18Occupied(&work->field_214);
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// the heading to it in `arg0`'s own frame, folded into -0x800..0x800, and
/// stores the horizontal world distance in `*arg1`. The offset is staged in
/// 0x40 bytes of the scratch stack.
///
/// `base` and `vec` hold the same address on purpose: the explicit `move`
/// reproduces the original's `addiu`/`addu` pair, and the head-relative
/// spelling of the vector copy and of the reloaded x keeps the scratch
/// pointer in `head`'s register. `COMPILER_BARRIER` stops cse from forwarding
/// the reload of x from the store just above it.
s32 Actor07000_Fn047F4(GsCOORDINATE2* arg0, u32* arg1)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            x;
    s16            z;
    void*          base;
    void*          head;
    void*          vec;
    void*          matrix;

    coord = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    head  = *(void**)0x1F8003FC;
    base  = head - 0x40;
    __asm__("move %0,%1" : "=r"(vec) : "r"(base));
    *(s16*)((s8*)base + 0) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    *(s16*)((s8*)vec + 2)  = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    *(void**)0x1F8003FC    = vec;
    *(s16*)((s8*)vec + 4)  = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                 = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);
    local = *(SVECTOR*)((s8*)head - 0x40);
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    *(s16*)((s8*)vec + 0) = (s16)(coord->coord.t[0] - arg0->coord.t[0]);
    COMPILER_BARRIER();
    x                     = *(s16*)((s8*)vec + 0);
    z                     = coord->coord.t[2] - arg0->coord.t[2];
    *(s16*)((s8*)vec + 4) = z;
    *arg1                 = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC  += 0x40;
    return angle;
}

/// Ground-burst tick of the specimen. The `Gp_LcgState` draw is folded to a
/// variant and each of the three effect-setup records, with its own part of the
/// model, is spawned as effect 0x80005: variant 2 uses part 5, variant 3 part 4,
/// and variants 0 and 1 share part 1. The spawned effect is re-coloured from the
/// specimen's own palette before the tick ends by arming effect 0x60030 on parts
/// 1 and 4.
void Actor07000_Fn049C0(Task* arg0)
{
    GpEffWork* effect;
    s32        r;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    r           = (Gp_LcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_80062730 = (s32)&Actor07000_D0B730;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 2:
            D_80062730 = (s32)&Actor07000_D0B194;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 5, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 3:
            D_80062730 = (s32)&Actor07000_D0AB40;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->extra)->coords + 4, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 1, 0x300, NULL);
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->extra)->coords + 4, 0x300, NULL);
}

/// Spawn handler of a specimen projectile, entry 0 of `Actor07000_D000E0`.
/// Allocates the 0x58-byte work, spawns effect 0x60081 on the parent's
/// coordinate and re-parents the task under it, and derives a launch velocity
/// from the spawn angle in `spawnArg1` and two `Gp_LcgState` draws, rotated
/// into the coordinate's frame and scaled on the GTE. The coordinate's rotation
/// is reset to identity and nudged by that velocity, and the render node is
/// linked with a capsule collision record keyed by `Actor07000_D08078`. The
/// task takes `Actor07000_Fn068F0` as its exit callback, cues the launch sound
/// and runs its first frame through `Actor07000_Fn04E60`.
void Actor07000_Fn04B18(Task* arg0)
{
    ActorsShared80136938Work* work;
    GsCOORDINATE2*            coord;
    GpEffWork*                eff;
    GpObj*                    obj;
    GpActorD4Rec*             rec;
    GpMtxWords*               rot;
    SVECTOR*                  head;
    SVECTOR*                  vec;
    SVECTOR                   local;
    MATRIX*                   matrix;
    u32                       rng;
    s32                       angle;
    s32                       pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(0x58, false);
    if (work == NULL) {
        Task_CallExit(arg0);
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    obj                        = &work->obj;
    arg0->work                 = work;
    eff                        = Gp_SpawnEff(0x60081, coord, 0, NULL);
    arg0->spawnArg2            = eff->task;
    Task_Reparent(arg0, eff->task);
    angle       = arg0->spawnArg1;
    vec->vy     = -rcos(angle);
    vec->vx     = rsin(angle);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    vec->vz     = 0xE000 - ((Gp_LcgState >> 16) & 0x1FFF);
    matrix      = &coord->coord;
    local       = *vec;
    SOFT_BARRIER();
    rec = &work->rec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)" : : "m"(local) : "$2");
    gte_rtv0();
    gte_stsv(vec);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    gte_lddp(((Gp_LcgState >> 16) & 0x1F) + 0x1E);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(&work->vec);
    rot                = (GpMtxWords*)&coord->coord;
    rot->w0            = 0x1000;
    rot->w1            = 0;
    rot->w2            = 0x1000;
    rot->w3            = 0;
    rot->h4            = 0x1000;
    coord->coord.t[0] += work->vec.vx;
    rng                = Gp_LcgState * 5 + 0x71357911;
    coord->coord.t[1] += (rng >> 16) & 0x7F;
    coord->coord.t[2] += work->vec.vz;
    Gp_LcgState        = rng;
    coord->flg         = 0;
    obj->coord         = coord;
    obj->ctx.d4rec     = rec;
    obj->pos.vx        = 0;
    obj->pos.vy        = 0;
    obj->pos.vz        = 0;
    obj->radius        = 0;
    obj->key           = Gp_PackPair(&Actor07000_D08078, 1);
    obj->flags         = 3;
    rec->recs          = &work->rec2;
    rec->end1.vx       = 0;
    rec->end1.vy       = 0;
    rec->end1.vz       = 0;
    rec->end0.vx       = 0;
    rec->end0.vy       = 0;
    rec->end0.vz       = 0;
    rec->end0Radius    = 0x96;
    rec->end1Radius    = 0x96;
    Gp_InitRec18Table(&work->rec2, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags                 |= 0xC000;
    arg0->exitCallback          = Actor07000_Fn068F0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
    arg0->state                += 1;
    __asm__("" : "=r"(rec), "+r"(coord));
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x40460002, pan, (s8)gpGetObjDepth(coord));
    Actor07000_Fn04E60(arg0);
}

/// Per-frame handler of a specimen projectile, entry 1 of
/// `Actor07000_D000E0`. `Gp_StateF0.field_4` mode 1 returns at once and mode 2 hides
/// the model; mode 0 shows it again before the update. The update moves the
/// coordinate by the velocity in the work (mirrored into the first collision
/// record's position), lets the vertical speed grow by 0xA a frame, and tests
/// the second collision record: a hit on an object of the 0x10000 kind or on
/// any occupied slot cues the impact sound, tells the child task how it landed
/// through `spawnArg1` (3, or 2 for a slot hit below -0xC00 in the normal's Y),
/// clears the top bits of the flag word, arms a 0x1E-frame kill countdown and
/// moves on to `Actor07000_Fn068B4`. The collision table is cleared either way.
///
/// The 2/3 pair is written into each arm rather than through a temp: the shared
/// store m2c reads as one variable is `jump.c` cross-jumping the two arms, and
/// a named temp puts the value's live range in front of the comparison that
/// picks it, where it can no longer share `$v0` with the `slti` result.
void Actor07000_Fn04E60(Task* arg0)
{
    ActorsShared80136c80Work* work;
    TmdObject*                part;
    Task*                     child;
    GsCOORDINATE2*            coord;
    GpRec18*                  rec;
    GpRec18*                  hit;
    GpRec18*                  recs;
    s32                       state;
    s32                       one;

    work  = (ActorsShared80136c80Work*)arg0->work;
    part  = (TmdObject*)arg0->extra;
    state = Gp_StateF0.field_4;
    child = arg0->firstChild;
    rec   = &work->recs[0];
    coord = part->coords;
    hit   = &work->recs[1];
    one   = 1;

    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    part->flags = 0;
    goto default_body;
case1:
    return;
case2:
    part->flags = 0x80;
    return;
default_body:
    rec->point.vx     = -work->field_0;
    rec->point.vy     = -work->field_2;
    rec->point.vz     = -work->field_4;
    coord->coord.t[0] = coord->coord.t[0] + (s16)work->field_0;
    recs              = &work->recs[1];
    coord->coord.t[1] = coord->coord.t[1] + (s16)work->field_2;
    coord->coord.t[2] = coord->coord.t[2] + (s16)work->field_4;
    coord->flg        = 0;
    work->field_2     = work->field_2 + 0xA;
    if (Gp_CountRec18Hi(recs, 0x10000) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan(coord),
                            (s8)gpGetObjDepth(coord));
        if (child != NULL) {
            child->spawnArg1 = 3;
        }
        goto block_16;
    }
    if (Gp_FindRec18(recs, 0) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan(coord),
                            (s8)gpGetObjDepth(coord));
        if (child != NULL) {
            if (hit->at10.normal.vy >= -0xC00) {
                child->spawnArg1 = 3;
            } else {
                child->spawnArg1 = 2;
            }
        }
    block_16:
        work->field_26      = work->field_26 & 0x3FFF;
        arg0->killCountdown = 0x1E;
        arg0->state         = arg0->state + 1;
    }
    Gp_ClearRec18Occupied(&work->recs[1]);
}

/// The variant's spawn handler: allocate the `Actor107000Spawn2Work` block,
/// rebind the model's light and colour matrices into it, link its three `GpObj`
/// render nodes and their collision tables, and hand the task over to the state
/// table in `Task::msgTable`.
///
/// Node 1 is the odd one: it points its context at the `GpActorD4Rec` at 0x1FC
/// rather than at a record table, and the record's own `recs` names the one
/// `GpRec18` beside it - the pair `GpActorD4` keeps, and the three constants it
/// carries are that record's fields rather than an object's. Node 3's `field_8`
/// is the model's seventh coordinate (`&coord[6]`), which is the value the
/// sibling `Actor07000_Fn000EC` computes for its `part`.
///
/// The spawn arg seeds `field_364`/`field_366` the same way it does there, and
/// a high halfword of 1 kills the specimen instead. The tail draws two numbers
/// off `Gp_LcgState` for `field_390`/`field_392`, the spawn countdown and the
/// running total the spawn cue fires on at 5.
void Actor07000_Fn05068(GpEnemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         part;
    u32                    draw;
    s32                    one;
    s32                    i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[6];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_366 = (u16)arg1->spawnArg1;
    work->field_364 = (s16)(arg1->spawnArg1 >> 16);
    obj->flags     |= 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_1BC;
    obj->colorMtx   = &work->field_19C;
    arg0->field_4   = &coord->coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord              = coord;
    arg0->node.state.b.flags = one;
    arg0->bodyPos.vx         = 0;
    arg0->bodyPos.vy         = 0;
    arg0->bodyPos.vz         = 0;
    arg0->param              = &Actor07000_D08080;
    arg0->hp                 = Actor07000_D08080.hpMax;
    arg0->recs               = &work->field_24C[0];
    work->field_35C          = &((TmdObject*)arg1->extra)->coords[1];
    work->field_360          = 0x100;
    work->field_362          = one;
    func_800B3F84((GpAnimCtx*)work, Actor07000_D0D77C, obj, work->field_12C,
                  (GpAnimSlot*)&work->slots[0]);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 7);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_370            = 1;
    work->field_372            = 1;
    work->field_388            = 0;
    work->field_384            = 0;
    work->field_386            = 0;
    work->field_38E            = 0;
    work->field_38A            = 0;
    work->field_1FC.end0.vz    = 0xBB8;
    work->field_1FC.end0Radius = 0xFA0;
    work->field_1FC.end1Radius = 0x7D0;
    work->field_1FC.recs       = work->field_214;
    work->obj1.ctx.d4rec       = &work->field_1FC;
    work->obj1.coord           = coord;
    work->obj1.pos.vx          = 0;
    work->obj1.pos.vy          = 0;
    work->obj1.pos.vz          = 0;
    work->obj1.key             = 0;
    work->obj1.radius          = 0;
    work->obj1.flags           = 3U;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->field_214, 1, 0);
    work->obj2.coord    = coord;
    work->obj2.ctx.recs = &work->field_24C[0];
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = -0x258;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x3002A;
    work->obj2.radius   = 0x258;
    work->obj2.flags    = 1U;
    work->obj1.flags    = (u16)(work->obj1.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(&work->field_24C[0], 4, 0);
    work->obj3.coord    = part;
    work->obj3.ctx.recs = &work->field_2CC[0];
    work->obj3.pos.vx   = -0x154;
    work->obj3.pos.vy   = 0;
    work->obj3.pos.vz   = 0;
    work->obj2.flags    = (u16)(work->obj2.flags & 0x3DFF);
    work->obj3.key      = Gp_PackPair(&Actor07000_D08078, 0);
    work->obj3.radius   = 0x12C;
    work->obj3.flags    = 1U;
    Gp_LinkObj(3, &work->obj3);
    Gp_InitRec18Table(&work->field_2CC[0], 1, 0);
    work->field_394  = 0;
    work->field_396  = 0;
    work->obj3.flags = (u16)(work->obj3.flags & 0x7FFF);
    draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    work->field_390    = (u16)(((u32)draw >> 16) % 20U + 0x50);
    draw = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    work->field_392    = (u16)(((u32)draw >> 16) % 50U + 0x32);
    arg1->msgTable     = Actor07000_D0D7C0;
    arg1->state        = 4;
}

static __inline__ void update_color(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block;
    block                 = (VECTOR*)((u8*)*(void**)0x1F8003FC - 0x10);
    *(VECTOR**)0x1F8003FC = block;
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}
static __inline__ void update_animation(Task* task)
{
    Actor107000Spawn2Work* work;
    s32                    i;
    work = (Actor107000Spawn2Work*)task->work;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        for (i = 1; i < 7; i++)
            func_800B4114((GpAnimCtx*)work, i, work->field_370, 0, 8);
    } else {
        work->field_374++;
        for (i = 1; i < 7; i++)
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
}
static __inline__ void rotate_parts(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                  = (Actor107000Spawn2Work*)arg0->work;
    head                  = *(u8**)0x1F8003FC;
    *(MATRIX**)0x1F8003FC = (MATRIX*)(head - 0x20);
    scratch               = (MATRIX*)(head - 0x20);
    coord                 = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    *(MATRIX**)0x1F8003FC                  = (MATRIX*)((u8*)*(MATRIX**)0x1F8003FC + 0x20);
}

/// Per-frame handler of the specimen's second form while it drops into place,
/// entry 4 of `Actor07000_D0004C`. `Gp_StateF0.field_4` mode 1 only re-colours the
/// actor and mode 2 hides the model; otherwise, once `field_396` has armed the
/// drop, the root is stepped along its facing and by the fall speed
/// `field_398`, the collision response is applied, the six helper animation
/// slots tick, the reaction twist is applied to coordinates 3 and 5, and the
/// root is recomputed, with the step length `field_378` decaying by 2 a frame.
/// Once the root is below the floor (Y above 0) the landing sound is cued, the
/// twist is armed at 0x400, the root is pinned at 0 and the task moves to
/// state 1; until then the fall speed grows by 10 a frame, or 20 once the
/// collision response has latched `field_39A`.
void Actor07000_Fn05400(GpEnemy* arg0, Task* arg1)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    s32                    sound;
    s32                    pan;
    work = (Actor107000Spawn2Work*)arg1->work;
    switch (Gp_StateF0.field_4) {
        case 1:
            update_color(arg1->spawnArg2, &((TmdObject*)arg1->extra)->coords[1]);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.state.b.flags         = 1;
            return;
        case 0:
        default:
            if (work->field_396 != 0) {
                Actor07000_Fn06820(arg1);
                Actor07000_Fn0595C(arg1);
                update_animation(arg1);
                rotate_parts(arg1);
                Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
                update_color(arg1->spawnArg2, &((TmdObject*)arg1->extra)->coords[1]);
                work->field_378 -= 2;
                if (work->field_378 < 0)
                    work->field_378 = 0;
                coord = ((TmdObject*)arg1->extra)->coords;
                if (coord->coord.t[1] > 0) {
                    sound = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0009;
                    pan   = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
                    work->field_38C                               = 1;
                    work->rotation.vx                             = 0x400;
                    work->field_398                               = 0;
                    work->field_378                               = 0;
                    ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                    arg1->state                                   = 1;
                    return;
                }
                if ((u16)work->field_39A == 0)
                    work->field_398 += 10;
                else
                    work->field_398 += 20;
            }
            break;
    }
}

/// Collision response of the specimen's second form: node 2's collision table
/// is run through `func_800E0C10` with a 0x38-byte scratch. Response 1 adds
/// the returned X and Z offsets to the root; only the first one (while
/// `field_39A` is clear) also adds Y, latches the response in `field_38C` and
/// `field_39A`, arms `field_2E4` to 0x400, sets the fall speed `field_398` to
/// -0x50 and takes a quarter off the step length `field_378`. Response 2 puts
/// the root back at the translation saved in `field_33C`. Both collision
/// tables are released either way.
void Actor07000_Fn0595C(Task* arg0)
{
    ActorsShared8013777cScratch* scratch;
    ActorsShared8013777cWork*    work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (ActorsShared8013777cWork*)arg0->work;
    scratch  = (ActorsShared8013777cScratch*)(SCRATCH_SP -= 0x38);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_24C[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_39A == 0) {
                work->field_38C    = movement;
                work->field_2E4    = 0x400;
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_398    = -0x50;
                work->field_378    = work->field_378 - (s16)work->field_378 / 4;
                work->field_39A    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_33C.vx;
            coord->coord.t[1] = work->field_33C.vy;
            coord->coord.t[2] = work->field_33C.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_24C[0]);
    Gp_ClearRec18Occupied(&work->field_2CC);
    SCRATCH_SP += 0x38;
}

/// Message 0x7DB handler of the second form's table (`Actor07000_D0D7C0`,
/// parked in `Task::msgTable` by `Actor07000_Fn05068`). The payload's
/// halfword at 0x2 is a command word.
///
/// 4 and 5 are the collapse arms: both spawn the 0x60080 effect on the model's
/// root coordinate, clear the spawn countdown `field_390` and arm the reaction
/// sub-state `field_36A` to 4; 5 clears the spawn counter `field_392` as well.
///
/// Low byte 1 is the reveal. Unless the task already runs one of the two live
/// states, the model is placed at the spawn point bits 8..11 select from the
/// current map's table - `D_8018B74C` on map 0x27, where the appearance sound
/// is cued through `SndEvt_EnqueueType6` as well, `D_801874C4` on 0x28 - the
/// buffers are re-armed, the 0x80 and 4 bits are cleared from the model's flag
/// word, the enemy's `node.state.b.flags` is zeroed, both render nodes are revealed, and
/// the model is turned to the spawn point's heading. Low byte 3 is the hide:
/// the two bits and the pose flag go the other way, both nodes are hidden, the
/// model's translation and rotation are zeroed, and the task moves to state 4.
s32 Actor07000_Fn05AB8(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor107000Spawn2Work* work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR                rot;
    u16                    word;
    s32                    mode;
    s32                    sound;
    s32                    pan;

    word  = arg2->command;
    obj   = (TmdObject*)arg0->extra;
    enemy = arg0->spawnArg2;
    work  = (Actor107000Spawn2Work*)arg0->work;
    mode  = word & 0xFFFF;
    coord = obj->coords;
    if (mode == 4) {
        Gp_SpawnEff(0x60080, coord, 0x400, &Actor07000_D0D7B8);
        work->field_390 = 0;
        work->field_36A = 4;
        return 0;
    }
    if (mode == 5) {
        Gp_SpawnEff(0x60080, coord, 0x400, &Actor07000_D0D7B8);
        work->field_390 = 0;
        work->field_392 = 0;
        work->field_36A = 4;
        return 0;
    }
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->command >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->command >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->command >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->command >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->command >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->command >> 8].z;
            }
            Tmd_AllocBuffers(arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            enemy->node.state.b.flags         = 0;
            work->obj1.flags                 |= 0x8000;
            work->obj2.flags                 |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_378                        = 0xC8;
            work->field_396                        = 1;
            work->field_398                        = 0x64;
            work->field_39A                        = 0;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        enemy->node.state.b.flags         = 1;
        work->obj1.flags                 &= 0x7FFF;
        work->obj2.flags                 &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 4;
        work->field_396 = 0;
    }
    return 0;
}

void Actor07000_Fn05E6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor07000_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Rebinds the second form's animation id `field_370` to its six helper
/// slots. When the id has changed since the last frame `field_372` follows it,
/// the frame count `field_374` restarts and every slot is pointed at the new
/// id with a blend of 8; otherwise the count ticks and the slots advance by
/// one frame.
void Actor07000_Fn05ED4(Task* arg0)
{
    ActorsShared80137cf4Work* work;
    s32                       i;

    work = arg0->work;
    i    = 1;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_370, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work->field_374 = (u16)(work->field_374 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 7);
}

/// Colours the specimen's second form from the world position of the model's
/// second coordinate, staged in a `VECTOR` taken off the scratch stack; the
/// colour target is the task's enemy.
void Actor07000_Fn05F84(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = task->spawnArg2;
    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Splices the work's own coordinate between the model's root and its second
/// part, with an identity rotation, marks both dirty and resets the scale
/// `SVECTOR` beside it to one. `field_36E` is cleared, and unless the reaction
/// sub-state `field_36A` is 5 the 0x600A5 effect is spawned on the root.
void Actor07000_Fn05FF8(Task* arg0)
{
    GsCOORDINATE2*           parts;
    GsCOORDINATE2*           coord;
    OverlayMat*              mat;
    ActorShared80137e18Work* work;

    work               = arg0->work;
    parts              = ((TmdObject*)arg0->extra)->coords;
    coord              = &work->coord;
    coord->sub         = parts;
    parts[1].sub       = coord;
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    coord->flg         = 0;
    parts[1].flg       = 0;
    work->scale.vx     = 0x1000;
    work->scale.vy     = 0x1000;
    work->scale.vz     = 0x1000;
    work->field_36E    = 0;
    if (work->field_36A != 5) {
        Gp_SpawnEff(0x600A5, ((TmdObject*)arg0->extra)->coords, 2, NULL);
    }
}

/// Flattens the coordinate `Actor07000_Fn05FF8` spliced in: the Y scale
/// `field_34E` loses 2 and the matrix's second column is scaled by it, then
/// the coordinate is marked dirty.
void Actor07000_Fn06088(Task* arg0)
{
    u16                      scale;
    GsCOORDINATE2*           coord;
    ActorShared80137ea8Work* work;

    work                 = arg0->work;
    coord                = &work->coord;
    scale                = work->field_34E - 2;
    work->field_34E      = scale;
    coord->coord.m[0][1] = (s16)((s32)(coord->coord.m[0][1] * (s16)scale) >> 0xC);
    coord->coord.m[1][1] = (s16)((s32)(coord->coord.m[1][1] * (s16)work->field_34E) >> 0xC);
    coord->coord.m[2][1] = (s16)((s32)(coord->coord.m[2][1] * (s16)work->field_34E) >> 0xC);
    work->coord.flg      = 0;
}

/// Stretches the model's sixth coordinate by the scale delta `field_384`
/// while it is non-zero: the matrix's first column is scaled by
/// `0x1000 + field_384`, the other two by a quarter of that delta, each column
/// through an `SVECTOR` on the GTE, and the coordinate is marked dirty.
void Actor07000_Fn060FC(Task* arg0)
{
    SVECTOR                   vec;
    MATRIX*                   m;
    ActorsShared80137f1cWork* work;
    GsCOORDINATE2*            coord;

    work  = (ActorsShared80137f1cWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_384 != 0) {
        m = &coord[5].coord;
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 0, 6, 12);
        gte_lddp(work->field_384 + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 0, 6, 12);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 2, 8, 14);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 2, 8, 14);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 4, 10, 16);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 4, 10, 16);

        coord[5].flg = 0;
    }
}

void Actor07000_Fn062A8(Task* arg0)
{
    SVECTOR        offset;
    u32            dist;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* child;
    Task*          task;
    s32            angle;

    coords    = ((TmdObject*)arg0->extra)->coords;
    child     = &coords[1];
    angle     = Actor07000_Fn047F4(coords, &dist);
    offset.vz = 0;
    offset.vy = 0;
    offset.vx = 0;
    task      = Task_SpawnFromTable(&Actor07000_D0D7D0, 1, angle, 0);
    if (task != NULL) {
        Gp_CopyCoordOffset(task, child, &offset);
        Task_Reparent(arg0, task);
    }
}

/// Task handler of the specimen's contact effect: runs the entry of
/// `Actor07000_D000E0` for the task's state. The table is copied onto the
/// stack before the call.
void Actor07000_Fn06338(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor07000_D000E0;
    sp.funcs[task->state](task);
}

/// Applies the second form's reaction twist: the rotation `rotation` is
/// turned into a matrix on the scratch stack and multiplied into the rotations
/// of coordinates 3 and 5 on the GTE. The twist's X angle then decays by 0x20
/// a frame; once it would drop to 0x20 or below it is cleared together with
/// `field_38C`.
void Actor07000_Fn06390(Task* arg0)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                  = (Actor107000Spawn2Work*)arg0->work;
    head                  = *(u8**)0x1F8003FC;
    *(MATRIX**)0x1F8003FC = (MATRIX*)(head - 0x20);
    scratch               = (MATRIX*)(head - 0x20);
    coord                 = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->rotation, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->rotation.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->rotation.vx = 0;
            work->field_38C   = 0;
        } else {
            work->rotation.vx = (u16)work->rotation.vx - 0x20;
        }
    }
    *(MATRIX**)0x1F8003FC = (MATRIX*)((u8*)*(MATRIX**)0x1F8003FC + 0x20);
}

/// Per-frame reaction handler: folds the generic hit flags into the enemy's
/// flag byte, applies a pending hit, and drops the work to its death pose when
/// the hit lands.
void Actor07000_Fn0662C(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = (Actor107000Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_36A       = 3;
            work->field_36E       = 0;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4(enemy);
            if (tick != 0) {
                Actor07000_Fn04274(arg0, tick);
                work->field_36A = 0;
                work->field_370 = 5;
            }
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}

/// Gives `dst`'s model the texture page and CLUT of `src`'s, re-streaming it
/// twice when it has a buffer.
void Actor07000_Fn066FC(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from      = (TmdObject*)src->extra;
    to        = (TmdObject*)dst->extra;
    to->tpage = from->tpage;
    to->clut  = from->clut;
    if (to->buffer != NULL) {
        tmdProcessStream(to);
        tmdProcessStream(to);
    }
}

/// Exit callback of the specimen's second form: flags the enemy's node, takes
/// it and the three render nodes back off their lists and runs the common
/// enemy task exit.
void Actor07000_Fn06750(Task* task)
{
    ActorShared80138570Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared80138570Work*)task->work;

    enemy->node.state.b.flags = 1;
    enemy->recs               = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_22C);
    Gp_UnlinkObj(&work->field_2AC);
    Gp_EnemyTaskExit(task);
}

/// Task handler of the specimen's second form: runs the entry of
/// `Actor07000_D0004C` for the task's state with the enemy and the task. The
/// table is copied onto the stack before the call.
void Actor07000_Fn067B4(Task* task)
{
    GpEnemyTaskFuncTable5 sp;

    sp = Actor07000_D0004C;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// Steps the second form's root one frame: saves the current translation in
/// `field_33C`, advances X and Z along the rotation's Z column scaled by the
/// step length `field_378`, and Y by the fall speed `field_398`.
void Actor07000_Fn06820(Task* arg0)
{
    GsCOORDINATE2*           coord;
    ActorShared80138640Work* work;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;

    work->field_33C.vx = coord->coord.t[0];
    work->field_33C.vy = coord->coord.t[1];
    work->field_33C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_378) >> 12;
    coord->coord.t[1] += work->field_398;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_378) >> 12;
}

/// Counts the task's kill countdown down and runs its exit callback once it
/// runs out.
void Actor07000_Fn068B4(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Exit callback of the contact effect: takes its render node back off the
/// object list and kills the task.
void Actor07000_Fn068F0(Task* arg0)
{
    Gp_UnlinkObj(&((ActorShared801511c8Work*)arg0->work)->obj);
    taskKill(arg0);
}
