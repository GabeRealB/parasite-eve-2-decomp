#include "common.h"

#include "main/fs.h"

/// Record handler (opcode 0x0D) of one of the room's script tables: queues
/// CD command 0x82.
void func_dryfield_water_tower_80180114(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
