#ifndef SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
#define SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

typedef struct {
    /* 0x0 */ s16 timer;
    /* 0x2 */ s16 index;
} MistParkingScanState;

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    Task* value;
    u8    retained[4];
} MistParkingStorage532C;
STATIC_ASSERT_SIZEOF(MistParkingStorage532C, 8);

typedef struct {
    /* 0x0 */ u16 timer; // ticks down between companion slots
    /* 0x2 */ s16 slot;  // companion slot 0..4 being walked
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 cmd;   // cap command replayed by state 5
} MistParkingCapState;

extern TaskDesc D_mist_parking_8018D75C[];

extern TaskDesc D_mist_parking_8018FC24[];

extern TaskDesc D_mist_parking_80190824[];

/// Tasks the room keeps a handle on while they run.
extern Task* gRoomCutsceneSoundTask;

extern Task* D_mist_parking_80195320;

extern Task* D_mist_parking_80195324;

extern TmdSource gMistParkingModel09B9C;

extern AnimationSet gMistParkingAnimation09FD4;

extern AnimationSet gMistParkingAnimation0A774;

extern AnimationSet gMistParkingAnimation0AC5C;

extern AnimationSet gMistParkingAnimation0B138;

extern AnimationSet gMistParkingAnimation0B700;

extern AnimationSet gMistParkingAnimation0BAB4;

extern AnimationSet gMistParkingAnimation0C1B0;

extern AnimationSet gMistParkingAnimation0C688;

extern AnimationSet gMistParkingAnimation0CD80;

extern AnimationSet gMistParkingAnimation0D060;

extern AnimationSet gMistParkingAnimation0D3E4;

extern AnimationSet gMistParkingAnimation0D6A4;

extern AnimationSet gMistParkingAnimation0DA94;

extern AnimationSet gMistParkingAnimation0DE94;

extern AnimationSet gMistParkingAnimation0E1D0;

extern AnimationSet gMistParkingAnimation0E6E0;

extern AnimationSet gMistParkingAnimation0EA0C;

extern AnimationSet gMistParkingAnimation0EDE0;

extern AnimationSet gMistParkingAnimation0F14C;

extern AnimationSet gMistParkingAnimation0F574;

extern AnimationSet gMistParkingAnimation0F7F0;

extern AnimationSet gMistParkingAnimation0FC60;

extern AnimationSet gMistParkingAnimation0FE5C;

extern AnimationSet gMistParkingAnimation10174;

extern AnimationBankCopyRequest D_mist_parking_8018D82C;

extern s8 D_mist_parking_8018DA28[28];

extern EvsCommand D_mist_parking_8018DF34[155];

extern EvsCommand D_mist_parking_8018EDBC[23];

extern EvsCommand D_mist_parking_8018EFE4[8];

extern EvsCommand D_mist_parking_8018F0A4[10];

extern EvsCommand D_mist_parking_8018F194[10];

extern WorldCollisionGrid D_mist_parking_8018FCB8;

extern AnimationSet gMistParkingAnimation129F8;

extern AnimationSet gMistParkingAnimation12DCC;

extern AnimationSet gMistParkingAnimation1323C;

extern AnimationBankCopyRequest D_mist_parking_80190870;

extern AnimationPlayRequest D_mist_parking_8019088C;

extern AnimationPlayRequest D_mist_parking_80190BC0;

extern AnimationPlayRequest D_mist_parking_80190C10;

extern AnimationPlayRequest D_mist_parking_80190C38;

extern AnimationPlayRequest D_mist_parking_80190C4C;

extern AnimationPlayRequest D_mist_parking_80190C60;

extern EvsCommand D_mist_parking_80191154[8];

extern EvsCommand D_mist_parking_80191214[10];

extern EvsCommand D_mist_parking_80191304[8];

extern EvsCommand D_mist_parking_801913C4[8];

extern WorldCollisionGrid D_mist_parking_80192204;

extern s32 Shop_Data_80187628;

extern EquipmentWeaponSupply* Shop_Data_8018762C;

extern s32 D_mist_parking_8019531C;

extern RoomCutsceneRec D_mist_parking_8019533C;

extern MistParkingScanState D_mist_parking_80195328;

extern MistParkingStorage532C D_mist_parking_8019532C;

extern MistParkingCapState D_mist_parking_80195334;

/// Resets the caption state and, for 1 or 2, loads that caption file.
void func_mist_parking_80183708(s32 arg0);

/// Drop the handles of room tasks without killing them; the argument their
/// caller passes is unused.
void func_mist_parking_801837A4(s32 arg0);

void func_mist_parking_8018471C(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_mist_parking_801828F0(Task*);

void func_mist_parking_80183634(s32);

void func_mist_parking_80183688(s32);

void func_mist_parking_801836CC(Task*);

void func_mist_parking_80183780(s32);

void func_mist_parking_80183B40(Task*);

#endif // SRC_ROOMS_MIST_PARKING_MIST_PARKING_PRIVATE_H
