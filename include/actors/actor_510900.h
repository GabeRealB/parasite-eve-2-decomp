#ifndef ACTOR_510900_H
#define ACTOR_510900_H

#include "common.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

typedef struct Actor510900Work {
    /* 0x000 */ GpObj  obj0;
    /* 0x020 */ byte   pad_20[0x18];
    /* 0x038 */ GpObj  obj38;
    /* 0x058 */ byte   pad_58[0x264];
    /* 0x2BC */ GpObj  obj2BC;
    /* 0x2DC */ byte   pad_2DC[0x18];
    /* 0x2F4 */ GpObj  obj2F4;
    /* 0x314 */ byte   pad_314[0x128];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::colorMtx
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::lightMtx
    /* 0x47C */ GpObj  obj47C;
    /// `obj47C`'s collision table (`Gp_InitRec18Table` seeds 3 records), and
    /// the enemy's `GpEnemy::recs`.
    /* 0x49C */ GpRec18 rec49C[3];
    /* 0x4E4 */ GpObj   obj4E4;
    /* 0x504 */ GpObj   obj504;
    /// Shared collision table of `obj4E4` and `obj504`; only `obj4E4`'s
    /// `Gp_InitRec18Table` seeds it.
    /* 0x524 */ GpRec18  rec524[1];
    /* 0x53C */ GpEffArg field_53C; // record the hit's effect is spawned with; `coord` is the model's `coords[3]`, as the enemy's `GpEnemy::coord`
    /* 0x544 */ MATRIX   field_544;
    /* 0x564 */ s32*     field_564; // 0x34 receives field_594 when it changes
                                    /// Task of the second enemy the spawn creates from `D_actor_510900_80167A18`;
                                    /// `obj4E4` hangs off its model's first coordinate.
    /* 0x568 */ Task* field_568;
    /// Task of the third enemy spawned from the same table.
    /* 0x56C */ Task* field_56C;
    /// Residual head rotation, stepped 0x20 at a time towards zero each frame
    /// by `func_actor_510900_80138D38` while it yaws the head coordinate.
    /* 0x570 */ SVECTOR field_570;
    /* 0x578 */ s32     field_578;
    /* 0x57C */ s32     field_57C; ///< sound id stopped alongside field_580
    /* 0x580 */ s32     field_580; ///< last sound id queued
    /* 0x584 */ s16     field_584;
    /// Animation id the 0x7D3 handler reseeds slots 1..0x12 with; the handler
    /// stores `Actor510900AnimArgs::field_4 + 0x1B` here.
    /* 0x586 */ s16 field_586;
    /// Animation id the slots were last reseeded with; `func_actor_510900_8013BB20`
    /// reseeds 1..0x12 whenever this differs from `field_586`.
    /* 0x588 */ s16 field_588;
    /// Blend weight the 0x7D3 handler is handed, cleared once the reseed is done.
    /* 0x58A */ s16 field_58A;
    /* 0x58C */ s16 field_58C;
    /// Handler index `func_actor_510900_8013B870` dispatches on each frame:
    /// case 7 enters state 0 below, so this is the currently running one.
    /* 0x58E */ s16 field_58E;
    /// Per-handler sub-state. State 0 waits for `field_20`'s spawn block flag
    /// to fire and then hands state 1 the animation 0x14; state 1 waits out
    /// `field_58A` and drops back to state 0 with a fresh `field_59C`.
    /* 0x590 */ s16 field_590;
    /* 0x592 */ s16 field_592;
    /* 0x594 */ s16 field_594;
    /// `field_594` as last pushed to `field_564`.
    /* 0x596 */ s16 field_596;
    /* 0x598 */ s16 field_598;
    /// The `GpAnimRec::flags` cue bits latched on the previous frame. The step
    /// handler plays a sound on the frame one of them has just dropped.
    /* 0x59A */ u16 field_59A;
    /// Rolled from `Gp_LcgState` when state 1 expires.
    /* 0x59C */ s16 field_59C;
    /* 0x59E */ s16 field_59E;
    /// Yaw the head coordinate is rebuilt from each frame: the actor's facing
    /// angle, stepped 0x1E at a time towards the direction `field_5A8` selects.
    /* 0x5A0 */ s16 field_5A0;
    /// Cleared by state 0 on the frame it restarts.
    /* 0x5A2 */ s16 field_5A2;
    /* 0x5A4 */ s16 field_5A4;
    /// Distance travelled around the patrol square, advanced by `field_5A2`
    /// each frame and clamped to 0xC8..0xB66C. Its quotient by the side length
    /// picks the corner below, the remainder the offset along that side.
    /* 0x5A6 */ u16 field_5A6;
    /* 0x5A8 */ s16 field_5A8;
    /* 0x5AA */ s16 field_5AA;
    /* 0x5AC */ s16 field_5AC;
    /// Below 0x3E8 the turn target is taken one entry further along
    /// `D_actor_510900_80167B9C`.
    /* 0x5AE */ s16 field_5AE;
    /* 0x5B0 */ s16 field_5B0;
    /// Latch that sends the wind-up out to state 8 instead of back to state 1;
    /// state 3 clears it on the way through.
    /* 0x5B2 */ s16 field_5B2;
    /// Handler phase latch: state 0 sets it to 1 on entry, state 2 to 2 once
    /// the 0x50 blend has passed.
    /* 0x5B4 */ s16 field_5B4;
    /// Sub-state of the state-3 handler's disc load: 1 queues the file, 2 waits
    /// for `CdCmd_IsIdle` and plays the cue, 0 is idle.
    /* 0x5B6 */ s16 field_5B6;
    /* 0x5B8 */ s16 field_5B8;
    /* 0x5BA */ s16 field_5BA;
    /* 0x5BC */ s16 field_5BC;
    /// Grab request the child task's state 0 watches: when it equals the
    /// child's `field_334 + 1` and `field_5C0` is 1 the grab lands, and the
    /// handler takes the request back by writing -1.
    /* 0x5BE */ s16 field_5BE;
    /* 0x5C0 */ s16 field_5C0;
    /// Written by the child task's frame handler from its `field_336` when
    /// that task's `field_334` is 2 or more.
    /* 0x5C2 */ s16 field_5C2;
    /// Phase the child task's state machine reads: 1 starts it, 2 makes the
    /// grab land on the node's occupancy tag rather than 0.
    /* 0x5C4 */ s16  field_5C4;
    /* 0x5C6 */ byte pad_5C6[2];
} Actor510900Work;
STATIC_ASSERT_SIZEOF(Actor510900Work, 0x5C8);

/// `TaskDesc` table the state hands `Gp_SpawnEnemyFromTable` (entry 4).
extern TaskDesc D_actor_510900_80167A18[];

/// The pair source the context's `field_50` points at; its `hpMax` seeds the
/// enemy's HP.
extern GpPairSrcE D_actor_510900_80167980;

/// The block the tick handler reaches through `Task::msgTable`.
extern u32 D_actor_510900_80167A6C;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_510900_80167AA4[];

void func_actor_510900_801350F8(GpEnemy* arg0, Task* arg1);
void func_actor_510900_801355B4(GpEnemy* arg0, Task* arg1);
void func_actor_510900_8013B424(s32 arg0);
void func_actor_510900_8013B524(Task* arg0);
void func_actor_510900_8013B608(Task* arg0);
void func_actor_510900_8013BC38(Task* arg0, GsCOORDINATE2* arg1);

#endif
