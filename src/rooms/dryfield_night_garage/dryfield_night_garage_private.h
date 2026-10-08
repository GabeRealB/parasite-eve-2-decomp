#ifndef SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"

#include "main/task_types.h"

extern AnimationSet gDryfieldNightGarageAnimation04B80;

extern AnimationSet gDryfieldNightGarageAnimation04F54;

extern AnimationSet gDryfieldNightGarageAnimation05344;

extern AnimationSet gDryfieldNightGarageAnimation056B0;

extern TaskDesc D_dryfield_night_garage_80182C98[2];

extern s32 D_dryfield_night_garage_80182DE0;

extern s32 D_dryfield_night_garage_80182DE4;

extern EvsCommand D_dryfield_night_garage_80182DF8[40];

extern EvsCommand D_dryfield_night_garage_801831B8[19];

extern WorldCollisionGrid D_dryfield_night_garage_80183DD4;

extern WorldCollisionTrigger D_dryfield_night_garage_80186D7C[16];

extern s32 Shop_Data_80187628;

extern const EquipmentWeaponSupply* Shop_Data_8018762C;

/// Finds a placed actor in the current stage and area, or returns NULL.
///
/// `placementIndex` must be 0..15; it occupies the high nibble of the u16
/// placement key. Requires a live scene manager whose children carry Enemy
/// work. Returns a borrowed task, valid only while that actor remains live.
Task* dryfieldNightGarageFindPlacedActor(s32 placementIndex);

void func_dryfield_night_garage_801807E4(Task* arg0);

/// Stages audio start for the refueling scene selected by the event script.
///
/// View loading later commits the deferred request. Selection and
/// playback buffers must remain live until it is consumed.
void dryfieldNightGarageStageSceneAudioStart(void);

/// Enqueues playback of the refueling scene after its setup and caption cues.
///
/// Requires the selected scene's prepared buffers to survive playback and
/// space in the CD request ring.
void dryfieldNightGarageEnqueueScenePlayback(void);

/// Finishes refueling-scene streaming and restores the saved random state.
///
/// Requires a successful scene selection. Marks streaming complete without
/// freeing buffers or tasks; the event script restores the remaining resources.
void dryfieldNightGarageFinishScene(void);

/// Cancels the refueling scene when its skip script restores the room.
///
/// Discards the deferred CD replacement and requests cancellation; subsequent
/// CD dispatches complete it. Finishes streaming and restores saved random
/// state immediately. Requires a successful scene selection; the script owns
/// buffer and task cleanup.
void dryfieldNightGarageCancelScene(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_GARAGE_DRYFIELD_NIGHT_GARAGE_PRIVATE_H
