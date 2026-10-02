#ifndef GAMEPLAY_CAP_H
#define GAMEPLAY_CAP_H

#include "common.h"

#include "main/text.h"

/// Callback for cursor effects and code cues during timed CAP text reveal.
///
/// Called after drawing and updating the reveal delay. `cursorX` / `cursorY`
/// are pixels relative to the screen centre, with vertical shake removed from Y.
/// `text` is the borrowed, 0xFFFF-terminated CAP code stream. `revealIndex` is a
/// zero-based u16 element index after the delay update; `codeAdvanced` is 1 when
/// that update advanced the index, otherwise 0. On the completion frame the
/// index can be one element past the terminator, within the containing CAP file.
/// The stream must remain live for the call, and the callback's overlay must
/// remain loaded until it is cleared. Starting CAP playback clears the callback.
typedef void (*CapTextUpdateCallback)(s32 cursorX, s32 cursorY, const u16* text, s32 revealIndex, s32 codeAdvanced);

struct _GpCapCmd;

/// End of a CAP sequence table; relocation leaves this reference unchanged.
enum { CAP_TEXT_REF_END = -1 };

/// A CAP sequence record's serialized or relocated text reference.
///
/// Nonterminal references are byte offsets from the CAP file base until in-place
/// relocation replaces them with addresses. `offset == CAP_TEXT_REF_END` ends
/// the sequence in either state and must not be dereferenced through `text`.
/// Text is a borrowed stream of u16 codes ending in 0xFFFF.
/// Its containing CAP file must remain loaded throughout playback.
typedef union {
    s32        offset; // File-relative byte offset before relocation; also tests CAP_TEXT_REF_END.
    const u16* text;   // Relocated, read-only CAP code stream borrowed from the loaded file.
} CapTextRef;
STATIC_ASSERT_SIZEOF(CapTextRef, 4);

/// Flags and encoded values in a CAP playback record.
enum {
    CAP_SEQUENCE_LEFT_ALIGN          = 0x02,
    CAP_SEQUENCE_FORCE_CARET         = 0x04,
    CAP_SEQUENCE_DELAYED_MESSAGE     = 0x08,
    CAP_SEQUENCE_TITLE_BANK          = 0x10,
    CAP_SEQUENCE_SCENE_CONTROL       = 0x20,
    CAP_SEQUENCE_SCENE_PHASE         = 0x40,
    CAP_SEQUENCE_VIEW_CONTROL        = 0x80,
    CAP_SEQUENCE_INSTANT_TEXT        = 0x01,
    CAP_SEQUENCE_SOUND_ID_MASK       = 0xFE,
    CAP_SEQUENCE_TIMING_MASK         = 0xFFFF0000,
    CAP_SEQUENCE_PAUSE_UNTIL_RESUMED = 0xFF,
    CAP_SEQUENCE_CHILD_ACTION_BASE   = 100
};

/// One keyed text, action or view-control record in a CAP playback sequence.
///
/// Records have a 12-byte stride. A script pointer addresses its command header
/// in slot zero; playback scans from slot one for the selected `key`, stopping
/// at `textRef.offset == CAP_TEXT_REF_END`. The terminal record has no text.
/// Nonterminal text references borrow storage from the loaded CAP file.
/// Each scan must reach its selected key or a terminal record within that file.
///
/// `control.text.flags` and `control.scene.flags` are the same flag byte.
/// `CAP_SEQUENCE_VIEW_CONTROL` selects the scene view; otherwise an `actionId`
/// selects an action whose fallback result can change the key through the action
/// view. Ordinary text uses the title and frame counts. The packed view tests
/// both timing bytes together while retaining the aligned word access.
typedef struct {
    union {
        struct {
            u8 title;         // Title glyph plus one (0 none); TITLE_BANK adds 256.
            u8 flags;         // CAP_SEQUENCE_* flags; bit 0 has no observed consumer.
            u8 displayFrames; // Frames displaying completed text before the pause (0 untimed if pauseFrames is also 0).
            u8 pauseFrames;   // Frames without text afterward (255 waits for external resume).
        } text;
        struct {
            u8 view;               // View selector (0 unchanged); lookup or direct index follows playback mode.
            u8 flags;              // Same CAP_SEQUENCE_* flag byte as the text view.
            u8 messageValue;       // Delayed message's low-byte argument when DELAYED_MESSAGE is set.
            u8 messageDelayFrames; // Frames before the delayed message is dispatched.
        } scene;
        struct {
            u8 fallbackKey; // Variant key when an action is declined/unavailable (0 keeps the current key).
        } action;
        u32 packed;         // First four bytes; TIMING_MASK selects the two timing/message bytes.
    } control;
    union {
        u8 soundAndTextFlags;    // Bit 0 reveals text instantly/hides the caption caret; bits 1-7 encode a room sound ID.
        u8 messageRecipient;     // Delayed message target (0 player, 1 companion, otherwise placed-actor index + 2).
    } trigger;
    u8         key;              // Variant key used when scanning a sequence.
    u8         actionId;         // Action (0 none, 1-100 two-bit flag/item action, 101-255 non-type-9 scene child ID + 100).
    u8         minDisplayFrames; // Minimum playback frames before advancing after confirmation; nonzero disables instant text.
    CapTextRef textRef;          // Serialized text offset, relocated text stream, or sequence terminator.
} CapSequenceRecord;
STATIC_ASSERT_SIZEOF(CapSequenceRecord, 0xC);

/// 0x10-byte header in front of a `CapSequenceRecord` array inside a `GpCapFile`
/// (`field_C`). `count` is the first halfword; the records start at
/// `records`. `Gp_RelocCapFile` relocates each record's `textRef.offset` unless
/// it is `-1`, in which case it also skips the next record.
typedef struct _GpCapEvtTable {
    /* 0x00 */ s16               count;
    /* 0x02 */ byte              pad_2[0xE];
    /* 0x10 */ CapSequenceRecord records[0];
} GpCapEvtTable;
STATIC_ASSERT_SIZEOF(GpCapEvtTable, 0x10);

/// The CAP file's command pointer table: a count, then that many entries. Each
/// entry is a file-relative offset until relocation adds the file base, making
/// it the address of a command record; a zero entry is left as no record.
typedef union {
    s32                offset;
    struct _GpCapCmd*  command;
    CapSequenceRecord* events;
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
/// `TextGlyphCell*` published as `Gp_CapGlyphs`, `field_C` is a
/// `GpCapEvtTable*`, and `field_10` is a `GpCapPtrTable*` whose
/// entries (nonzero) address command headers followed by `CapSequenceRecord` values.
typedef struct _GpCapFile {
    /* 0x00 */ char magic[4];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ union {
        s32            offset;
        TextGlyphCell* ptr;
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
static inline CapSequenceRecord* Gp_CapEventAt(CapSequenceRecord* events, s32 index)
{
    GpCapEntry entry;
    entry.events = events;
    entry.offset = index * sizeof(CapSequenceRecord) + entry.offset;
    return entry.events;
}

#endif // GAMEPLAY_CAP_H
