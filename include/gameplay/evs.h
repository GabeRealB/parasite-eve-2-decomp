#ifndef GAMEPLAY_EVS_H
#define GAMEPLAY_EVS_H

#include "common.h"

#include "gameplay/cap.h"
#include "gameplay/message.h"

struct EvsCommand;
struct PadScriptCmd;
struct PadScriptVibrationSegment;

/// Key an event script uses to select a scene/audio stream.
///
/// Opcode 12 installs the pointer for caption playback. `group` and `streamId`
/// match the stream key, and `subId` matches the stream's first qualifier. The
/// second qualifier is absent from this record; playback matches it as zero.
/// Group zero selects the stage-zero stream table, and any other group is
/// matched in the current folder's table. Debug output prints the three
/// numbers as `evs<group>_<streamId>_<subId>.txt`.
typedef struct {
    u16 group;    // Scene/audio selection group (0 selects the stage-zero table)
    u16 streamId; // Stream ID matched in that table
    u16 subId;    // First exact stream qualifier
} EvsSceneKey;
STATIC_ASSERT_SIZEOF(EvsSceneKey, 6);

/// Event-script opcodes and their positional operands.
///
/// Numbers in the comments identify `operand0` through `operand4`. Only the
/// operands read by the selected opcode need meaningful initialization. Width
/// conversions below describe truncation at dispatch, not narrower storage.
enum {
    /// Terminates the entire event script; operands 0-4 are ignored.
    ///
    /// Requests framebuffer-blend task exit, clears the selected scene key and
    /// event state, releases the menu hold, and restores CAP view-id
    /// mapping. Releases event HUD control if still held. If any scene was
    /// selected during the event, also ends its stream and restores the saved
    /// random states. The script task enters its kill state for the next update
    /// without advancing the command pointer.
    /// Unlike `EVENT_SCRIPT_OPCODE_RETURN`, this does not pop a script call.
    EVENT_SCRIPT_OPCODE_END = -1,
    /// Sends a synchronous task message to a registered task or a scene child.
    ///
    /// Operand 0 is a resident task-slot index in 0..15, or
    /// `EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD`. `GAME_TASK_SLOT_SCENE`
    /// selects a type-9 actor by operand 1's placement index (0..15) in the
    /// current stage/area;
    /// `EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER` instead selects the manager.
    /// The other-child target selects a child outside type 9 by operand 1's
    /// byte ID (0..255). Other slots ignore operand 1. Child lookups require a
    /// live scene manager; every selected non-NULL task must remain live.
    ///
    /// Operand 2 is the message ID; operands 3/4 are complete argument words,
    /// interpreted by the recipient as integers or borrowed addresses through
    /// `TaskMessageArg`. Payload storage must satisfy that message's type,
    /// extent and lifetime. Missing targets are skipped, handler return values
    /// are discarded, and execution advances without yielding.
    EVENT_SCRIPT_OPCODE_SEND_MESSAGE = 1,
    EVENT_SCRIPT_OPCODE_START_FLASH  = 2, // 0 hold frames, 1 blend-mode selector; replaces the primary effect-task pointer.
    EVENT_SCRIPT_OPCODE_SET_VIEW     = 3, // 0 saved view id, truncated to u8.
    /// Yields the script and delays the following command by a frame countdown.
    ///
    /// `operand0.value` is a nonnegative s32 count of interpreter updates.
    /// Dispatch advances once and yields; the next N unfrozen updates only
    /// decrement the count, so zero resumes on the next update, not immediately.
    /// A script freeze suspends the countdown; an accepted skip clears it.
    /// The CAP pause gate is checked after the countdown. Operands 1-4 are ignored.
    EVENT_SCRIPT_OPCODE_WAIT_FRAMES             = 4,
    EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE     = 5,  // Kill and clear the primary flash/fade task, if present.
    EVENT_SCRIPT_OPCODE_RESTORE_HUD             = 6,  // Show the HUD immediately, or remove its demo replacement task.
    EVENT_SCRIPT_OPCODE_SET_EVENT_STATE         = 7,  // 0 event-state value, truncated to u8.
    EVENT_SCRIPT_OPCODE_WAIT_ANIMATION          = 8,  // 0 actor slot; retry until its animation-busy message returns zero.
    EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE            = 9,  // Advance once and yield until CAP clears the script-pause bit.
    EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION   = 10, // 0 actor slot (3 player, otherwise companion bank), 2 message id, 3 animation request, 4 payload.
    EVENT_SCRIPT_OPCODE_RESTORE_VIEW            = 11, // Restore the previously saved view.
    EVENT_SCRIPT_OPCODE_SELECT_SCENE            = 12, // 0 scene key, or NULL; retain the key and request its scene/audio stream when non-NULL.
    EVENT_SCRIPT_OPCODE_CALLBACK                = 13, // 0 callback address, 1 raw argument word; discard any return value.
    EVENT_SCRIPT_OPCODE_START_VIBRATION         = 14, // 0 pad command array, 1 vibration segment array; both borrowed by the spawned task.
    EVENT_SCRIPT_OPCODE_START_SOUND             = 15, // 0 sound-script id, 1 pan offset, 2 attenuation; both mix values truncated to s8.
    EVENT_SCRIPT_OPCODE_STOP_SOUND              = 16, // 0 sound selector, 1 u16 stop control (0 override release, 1 keep release, otherwise fade ticks).
    EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND   = 17, // 0 nonzero starts the framebuffer-blend task; zero releases the current one.
    EVENT_SCRIPT_OPCODE_START_AREA_MUSIC        = 18, // 0 fade ticks, narrowed to s16 then u16; start area-table music at most once per event.
    EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC         = 19, // 0 fade control, narrowed to s16; stop area-table music with u16(control + 1).
    EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC     = 20, // 0 u8 scene event, 1 u16 fade-out ticks, 2 u16 stored but never read; once per event.
    EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD         = 21, // Retry until the stage music load state is nonzero.
    EVENT_SCRIPT_OPCODE_SHAKE_SCREEN            = 22, // 0 amplitude, 1 counter bound; spawn with (amplitude << 8) | bound.
    EVENT_SCRIPT_OPCODE_CLEANUP_SCENE           = 23, // Release CAP/HUD control, reset player scripting, and cancel the secondary fade.
    EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE      = 24, // 0 u8 blend mode, 1 u16 ramp frames (0 selects 7); only if no primary effect task.
    EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE     = 25, // Request the primary fade's return phase.
    EVENT_SCRIPT_OPCODE_FADE_VOLUME             = 26, // 0 u16 target volume-table index, 1 u16 duration in frames (0 applies immediately).
    EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION  = 27, // 0 sound-script id, 1 u16 target (sound updates use its low s8), 2 u16 duration in frames.
    EVENT_SCRIPT_OPCODE_REBUILD_TMD_BUFFERS     = 28, // Reset GPU/ordering-table state and allocate missing TMD buffers.
    EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION       = 29, // 0 actor slot; retry until its scripted-action-busy message returns zero.
    EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO        = 30, // Queue playback of the selected scene audio, or mark an audio-free scene playing.
    EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO       = 31, // Replace queued work with a start request for the selected scene audio, if present.
    EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB         = 32, // 0/1/2 RGB components; multiply each by 16 and truncate to s16 matrix units.
    EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB       = 33, // Release the ambient RGB override.
    EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM     = 34, // Mark the scene stream ended and restore its saved random states.
    EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE    = 35, // 0 u8 blend mode, 1 u16 ramp frames (0 selects 7), 2 fade-task spawn argument.
    EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE   = 36, // Request return; a nonzero operand 0 replaces the u16 ramp duration.
    EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE   = 37, // Kill an unfinished secondary fade and clear its task pointer.
    EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS         = 38, // 0 actor selection (0 player, 1 companion, 2 both); restore suppressed weapon effects.
    EVENT_SCRIPT_OPCODE_HIDE_WEAPONS            = 39, // 0 actor selection (0 player, 1 companion, 2 both); suppress weapon effects.
    EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE         = 40, // 0 uniform light multiplier in 1/256 units, times 16 to Q12; zero disables the override.
    EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS = 41, // 0 u8 flag (0 map CAP view ids, nonzero use the ids directly).
    EVENT_SCRIPT_OPCODE_START_STAGE_SOUND       = 42, // Start a sound script, substituting the current stage into stage-relative sound ids.
    EVENT_SCRIPT_OPCODE_JUMP                    = 43, // 0 command target; transfer without pushing a return address.
    EVENT_SCRIPT_OPCODE_CALL_SCRIPT             = 44, // 0 command target; push the following instruction on the eight-entry return stack.
    EVENT_SCRIPT_OPCODE_RETURN                  = 45, // Pop a return address; requires a matching script call.
    EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET         = 46, // 0 command target for input-driven skip (NULL disables skipping).
    EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND     = 47, // 0 u8 flag (0 stop selected sound scripts on skip, nonzero keep them).
    EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW          = 48, // 0 saved view id, truncated to u8; also mark the view dirty.
    EVENT_SCRIPT_OPCODE_SAVE_VIEW               = 49, // Save the current view for later restoration.
};

