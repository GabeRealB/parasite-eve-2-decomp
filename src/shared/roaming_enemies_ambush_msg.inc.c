/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Revives the first placed enemy from reserve slot zero at the ambush position.
///
/// Requires the prepared ambush command and actor 0's live TMD body when that
/// actor exists. Dispatches before looking up the enemy and placing its root at
/// (5, 0, -800) world units, facing a quarter-turn. Transfers reserve slot 0's
/// HP and clears reactions only when an enemy exists, then restarts the shared
/// cooldown. Repeated lookups preserve the order around synchronous dispatch.
static inline void _roamerReviveAmbushEnemy(void)
{
    enum { ROAMER_AMBUSH_ACTOR_INDEX  = 0,
           ROAMER_AMBUSH_RESERVE_SLOT = 0,
           ROAMER_AMBUSH_WORLD_X      = 5,
           ROAMER_AMBUSH_WORLD_Y      = 0,
           ROAMER_AMBUSH_WORLD_Z      = -800 };
    Enemy* enemy;

    if (sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX) != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX), ACTOR_COMMAND_MESSAGE_APPLY,
                                      &gRoamerCommand, 0);
        enemy                                                                          = sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX)->spawnArg2.pointer;
        sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX)->extra.tmd->coords->coord.t[0] = ROAMER_AMBUSH_WORLD_X;
        sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX)->extra.tmd->coords->coord.t[1] = ROAMER_AMBUSH_WORLD_Y;
        sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX)->extra.tmd->coords->coord.t[2] = ROAMER_AMBUSH_WORLD_Z;
        if (enemy != NULL) {
            enemy->hp                                    = gRoamerReserveHp[ROAMER_AMBUSH_RESERVE_SLOT];
            gRoamerReserveHp[ROAMER_AMBUSH_RESERVE_SLOT] = 0;
            enemy->reactionFlags                         = 0;
        }
        gfxRotMatrixY(&sceneFindPlacedActor(ROAMER_AMBUSH_ACTOR_INDEX)->extra.tmd->coords->coord,
                      ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_REPLACE);
        _gRoamerCooldownFrames = ROAMER_ACTION_COOLDOWN_FRAMES;
    }
}

/// Handles the forest pool's pause and placed-enemy ambush commands.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; borrows `msg` through dispatch.
/// Accepts only the Neo Ark forest-zone namespace. Command 0 pauses the shared
/// cooldown; command 2 requests actor 0's ambush, transfers reserve slot 0's
/// HP if its enemy exists, places its root and restarts the cooldown.
/// Returns 1 for command 2 even when the actor is absent, otherwise 0.
/// The receiver, message ID and second payload are unused.
static s32 _roamerAmbushMsg(Task* task, s32 messageId, struct ActorCommand* msg, s32 unusedArg)
{
    enum {
        ROAMER_AMBUSH_CONTEXT       = (GAME_AREA_NEO_ARK_FOREST_ZONE << 8) | GAME_STAGE_SHELTER_NEO_ARK,
        ROAMER_AMBUSH_COMMAND_PAUSE = 0,
        ROAMER_AMBUSH_COMMAND_START = 2,
        ROAMER_AMBUSH_ENEMY_COMMAND = 12,
    };
    s32 result;
    u16 command;
    result = 0;
    if (msg->context.key == ROAMER_AMBUSH_CONTEXT) {
        command = msg->command;
        switch (command) {
            case ROAMER_AMBUSH_COMMAND_PAUSE:
                _gRoamerCooldownFrames = ROAMER_COOLDOWN_PAUSED;
                result                 = 0;
                return result;
            case ROAMER_AMBUSH_COMMAND_START:
                gRoamerCommand.context.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
                gRoamerCommand.context.loc.area  = GAME_AREA_NEO_ARK_FOREST_ZONE;
                gRoamerCommand.command           = ROAMER_AMBUSH_ENEMY_COMMAND;
                result                           = 1;
                _roamerReviveAmbushEnemy();
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}
