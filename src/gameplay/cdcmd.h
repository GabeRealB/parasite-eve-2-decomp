#ifndef GAMEPLAY_PRIVATE_CDCMD_H
#define GAMEPLAY_PRIVATE_CDCMD_H

#include "main/fs.h"

/// Appends a copy of a saved CD request and returns its ring slot (0..7).
///
/// Preserves all four argument bytes, including those unused by the opcode.
/// `entry` must be a readable complete request; its pointer is not retained.
/// Uses `cdCmdEnqueue`'s free-ring-capacity contract. File-key byte 1 is ignored
/// by that API and is left uninitialized.
static inline s32 _cdCmdEnqueueEntry(const CdCmdEntry* entry)
{
    u8 fileKeyBytes[4];
    u8 commandArgs[sizeof(entry->args.bytes)];

    // Snapshot both byte blocks before an enqueue can reuse the source ring slot.
    fileKeyBytes[3] = entry->stage;
    fileKeyBytes[2] = entry->fileGroup;
    fileKeyBytes[0] = entry->fileIndex;
    commandArgs[0]  = entry->args.bytes[0];
    commandArgs[1]  = entry->args.bytes[1];
    commandArgs[2]  = entry->args.bytes[2];
    commandArgs[3]  = entry->args.bytes[3];
    return cdCmdEnqueue(entry->cmd, fileKeyBytes, commandArgs);
}

#endif // GAMEPLAY_PRIVATE_CDCMD_H
