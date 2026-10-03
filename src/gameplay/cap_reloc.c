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

s32 Gp_RelocCapFile(CapFileAddress base)
{
    s32                i;
    s32                count;
    s32                flag;
    CapSequenceRecord* rec;
    CapCommandRef*     ptr;
    CapSequenceTable*  sequenceTable;
    CapCommandTable*   commandTable;

    if (strncmp(base.file->magic, Gp_StrCapMagic, 3) != 0) {
        return 0;
    }

    i = 0;
    if (base.file->glyphs.offset > 0) {
        base.file->glyphs.offset    += base.address;
        base.file->sequences.offset += base.address;
        base.file->commands.offset  += base.address;
        sequenceTable                = base.file->sequences.table;
        rec                          = sequenceTable->records;
        count                        = sequenceTable->count;
        if (count > 0) {
            flag = CAP_TEXT_REF_END;
            do {
                if (rec->textRef.offset != flag) {
                    rec->textRef.offset += base.address;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        commandTable = base.file->commands.table;
        i            = 0;
        count        = commandTable->count;
        ptr          = commandTable->entries;
        if (count > 0) {
            do {
                if (ptr->offset != 0) {
                    ptr->offset += base.address;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    Gp_CapGlyphs = base.file->glyphs.cells;
    Gp_CapCmds   = (base.file->commands.table)->entries;
    return 1;
}
