/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Applies a borrowed actor command in the Patio or Dryfield Toilet namespace.
///
/// The task requires initialized work and a model. Patio command 1 selects
/// `ODD_STRANGER_STATE_DORMANT_SCRIPTED`; Toilet command 0 hides it and command
/// 2 places it for `ODD_STRANGER_STATE_AMBUSH`. Returns 1 for those commands,
/// zero otherwise. `messageId` and `unused` are ignored. Caches exactly the
/// first three payload bytes, including only the low byte of the command.
static s32 _oddStrangerApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused)
{
    enum {
        ODD_STRANGER_COMMAND_PATIO_DORMANT = 1,
        ODD_STRANGER_COMMAND_TOILET_HIDE   = 0,
        ODD_STRANGER_COMMAND_TOILET_AMBUSH = 2
    };

    u16              contextKey;
    u16              patioCommand;
    u16              toiletCommand;
    OddStrangerWork* work;

    work = task->work;
    // Retain the partial byte cache; comparisons below read both full halfwords.
    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = (u8)command->command;
    contextKey            = command->context.key;
    if (contextKey == ((GAME_AREA_ACROPOLIS_PATIO << 8) | GAME_STAGE_ACROPOLIS)) {
        patioCommand = command->command;
        if (patioCommand == ODD_STRANGER_COMMAND_PATIO_DORMANT) {
            work->state = ODD_STRANGER_STATE_DORMANT_SCRIPTED;
            return 1;
        }
        return 0;
    }
    if (contextKey == ((GAME_AREA_DRYFIELD_TOILET << 8) | GAME_STAGE_DRYFIELD)) {
        toiletCommand = command->command;
        switch (toiletCommand) {
            case ODD_STRANGER_COMMAND_TOILET_HIDE:
                work->state = ODD_STRANGER_STATE_HIDDEN;
                return 1;
            case ODD_STRANGER_COMMAND_TOILET_AMBUSH:
                work->state                         = ODD_STRANGER_STATE_AMBUSH;
                task->extra.tmd->coords->coord.t[0] = -0x595;
                task->extra.tmd->coords->coord.t[1] = 0;
                task->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 1);
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}
