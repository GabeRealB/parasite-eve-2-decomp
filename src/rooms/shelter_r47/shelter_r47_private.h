#ifndef SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H
#define SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H

#include "common.h"

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

#include "rooms/room.h"

extern Task* gRoomCutsceneSoundTask;

/// Hotspot tables of the second cap script; `spawnArg1` 2 selects the second.
extern ActionPromptHotspot D_shelter_r47_8018739C[];
extern ActionPromptHotspot D_shelter_r47_801873D8[];
/// Area views of the map pages the second cap script steps through, indexed by
/// `ShelterR47State2::field_1C`.
extern u8 D_shelter_r47_801873FC[];

/// Work block of the room's control console: the screen on which the player
/// selects one of five rows and flips its switch, each switch a game flag.
///
/// The task family's state table is `D_shelter_r47_8017D6C8`; the block is
/// allocated zeroed by its state 0 and stored at `Task::work`. Positions are
/// screen pixels relative to the display centre, and each one eases a quarter
/// of the way toward its target every frame.
typedef struct {
    s16 rowX[5];        // x of each row's switch sprite: 0x78 in the list, fanned out except for the selected row while the view changes
    u8  pad_A[2];
    s16 rowY[5];        // y of each row's switch sprite
    u8  pad_16[2];
    s16 toggles[5];     // working copies of the five console switches (0 off, 1 on), written back to their game flags on exit
    u8  pad_22[2];
    s16 labelX;         // x of the selected row's name sprite
    s16 labelY;         // y of the selected row's name sprite
    s16 headerX;        // x of the top-left header sprite, which slides in from the left
    s16 headerY;        // y of the top-left header sprite
    s16 buttonX;        // x of the switch button that flips the selected row
    s16 buttonY;        // y of the switch button; it slides off the bottom while the view changes
    s16 messageX;       // x of the status message sprite
    s16 messageY;       // y of the status message and its reveal cursor; slides off the bottom with the button
    s16 selection;      // `ActionPromptHotspot::id` of the confirmed hotspot: high byte the action kind (0 select row, 1 flip switch, 2..4 cap events), low byte its row
    u16 fade;           // fade-to-black level on exit: +0x10 a frame, clamped at 0xFF
    u8  pad_38[2];
    s16 wipeRed;        // red of the subtractive circular wipe shown on opening and while the view changes (0 clear .. 0xFF black)
    s16 wipeGreen;      // green of that wipe
    s16 wipeBlue;       // blue of that wipe
    s16 wipeGrey;       // grey level of the wipe's other vertices; reaching 0 or 0xFF ends the wipe
    s16 buttonFlash;    // frames left showing the pressed switch button; input resumes once it is 0
    s16 status;         // status message shown: 2 * row + that row's switch, indexing the message's reveal stops and cap events
    s16 backdropScroll; // horizontal scroll of the backdrop drawn in view 0x14, 0..0x140
    s16 revealPos;      // index into the status message's reveal stops; the message is revealed up to that stop
    s8  promptKind;     // `ActionPromptHotspot::promptKind` of the confirmed hotspot
    u8  pad_4B[3];
    u8  savedView;      // room view on entry, restored when the player dismisses the console
    s8  row;            // selected row, 0..4
    s8  previousRow;    // row selected before `row`, whose status stays shown while the view changes
    s8  guideStep;      // guided sequence (0 free use; 1..3 row the player must pick next; 4 done, so the console closes)
    s8  backdropToggle; // mirror of switch 3: bit 0 scrolls the backdrop toward 0, clear toward 0x140
    u8  pad_53;
} ShelterR47ConsoleWork;
STATIC_ASSERT_SIZEOF(ShelterR47ConsoleWork, 0x54);

/// Work block of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `memCalloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::work`.
typedef struct {
    u8                   pad_0[4];
    ActionPromptHotspot* hotspots; ///< table hit-tested against the action cursor
    u8                   pad_8[2];
    s16                  field_A;  ///< width of the first quad drawn by `func_shelter_r47_80183B84`; eases toward `field_E`
    s16                  field_C;  ///< height of that quad; eases toward `field_10`
    s16                  field_E;
    s16                  field_10;
    s16                  field_12; ///< width of the second quad drawn by `func_shelter_r47_80183B84`; eases toward `field_16`
    s16                  field_14; ///< height of that quad; eases toward `field_18`
    s16                  field_16;
    s16                  field_18;
    s16                  field_1A;   ///< id of the confirmed hotspot
    s16                  field_1C;   ///< index of the map page shown, wrapping over 0..4
    s16                  field_1E;   ///< target that `field_20` eases toward by a quarter of the gap a frame
    s16                  field_20;   ///< x of the sprite drawn by `func_shelter_r47_80183FF4`
    u16                  fade;       ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    s16                  field_24;   ///< frame counter; past 300 the task moves to state 9
    u16                  field_26;   ///< frames the first quad has been fully open; zeroed while it grows
    s8                   promptKind; ///< `promptKind` of the confirmed hotspot, forwarded to `func_800D4E78`
    u8                   field_29;   ///< low byte of the area view saved on entry
    s8                   field_2A;
    s8                   field_2B;   ///< non-zero holds the prompt off
    s8                   field_2C;   ///< countdown; a sound plays as it reaches zero
    u8                   pad_2D[3];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

/// Task spawned by the room's cap script; polled and cleared by
/// `func_shelter_r47_80180714`.
extern Task* D_shelter_r47_8018A690;

extern u8 D_shelter_r47_8018A694;

extern u8 D_shelter_r47_8018A695;

/// Latches set the first time the second cap script's hotspots 2 and 3 play
/// their one-off cap events.
extern u8 D_shelter_r47_8018A696;

extern u8 D_shelter_r47_8018A697;

extern RoomCutsceneRec D_shelter_r47_8018A698;

extern u8 D_shelter_r47_80186FAC[5];

extern u8* D_shelter_r47_80187374[10];

s32 func_shelter_r47_8018097C(Task* task);

s32 func_shelter_r47_80180C48(Task* task);

void func_shelter_r47_80180F38(s16 x, s16 y, s16 id);

void func_shelter_r47_8018138C(Task* task);

void func_shelter_r47_80181568(Task* task);

void func_shelter_r47_80181914(Task* task, s16 arg1);

void func_shelter_r47_80182AA0(Task* task);

s32 func_shelter_r47_80182B9C(Task* task, ActionPromptHotspot* table, s16 x, s16 y);

void func_shelter_r47_80183210(void);

void func_shelter_r47_80183B84(Task* task);

void func_shelter_r47_80183E24(void);

void func_shelter_r47_80183F0C(void);

void func_shelter_r47_80183FF4(Task* task, s16 arg1);

void func_shelter_r47_80184124(Task* task, s16 arg1);

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_r47_80183234(Task*);

void func_shelter_r47_80182B18(Task*);

#endif // SRC_ROOMS_SHELTER_R47_SHELTER_R47_PRIVATE_H
