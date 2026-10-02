#ifndef MAIN_PRIVATE_TEXT_TYPES_H
#define MAIN_PRIVATE_TEXT_TYPES_H

#include "common.h"

#include "main/text.h"

/// Text stream / font draw object (e.g. D_800630B0).
/// tpageX low 6 bits are the SPRT u base. chars: 0xFE newline, 0xFF end.
typedef struct _TextStream {
    /* 0x00 */ s16            x;
    /* 0x02 */ s16            y;
    /* 0x04 */ s16            tpageX;
    /* 0x06 */ s16            tpageY;
    /* 0x08 */ s16            clutX;
    /* 0x0A */ s16            clutY;
    /* 0x0C */ s16            charDelay;
    /* 0x0E */ s16            cursor;
    /* 0x10 */ u8*            chars;
    /* 0x14 */ TextGlyphCell* glyphs;
    /* 0x18 */ s16            lineHeight;
    /* 0x1A */ s16            delayReload;
    /* 0x1C */ s16            field_1C;
    /* 0x1E */ s16            field_1E;
} TextStream;
STATIC_ASSERT_SIZEOF(TextStream, 0x20);

#endif // MAIN_PRIVATE_TEXT_TYPES_H
