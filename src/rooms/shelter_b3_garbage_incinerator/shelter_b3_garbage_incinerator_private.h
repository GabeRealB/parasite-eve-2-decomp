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

extern GpGridParams D_shelter_b3_garbage_incinerator_80188388[1];

extern GpRoomCoordSet D_shelter_b3_garbage_incinerator_8018DCF0[1];

extern GpRoomCoordSet D_shelter_b3_garbage_incinerator_8018E598[1];

extern GpObj4C D_shelter_b3_garbage_incinerator_8018E5B0[22];

extern GpObj4C D_shelter_b3_garbage_incinerator_8018EC38[20];

extern GpObj4C D_shelter_b3_garbage_incinerator_8018F228[17];

extern GpObj4C D_shelter_b3_garbage_incinerator_8018F734[6];

extern GpObj3A D_shelter_b3_garbage_incinerator_8018FAD8[1];

extern RoomEventMsg D_shelter_b3_garbage_incinerator_8018FC2C;

extern Task* D_shelter_b3_garbage_incinerator_8018FC34;

extern OverlayWaveCtx* gScreenWaveCtx;

extern Task* D_shelter_b3_garbage_incinerator_8018FC3C;

extern GpCapEntry* CapCaption_Data_8015E650;

extern GlyphUvwh* CapCaption_Data_8015E654;

extern GpEvt12* CapCaption_Data_8015E658;

extern s16 CapCaption_Data_8015E65C;

extern s16 CapCaption_Data_8015E65E;

extern s16 CapCaption_Data_8015E660;

extern s16 CapCaption_Data_8015E662;

extern s16 CapCaption_Data_8015E664;

extern s16 CapCaption_Data_8015E666;

extern u16 CapCaption_Data_8015E668;
extern u16 CapCaption_Data_8015E66A;

extern u8 CapCaption_Data_8015E66C[4];

extern OverlayWaveRec gScreenWaveColumns[10];

extern OverlayWaveRec gScreenWaveRows[30];

extern POLY_FT4 gScreenWaveGrid[2][30][8];

void func_shelter_b3_garbage_incinerator_8018108C(s16 arg0, s16 arg1, s16 arg2);

void func_shelter_b3_garbage_incinerator_801853C4(void);

#endif // SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H
