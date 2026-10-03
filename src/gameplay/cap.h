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

/// Message delivering a `CapActionRequest` to a scene child that is not a
/// placed actor.
///
/// The first argument addresses the request and the second is zero. The
/// receiver keeps the address past the dispatch and completes the request
/// later, so the result of the dispatch carries nothing. Placed actors bind
/// the same number to `ACTOR_COMMAND_MESSAGE_APPLY`, whose payload is a
/// different record; only children outside type 9 receive this one.
enum { CAP_ACTION_MESSAGE_REQUEST = 0x7DB };

/// The action a CAP sequence record asks for, and its outcome.
///
/// Playback copies a record's `actionId` here, hands the address to whatever
/// carries the action out, and holds the sequence until `done` is set. Ids up
/// to `CAP_SEQUENCE_CHILD_ACTION_BASE` name a placed object by its flag index:
/// a prompt task looks the object up and opens its pickup, save or item-box
/// panel. Larger ids name a scene child outside type 9, which receives
/// `CAP_ACTION_MESSAGE_REQUEST`. A request whose outcome is not `accepted`
/// switches the sequence to the record's fallback key.
///
/// Receivers borrow the request across frames - a scene child keeps the
/// address until its own animation ends - so it must stay live until `done`.
/// Playback owns the only instance and runs one action at a time.
typedef struct {
    u16 actionId;      // Placed object's flag index, or scene child ID + CAP_SEQUENCE_CHILD_ACTION_BASE.
    s8  done;          // (0 pending, 1 finished); set by whoever carries the action out.
    s8  accepted;      // (0 declined or object not found, 1 confirmed or carried out); meaningful once done.
    u8  defaultPrompt; // Prompt panel form (1 default, 0 selected by the object's AREA_OBJECT_PLACE_PROMPT bit); written and read by the prompt task alone.
} CapActionRequest;
STATIC_ASSERT_SIZEOF(CapActionRequest, 6);

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
