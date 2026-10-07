/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x7DB handler of the second arming state's message table, for messages
/// from sender 0xB05: command 0 stops the countdown at -1, command 2 hands
/// spawn slot 0 to placed actor 0, sends it message 0x7DB and places it
/// at (5, 0, -0x320) facing 0x400, restarting the countdown. Answers 1 only
/// for command 2.
s32 roamerAmbushMsg(Task* task, s32 arg1, struct ActorCommand* msg, s32 arg3)
{
    s32    result;
    u16    cmd;
    Enemy* obj;

    result = 0;
    if (msg->context.key == 0xB05) {
        cmd = msg->command;
        switch (cmd) {
            case 0:
                gRoamerCooldown = -1;
                result          = 0;
                return result;
            case 2:
                gRoamerCommand.context.loc.stage = 5;
                gRoamerCommand.context.loc.area  = 0xB;
                gRoamerCommand.command           = 0xC;
                result                           = 1;
                if (sceneFindPlacedActor(0) != 0) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY,
                                                  &gRoamerCommand, 0);
                    obj                                                    = sceneFindPlacedActor(0)->spawnArg2.pointer;
                    sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[0] = 5;
                    sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[1] = 0;
                    sceneFindPlacedActor(0)->extra.tmd->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        obj->hp             = gRoamerReserveHp[0];
                        gRoamerReserveHp[0] = 0;
                        obj->reactionFlags  = 0;
                    }
                    gfxRotMatrixY(&sceneFindPlacedActor(0)->extra.tmd->coords->coord,
                                  0x400, 1);
                    gRoamerCooldown = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}
