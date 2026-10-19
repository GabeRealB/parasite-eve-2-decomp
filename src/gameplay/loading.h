#ifndef GAMEPLAY_PRIVATE_LOADING_H
#define GAMEPLAY_PRIVATE_LOADING_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/view.h"

#include "main/fs_types.h"
#include "main/task_types.h"

// Room-resource loading, view setup and sprite-list construction.

/// Phases shared by the base-resource and additional-file loading passes.
enum {
    LOADING_AREA_INIT  = 0,
    LOADING_AREA_QUEUE = 1,
    LOADING_AREA_WAIT  = 2
};

/// Additional-placement-file loading phase (0 initialize, 1 queue, 2 wait CD idle).
///
/// Reset to `LOADING_AREA_INIT` after the base-resource pass completes. Queue
/// and wait alternate without changing the borrowed layout's lifetime; both
/// passes must remain serialized for the same live saved destination.
extern s16 gLoadingAreaPlacementFilePhase;

/// Base-resource loading phase (0 initialize, 1 queue, 2 wait CD idle).
///
/// Reset to `LOADING_AREA_INIT` when the area-resource sequence starts. Queue
/// and wait alternate until the resource terminator; its layout and saved
/// destination must stay live through the subsequent placement-file pass.
extern u16 gLoadingAreaBaseResourcePhase;

/// Serialized area-resource pass selector (0 initialize, 1 base resources, 2 placement files).
///
/// Area preparation resets it to zero. Entering each pass initializes that
/// pass's phase; completion of the placement pass finishes the sequence.
extern u16 gLoadingAreaResourcePass;

/// Polls serialized additional-file loads selected by the saved area's placements.
///
/// Returns 0 while walking or waiting for CD idle, and 1 on completion.
/// Start with `gLoadingAreaPlacementFilePhase == LOADING_AREA_INIT`, after
/// `loadingPollAreaBaseResources` finishes. Both passes share cursors and
/// must run serially, with the same live saved destination and loaded layout.
/// The layout lookup must succeed: its placements are read before the NULL
/// test. Missing tables complete immediately. Live nonzero entry IDs must
/// match a resource with file-group selector 0..8 and a cataloged file number.
/// Both tables must retain their `AREA_PLACEMENT_END` records through loading.
/// Requests borrow stack bytes only until enqueue returns. Texture offsets
/// retain their signed bytes: X counts 64-word VRAM columns, CLUT Y counts rows.
u16 loadingPollAreaPlacementFiles(void);

/// Polls serialized base-resource loads for the saved area's layout.
///
/// Returns 0 while walking or waiting for CD idle, and 1 on completion.
/// Start with `gLoadingAreaBaseResourcePhase == LOADING_AREA_INIT`; the saved destination and
/// loaded layout/tables must remain valid until both area-loading passes finish.
/// The layout lookup must succeed: its resources are read before the NULL
/// test. A NULL resource table completes immediately; a live resource table
/// requires a placement table, both ended by `AREA_PLACEMENT_END`.
/// File-group selectors are 0..8 and file numbers must name cataloged files.
/// Base-60 resources load only if an entry has a placement with fileIdLow=0;
/// other groups load even without one. The first such placement supplies signed
/// texture offsets, or zero offsets when absent. Enqueue copies each request;
/// the next resource is visited only after the entire CD queue becomes idle.
u16 loadingPollAreaBaseResources(void);

/// State selectors for the session-loading handler table.
enum {
    LOADING_SESSION_STATE_PREPARE_CHARACTER  = 0,
    LOADING_SESSION_STATE_ENQUEUE_STAGE      = 1,
    LOADING_SESSION_STATE_INITIALIZE_AREA    = 2,
    LOADING_SESSION_STATE_ENQUEUE_AREA       = 3,
    LOADING_SESSION_STATE_PREPARE_AREA       = 4,
    LOADING_SESSION_STATE_POLL_AREA          = 5,
    LOADING_SESSION_STATE_RELEASE_BOOT_IMAGE = 6,
    LOADING_SESSION_STATE_START_ROOM         = 7,
};

