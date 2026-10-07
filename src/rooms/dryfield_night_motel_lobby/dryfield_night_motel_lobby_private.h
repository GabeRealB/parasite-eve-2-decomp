#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_DRYFIELD_NIGHT_MOTEL_LOBBY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_DRYFIELD_NIGHT_MOTEL_LOBBY_PRIVATE_H

#include "common.h"

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

#include "rooms/room.h"

/// Keys of the lobby's cash register, as the ids of their hotspots. The digit
/// keys 0 to 9 carry their own value as the id.
///
/// The hash key opens code entry and, once it is open, clears the entry like
/// the clear key does. The clear key is named for that effect; what its face
/// reads is not established.
enum {
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_DOUBLE_ZERO = 10, // Enters two zeros
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_HASH        = 11, // Opens code entry, then clears the entry
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_CLEAR       = 12, // Clears the entry
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_TOTAL       = 13, // Submits the entry as the code
};

/// Work block of the lobby's cash-register task, held in `Task::work`.
///
/// The register is a close-up view with one hotspot per key. Until it has been
/// examined, confirming any key opens the examine prompt, and accepting that
/// prompt plays the register's caption. From then on a confirm is a key press:
/// the hash key has to open code entry before any other key registers, the
/// digit keys fill a seven-digit display, and the total key ends the task's
/// keypad phase when the display holds the code. The task allocates the block
/// zeroed when it starts.
typedef struct {
    s16 hotspotId;    // Key the examine prompt was opened on; stored on that confirm and not read back
    s16 digitCount;   // Digits entered since the entry was last cleared (0 to 7)
    s8  promptKind;   // First row of the prompt opened for that key (0 "Examine", 1 "Push")
    s8  examined;     // 1 once the examine prompt was accepted, so confirms press keys instead of opening it
    s8  entryOpen;    // 1 once the hash key opened code entry; while 0 the display is blanked every frame
    s8  entryCleared; // 1 on the frame a key cleared the entry: the display is reset to a lone zero instead of drawn
    s8  codeAccepted; // 1 once the total key was pressed with the code on the display
} DryfieldNightMotelLobbyCashRegisterWork;
STATIC_ASSERT_SIZEOF(DryfieldNightMotelLobbyCashRegisterWork, 0xA);

/// The lobby's hotspot table: fifteen `ActionPromptHotspot` entries, the last of them
/// (index 14) the `ACTION_PROMPT_HOTSPOT_END` terminator the scans stop on. The room's init
/// clears every entry's `hit` flag on the way in.
extern ActionPromptHotspot D_dryfield_night_motel_lobby_80182820[];

extern TaskDesc D_dryfield_night_motel_lobby_801828D4;

extern Task* D_dryfield_night_motel_lobby_801844CC;

extern Task* gRoomCutsceneSoundTask;

extern s32 D_dryfield_night_motel_lobby_801844D4;

extern RoomCutsceneRec D_dryfield_night_motel_lobby_801844E0;

/// Updates and draws the cash register's seven-slot digit display.
///
/// Borrows live `DryfieldNightMotelLobbyCashRegisterWork` from `task->work`.
/// Closed entry resets every slot to the empty marker before drawing. A clear
/// skips drawing for this frame and sets the newest slot to zero. Slots hold
/// 0..9 or the empty marker 10, with the newest at index 0 on the right.
/// Requires this overlay and its digit textures, a current ordering table, and
/// writable GPU primitive storage for seven `POLY_FT4` packets when drawing.
void dryfieldNightMotelLobbyDrawCashRegisterDisplay(Task* task);

/// Applies one keypad press, `key` being the id of the hotspot pressed. Keys
/// 0-9 shift that digit in at index 0, the double-zero key shifts in two
/// zeros, the hash and clear keys clear the entry and the total key submits
/// it, raising `codeAccepted` when the code checks out and playing the reject
/// sound otherwise. A digit is refused once seven are entered, and a zero is
/// refused while the entry is a lone zero.
void func_dryfield_night_motel_lobby_80180440(Task* task, s16 key);

/// Hit-tests (`x`, `y`) against every entry of the hotspot `table`, setting
/// each entry's `hit` flag, and returns whether any entry was hit.

void func_dryfield_night_motel_lobby_8017FE90(Task* task);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_DRYFIELD_NIGHT_MOTEL_LOBBY_PRIVATE_H
