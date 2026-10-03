#ifndef GAMEPLAY_PRIVATE_CAP_H
#define GAMEPLAY_PRIVATE_CAP_H

#include "common.h"

#include "gameplay/cap.h"

/// Confirm-sound class in bits 8-11 of a CAP choice code.
///
/// Confirming a choice plays `SOUND_SYSTEM_CONFIRM` for confirm, nothing for
/// silent, and `SOUND_SYSTEM_CURSOR` for cursor. Retail text uses confirm and
/// silent; cursor is handled and does not occur.
enum {
    CAP_CHOICE_SOUND_CONFIRM = 1,
    CAP_CHOICE_SOUND_SILENT  = 2,
    CAP_CHOICE_SOUND_CURSOR  = 3
};

/// Choice-code family. Masking a text code with 0xFF00 compares it with one
/// of these. The code's low byte is the variant key and is not part of the family.
enum {
    CAP_TEXT_CHOICE_CONFIRM = 0x8100,
    CAP_TEXT_CHOICE_SILENT  = 0x8200,
    CAP_TEXT_CHOICE_CURSOR  = 0x8300
};

/// Bits 8-11 of a choice code, the confirm-sound class.
enum { CAP_TEXT_CHOICE_SOUND_MASK = 0xF00 };

/// Slots in the choice table filled while one CAP text stream is drawn.
///
/// Retail streams use at most ten. Fifteen slots fill the bytes up to the
/// next gameplay BSS object.
enum { CAP_CHOICE_CAPACITY = 15 };

/// One dialogue choice recorded while drawing a CAP text stream.
///
/// A choice code stores the pen where its line begins, the variant key in the
/// code's low byte, and the confirm-sound class in bits 8-11. Confirming the
/// highlighted choice copies `eventKey` into the running script's variant key.
/// The record is eight bytes so each slot stays two-byte aligned; nothing
/// stores the byte after `confirmSound`.
typedef struct {
    s16 x;            // Pen X in pixels from the screen centre.
    s16 y;            // Pen Y in pixels from the screen centre, increasing downward.
    u16 eventKey;     // Variant key from the choice code's low byte. The stored high byte is 0.
    u8  confirmSound; // CAP_CHOICE_SOUND_CONFIRM, CAP_CHOICE_SOUND_SILENT or CAP_CHOICE_SOUND_CURSOR.
} CapChoice;
STATIC_ASSERT_SIZEOF(CapChoice, 8);

/// Writing direction of drawn CAP text.
///
/// The text drawer consults it wherever the pen moves: after a glyph, after a
/// spacer code and at a line break. The one instance is read-only and selects
/// horizontal text, so the vertical paths are handled and never run.
///
/// The flag stays a member of an aggregate: the drawer's reads are ordered as
/// member accesses, which a bare byte is not. Only this byte is read, and no
/// further member is established.
typedef struct {
    u8 vertical; // 0 lines run left to right and stack downward; nonzero columns run top to bottom and stack leftward.
} CapTextLayout;

#endif // GAMEPLAY_PRIVATE_CAP_H