/// Special recipient selectors for `EVENT_SCRIPT_OPCODE_SEND_MESSAGE`.
enum {
    EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD = -1, // Operand 0: resolve a non-type-9 child by operand 1's byte ID
    EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER     = -1, // Operand 1 with GAME_TASK_SLOT_SCENE: address the manager itself
};

/// One four-byte event-script operand, interpreted by its instruction's opcode.
///
/// Integer values, object pointers and callback addresses share the PS1 word
/// representation. Message payloads and callback arguments can transport address
/// bits through `value`; this does not copy or take ownership of their storage.
/// Keep borrowed payloads live until their instruction executes, and longer when
/// the receiving task retains them. Command targets must remain live while the
/// script can reach them; vibration tables remain live for their spawned task.
/// The selected scene key is retained until it is replaced or the script ends.
///
/// The callback views preserve source signatures in initializers. The callback
/// opcode invokes the address through `callback`, passing the complete second
/// operand word in the PS1 argument register. Callees may ignore it or consume
/// only its low byte/halfword; any return value is discarded. Keep callback code
/// loaded through invocation, and `captionText` loaded until CAP clears it.
typedef union {
    s32                               value;                                                  // Signed integer or transported address bits
    void*                             storage;                                                // Borrowed object payload or callback argument
    struct EvsCommand*                commands;                                               // Jump/call target or optional skip target (NULL disables skip)
    const AnimationPlayRequest*       animation;                                              // Borrowed request copied before resolving its weapon bank
    const EvsSceneKey*                sceneKey;                                               // Borrowed scene/audio key (NULL makes no stream request)
    struct PadScriptCmd*              padCommands;                                            // Borrowed two-lane vibration script
    struct PadScriptVibrationSegment* vibrationSegments;                                      // Borrowed vibration segments indexed by that script
    TaskMessageArg                    message;                                                // Receiver-selected integer/address message payload
    void                              (*callback)(s32 argumentWord);                          // Dispatch view: one complete argument word
    void                              (*callbackPointer)(void* argument);                     // Source signature consumes a borrowed object pointer
    void                              (*callbackNoArg)(void);                                 // Source signature ignores the argument register
    void                              (*callbackS8)(s8 value);                                // Source signature consumes a signed byte
    void                              (*callbackU8)(u8 value);                                // Source signature consumes an unsigned byte
    void                              (*callbackS16)(s16 value);                              // Source signature consumes a signed halfword
    void                              (*callbackU16)(u16 value);                              // Source signature consumes an unsigned halfword
    void                              (*callbackU32)(u32 argumentWord);                       // Source signature consumes an unsigned word
    s32                               (*callbackResult)(s32 argumentWord);                    // Source signature returns a word that dispatch discards
    void                              (*callbackSetText)(CapTextUpdateCallback textCallback); // Source signature installs a CAP text callback
    CapTextUpdateCallback             captionText;                                            // Callback argument retained by CAP text playback
} EvsOperand;
STATIC_ASSERT_SIZEOF(EvsOperand, 4);

/// One six-word instruction in an overlay's event script.
///
/// The signed opcode selects the meaning and required union view of each
/// positional operand. Ordinary instructions execute consecutively in the same
/// task update; waits and CAP cues can yield to later frames. A linear path must
/// reach an end or a valid control transfer before leaving live command storage.
/// Calls and returns must be balanced, with no more than eight pending returns.
/// Targets name instructions, not byte offsets. Unhandled opcodes are skipped.
typedef struct EvsCommand {
    s32        opcode;   // EVENT_SCRIPT_OPCODE_* selector; -1 ends the script
    EvsOperand operand0; // First operand: target, callback, payload pointer or value
    EvsOperand operand1; // Second operand: selector, callback argument, table or duration
    EvsOperand operand2; // Third operand: message id, sound/light value or fade argument
    EvsOperand operand3; // Fourth operand: message payload or borrowed animation request
    EvsOperand operand4; // Fifth operand: second message payload
} EvsCommand;
STATIC_ASSERT_SIZEOF(EvsCommand, 0x18);

#endif // GAMEPLAY_EVS_H
