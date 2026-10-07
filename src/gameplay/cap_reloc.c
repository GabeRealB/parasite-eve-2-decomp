#include "captions.h"

#include <psyq/strings.h>

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "gameplay/captions.h"

#include "main/text.h"

CapChoice D_801155D0[CAP_CHOICE_CAPACITY];

u8 D_80115648;

s16 D_8011564A;

u16 Gp_CapCaretX;

u16 Gp_CapCaretY;

s16 D_80115650;

s16 D_80115652;

s16 D_80115654;

s16 D_80115656;

u8 Gp_CapCaretDelay;

u8 D_80115659;

u8 D_8011565A;

u16 D_8011565C;

CapTextUpdateCallback D_80115660;

s16 D_80115664;

s16 D_80115666;

s16 Gp_CapEventKey;

s16 D_8011566A;

u8 D_8011566C;

u8 D_8011566D;

u8 D_8011566E;

u8 D_8011566F;

u8 D_80115670;

Task* Gp_CapTask;

s16 D_80115678;

s16 D_8011567A;

TextGlyphCell* Gp_CapGlyphs;

u8 D_80115680;

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

    if (strncmp(file->magic, Gp_StrCapMagic, CAP_MAGIC_PREFIX_BYTES) != 0) {
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
    Gp_CapGlyphs = file->glyphs.cells;
    Gp_CapCmds   = file->commands.table->entries;
    return true;
}
