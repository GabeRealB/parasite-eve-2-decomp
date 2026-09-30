/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x7DB handler of the second arming state's message table, for messages
/// from sender 0xB05: command 0 stops the countdown at -1, command 2 hands
/// spawn slot 0 to the first slot-4 task, sends it message 0x7DB and places it
/// at (5, 0, -0x320) facing 0x400, restarting the countdown. Answers 1 only
/// for command 2.
s32 roamerAmbushMsg(Task* task, s32 arg1, TaskMessageArg msg, TaskMessageArg arg3)
{
    s32    result;
    u16    cmd;
    Enemy* obj;

    result = 0;
    if (msg.command->context.key == 0xB05) {
        cmd = msg.command->command;
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
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY,
                                      &gRoamerCommand, 0);
                    obj                                              = Gp_LookupSlot4(0)->spawnArg2.pointer;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[0] = 5;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[1] = 0;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        obj->hp             = gRoamerReserveHp[0];
                        gRoamerReserveHp[0] = 0;
                        obj->reactionFlags  = 0;
                    }
                    gfxRotMatrixY(&Gp_LookupSlot4(0)->extra.tmd->coords->coord,
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
