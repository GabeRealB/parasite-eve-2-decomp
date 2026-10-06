#ifndef SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H
#define SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"
#include "main/text.h"

#include "overlay.h"

extern TaskDesc D_shelter_b3_garbage_incinerator_801855E0;

extern TaskDesc D_shelter_b3_garbage_incinerator_80185BA0;

extern WorldCollisionGrid D_shelter_b3_garbage_incinerator_80188388[1];

extern WorldCoordRoomLights D_shelter_b3_garbage_incinerator_8018DCF0[1];

extern WorldCoordRoomLights D_shelter_b3_garbage_incinerator_8018E598[1];

extern WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018E5B0[22];

extern WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018EC38[20];

extern WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018F228[17];

extern WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018F734[6];

extern WorldCollisionOccluder D_shelter_b3_garbage_incinerator_8018FAD8[1];

extern RoomEventMsg D_shelter_b3_garbage_incinerator_8018FC2C;

extern Task* D_shelter_b3_garbage_incinerator_8018FC34;

extern ScreenWaveCtx* gScreenWaveCtx;

extern Task* D_shelter_b3_garbage_incinerator_8018FC3C;

extern CapCommandRef* CapCaption_Data_8015E650;

extern TextGlyphCell* CapCaption_Data_8015E654;

extern CapSequenceRecord* CapCaption_Data_8015E658;

extern s16 CapCaption_Data_8015E65C;

extern s16 CapCaption_Data_8015E65E;

extern s16 CapCaption_Data_8015E660;

extern s16 CapCaption_Data_8015E662;

extern s16 CapCaption_Data_8015E664;

extern s16 CapCaption_Data_8015E666;

extern u16 CapCaption_Data_8015E668;
extern u16 CapCaption_Data_8015E66A;

extern u8 CapCaption_Data_8015E66C[4];

extern ScreenWaveGridOscillator gScreenWaveColumns[10];

extern ScreenWaveGridOscillator gScreenWaveRows[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2);

/// Installs the six collision walls for the lift's first rest pose.
///
/// Requires this room's active writable grid: vertices 0..23 and faces/normals
/// 0..5, plus vertices 24..31 and faces/normals 6..7 for variant 2. Uses the
/// low lift walls' XZ layout at grid-local Y=1000..2000, in game units. Surface
/// class 1 passes probes, ignores weapon impacts and applies pushback; unit
/// normals use 4096. Retains cell lists and the room's ownership of storage.
void shelterB3GarbageIncineratorSetLiftArrivalCollisionWalls(void);

#endif // SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H
