#include "common.h"

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_801804DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_factory/dryfield_factory_5", func_dryfield_factory_80180644);
