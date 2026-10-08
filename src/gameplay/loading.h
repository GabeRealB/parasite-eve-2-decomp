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

/// Phase for `Gp_PollAreaCdLoads` (0 init, 1 walk/enqueue, 2 wait idle).
/// `Gp_LoadWaitAreaCd` clears it when phase 1 (`func_800AA120`) finishes
/// so phase 2 can start.
extern s16 Gp_AreaCdPhase;

/// Phase for `func_800AA120`. `Gp_LoadWaitAreaCd` clears it when entering
/// its own phase 1.
extern u16 D_80114C70;

/// Phase for `Gp_LoadWaitAreaCd` (0 init, 1 `func_800AA120`, 2 `Gp_PollAreaCdLoads`).
/// `loadingPrepareAreaStateTask` clears it when advancing to this task state.
extern u16 D_80114C74;

/// Queues additional files selected by the layout's placements; returns 1 when finished.
u16 Gp_PollAreaCdLoads(void);

/// Queues the layout's base resources and their texture relocation; returns 1 when finished.
u16 func_800AA120(void);

extern const TaskFuncTable3 Gp_SessionStates;

extern const TaskFuncTable8 Gp_LoadStateFns;

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

void func_800AA548(s32 arg0);

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

void Gp_LoadWaitAreaCd(Task* task);

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

void Gp_ViewLoadImage(Task* task);

extern s16 D_80114C40;

void Gp_ViewBeginLoad(Task* task);

#endif // GAMEPLAY_PRIVATE_LOADING_H
