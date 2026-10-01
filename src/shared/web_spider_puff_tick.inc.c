#include "main/random.h"

/* Part of the web spider library; see web_spider.h. */

/// Per-frame tick of the homing projectile: while the global mode is 1 the
/// frame is just drawn, in mode 2 nothing happens at all, and otherwise the
/// work is stepped. A live collision record whose kind is not 0x10 drops the
/// object's 0x8000 linked bit and wipes the record, which sends the tick
/// straight past the frame counter. Every other frame the work's flags mirror
/// the low two bits of the counter, the coordinate is advanced along its own
/// forward axis by `field_3A`, and the counter is bumped; at 0xF frames the
/// object is unlinked and the actor switches to state 2, otherwise `field_3A`
/// decays by an LCG-derived 0..0x1F and clamps at zero.
void spiderPuffTick(Enemy* arg0, Task* arg1)
{
    ActorsShared80135c4cObjWork* work;
    GfxCoord*                    coord;
    s16                          age;
    s16                          speed;
    s32                          contact;
    u16                          flags;
    u32                          random;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    switch (Gp_StateF0.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            spiderDrawPuff(arg1, work->field_38);
            return;
        default:
        default_case:
            contact = work->rec.key.value;
            if (contact != 0) {
                if ((contact & 0xFFFF0000) != 0x100000) {
                    work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ClearRec18Occupied(&work->rec);
                    goto block_7;
                }
                goto block_11;
            }
        block_7:
            if (!((u16)work->field_38 & 3)) {
                flags = work->obj.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                flags = work->obj.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            work->obj.flags     = flags;
            coord->coord.t[0]  += (s32)(coord->coord.m[0][2] * work->field_3A) >> 0xC;
            coord->coord.t[1]  += (s32)(coord->coord.m[1][2] * work->field_3A) >> 0xC;
            coord->coord.t[2]  += (s32)(coord->coord.m[2][2] * work->field_3A) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            spiderDrawPuff(arg1, work->field_38);
            age            = (u16)work->field_38 + 1;
            work->field_38 = age;
            if (age >= 0xF) {
            block_11:
                Gp_UnlinkObj(&work->obj);
                arg1->state = 2;
                return;
            }
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            speed           = (u16)work->field_3A - ((random >> 0x10) & 0x1F);
            work->field_3A  = speed;
            if (speed < 0) {
                work->field_3A = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            goto default_case;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}
