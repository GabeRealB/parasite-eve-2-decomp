/* Events of the Dryfield driveway (day and night packages). The room's resolve
 * message answers area 0x17 and 0x20 with the variant from progress nibbles.
 * Area 2 runs caption 6 once a nibble is set. At area 0x20 it starts the bat
 * encounter task on stage 2 variant 1 while nibble 0x50 is clear, otherwise
 * runs caption 1. At area 0x17 it either runs caption 2 or latches a staged
 * event (caption 9, flag 0x11C) for roomEventStagedTask. The cutscene tasks
 * either run the blackout stream with its skip continuation, or run a scripted
 * scene and clear the area flag when it ends. A script hook plays two stage
 * sounds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DRYFIELD_DRIVEWAY_H
#define SRC_SHARED_DRYFIELD_DRIVEWAY_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

// Bind ROOM_VARIANT_DRIVEWAY_SCENE_TASK_TABLE immediately before the resolver
// fragment and undefine it immediately afterwards. The day and night driveway
// carriers each supply their own private TaskDesc array with a live encounter
// recipe at index 1. Spawning reads that recipe synchronously; its callback code
// and borrowed resources must remain loaded while used. This header and the
// other fragments do not require the identifier binding.
// Bind DRIVEWAY_ENCOUNTER_SCRIPT immediately before the cutscene fragment and
// undefine it immediately afterwards. The day and night driveway carriers each
// supply their own private END-terminated EvsCommand array, borrowed as
// EvsCommand* by evsStartScript until the interpreter ends. The script restores
// player control; its installed animation data remains borrowed during player
// bank playback. This header and the other fragments do not require the binding.
// Bind DRIVEWAY_BLACKOUT_SCRIPT and DRIVEWAY_BLACKOUT_SKIP_SCRIPT immediately
// before the blackout fragment and undefine both immediately afterwards. The
// skip binding selects the carrier's END-terminated EvsCommand array, borrowed
// as EvsCommand* until the interpreter ends. This header and the other fragments
// do not require either blackout binding.

/// Playback choices shared by the paired driveway scene recipes.
enum {
    DRIVEWAY_CAP_BLACKOUT                         = 3,
    DRIVEWAY_CAP_ENCOUNTER                        = 4,
    DRIVEWAY_BLACKOUT_FADE_FRAMES                 = 30,
    DRIVEWAY_FADE_USE_DEFAULT_FRAMES              = 0, // Interpreter default: seven frames
    DRIVEWAY_SCRIPT_WAIT_NEXT_UPDATE              = 0, // Yield even with no countdown
    DRIVEWAY_DISPLAY_ENABLE                       = 1,
    DRIVEWAY_SAVED_VIEW_RELOAD                    = 1,
    DRIVEWAY_PLAYER_UPDATE_RUN                    = 0,
    ROOM_VARIANT_DRIVEWAY_FACTORY_EVENT_NONE      = 0,
    ROOM_VARIANT_DRIVEWAY_FACTORY_EVENT_REQUESTED = 1,
};

static s32  _roomVariantResolveDriveway(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _drivewayBlackoutTask(Task* task);
static void _drivewayCutsceneTask(Task* task);
static s32  _drivewayScriptSound(Task* task, s32 messageId, s32 cueKey, s32 unusedArg);

#endif /* SRC_SHARED_DRYFIELD_DRIVEWAY_H */
