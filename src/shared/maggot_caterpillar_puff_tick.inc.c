#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Advances a slowing puff and ends it on a grid contact or after fifteen ticks.
///
/// Paused actors draw the current age without advancing; hidden actors do
/// nothing. Otherwise grid and pair collision tests are enabled every fourth
/// tick. Non-grid contacts are cleared, and movement follows all three local
/// forward-axis components with 12 fractional bits. The enemy parameter is
/// unused by this handler but belongs to its dispatch signature. The caller
/// keeps the parent model alive for drawing.
static void _maggotCaterpillarPuffTick(Enemy* enemy, Task* task)
{
    enum { MAGGOT_CATERPILLAR_PUFF_CONTACT_PERIOD    = 4,
           MAGGOT_CATERPILLAR_PUFF_DECELERATION_MASK = 31 };
    MaggotCaterpillarPuffWork* work;
    GfxCoord*                  coord;
    s32                        contactKey;
    u16                        flags;
    u32                        random;

    coord = task->extra.coordBody->coord;
    work  = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _maggotCaterpillarDrawPuff(task, work->age);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            contactKey = work->contacts[0].key.value;
            if (contactKey != 0) {
                if ((contactKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {
                    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCollisionClearContacts(work->contacts);
                } else {
                    worldCollisionUnlinkBody(&work->body);
                    task->state = MAGGOT_CATERPILLAR_PUFF_TASK_DESTROY;
                    return;
                }
            }
            // Test collision once per four flight ticks.
            if (!(work->age & (MAGGOT_CATERPILLAR_PUFF_CONTACT_PERIOD - 1))) {
                flags = work->body.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                flags = work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            work->body.flags    = flags;
            coord->coord.t[0]  += (s32)(coord->coord.m[0][2] * work->forwardSpeed) >> 12;
            coord->coord.t[1]  += (s32)(coord->coord.m[1][2] * work->forwardSpeed) >> 12;
            coord->coord.t[2]  += (s32)(coord->coord.m[2][2] * work->forwardSpeed) >> 12;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            _maggotCaterpillarDrawPuff(task, work->age);
            work->age++;
            if (work->age >= MAGGOT_CATERPILLAR_PUFF_LIFETIME) {
                worldCollisionUnlinkBody(&work->body);
                task->state = MAGGOT_CATERPILLAR_PUFF_TASK_DESTROY;
                return;
            }
            random              = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState     = random;
            work->forwardSpeed -= (random >> 0x10) & MAGGOT_CATERPILLAR_PUFF_DECELERATION_MASK;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}
