#include "captions.h"

#include <psyq/strings.h>

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "gameplay/captions.h"

#include "main/text.h"

CapChoice gCapChoices[CAP_CHOICE_CAPACITY];

u8 gCapRecordSoundIssued;

s16 gCapDebugPlaybackFrame;

u16 gCapContinueCaretX;

u16 gCapContinueCaretY;

s16 gCapTextCursorX;

s16 gCapTextCursorY;

s16 gCapTexturePageVramX;

s16 gCapTexturePageVramY;

u8 gCapContinueCaretDelayCalls;

u8 gCapChoiceConfirmFramesRemaining;

u8 gCapPlaybackStarted;

u16 gCapSavedImageMdecMode;

CapTextUpdateCallback gCapTextUpdateCallback;

s16 D_80115664;

s16 gCapPlaybackMode;

s16 gCapVariantKey;

s16 gCapSceneSyncAdjustmentFrames;

u8 gCapSavedView;

u8 gCapActionReturnView;

u8 gCapSceneSyncPhase;

u8 D_8011566F;

u8 gCapRecordSoundAndTextFlags;

Task* gCapPlaybackTask;

s16 gCapMinDisplayFramesRemaining;

s16 gCapAdvanceDelayFrames;

const TextGlyphCell* gCapGlyphCells;

u8 gCapChoiceRowStride;

bool capRelocateFile(CapFile* file)
{
    /// Rebases a text reference or skips the slot following a terminator.
    ///
    /// `recordCursor` is a writable `CapSequenceRecord*` local, `fileBase` is
    /// its containing `CapFile*`, and `endTextRef` is `CAP_TEXT_REF_END`.
    /// Arguments have no side effects; the cursor is evaluated more than once.
    /// The caller advances one record after this block. A terminal reference
    /// is preserved, and its extra skip is not counted.
#define CAP_RELOCATE_SEQUENCE_TEXT_RECORD(recordCursor, fileBase, endTextRef) \
    {                                                                         \
        if ((recordCursor)->textRef.offset != (endTextRef)) {                 \
            (recordCursor)->textRef.offset += (u32)(fileBase);                \
        } else {                                                              \
            (recordCursor)++;                                                 \
        }                                                                     \
    }

    enum { CAP_MAGIC_PREFIX_BYTES = 3 };
    s32                entryIndex;
    s32                sequenceRecordCount;
    s32                sequenceEndRef;
    s32                commandCount;
    CapSequenceRecord* record;
    CapCommandRef*     commandRef;
    CapSequenceTable*  sequenceTable;
    CapCommandTable*   commandTable;

    if (strncmp(file->magic, gCapMagicPrefix, CAP_MAGIC_PREFIX_BYTES) != 0) {
        return false;
    }

    entryIndex = 0;
    // Relocated KSEG0 addresses are negative in the signed offset word.
    if (file->glyphs.offset > 0) {
        // Add the 32-bit address to serialized byte offsets, without pointer scaling.
        file->glyphs.offset    += (u32)file;
        file->sequences.offset += (u32)file;
        file->commands.offset  += (u32)file;
        sequenceTable           = file->sequences.table;
        record                  = sequenceTable->records;
        sequenceRecordCount     = sequenceTable->count;
        // Terminators skip the next slot so later sequences' commands stay intact.
        if (sequenceRecordCount > 0) {
            sequenceEndRef = CAP_TEXT_REF_END;
            do {
                CAP_RELOCATE_SEQUENCE_TEXT_RECORD(record, file, sequenceEndRef);
                entryIndex++;
                record++;
            } while (entryIndex < sequenceRecordCount);
#undef CAP_RELOCATE_SEQUENCE_TEXT_RECORD
        }
        commandTable = file->commands.table;
        entryIndex   = 0;
        commandCount = commandTable->count;
        commandRef   = commandTable->entries;
        if (commandCount > 0) {
            do {
                if (commandRef->offset != 0) {
                    commandRef->offset += (u32)file;
                }
                entryIndex++;
                commandRef++;
            } while (entryIndex < commandCount);
        }
    }

    // Repeated calls republish the tables without rebasing them again.
    gCapGlyphCells     = file->glyphs.cells;
    gCapCommandEntries = file->commands.table->entries;
    return true;
}
