#ifndef INCLUDE_ACTORS_ACTOR_215100_H
#define INCLUDE_ACTORS_ACTOR_215100_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Live gallery training-session latch (0 inactive, 1 active).
///
/// Set when the menu stages a selected course; cleared on completion or a
/// committed exit/abort. Room actions and exit gates read it while both the
/// actor and shooting-gallery overlays remain loaded.
extern s32 gActor215100GalleryTrainingActive;

/// Descriptor slots for the gallery's introduction, training menu and red-flash loop.
enum {
    ACTOR_215100_GALLERY_INTRO_TASK          = 0,
    ACTOR_215100_GALLERY_MENU_TASK           = 1,
    ACTOR_215100_GALLERY_RED_FLASH_LOOP_TASK = 2,
    ACTOR_215100_GALLERY_TASK_COUNT          = 3,
};

/// Bodyless gallery event tasks, indexed by `ACTOR_215100_GALLERY_*_TASK`.
///
/// Introduction and menu tasks start at state 0. The menu's first spawn argument
/// is zero for direct entry or nonzero to ask the repeat-entry confirmation.
/// The red-flash loop ignores both arguments and must be killed by its owner.
/// Keep the actor and gallery overlays and their CAP/script resources loaded
/// through playback. Spawning borrows the descriptors and callback code.
extern TaskDesc gActor215100GalleryTaskDescs[ACTOR_215100_GALLERY_TASK_COUNT];

/// Descriptor slots for Pierce's body and its attached paired model.
enum {
    ACTOR_215100_PIERCE_BODY_TASK  = 0,
    ACTOR_215100_PIERCE_PAIR_TASK  = 1,
    ACTOR_215100_PIERCE_TASK_COUNT = 2,
};

/// Pierce's body and paired-model spawn descriptors.
///
/// Area placement uses the body slot; its initializer spawns and adopts the
/// pair slot as a child. Enemy spawning supplies the owning Enemy in the second
/// task argument. Both callbacks require the actor overlay and model sources
/// to stay loaded until the task tree is destroyed.
extern TaskDesc gActor215100PierceTaskDescs[ACTOR_215100_PIERCE_TASK_COUNT];

/// Pierce's one-time story scene at shooting-gallery action point 4.
///
/// The 117 commands include END. Switches to conversation CAP command 1,
/// installs player clips, stages both actors, then restores weapons, view 2,
/// placements, player control and the default CAP resource. Start with
/// `EVENT_SCRIPT_HUD_HIDE_RESTORE` and the matching skip script. Both overlays,
/// CAP resources, callbacks and borrowed animation data must stay loaded.
extern EvsCommand gActor215100PierceStoryScript[117];

/// Restores the gallery after skipping `gActor215100PierceStoryScript`.
///
/// The 15 commands include END. Cleans up scene playback under a primary fade,
/// restores Pierce and the player in view 2, and releases scripted control
/// before resetting CAP selection. Requires the story's loaded resources and
/// live actor tasks through completion; the event task restores the HUD.
extern EvsCommand gActor215100PierceStorySkipScript[15];

/// Gallery exit-gate replies to the room-transition resolver.
enum {
    ACTOR_215100_GALLERY_EXIT_ALLOW = 1,
    ACTOR_215100_GALLERY_EXIT_DEFER = 2,
};

/// Checks movement input near the gallery exit and starts a training-exit prompt.
///
/// Called each room tick with live player/model and gallery overlays. Active
/// courses 3..5 rearm at view 18 after a 60-tick attachment-action cooldown.
/// The zone uses root world coordinates: X < -6150 and 4301 <= Z < 5700.
/// Held UP with yaw 2561..3583, or DOWN with yaw 513..1535, starts the prompt;
/// yaw uses 4096 units per turn. Scripted control, CAP playback, an attachment
/// menu or pending display mode suppress it. The decision task restores control.
void actor215100CheckGalleryExitInput(void);

/// Ends the completed gallery course's interaction and restores its barrier or view.
///
/// Courses 1..2 select saved return view 8 and default lights; 3..5 lower the
/// training barrier. Courses below 4 also release weapon-swap locking. Stops
/// music with a 30-audio-tick fade request. Requires both overlays to stay loaded.
void actor215100FinishGalleryCourse(void);

