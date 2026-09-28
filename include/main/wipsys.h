#ifndef MAIN_WIPSYS_H
#define MAIN_WIPSYS_H

#include "main/wipsys_types.h"

extern WipSysFlags Wip_SysFlags;

/// Resident player state, followed by its memory-card backup copy.
/// Character IDs are one-based; the indexed callers use (&Player_Status)[id - 1].
/// The compiler folds -1 into the address as 0x80073B08, inside the preceding
/// save buffer. That adjusted address is not a second PlayerStatus object.
extern PlayerStatus Player_Status;

#endif // MAIN_WIPSYS_H
