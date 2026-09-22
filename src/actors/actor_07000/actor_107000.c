#include "actors/actors_shared_80134810.h"
#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actor_107000_anim.h"
#include "actors/actors_shared_8013454c.h"
#include "actors/actors_shared_8014ca28.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Actor07000_Fn00654(Task* arg0);
void Actor07000_Fn016A8(Task* arg0, u8 arg1);
void Actor07000_Fn0107C(Task* arg0);

/// Node 3's pair table, packed by `Gp_PackPair` into `obj1B4`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern GpU16Pair  Actor07000_D06924;
extern GpPairSrcE Actor07000_D06928;

/// Message dispatch table the spawn parks in `Task::msgTable`.
extern u8 Actor07000_D08030[];

/// The animation data `func_800B3F84` seeds the work block's slots from.
extern u8 Actor07000_D08058[];

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR Actor07000_D08070;

extern u32     Actor07000_D06938[];
extern u32     Actor07000_D06944[];
extern SVECTOR Actor07000_D08068;

// actor_104600 (func_actor_104600_80131E68), actor_204600
// (func_actor_204600_80149E68) and actor_207000 (func_actor_207000_80149F0C)
// carry the same body, refused promotion for the reason its sibling below is:
// the pair table, the animation bank and the node-3 record it names -
// Actor07000_D06924, Actor07000_D06928 and
// Actor07000_D08058 - are this overlay's own data, so one shared object
// could not link into the other three.

/// Spawn handler of the specimen, the `GpEnemyTaskFunc` the task dispatch runs
/// first: it allocates the `Actor107000SpawnWork` block, wires the enemy's four
/// `GpObj` render nodes and their `GpRec18` tables into it and hands the task
/// over to `ActorsShared8014ca28`. The spawn arg's high halfword is the variant
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
    arg0->coord      = part;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor07000_D06928;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor07000_D06928.hpMax;
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
    arg1->exitCallback = ActorsShared8014ca28;
    arg1->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_07000/actor_107000", Actor07000_D00010);

// actor_104600 (func_actor_104600_801321F4), actor_204600
// (func_actor_204600_8014A1F4) and actor_207000 (func_actor_207000_8014A298)
// carry the same body, refused promotion for the reason its sibling below is:
// its three remaining calls - Actor07000_Fn00654,
// Actor07000_Fn00854 and ActorsShared80134810 - and the effect
// offset it spawns at are named in this overlay only, so one shared object
// could not link into the other three.

/// Per-frame dispatch of the caged specimen, on the reaction state in
/// `field_2B2`: 0 is the dormant arm `Actor07000_Fn00654` and 1 the
/// live handler `Actor07000_Fn00854`. 3 is the arm the reaction
/// dispatch shoots when the enemy's flag byte carries bit 0x2 - it suppresses
/// the rebind, waits out the generic flag-2 helper on the spawn arg and, once
/// that expires, wakes the specimen: the rebind is released and
/// `field_2B2`/`field_2C8` move to 1, the live stage. The arm ends in
/// `ActorsShared8013454c` either way.
///
/// 4 and 5 are the two collapse arms. Both drive the model's second coordinate
/// through `ActorsShared80134810`, count `field_2BC` up and spawn the
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
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            ActorsShared8013454c(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            ActorsShared80134810(arg0, &((TmdObject*)arg0->extra)->coords[1]);
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
            ActorsShared80134810(arg0, &((TmdObject*)arg0->extra)->coords[1]);
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

// actor_104600 (func_actor_104600_801323D0), actor_204600
// (func_actor_204600_8014A3D0) and actor_207000 (func_actor_207000_8014A474)
// carry the same body. Unlike the siblings around it, nothing this one names is
// overlay-local - every call it makes is already shared, so one shared object
// *would* link into all four carriers. It is the span that cannot be placed:
// 0x654..0x854 sits inside this overlay's first code unit, and this overlay and
// actor_207000 already carry a hand-placed `rodata` cut, which is the layout
// `bulk_m2c_promote.py` refuses to re-derive (`already has hand-placed
// rodata/units cuts; splitting a unit there needs them re-derived by hand`).
// So the body stays matched in each carrier until that pass is done with a
// person re-deriving the cut.
