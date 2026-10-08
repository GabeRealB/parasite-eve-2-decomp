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

/// Four-byte file key read synchronously by `cdCmdEnqueue`.
///
/// Byte 1 is ignored; these fields select a global file or a stage-folder file.
typedef struct {
    u8 fileIndex;      // Low file-ID component or mapped view index
    u8 ignoredByQueue; // Not read by the enqueue API
    u8 fileGroup;      // Global category or area-folder hundreds component
    u8 stage;          // CDF selector (0 global library, 1..5 stage folders)
} _LoadingFileKey;
STATIC_ASSERT_SIZEOF(_LoadingFileKey, 4);

/// File-load options using `CdCmdEntry.args.file`'s types and four-byte layout.
typedef __typeof__(((CdCmdEntry*)0)->args.file) _LoadingFileArgs;
STATIC_ASSERT_SIZEOF(_LoadingFileArgs, 4);

/// Phases shared by the base-resource and additional-file loading passes.
enum {
    LOADING_AREA_INIT  = 0,
    LOADING_AREA_QUEUE = 1,
    LOADING_AREA_WAIT  = 2
};

/// Phase for `loadingPollAreaPlacementFiles` (0 init, 1 walk/enqueue, 2 wait idle).
/// `loadingPollAreaResourcesTask` clears it when phase 1 (`loadingPollAreaBaseResources`) finishes
/// so phase 2 can start.
extern s16 Gp_AreaCdPhase;

/// Phase for `loadingPollAreaBaseResources`. `loadingPollAreaResourcesTask` clears it when entering
/// its own phase 1.
extern u16 D_80114C70;

/// Phase for `loadingPollAreaResourcesTask` (0 init, 1 `loadingPollAreaBaseResources`, 2 `loadingPollAreaPlacementFiles`).
/// `loadingPrepareAreaStateTask` clears it when advancing to this task state.
extern u16 D_80114C74;

/// Polls serialized additional-file loads selected by the saved area's placements.
///
/// Returns 0 while walking or waiting for CD idle, and 1 on completion.
/// Start with `Gp_AreaCdPhase == LOADING_AREA_INIT`, after
/// `loadingPollAreaBaseResources` finishes. Both passes share cursors and
/// must run serially, with the same live saved destination and loaded layout.
/// The layout lookup must succeed: its placements are read before the NULL
/// test. Missing tables complete immediately. Live nonzero entry IDs must
/// match a resource with file-group selector 0..8 and a catalogued file number.
/// Both tables must retain their `AREA_PLACEMENT_END` records through loading.
/// Requests borrow stack bytes only until enqueue returns. Texture offsets
/// retain their signed bytes: X counts 64-word VRAM columns, CLUT Y counts rows.
u16 loadingPollAreaPlacementFiles(void);

/// Polls serialized base-resource loads for the saved area's layout.
///
/// Returns 0 while walking or waiting for CD idle, and 1 on completion.
/// Start with `D_80114C70 == LOADING_AREA_INIT`; the saved destination and
/// loaded layout/tables must remain valid until both area-loading passes finish.
/// The layout lookup must succeed: its resources are read before the NULL
/// test. A NULL resource table completes immediately; a live resource table
/// requires a placement table, both ended by `AREA_PLACEMENT_END`.
/// File-group selectors are 0..8 and file numbers must name catalogued files.
/// Base-60 resources load only if an entry has a placement with fileIdLow=0;
/// other groups load even without one. The first such placement supplies signed
/// texture offsets, or zero offsets when absent. Enqueue copies each request;
/// the next resource is visited only after the entire CD queue becomes idle.
u16 loadingPollAreaBaseResources(void);

extern const TaskFuncTable3 Gp_SessionStates;

extern const TaskFuncTable8 Gp_LoadStateFns;

/// Waits for boot presentation to finish, then starts the loaded room and reveal fade.
///
/// Final state of `loadingSessionLoadTask`. Requires valid loaded room resources
/// and disposable frame ordering tables. Kills the loading task before room
/// startup and never accesses it afterwards. The Acropolis plaza omits the
/// view gate and starts its opening scene with reserved movie workspaces;
/// other rooms start the ordinary view gate. Takes a menu hold until the
/// spawned session-resume fade releases it; task-allocation failures are ignored.
void loadingFinishSessionLoadTask(Task* task);

extern const TaskFuncTable3 Gp_RoomObjStates;

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

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern ViewCountTable* Gp_ViewCountTables[];

/// Five stage-map directories of room-local logical-view byte maps.
///
/// Indexed by stage minus one (stages 1..5), then area, room and logical view
/// minus one through separately bounded directories. All selected entries must
/// exist; mapped bytes are one-based camera/image/sprite indices. The map borrows
/// room data, so both owning overlays must stay loaded during lookup.
extern ViewIndexTable* gViewIndexTables[5];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern WorldCollisionStageResources* Gp_RoomObjTables[];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`. Each
/// entry is an array of `DirectionWarpEntry*`, indexed 1-based by
/// `GameSession.location.loc.area` / `GameLocationKey.area`.
extern DirectionWarpEntry** Gp_WarpTables[];

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
/// State 5 of the gameplay loading task. Start with `D_80114C74 == 0`; both
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

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern ViewCameraTable* Gp_ViewTables[];

extern DR_STP D_80114C50;

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

extern s16 D_80114C40;

void Gp_ViewBeginLoad(Task* task);

#endif // GAMEPLAY_PRIVATE_LOADING_H