/// Eight phase handlers copied by value by `loadingSessionLoadTask`.
///
/// Task states 0..7 select character preparation, stage requests, area memory
/// and audio, area/companion requests, saved-area preparation, area-resource
/// polling, held-fade release and room startup. There is no bounds check or
/// terminator. The final handler releases the task after boot presentation;
/// gameplay and all resources required by each selected phase must stay live.
extern const TaskFuncTable8 gLoadingSessionStateHandlers;

/// Waits for boot presentation to finish, then starts the loaded room and reveal fade.
///
/// Final state of `loadingSessionLoadTask`. Requires valid loaded room resources
/// and disposable frame ordering tables. Kills the loading task before room
/// startup and never accesses it afterwards. The Acropolis plaza omits the
/// view gate and starts its opening scene with reserved movie workspaces;
/// other rooms start the ordinary view gate. Takes a menu hold until the
/// spawned session-resume fade releases it; task-allocation failures are ignored.
void loadingFinishSessionLoadTask(Task* task);

/// State selectors for the persistent room-resource handler table.
enum {
    LOADING_ROOM_RESOURCE_STATE_INITIALIZE = 0,
    LOADING_ROOM_RESOURCE_STATE_UPDATE     = 1,
    LOADING_ROOM_RESOURCE_STATE_TEARDOWN   = 2,
};

/// Room-resource handlers copied by value by `loadingRoomResourcesTask`.
///
/// Task states 0..2 select collision/sprite-child initialization, persistent
/// room/view maintenance and task teardown. Room-object freeze suppresses every
/// state, including teardown. There is no bounds check or terminator; gameplay
/// and the room/view resources must remain loaded through dispatch.
extern const TaskFuncTable3 gLoadingRoomResourceStateHandlers;

/// Queues the current mapped view's resources once the CD queue is idle.
///
/// Loading-dispatch state 1. Uses the session's stage/area folder with suffix 1
/// and the mapped view file index, default policy and zero image displacement.
/// Advances the live task's state by one only after queueing. Requires loaded
/// location/view tables and writable file destinations. Sources are copied
/// immediately; advancing the state does not mean the load has completed.
void loadingEnqueueViewResourcesTask(Task* task);

/// Applies the companion texture relocation to a task's model and its cached packets.
///
/// A live TMD task must have a valid `extra.tmd`; other body kinds are unchanged.
/// Sets an encoded texture-page displacement of 4 (256 VRAM words horizontally)
/// and a CLUT displacement of 6 rows. Rebuilds both buffer halves when present,
/// preserving the half selector; a missing buffer retains the offsets for its
/// later construction. Does not allocate, draw, change task state or kill it.
/// Existing buffers and borrowed sources must satisfy `tmdBuildBufferHalf`'s
/// contract, with no GPU work still reading packets that will be rebuilt.
void companionRelocateModelTextures(Task* companionTask);

/// Completes a view load and releases its task according to the presentation mode.
///
/// Requires a live view-load task and session. `spawnArg1.value` is 0 (update
/// ambient audio, clear the deferred-view request and resume the game loop),
/// 1 (enable task-controlled flips and publish scene readiness), or 2 (publish
/// readiness while leaving flips under the stage transition's control).
/// Every mode clears port 0's input-block countdown and kills the task; nonzero
/// modes also select transition image strips and spawn cached-sprite setup.
/// Other nonzero values behave like 2. The task must not be used after this call.
/// Mode 0 needs valid stage/area music data and `displayResumeGameLoop`'s
/// ownership contract. Nonzero modes require task capacity and loaded sprite
/// resources/auxiliary storage for the later setup callback. Readiness is
/// published even if that spawn fails, before cached-sprite setup runs.
void loadingFinishViewLoad(Task* viewLoadTask);

