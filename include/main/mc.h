#ifndef MAIN_MC_H
#define MAIN_MC_H

#include "main/mc_types.h"
#include "main/ui_types.h"

/// Resident memory-card dialog recipes, in load/save order.
enum {
    MEMORY_CARD_DIALOG_LOAD  = 0,
    MEMORY_CARD_DIALOG_SAVE  = 1,
    MEMORY_CARD_DIALOG_COUNT = 2
};

/// Resident panel recipes for the complete load and save dialogs.
///
/// Pass the selected element to `uiSpawnObject`. Save contentArg zero asks the
/// initial save question; nonzero skips it. Both dialogs share one resident
/// work area and must run sequentially, including their child panels and I/O.
/// The UI lifecycle owns each spawned object; it reads the recipe synchronously.
extern UiObjectDesc gMcDialogDescriptors[MEMORY_CARD_DIALOG_COUNT];

/// Resident memory-card save images: the live record and its backup.
///
/// Each element is one `McSaveData` image. The game reads and writes
/// `MEMORY_CARD_SAVE_LIVE`. `MEMORY_CARD_SAVE_BACKUP` is addressed only as the
/// following bytes of that same buffer: the card code compares the two images,
/// copies the live image over the backup, and transfers both when loading.
/// Initialization clears the live image and fills the backup with 0xFF. Saving
/// checksums the live image and writes it to the card twice before that copy.
/// A save-header check reads the live image through its `preview` member.
extern McSaveData gMcSaveData[MEMORY_CARD_SAVE_COUNT];

/// Restore the saved option defaults and apply stereo output and music volume.
///
/// Selects vibration on, key layout A, full music volume, cursor memory,
/// stereo sound and walking as the default movement.
void mcResetOptions(void);

/// Reset the resident save records for a new game and apply the default options.
///
/// Clears each live record, fills its adjacent backup with 0xFF and writes the
/// live record's checksum before seeding player status and the opening location.
/// Seeding and option writes change the payloads after those initial checksums;
/// saving computes fresh checksums. The card file header is kept intact.
/// Also selects display resource variant 1 and applies stereo and music volume.
void mcResetSaveData(void);

#endif // MAIN_MC_H
