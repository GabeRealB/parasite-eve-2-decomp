#ifndef GAMEPLAY_ACTOR_PRESENTATION_H
#define GAMEPLAY_ACTOR_PRESENTATION_H

#include "types.h"

/// Scripted-control requests for the current player or companion.
enum {
    GAME_ACTOR_SCRIPTED_CONTROL_HOLD   = 0,
    GAME_ACTOR_SCRIPTED_CONTROL_RESUME = 1
};

/// Sends a draw/buffer mode to the current player and its attachments.
///
/// Uses the `PLAYER_ACTOR_MODEL_DRAW_*` modes of `playerActorSetModelDraw`:
/// 0 hides and allocates, 1 shows with automatic buffering, 2 hides and
/// releases, 3 hides and keeps buffers, and 4 shows and allocates. Other
/// values retain the model's flags but still propagate them to attachments.
/// The player task, model and present attachments must be live. Dispatch is
/// synchronous and its result is discarded; buffer lifetime follows the handler.
void playerActorSetDrawMode(s32 drawMode);

/// Sends a draw/buffer mode to the current companion, if one is present.
///
/// Uses the same `PLAYER_ACTOR_MODEL_DRAW_*` modes and live-model requirements
/// as `playerActorSetDrawMode`. A missing companion does nothing.
void companionSetDrawMode(s32 drawMode);

/// Holds the player in its equipped-bank idle animation or resumes ordinary control.
///
/// `GAME_ACTOR_SCRIPTED_CONTROL_HOLD` plays animation 1 with an eight-frame
/// blend and disables world collision, taking scripted control. Any nonzero
/// `resume` sends the end-scripted message with resume mode zero; the receiver
/// ignores that message when it is not under scripted control. No combat-pause
/// or model-draw state is changed here. The player task and model must be live;
/// holding also requires a loaded bank selected by the current character and
/// equipped weapon. The stack request is consumed synchronously, while its
/// animation resources stay borrowed through playback.
void playerActorSetScriptedControl(s32 resume);

/// Holds the companion in its selected-bank idle animation or resumes ordinary control.
///
/// A missing companion does nothing. Zero plays animation 1 with an eight-frame
/// blend and disables world collision, taking scripted control. Any nonzero
/// `resume` sends the end-scripted message: the companion folds root motion into
/// its placement, reinstalls its bank and enters idle, even outside scripted mode.
/// Both paths require the live companion's valid family/variant, model and loaded
/// animation bank. Dispatch consumes the stack request synchronously and playback
/// borrows its animation resources.
void companionSetScriptedControl(s32 resume);

/// Writes the current character and equipped weapon's animation-bank table index.
///
/// `bankIndexOut` must address one writable s32, commonly an
/// `AnimationPlayRequest.source.index`. The incoming value is overwritten, never inspected.
/// The live save's `characterId` must be 1 or 2. Its bank base plus the equipped
/// weapon must select a valid bank-table entry (0..33); playback additionally
/// requires a non-null loaded bank. No pointer is retained and no clip is chosen.
void playerActorWriteWeaponAnimationBankIndex(s32* bankIndexOut);

/// Writes the live companion family and variant's animation-bank table index.
///
/// `bankIndexOut` must address one writable s32, commonly an
/// `AnimationPlayRequest.source.index`. The incoming value is overwritten, never inspected.
/// The live save must select `companionType` 1..3 and a variant whose family base
/// plus variant is a valid bank entry (1..7). There is no absent-companion guard;
/// playback requires that bank to be loaded. No pointer is retained or clip chosen.
void companionWriteAnimationBankIndex(s32* bankIndexOut);

#endif // GAMEPLAY_ACTOR_PRESENTATION_H
