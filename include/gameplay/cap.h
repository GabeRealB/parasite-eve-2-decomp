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

/// 0x10-byte header in front of a `CapSequenceRecord` array inside a `CapFile`
/// (`sequences`). `count` is the first halfword; the records start at
/// `records`. `Gp_RelocCapFile` relocates each record's `textRef.offset` unless
/// it is `-1`, in which case it also skips the next record.
typedef struct _GpCapEvtTable {
    /* 0x00 */ s16               count;
    /* 0x02 */ byte              pad_2[0xE];
    /* 0x10 */ CapSequenceRecord records[0];
} GpCapEvtTable;
STATIC_ASSERT_SIZEOF(GpCapEvtTable, 0x10);

/// How a command chooses the variant key played from its sequence.
enum {
    CAP_COMMAND_PLAIN   = 0, // Play variant 0.
    CAP_COMMAND_COUNTER = 1, // Play the counter, then advance it.
    CAP_COMMAND_FLAG    = 2, // Play the game-flag nibble named by this command.
    CAP_COMMAND_ROOM    = 3, // Give the command index to the room; do not start playback.
    CAP_COMMAND_TALLY   = 4  // Play how many of a run of two-bit flags are 0, 1 or 3.
};

/// Modifiers in `CapCommand.flags`.
///
/// The counter opcode reads all three. The tally opcode reads only
/// `CAP_COMMAND_BRANCH`. Other opcodes leave the byte unused.
enum {
    CAP_COMMAND_WRAP    = 0x01, // Store 0 when the counter has reached the limit and BRANCH is clear.
    CAP_COMMAND_PERSIST = 0x02, // Keep the counter in the game-flag nibble instead of `counter`.
    CAP_COMMAND_BRANCH  = 0x04  // Continue at `nextIndex` instead of playing: counter above the limit, or a zero tally.
};

/// Selects the variant key for one CAP sequence, and occupies its first slot.
///
/// The command pointer table addresses this record. Playback keeps that address
/// and starts at the next slot, so command selection and text playback share a
/// base without sharing a meaning. The game-flag nibble index is
/// `flagIndexLo | (flagIndexHi << 8)`. A counter plays that value and then
/// advances it: `CAP_COMMAND_PERSIST` uses the game-flag nibble, and otherwise
/// the value is `counter` in the loaded file. `CAP_COMMAND_BRANCH` continues
/// at `nextIndex` without playing when the counter is above `counterLimit`, or
/// when a tally is 0. With BRANCH clear, `CAP_COMMAND_WRAP` stores 0 at the
/// limit and any other counter stays there.
typedef struct {
    u8 opcode;       // CAP_COMMAND_* selector.
    u8 flags;        // CAP_COMMAND_WRAP, CAP_COMMAND_PERSIST, CAP_COMMAND_BRANCH.
    u8 counterLimit; // Highest counter value that still plays.
    u8 flagIndexLo;  // Low byte of the game-flag nibble index.
    u8 counter;      // Live file counter when CAP_COMMAND_PERSIST is clear. Retail commands store 0.
    u8 bitFlagIndex; // First current-stage two-bit flag read by CAP_COMMAND_TALLY.
    u8 bitFlagCount; // How many consecutive two-bit flags CAP_COMMAND_TALLY reads.
    u8 flagIndexHi;  // High byte of the game-flag nibble index.
    u8 nextIndex;    // Command-table index taken when CAP_COMMAND_BRANCH skips playback.
    u8 slotTail[3];  // Unread. Zero in every retail command; the next sequence slot starts after them.
} CapCommand;
STATIC_ASSERT_SIZEOF(CapCommand, 0xC);
STATIC_ASSERT(sizeof(CapCommand) == sizeof(CapSequenceRecord), cap_command_sequence_slot);

/// One command-table word: a file offset until relocation, then the sequence base.
///
/// Nonzero words are byte offsets from the CAP file base until in-place relocation
/// adds that base. Zero stays null and names no sequence. The relocated address is
/// the command in slot zero, and playback indexes that same address as sequence
/// records starting at slot one. The command and those records share the 12-byte
/// stride. The loaded CAP file owns the storage.
typedef union {
    s32                offset;   // File-relative byte offset, or the address as an integer (0 names no sequence).
    CapCommand*        command;  // Relocated command in slot zero.
    CapSequenceRecord* sequence; // Same address indexed as sequence records; playback starts at slot one.
} CapCommandRef;
STATIC_ASSERT_SIZEOF(CapCommandRef, 4);

typedef struct _GpCapPtrTable {
    /* 0x0 */ s32           count;
    /* 0x4 */ CapCommandRef entries[0];
} GpCapPtrTable;
STATIC_ASSERT_SIZEOF(GpCapPtrTable, 4);

/// Loaded CAP dialogue file.
///
/// Retail payloads begin with the four bytes "CAP2". Loaders compare only the
/// first three, so any magic starting with "CAP" is accepted. `glyphs`,
/// `sequences` and `commands` are file-relative byte offsets until relocation
/// adds the file base; afterwards they borrow tables stored in this file.
/// The file must remain loaded while those pointers are used. Relocation of
/// all three runs only while `glyphs.offset` is still positive: a relocated
/// KSEG0 address is negative in that signed word. Retail payloads place the
/// glyph table at the first byte after this header.
typedef struct {
    char magic[4];             // On-disc "CAP2". Loaders compare the first three bytes with "CAP".
    s32  field_4;              // Constant 8 in every retail payload. No loader reads it; role unproven.
    union {
        s32            offset; // File-relative byte offset of the glyph cells.
        TextGlyphCell* cells;  // Relocated cells borrowed for drawing and measurement.
    } glyphs;
    union {
        s32            offset; // File-relative byte offset of the sequence-record table.
        GpCapEvtTable* table;  // Relocated sequence records. The count precedes the array.
    } sequences;
    union {
        s32            offset; // File-relative byte offset of the command-reference index.
        GpCapPtrTable* table;  // Relocated command references. A zero entry names no sequence.
    } commands;
} CapFile;
STATIC_ASSERT_SIZEOF(CapFile, 0x14);

/// CAP relocation runs in the PS1's 32-bit address space. Keep the base's
/// numeric address explicit while adding it to serialized offset words.
typedef union {
    CapFile* file;
    u32      address;
} GpCapFileAddress __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(GpCapFileAddress, 4);

/// Resolve a record's byte displacement within a relocated CAP script.
/// CAP references share their integer address and pointer representations;
/// this preserves that representation through the indexed address addition.
static inline CapSequenceRecord* Gp_CapEventAt(CapSequenceRecord* events, s32 index)
{
    CapCommandRef address;
    address.sequence = events;
    address.offset   = index * sizeof(CapSequenceRecord) + address.offset;
    return address.sequence;
}

#endif // GAMEPLAY_CAP_H
