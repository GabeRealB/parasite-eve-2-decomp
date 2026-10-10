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

/// Controller state indices in the cash register's eleven-handler dispatch table.
///
/// Cancel releases the cursor and presentation holds directly. Accepted entry
/// instead runs the saved-area, sound and CAP sequence before restoring play.
enum {
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_INITIALIZE           = 0,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_ARM_CURSOR           = 1,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_SCAN                 = 2,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_OPEN_PROMPT          = 3,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_ANSWER_PROMPT        = 4,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_CANCEL               = 5,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_ACCEPT_CODE          = 6,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_PLAY_ACCEPT_SOUND    = 7,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_START_COMPLETION_CAP = 8,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_EXIT_DELAY           = 9,
    DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_STATE_FINISH               = 10,
};

/// Fourteen cash-register key rectangles followed by a hotspot terminator.
///
/// Bounds use screen-center pixels with inclusive far edges; ids 0..13 select
/// the digit and special keys. Every key initially offers Examine. Hit testing
/// rewrites the hit bytes, and each interaction clears them before scanning.
/// Both room translation units borrow this writable overlay storage.
extern ActionPromptHotspot gDryfieldNightMotelLobbyCashRegisterHotspots[15];

/// Bodyless cash-register controller recipe with execution priority 192.
///
/// Its polling owner spawns index zero; the callback allocates its own work
/// and independent cursor child. The recipe is read only during spawning,
/// while the controller's callbacks require the lobby to remain loaded.
extern TaskDesc gDryfieldNightMotelLobbyCashRegisterTaskDesc;

/// Controller handle retained by the cash register's polling owner.
///
/// NULL before the first spawn. Polling assumes that spawning and controller
/// work initialization both succeeded; the owner handles neither failure.
/// The owner leaves the released handle here, so it is stale after teardown
/// and must not be polled again until the next spawn replaces it.
extern Task* gDryfieldNightMotelLobbyCashRegisterTask;

extern Task* gRoomCutsceneSoundTask;

/// Previous zero-or-one result of the persistent Bronco masterkey collection bit.
///
/// Room initialization seeds one to suppress an objective update for a key
/// collected before entry. Monitoring records objective 20 on a zero-to-one
/// change and replaces this signed word with the current result each tick.
extern s32 gDryfieldNightMotelLobbyPreviousMasterkeyCollected;

/// Borrowed playback record for the lobby's revisit telephone scene.
///
/// Uses view 5 and CAP slot 1, with CAP file 1 at VRAM word X 896 before
/// story chapter 4 or file 2 at X 960 thereafter; texture-page Y remains zero.
/// Area-bank sounds 3..6 accompany playback and restoration. The command
/// handler fills this record before spawning the runner and must keep it
/// unchanged and the lobby loaded until playback ends.
extern RoomCutsceneRec gDryfieldNightMotelLobbyRevisitCutscene;

/// Updates and draws the cash register's seven-slot digit display.
///
/// Borrows live `DryfieldNightMotelLobbyCashRegisterWork` from `task->work`.
/// Closed entry resets every slot to the empty marker before drawing. A clear
/// skips drawing for this frame and sets the newest slot to zero. Slots hold
/// 0..9 or the empty marker 10, with the newest at index 0 on the right.
/// Requires this overlay and its digit textures, a current ordering table, and
/// writable GPU primitive storage for seven `POLY_FT4` packets when drawing.
void dryfieldNightMotelLobbyDrawCashRegisterDisplay(Task* task);

/// Applies one key press to the open cash-register entry.
///
/// Borrows live cash-register work from `task->work`; the caller must already
/// have opened entry with the hash key. `keyId` is the hotspot id: 0..9 enter
/// digits, 10 enters up to two zeros, 11/12 clear, and 13 submits the code.
/// Other ids have no effect. New digits shift older slots toward index 6.
/// The count stays in 0..7. A full display refuses any digit; a lone leading
/// zero refuses another zero. Both still play the key sound.
/// Clears reset the count and all seven slots and latch `entryCleared`, so the
/// display drawer emits no packets and leaves a lone zero for the next frame.
/// Submission latches `codeAccepted` for exactly 3033, or plays the error sound.
void dryfieldNightMotelLobbyPressCashRegisterKey(Task* task, s16 keyId);

/// Scans the cash-register hotspots and handles one frame of confirm/cancel input.
///
/// Borrows live cash-register work from `task->work` and port 0's cursor.
/// A busy CAP hides and stops the cursor but still updates the digit display.
/// Confirmation takes the first hit: before examination it selects prompt state
/// 3; afterwards hash opens entry and keys edit it. Accepted code selects saved
/// view 7 and completion state 6; cancel selects state 5. Clears last one draw.
/// Every path draws the display exactly once before returning, including the
/// two early exits; keep those calls separate to preserve the compiled tail.
void dryfieldNightMotelLobbyCashRegisterScanTask(Task* task);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_DRYFIELD_NIGHT_MOTEL_LOBBY_PRIVATE_H
