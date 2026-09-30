#ifndef MAIN_PRIVATE_MC_H
#define MAIN_PRIVATE_MC_H

#include <psyq/sys/types.h>
#include <psyq/kernel.h>

#include "types.h"

#include "main/mc_types.h"
#include "main/task_types.h"

/// State shared by memory-card tasks and their slot/detail UI children.
/// Each of the fifteen directory entries has a bounded 128-byte save preview.
/// blockOwners maps the fifteen usable card blocks to their directory indices:
/// -1 means free, -2 means occupied by another game's file.
/// syncCommand/syncResult are the MemCardSync outputs; sectorOffset is measured
/// in 128-byte sectors. checksum and checksumComplement cover the transfer data.
typedef struct _McWork {
    /* 0x000 */ s32             field_0;
    /* 0x004 */ s32             field_4;
    /* 0x008 */ s32             promptId;
    /* 0x00C */ s32             field_C;
    /* 0x010 */ long            syncCommand;
    /* 0x014 */ long            syncResult;
    /* 0x018 */ void*           buffer;
    /* 0x01C */ s32             sectorOffset;
    /* 0x020 */ s32             transferBytes;
    /* 0x024 */ s32             field_24;
    /* 0x028 */ s32             field_28;
    /* 0x02C */ s32             field_2C;
    /* 0x030 */ struct DIRENTRY directory[15];
    /* 0x288 */ long            entryCount;
    /* 0x28C */ s32             freeBlockCount;
    /* 0x290 */ s32             selectedSlot;
    /* 0x294 */ McSavePreview   previews[15];
    /* 0xA14 */ s32             currentSlot;
    /* 0xA18 */ s32             field_A18;
    /* 0xA1C */ u16             checksum;
    /* 0xA1E */ u16             checksumComplement;
    /* 0xA20 */ s32             field_A20;
    /* 0xA24 */ s8              blockOwners[0x10];
} McWork;
STATIC_ASSERT_SIZEOF(McWork, 0xA34);

struct UiObject;

extern u8 McText_Yes[];

extern u8 McText_No[];

extern u8 McText_Cancel[];

extern u8 McText_Ok[];

// Save-slot detail labels and indexed descriptions.
extern u8* Mc_LocationTitleLabels[];

extern const char McText_Time[];

extern const char McText_Clear[];

extern const char McText_OpenParen[];

extern const char McText_Exp[];

extern const char McText_Unavailable[];

extern const char McText_Bp[];

extern u8* Mc_LocationLabels[];

extern const char McText_Nightmare[];

extern const char McText_Scavenger[];

extern const char McText_Bounty[];

extern const char McText_Replay[];

/// Render the selected memory-card slot and its saved statistics.
/// Draws the selected slot's place, play time, clear count and saved statistics.
void Mc_DrawSlotDetails(struct UiObject* obj, McWork* work, s32 slot, s32 x, s32 y);

void Mc_DispatchStateTable(Task* task);

void Mc_InitLib(void);

void Mc_DispatchStateTable26(Task* task);

/// Reserved memory-card task callback with no runtime work.
void McMenu_NoOpTask(Task* unused);

#endif // MAIN_PRIVATE_MC_H
