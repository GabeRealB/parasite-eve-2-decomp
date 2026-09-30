#ifndef MAIN_SOUND_TYPES_H
#define MAIN_SOUND_TYPES_H

#include "common.h"

/// 0x10-byte linear interpolator state used by LinInterp_Setup / LinInterp_Apply /
/// LinInterp_Step. Embedded at MidiSong::field_14 and in the CD audio player's state.
typedef struct _LinInterp {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} LinInterp;
STATIC_ASSERT_SIZEOF(LinInterp, 0x10);

/// Type-1 script request with only the bank-type key set.
///
/// Bits 16..31 hold the type-1 bank key (`SOUND_BANK_TYPE_1`, 0x1000) and the
/// low half is zero, so the word names the loaded type-1 bank rather than one
/// entry or instance. Bits 28..31 are that type's nibble. Masking any type-1
/// request with 0xF0000000 yields this value, including one that carries a
/// retail bank number and an entry.
enum { SOUND_SCRIPT_REQUEST_TYPE_1 = 0x10000000 };

#endif // MAIN_SOUND_TYPES_H
