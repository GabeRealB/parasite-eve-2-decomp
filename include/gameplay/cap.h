#ifndef GAMEPLAY_CAP_H
#define GAMEPLAY_CAP_H

#include "common.h"

#include "main/text.h"

/// Text renderer installed by CAP event scripts.
typedef void (*GpCapTextCb)(s16, s16, u16*, s16, s32);

struct _GpCapCmd;

/// File-relative byte offset before relocation; text pointer afterward.
/// The -1 terminator remains an integer in either state.
typedef union {
    s32  offset;
    u16* text;
} GpCapTextRef;
STATIC_ASSERT_SIZEOF(GpCapTextRef, 4);

/// 0xC-byte sequence-table record. `Gp_CapTable` is the current table
/// (`Gp_StartCap` stores its first arg there). `Gp_FindCapEvt` walks
/// from a start index until `field_8.offset == -1` (terminator) or `field_5`
/// equals `Gp_CapEventKey` (the key `Gp_StartCap` saved from its third arg).
/// When not -1, `field_8.text` is a relocated `u16*` text stream walked by
/// `Gp_CapTextTopY` / `Gp_CapTextHeight` / `func_800E6BB8` / `Gp_CapCenterX` / `Gp_CapCenterXLine` (codes `-1` end,
/// `-2` newline, `-3` skip; else glyph index `& 0x3FF` into `Gp_CapGlyphs`).
typedef struct _GpEvt12 {
    union {
        struct {
            /* 0x0 */ u8 field_0;
            /* 0x1 */ u8 field_1;
            /* 0x2 */ u8 field_2;
            /* 0x3 */ u8 field_3;
        } bytes;
        u32 packed;
    } prefix;
    /* 0x4 */ u8           field_4; // flags copied to D_80115670; bit 0 cleared if field_7
    /* 0x5 */ u8           field_5; // compared with Gp_CapEventKey
    /* 0x6 */ u8           field_6;
    /* 0x7 */ u8           field_7; // copied to D_80115678
    /* 0x8 */ GpCapTextRef field_8;
} GpEvt12;
STATIC_ASSERT_SIZEOF(GpEvt12, 0xC);

/// 0x10-byte header in front of a `GpEvt12` array inside a `GpCapFile`
/// (`field_C`). `count` is the first halfword; the records start at
/// `records`. `Gp_RelocCapFile` relocates each record's `field_8.offset` unless
/// it is `-1`, in which case it also skips the next record.
typedef struct _GpCapEvtTable {
    /* 0x00 */ s16     count;
    /* 0x02 */ byte    pad_2[0xE];
    /* 0x10 */ GpEvt12 records[0];
} GpCapEvtTable;
STATIC_ASSERT_SIZEOF(GpCapEvtTable, 0x10);

/// The CAP file's command pointer table: a count, then that many entries. Each
/// entry is a file-relative offset until relocation adds the file base, making
/// it the address of a command record; a zero entry is left as no record.
typedef union {
    s32               offset;
    struct _GpCapCmd* command;
    GpEvt12*          events;
} GpCapEntry;
STATIC_ASSERT_SIZEOF(GpCapEntry, 4);

typedef struct _GpCapPtrTable {
    /* 0x0 */ s32        count;
    /* 0x4 */ GpCapEntry entries[0];
} GpCapPtrTable;
STATIC_ASSERT_SIZEOF(GpCapPtrTable, 4);

/// Command record pointed to by `Gp_CapCmds[index]`. `Gp_RunCapCmd` switches
/// on `field_0` and may follow `field_8` to another index. Flag id is
/// `field_3 | (field_7 << 8)`. `field_1` bits: 0 = wrap counter, 1 = persist
/// counter in a game-flag nibble, 2 = skip/compare against `field_2`.
/// `Gp_StartCapSlot` then starts the event at the same table slot.
typedef struct _GpCapCmd {
    /* 0x0 */ u8 field_0; // opcode (0..4)
    /* 0x1 */ u8 field_1; // flags
    /* 0x2 */ u8 field_2; // counter limit
    /* 0x3 */ u8 field_3; // flag id lo
    /* 0x4 */ u8 field_4; // live counter
    /* 0x5 */ u8 field_5; // first 2-bit slot (`Gp_GetCurBit2Flag`)
    /* 0x6 */ u8 field_6; // slot count
    /* 0x7 */ u8 field_7; // flag id hi
    /* 0x8 */ u8 field_8; // next command index
} GpCapCmd;

/// In-memory CAP dialogue file (`strncmp` magic `"CAP"`). Offsets at
/// `field_8` / `field_C` / `field_10` are file-relative until
/// `Gp_RelocCapFile` adds the file base. The signed glyph offset is positive
/// before relocation; a relocated PS1 KSEG0 pointer has its high bit set. After that, `field_8` is a
/// `GlyphUvwh*` published as `Gp_CapGlyphs`, `field_C` is a
/// `GpCapEvtTable*`, and `field_10` is a `GpCapPtrTable*` whose
/// entries (nonzero) are relocated `GpEvt12*` values.
typedef struct _GpCapFile {
    /* 0x00 */ char magic[4];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ union {
        s32        offset;
        GlyphUvwh* ptr;
    } field_8;
    /* 0x0C */ union {
        s32            offset;
        GpCapEvtTable* ptr;
    } field_C;
    /* 0x10 */ union {
        s32            offset;
        GpCapPtrTable* ptr;
    } field_10;
} GpCapFile;
STATIC_ASSERT_SIZEOF(GpCapFile, 0x14);

/// CAP relocation runs in the PS1's 32-bit address space. Keep the base's
/// numeric address explicit while adding it to serialized offset words.
typedef union {
    GpCapFile* file;
    u32        address;
} GpCapFileAddress __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(GpCapFileAddress, 4);

/// Resolve a record's byte displacement within a relocated CAP script.
/// CAP references share their integer address and pointer representations;
/// this preserves that representation through the indexed address addition.
static inline GpEvt12* Gp_CapEventAt(GpEvt12* events, s32 index)
{
    GpCapEntry entry;
    entry.events = events;
    entry.offset = index * sizeof(GpEvt12) + entry.offset;
    return entry.events;
}

#endif // GAMEPLAY_CAP_H
