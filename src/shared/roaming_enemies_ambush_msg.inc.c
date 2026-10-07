/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Revives the first placed enemy from reserve slot zero at the ambush position.
///
/// Uses the prepared shared command and keeps repeated actor lookups around
/// synchronous dispatch. No reserve HP is consumed when the enemy work is absent.
static inline void _roamerReviveAmbushEnemy(void)
{
    Enemy* enemy;

    if (sceneFindPlacedActor(0) != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY,
                                      &gRoamerCommand, 0);
        enemy                                                  = sceneFindPlacedActor(0)->spawnArg2.pointer;
        sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[0] = 5;
        sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[1] = 0;
        sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[2] = -0x320;
        if (enemy != 0) {
            enemy->hp            = gRoamerReserveHp[0];
            gRoamerReserveHp[0]  = 0;
            enemy->reactionFlags = 0;
        }
        gfxRotMatrixY(&sceneFindPlacedActor(0)->extra.tmd->coords->coord,
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
