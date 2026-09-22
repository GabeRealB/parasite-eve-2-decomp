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

/// Node 3's pair table, packed by `Gp_PackPair` into `obj1B4`, and the enemy
/// record whose `pairTable` points at it; its `hpMax` seeds the enemy's
/// `field_40`.

/// Message dispatch table the spawn parks in `Task::msgTable`.

/// The animation data `func_800B3F84` seeds the work block's slots from.

/// Offset the collapse arms spawn the 0x60080 effect at.

// actor_104600 (func_actor_104600_80131E68), actor_204600
// (func_actor_204600_80149E68) and actor_207000 (func_actor_207000_80149F0C)
// carry the same body, refused promotion for the reason its sibling below is:
// the pair table, the animation bank and the node-3 record it names -
// Actor07000_D06924, Actor07000_D06928 and
// Actor07000_D08058 - are this overlay's own data, so one shared object
// could not link into the other three.

// actor_104600 (func_actor_104600_801325D0), actor_204600
// (func_actor_204600_8014A5D0) and actor_207000 (func_actor_207000_8014A674)
// carry the same body, refused promotion because both of its remaining calls -
// Actor07000_Fn0107C and Actor07000_Fn016A8 - are named in this
// overlay only, so one shared object could not link into the other three.

void ActorsSharedFn00fd8(Task* arg0)
{
    Actor107000Work*       work;
    GsCOORDINATE2*         coord;
    Actor107000RotScratch* sc;
    s16                    cur;
    s32                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;
    s32                    current;

    coord      = ((TmdObject*)arg0->extra)->coords;
    work       = (Actor107000Work*)arg0->work;
    sc         = (Actor107000RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    cur        = work->field_2B0 & 0xFFF;
    diff       = want - cur;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
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