/// Requests course-dependent gallery cancellation or abort confirmation.
///
/// Course 5 ends training, lowers the barrier and requests weapon restoration;
/// courses 1..2 start CAP abort confirmation and its decision task. Courses
/// 3..4 do nothing. Requires the live gallery/controller and loaded CAP resources.
void actor215100RequestGalleryAbort(void);

/// Resolves a gallery exit request, deferring it through confirmation when needed.
///
/// Used for arrival warp 5. Returns `ACTOR_215100_GALLERY_EXIT_ALLOW` when no
/// training session is active and `EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT` is clear; otherwise returns
/// `ACTOR_215100_GALLERY_EXIT_DEFER`. Query mode has no effects. Deferred
/// execution copies the complete request into overlay-owned pending
/// storage and queues the exit decision task, with a confirmation during training
/// or transition playback when the flag is set. The request is borrowed only
/// during this call; both overlays and CAP resources stay loaded until handoff.
s32 actor215100ResolveGalleryExit(const RoomEventMsg* request);

/// Handles the gallery's action point by signaling training or starting its CAP event.
///
/// During training the published controller must be live. Otherwise CAP command
/// 17 starts only when playback is idle and pauses actors. Requires both overlays.
void actor215100HandleGalleryAction(void);

/// Selects the gallery caption CAP payload and its font texture-page origin.
///
/// texturePageX counts VRAM words and texturePageY rows. dataResourceIndex is a
/// zero-based ordinal among loaded data resources. Selection relocates existing
/// storage, with no I/O or allocation; CAP/font storage must outlive caption use.
/// Coordinates are stored even when the ordinal is absent or CAP magic invalid;
/// failed selection keeps the previous caption tables.
void actor215100SelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

/// Selects a keyed caption record and caches its screen-pixel text layout.
///
/// commandIndex is zero-based and must be below the loaded `CapCommandTable`'s
/// count and in 0..32767. A null entry hides the caption and returns 1, retaining
/// the other selection state. Otherwise key must be in 0..255 and match a
/// nonterminal record in slots 1..32767; a missing key reaches the terminal and
/// its invalid text pointer is still measured. The command's key-selection mode
/// is ignored. Returns 0 after selection; this neither draws nor starts a task.
///
/// bottomBaselineY is the last text baseline in screen pixels, stored as a
/// signed halfword before layout measurement. Selection resets the caret's
/// thirty-draw delay. Keep this actor overlay and the relocated CAP file and
/// glyph cells loaded through caption use. Text must end within 32768 u16 words
/// and every nonnegative code's low-ten-bit glyph index must exist in the table.
s32 actor215100CapCaptionSelectRecord(s16 commandIndex, s16 key, s32 bottomBaselineY);

/// Starts Pierce's next shooting-gallery conversation from persistent talk progress.
///
/// Requires this actor overlay and the gallery's conversation resources to be
/// loaded, with event playback available. Progress 0 starts the first scene
/// and becomes 1 before playback; progress 1 starts the second and becomes 2
/// after starting playback. Progress 2 repeats the final scene. Other values
/// do nothing. The event script hides the HUD and restores it on completion.
void actor215100StartPierceConversation(void);

/// Queues the actor's selected caption, background, title and continuation caret.
///
/// Does nothing without a selected sequence, at its terminal record, or while
/// resident CAP playback is busy. The selected slot must be in 1..32767 within
/// a live relocated sequence. Keep this actor overlay and its caption file,
/// glyph metrics, textures and palettes loaded; selection must already have
/// established the block metrics. Text must end within 32768 u16 words, use
/// valid glyph indices and icon selectors 0..3, and provide a line break for
/// the caret position. OT entries 2 and 3 and enough primitive storage must
/// remain writable and live through GPU completion.
///
/// Draws all text without advancing the sequence. Title 0 means none; 1..255
/// selects glyph 0..254. The title-bank flag is passed through but ignored by
/// this drawer. Instant-text records suppress the caret; other drawing calls
/// step its initial thirty-call delay, then emit and pulse it.
void actor215100CapCaptionDrawCurrent(void);

#endif // INCLUDE_ACTORS_ACTOR_215100_H