/// Uploads the retained current-view image and queues a reload of its resources.
///
/// Requires valid session/view tables, live retained filesystem resources and
/// `fsUploadImageChunk`'s scratch/GPU contract. A matching image slot is retried
/// until upload returns COMPLETE, including after timer failure; no timeout is
/// imposed. An absent image skips the upload. Nonzero `skipBackground` omits
/// the background from the queued reload; zero selects the default policy.
/// Requires one free CD ring slot. The request is copied immediately and
/// completes asynchronously; this function does not rebuild model/sprite packets.
void loadingRestoreViewImageAndEnqueueResources(u8 skipBackground);

/// Uploads the retained image for the current mapped view to VRAM.
///
/// Searches all fifty filesystem directory slots for an image at mapped view
/// minus one. An absent image performs no upload. Requires valid loaded
/// session/view maps, completed retained payloads and `fsUploadImageChunk`'s
/// source, scratch, timer and GPU contracts; the borrowed payload must remain
/// live throughout the call. Retries every non-COMPLETE result, including timer
/// failure, without a timeout. Disables the uploader's GPU-time cutoff; does
/// not queue file loads, rebuild packets or change presentation ownership.
void loadingUploadCachedViewImage(void);

/// Queues the current stage's CDF mount followed by its map and room-name package.
///
/// Requires a live session with stage 1..5 and two free CD ring slots. The map
/// package is global file 900000 + stage. Both requests are copied immediately;
/// this function does not wait or update the session's loaded-stage cache.
/// The mount retains the enqueue API's reads of argument bytes from address zero.
void loadingEnqueueStageResources(void);

/// Queues a companion family's base package and an optional variant package.
///
/// `companionType` is 0 (no operation) or 1..3. Current schedules use family 1
/// variants 1..5 and variant 0 for families 2/3; the arguments are not checked.
/// Global files are 800000 + type*100, followed by that ID + variant when
/// nonzero. Both use default policy, a four-page horizontal image displacement
/// (256 VRAM words) and a six-row palette displacement. After queueing variant
/// 5, writes variant 3 into the session and live save for subsequent actor setup.
///
/// Requires a live session/save, one free CD ring slot (two for a variant),
/// valid file destinations and an initialized scratch stack with eight bytes
/// available. The reservation is released before return; request sources are
/// copied synchronously.
void companionEnqueueResources(u8 companionType, u8 resourceVariant);

/// Five stage-map directories of per-room logical-view counts.
///
/// Stage 1..5 selects stage minus one; area/room are valid one-based indices
/// in a populated directory. NULL area slots are excluded. Counts are signed
/// search lengths; the room-transition reader retains only their low byte.
/// Each map wrapper borrows room counts, so both overlays must stay loaded.
extern ViewCountTable* gViewCountTables[5];

/// Five stage-map directories of room-local logical-view byte maps.
///
/// Indexed by stage minus one (stages 1..5), then area, room and logical view
/// minus one through separately bounded directories. All selected entries must
/// exist; mapped bytes are one-based camera/image/sprite indices. The map borrows
/// room data, so both owning overlays must stay loaded during lookup.
extern ViewIndexTable* gViewIndexTables[5];

/// Five stage-map directories of mutable room collision resources.
///
/// Stage 1..5 selects stage minus one; each directory then uses area minus one
/// and room minus one with its own bounds. NULL area entries bind no resources.
/// The selector slots are fixed, while grids, triggers and occluders are mutable.
/// Keep the selected stage-map and room overlays loaded while their resources
/// or linked lists are in use; gameplay borrows them and releases no storage.
extern WorldCollisionStageResources* const gWorldCollisionStageResources[5];

