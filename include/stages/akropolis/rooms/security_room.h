#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_SECURITY_ROOM_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_SECURITY_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Number of identity-mapped camera and sprite slots in the security room.
enum { AKROPOLIS_SECURITY_ROOM_VIEW_COUNT = 16 };

/// Placement variants 0..3 of the security room, indexed by saved layout variant.
extern AreaVariant gAkropolisSecurityRoomAreaVariants[4];

extern TmdSource gAkropolisSecurityRoomItemBoxModel;

/// Collision resources for the security room's sole room, indexed by room minus one.
extern WorldCollisionRoomResources gAkropolisSecurityRoomRoomCollisionResources[1];

/// Logical-view maps for the security room's sole room.
extern u8* gAkropolisSecurityRoomViewMaps[1];

/// Logical-view search count for the security room's sole room.
extern ViewCount gAkropolisSecurityRoomViewCounts[1];

/// Lighting for the security room's sole room, indexed by room minus one.
extern WorldCoordRoomLighting gAkropolisSecurityRoomRoomLightingTable[1];

/// Single hallway warp endpoint, selected by one-based warp ID 1.
extern DirectionWarpEntry gAkropolisSecurityRoomWarpEntries[1];

/// Sprite sources and batches for security-room mapped views 1..16.
extern SpriteView gAkropolisSecurityRoomSpriteViews[AKROPOLIS_SECURITY_ROOM_VIEW_COUNT];

/// Cameras for security-room mapped views 1..16.
extern ViewCamera gAkropolisSecurityRoomViewCameras[AKROPOLIS_SECURITY_ROOM_VIEW_COUNT];

/// Security-room collision policies indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisSecurityRoomSurfacePropertiesByClass[8];

/// Lights the power supply's four lock lamps by the keys inserted: in its view their pictures, elsewhere their glows.
void akropolisSecurityRoomLockLampsTask(Task* task);

/// Draws one 128x128 lock lamp and retires the counted effect.
void akropolisSecurityRoomLockLampTask(Task* task);

/// Draws and tumbles an opaque textured square until it settles in room view 15.
void akropolisSecurityRoomFallingQuadTask(Task* task);

/// Draws one flickering additive lock-lamp glow and retires the counted effect.
void akropolisSecurityRoomLockLampGlowTask(Task* task);

/// Runs the security monitor's camera-selection and brightness panel.
void akropolisSecurityRoomMonitorTask(Task* task);

/// Runs the power-supply panel's lock prompts and unlock scenes.
void akropolisSecurityRoomPowerSupplyTask(Task* task);

/// Keeps the security room's transition, action and key-item messages available.
void akropolisSecurityRoomMessageTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_SECURITY_ROOM_H
