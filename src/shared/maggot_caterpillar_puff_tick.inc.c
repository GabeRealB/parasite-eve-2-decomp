#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Per-frame tick of the puff projectile: while the scene's actors are paused
/// the puff is just drawn, while they are hidden nothing happens at all, and
/// otherwise the work is stepped. A contact with the room's collision grid
/// ends the puff; any other contact is discarded. The collision sphere is
/// enabled only on the ticks whose `age` is a multiple of four. The
/// coordinate is advanced along its own forward axis by `forwardSpeed`, the
/// puff is drawn, and `age` is counted: at 0xF the sphere is unlinked and the
/// task moves to state 2, otherwise `forwardSpeed` loses a random 0 to 0x1F
/// and is held at zero.
void maggotCaterpillarPuffTick(Enemy* arg0, Task* arg1)
{
    MaggotCaterpillarPuffWork* work;
    GfxCoord*                  coord;
    s32                        contact;
    u16                        flags;
    u32                        random;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            maggotCaterpillarDrawPuff(arg1, work->age);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            contact = work->contacts[0].key.value;
            if (contact != 0) {
                if ((contact & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {
                    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCollisionClearContacts(work->contacts);
                } else {
                    worldCollisionUnlinkBody(&work->body);
                    arg1->state = 2;
                    return;
                }
            }
            if (!(work->age & 3)) {
                flags = work->body.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                flags = work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            work->body.flags    = flags;
            coord->coord.t[0]  += (s32)(coord->coord.m[0][2] * work->forwardSpeed) >> 0xC;
            coord->coord.t[1]  += (s32)(coord->coord.m[1][2] * work->forwardSpeed) >> 0xC;
            coord->coord.t[2]  += (s32)(coord->coord.m[2][2] * work->forwardSpeed) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            maggotCaterpillarDrawPuff(arg1, work->age);
            work->age++;
            if (work->age >= 0xF) {
                worldCollisionUnlinkBody(&work->body);
                arg1->state = 2;
                return;
            }
            random              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState     = random;
            work->forwardSpeed -= (random >> 0x10) & 0x1F;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}
