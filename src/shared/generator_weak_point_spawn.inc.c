/* Part of the Generator library; see generator.h. */

/// Creates the Generator's targetable Life Support sphere and marks it intact.
///
/// Requires a coordinate-body child of a live Generator task whose kind is
/// GENERATOR_BETA or GENERATOR_PROTO. Positions use world game units beneath
/// the view. Owns zeroed part work and one contact slot until enemy teardown;
/// the parent must outlive it. Hides the room's Life Support sprites, clears
/// its part-down flag and enters the active state. Allocation failure destroys
/// the child before linking any target or collision records.
static void _generatorLifeSupportSpawn(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_LIFE_SUPPORT_RADIUS               = 200,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_MAGNITUDE = 1280,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_COUNT     = 2
    };
    GeneratorWork*            parentWork;
    GeneratorWork*            reloadedParentWork;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    WorldCollisionContact*    contacts;
    s32                       flag;
    s16                       kind;

    coord      = task->extra.coordBody->coord;
    parentWork = task->parent->work;
    part       = memCalloc(sizeof(GeneratorLifeSupportWork), 0);
    if (part == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = part;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = gGeneratorLifeSupportPos[parentWork->kind].x;
    coord->coord.t[1]   = gGeneratorLifeSupportPos[parentWork->kind].y;
    coord->coord.t[2]   = gGeneratorLifeSupportPos[parentWork->kind].z;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    contacts                    = part->contacts;
    enemy->coord                = coord;
    enemy->bodyPos.vx           = 0;
    enemy->bodyPos.vy           = 0;
    enemy->bodyPos.vz           = 0;
    enemy->param                = &gGeneratorLifeSupportParams;
    enemy->recs                 = contacts;
    enemy->hp                   = gGeneratorLifeSupportParams.hpMax;
    part->effectArg.spawnArgLo  = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_MAGNITUDE;
    part->effectArg.coord       = coord;
    part->effectArg.spawnArgHi  = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_COUNT;
    part->body.coord            = coord;
    part->body.context.contacts = contacts;
    part->body.pos.vx           = 0;
    part->body.pos.vy           = 0;
    part->body.pos.vz           = 0;
    // Inherit the parent's collision key for this independently linked sphere.
    reloadedParentWork = task->parent->work;
    part->body.key     = reloadedParentWork->rootBody.key;
    part->body.radius  = GENERATOR_LIFE_SUPPORT_RADIUS;
    part->body.flags   = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &part->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(part->contacts), 0);
    part->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    kind              = parentWork->kind;
    part->kind        = kind;
    // Use the same variant for sprite visibility and the saved intact flag.
    if (kind == GENERATOR_BETA) {
        neoArkPowerPlant2SetView6SpritesHidden(1);
        flag = GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN;
    } else {
        neoArkPowerPlant1SetLifeSupportSpritesHidden(1);
        flag = GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN;
    }
    gameFlagSetNibble(flag, 0);
    task->state = GENERATOR_TASK_ACTIVE;
}
