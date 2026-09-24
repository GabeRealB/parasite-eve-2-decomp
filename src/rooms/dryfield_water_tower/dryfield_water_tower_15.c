#include "common.h"

#include "gameplay/1BC.h"
#include "main/fs.h"

/// Record handler (opcode 0x0D) of one of the room's script tables: queues
/// CD command 0x81.
void func_dryfield_water_tower_80180134(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of one of the room's script tables: calls
/// `Gp_RestoreStreamRng`.
void func_dryfield_water_tower_80180154(void)
{
    Gp_RestoreStreamRng();
}
