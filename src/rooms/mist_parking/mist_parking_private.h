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

/// Timing of one prize's turn in a parking-lot talk's prize announcements,
/// counted down in `prizeTimer`.
enum {
    MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES        = 10, // Idle frames each prize's turn lasts
    MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME = 5   // Frames left when a waiting prize's caption starts
};

/// Progress through the prize announcements of the parking-lot talk that runs
/// in every area variant but 1.
///
/// When the talk's prize prompt chooses the announcements, each
/// shooting-gallery prize gets a turn in course order and the ones waiting
/// here are captioned. It is cleared when the talk starts.
typedef struct {
    s16 prizeTimer; // Frames left on the current prize's turn
    s16 prizeIndex; // Shooting-gallery course whose prize has the turn (0..4; 5 once all have had one)
} MistParkingPrizeAnnouncementState;
STATIC_ASSERT_SIZEOF(MistParkingPrizeAnnouncementState, 4);

/// The room's hold on the task that turns the player's head toward the
/// index-0 placement for this stage and area.
///
/// The second word is zero and unreferenced. Its role is unproven.
typedef struct {
    Task* task;       // Head-aim task, or NULL when the room holds none
    u8    field_4[4]; // Unreferenced zeros; role unproven
} MistParkingHeadAimHandle;
STATIC_ASSERT_SIZEOF(MistParkingHeadAimHandle, 8);

/// Work of the talk at the parking-lot shop while the area is in variant 1.
///
/// The talk greets, announces each shooting-gallery prize waiting here, then
/// offers a menu that can open the shop. It is cleared when the talk starts.
typedef struct {
    s16 prizeTimer;          // Frames left on the current prize (10 each; a waiting prize's caption starts at 5)
    s16 prizeIndex;          // Shooting-gallery course whose prize is being announced (0..4)
    s16 businessDone;        // Nonzero once prizes came up or a menu choice other than leaving ran; picks the farewell
    s16 prizeClosingCommand; // CAP command closing the prize exchange (2 after the announcements, 3 when skipped)
} MistParkingShopTalkState;
STATIC_ASSERT_SIZEOF(MistParkingShopTalkState, 8);

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

extern const EquipmentWeaponSupply* Shop_Data_8018762C;

extern s32 D_mist_parking_8019531C;

extern RoomCutsceneRec D_mist_parking_8019533C;

extern MistParkingPrizeAnnouncementState D_mist_parking_80195328;

extern MistParkingHeadAimHandle D_mist_parking_8019532C;

extern MistParkingShopTalkState D_mist_parking_80195334;

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