/// Five stage-map directories of per-area direction warp endpoints.
///
/// Indexed by stage minus one (1..5), area minus one and warp ID minus one.
/// Area and endpoint bounds vary; require a populated area and a valid endpoint,
/// independently of room ID. Entries supply departure facing/sounds and arrival
/// placements, view and effects. Fixed selector slots borrow stage-map directories
/// which borrow room-overlay endpoints; keep both owning overlays loaded for lookup.
extern DirectionWarpEntry** const gDirectionAreaWarpEntriesByStage[5];

/// Room-start choice for creating the automatic view-transition gate.
enum {
    AREA_ROOM_START_WITH_VIEW_GATE = 0,
    AREA_ROOM_START_SKIP_VIEW_GATE = 1,
};

/// Starts the destination room's actors, controllers, placements and arrival effects.
///
/// Called after area resources and the boot-image fade finish. `skipViewGate`'s
/// low halfword equal to `AREA_ROOM_START_SKIP_VIEW_GATE` omits the automatic
/// view-transition gate; every other value creates it. Resets runtime controls,
/// raises nonpositive player/present-companion HP to 1 and clears actor task
/// publications before spawning replacements. Uses the warp's default view
/// unless room-start display flags preserve the saved view; nighttime Garage
/// room 2 / warp 2 selects view 2. Pending saved-position restoration uses the
/// resident player pose, otherwise the warp supplies both actor transforms.
/// Arrival sound/map effects are skipped on the session's first room start.
///
/// Requires matching live save/session state; valid loaded stage (1..5), area,
/// one-based warp, view, actor and placement resources; empty/disposable prior
/// room registrations; and enough task/model/packet storage for all spawns.
/// A saved demo selector other than 0 or 11 must name a loaded task bank with
/// a valid descriptor at index 1.
/// Player spawning must succeed. Saved-pose restoration requires `characterId`
/// to select valid resident player storage; only value 1 is established for the
/// single `gPlayerStatus` record. Spawn transforms/options are read synchronously.
void areaStartRoomRuntime(s32 skipViewGate);

/// Queues character, weapon and healing-sound resources after CD and disk readiness.
///
/// State 0 of the gameplay loading task. Renews the loading-status byte while
/// waiting. An incomplete disk-swap prompt returns before drawing; otherwise
/// holds/starts an active boot image, clears all 15 stream slots and queues
/// character/weapon resources only if the saved character or resource variant
/// differs from the session cache. Always queues the healing sound and advances
/// to state 1 without waiting; cache updates record requests, not completion.
///
/// Requires a live task, matching live save/session destinations, retired stream
/// slots, valid character/equipment resources, initialized scratch/CD state and
/// capacity for the queued requests. Boot-image starts require configured load
/// screen assets. Draws with the darkness-8 packet/OT contract of
/// `loadingEnqueueStageResourcesTask` unless a boot image is active.
void loadingPrepareCharacterResourcesTask(Task* task);

/// Queues a changed stage's mount and map resources once pending CD work finishes.
///
/// State 1 of the gameplay loading task. Advances to state 2 even when the
/// stage is already cached; `loadedStage` records a queued request, not its
/// completion. Requires a live task/session with stage 1..5 and an initialized
/// CD queue. While no boot image is active, links a subtractive overlay of
/// darkness 8 into normal-frame foreground OT tag -16. The current packet
/// buffer index must be 0 or 1, with previous GPU uses of that half finished.
void loadingEnqueueStageResourcesTask(Task* task);

/// Initializes destination-area image memory, first-visit state and audio policy.
///
/// State 2 of the gameplay loading task. Waits for CD idle, initializes new-game
/// or unvisited-stage state as needed, then configures and resets the auxiliary
/// heap for the saved area. Sets companion sound retention before resetting the
/// live area's sound context. Initializes the later countdown-music selector
/// to entry 1 for nighttime Dryfield at story chapter 4 or later, otherwise 0;
/// restores death sound/restart timing to 1/30 task ticks and requests ordinary
/// area music with a nominal 60-audio-update fade out.
/// Advances to state 3 without waiting for the music task or its resource load.
///
/// Requires a live task and matching saved/session stage and area, valid loaded
/// map/object/music and companion-schedule tables, available music-task storage,
/// and finished uses of the image/heap storage being repurposed. Uses
/// `memConfigureImageMemory`'s bounds/lifetime contract. Drawing has the
/// darkness-8 packet/OT contract of `loadingEnqueueStageResourcesTask`.
void loadingInitializeAreaMemoryAndAudioTask(Task* task);

