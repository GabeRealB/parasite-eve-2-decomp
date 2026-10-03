#ifndef MAIN_WIPSYS_H
#define MAIN_WIPSYS_H

#include "main/wipsys_types.h"

extern GameMainPersistentState Wip_SysFlags;

/// Resident player state and its serialized memory-card backup.
///
/// The live image occupies the first `PLAYER_STATUS_SAVE_RECORD_BYTES` bytes;
/// memory-card operations maintain the following `saveBackup` image. This
/// storage survives overlay loads and room changes. Its borrowed `coordMtx`
/// must be rebound to the live player actor after loading saved bytes and
/// before querying the player's position.
/// Only one `PlayerStatus` object is reserved; character-indexed accesses
/// require a zero index into this object.
extern PlayerStatus gPlayerStatus;

#endif // MAIN_WIPSYS_H
