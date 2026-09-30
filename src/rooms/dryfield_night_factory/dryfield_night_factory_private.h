#ifndef SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern GpRoomCoordSet D_dryfield_night_factory_80189C88[1];

extern GpObj4C D_dryfield_night_factory_80189CA0[14];

extern WorldCoordRoomAmbientEntry D_dryfield_night_factory_8018A0C8[20];

extern GpObj4C D_dryfield_night_factory_8018A168[19];

extern RoomEventMsg D_dryfield_night_factory_8018A7D4;

extern u8 D_dryfield_night_factory_8018A7DC;

extern TaskDesc* D_dryfield_night_factory_8018A7E0;

extern TaskDesc* D_dryfield_night_factory_8018A7E4;

extern Task** D_dryfield_night_factory_8018A7E8;

extern RoomEventReq D_dryfield_night_factory_8018A7EC;

extern GpGridParams D_dryfield_night_factory_80186C20;

extern GpGridParams D_dryfield_night_factory_80186CF0;

extern GpGridParams D_dryfield_night_factory_80186DBC;

extern SpriteBatch D_dryfield_night_factory_80187EC0[2];

extern GpSprtElem D_dryfield_night_factory_80187ED0[13];

extern SpriteBatch D_dryfield_night_factory_80187FD4[6];

extern GpSprtElem D_dryfield_night_factory_80188004[12];

extern SpriteBatch D_dryfield_night_factory_801880F4[5];

extern GpSprtElem D_dryfield_night_factory_8018811C[82];

extern SpriteBatch D_dryfield_night_factory_80188784[7];

extern SpriteBatch D_dryfield_night_factory_801887BC[2];

extern GpSprtElem D_dryfield_night_factory_801887CC[32];

extern SpriteBatch D_dryfield_night_factory_80188A4C[7];

extern GpSprtElem D_dryfield_night_factory_80188A84[39];

extern SpriteBatch D_dryfield_night_factory_80188D90[8];

extern GpSprtElem D_dryfield_night_factory_80188DD0[12];

extern SpriteBatch D_dryfield_night_factory_80188EC0[3];

extern GpSprtElem D_dryfield_night_factory_80188ED8[4];

extern SpriteBatch D_dryfield_night_factory_80188F28[3];

extern SpriteBatch D_dryfield_night_factory_80188F40[2];

extern SpriteBatch D_dryfield_night_factory_80188F50[2];

extern SpriteBatch D_dryfield_night_factory_80188F60[2];

extern SpriteBatch D_dryfield_night_factory_80188F70[2];

extern SpriteBatch D_dryfield_night_factory_80188F80[2];

extern GpSprtElem D_dryfield_night_factory_80188F90[78];

extern SpriteBatch D_dryfield_night_factory_801895A8[6];

extern GpSprtElem D_dryfield_night_factory_801895D8[32];

extern SpriteBatch D_dryfield_night_factory_80189858[5];

extern SpriteBatch D_dryfield_night_factory_80189880[2];

extern GpSprtElem D_dryfield_night_factory_80189890[17];

extern SpriteBatch D_dryfield_night_factory_801899E4[6];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_factory_8017F330(Task*);

void func_dryfield_night_factory_8017F4F4(Task*);

void func_dryfield_night_factory_8017F734(Task*);

void func_dryfield_night_factory_8017FE44(Task*);

void func_dryfield_night_factory_8017FE9C(Task*);

void func_dryfield_night_factory_8017FEF4(Task*);

void func_dryfield_night_factory_80180038(Task*);

void func_dryfield_night_factory_8018007C(Task*);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H