/// Queues the area's base file and any companion resources needing replacement.
///
/// State 3 of the gameplay loading task. Waits for CD idle, then queues file 0
/// from folder area*100+1 in the session's stage CDF, using default policy and
/// zero image displacement. Selects the destination companion from the live
/// save and queues its resources only when replacement is needed. Advances
/// immediately to state 4; request bytes are copied and loads finish later.
/// Requires matching live save/session destinations, loaded area/schedule tables
/// within `companionSelectForArea`'s bounds, and initialized CD/scratch state.
/// Drawing has the same darkness-8 packet/OT contract as
/// `loadingEnqueueStageResourcesTask` and is skipped while a boot image is active.
void loadingEnqueueAreaAndCompanionResourcesTask(Task* task);

/// Reconciles saved area placements and prepares the destination view's movie.
///
/// State 4 of the gameplay loading task. After CD idle, restores the saved
/// placement variant only for `applySaveVariant == 1`, clears that request,
/// records the destination visit and synchronizes the save/session variant.
/// Nighttime Gas Station rooms 4 and above also reset the area's sound context
/// and queue its replacement sound banks from folder 101, file 22. Resets the
/// area-resource loading phase and advances to state 5 without waiting for
/// any newly queued work.
/// Requires a live task, valid matching save/session destinations, loaded
/// stage/area/variant and movie tables, and initialized CD state. Drawing has
/// the darkness-8 packet/OT contract of `loadingEnqueueStageResourcesTask`.
void loadingPrepareAreaStateTask(Task* task);

/// Completes the area's base and placement-resource passes before runtime setup.
///
/// State 5 of the gameplay loading task. Start with `gLoadingAreaResourcePass == 0`; both
/// passes share cursors and run serially, with the placement pass beginning on
/// the callback after base completion. On completion, resets collision/model
/// lists, runs the model composition/draw pass, then advances to state 6. A
/// nonzero saved interlace option enables both display environments; zero
/// leaves their existing flags unchanged.
///
/// Requires a live task and stable matching saved/session destinations, loaded
/// area layouts and resource tables under the two polling functions' contracts,
/// and disposable old collision/model lists. Drawing uses the darkness-8 packet
/// and OT contract of `loadingEnqueueStageResourcesTask`, and is skipped while
/// a boot image owns presentation. The current draw-buffer index must be 0 or 1.
void loadingPollAreaResourcesTask(Task* task);

/// Holds the loading fade for seven callback ticks, then releases the boot image.
///
/// State 6 of the gameplay loading task, entered with `killCountdown == 0`.
/// Counts every tick, including while a boot image suppresses this overlay.
/// Draws with subtractive darkness 100 when no boot image is active; packet
/// lifetime and OT requirements match `loadingEnqueueStageResourcesTask`.
/// At count 7, clears `holdBootImage` and advances to state 7, which waits for
/// the boot-image machine's closing fade. Requires a live task and CD queue;
/// this callback neither kills the task nor clears `bootLoadActive` itself.
void loadingHoldFadeAndReleaseBootImageTask(Task* task);

/// Binds initial room collision resources and starts its child view-sprite task.
///
/// State 0 of loadingRoomResourcesTask. Requires a live task, loaded stage
/// 1..5 and valid one-based area/room indices. Non-NULL trigger and occluder
/// lists must be writable, LAST-terminated and remain loaded until unlinked;
/// previous registrations must already be cleared. Binds grids and triggers
/// to the view coordinate without applying the camera or clearing prior lists.
/// A NULL area list or grid retains the prior grid publication. Spawns bank-0
/// slot 0x1B as a teardown child when allocation succeeds, then clears the
/// dirty request and advances to state 1 even when that spawn fails.
void loadingInitRoomResourcesTask(Task* task);

/// Refreshes room collision/view state and queues the current view's clipping packets.
///
/// State 1 of `loadingRoomResourcesTask`. A live `task` caches the last logical
/// view in `spawnArg1.value`; a change invalidates and rebuilds the view cache.
/// A nonzero `roomObjsDirty` reapplies the camera and replaces borrowed collision
/// resources before clearing the request. Clipping commands are queued on every
/// call. Requires valid loaded view/room resources, live old lists until clearing,
/// sufficient current-frame packet storage and a 1024-tag depth-sorted OT.
void loadingUpdateRoomResourcesTask(Task* task);

/// Five stage-map directories of per-area mapped-camera arrays.
///
/// Select the directory with stage - 1 (stages 1..5), then area - 1 with
/// extents 20, 38, 38, 49 and 33. Each camera array has its own extent and some
/// areas are NULL. Lookups require populated slots and an in-range mapped
/// camera index; `ViewCameraTable` defines the 1-based camera convention.
/// The selected map and room overlays must stay loaded through borrowed use.
extern ViewCameraTable* gViewCameraTables[5];

/// Reusable GPU command that resets mask-bit control before a view-image load.
///
/// Initialized for each load; shared storage remains valid for the gameplay
/// overlay's lifetime while the GPU consumes the submitted packet.
extern DR_STP gLoadingViewMaskResetCommand;

/// Uploads the loaded view image, then starts its selected movie or finishes loading.
///
/// State 2 of `loadingViewLoadTask`; a busy CD queue leaves the task untouched.
/// Clears the complete active-request snapshot, searches all fifty retained
/// directory slots using the mapped view minus one, and retries a matching image
/// until upload completes, including timer failure. A missing image skips upload.
/// Requires live view maps/payloads and `fsUploadImageChunk`'s GPU/scratch contract.
/// The selected movie slot must be negative (none) or in 0..14.
/// A movie queues playback and advances to state 3 with its ready counter zero;
/// otherwise a staged replacement advances to state 4, and an empty replacement
/// finishes immediately. Requires one free CD ring slot and the completion
/// resources of `loadingFinishViewLoad`; immediate completion kills the task.
void loadingUploadViewImageTask(Task* viewLoadTask);

/// Selected view-movie slot: 0..14, or `STREAM_SLOT_NOT_FOUND` (-1) when absent.
///
/// View loading caches the loaded stream-directory selection for the subsequent
/// image-upload task; playback encodes the live slot in one command byte. Keep
/// the stream directory and view selection stable between selection and playback.
/// Cached-scene-image loading bypasses this selection and leaves the value intact.
extern s16 gLoadingViewMovieSlot;

/// Holds a synchronized frame and starts the current view's image/movie load.
///
/// State 0 of `loadingViewLoadTask`. Requires the selected view/location tables,
/// live file destinations and movie descriptors; presentation modes 0..2 follow
/// `loadingFinishViewLoad`. Nonzero modes request a flip hold before waiting
/// for the game/task buffer selectors to agree. Agreement waits for GPU work
/// and disables mask-bit writes before holding the frame.
/// A cached scene payload queues decoding and selects state 5. Otherwise selects
/// a view-enabled movie (0..14 or negative), releasing model packets for movies
/// or restoring their policy on leaving movie mode. A busy CD queue requests
/// cancellation and advances to state 1; an idle queue requests the mapped file,
/// advances to state 2 and calls its image-load handler immediately. That call
/// can complete loading and kill the task. CD and decode work remain asynchronous.
void loadingBeginViewLoadTask(Task* viewLoadTask);

#endif // GAMEPLAY_PRIVATE_LOADING_H
