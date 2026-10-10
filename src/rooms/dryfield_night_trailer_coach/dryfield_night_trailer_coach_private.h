#ifndef SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room.h"

extern ActorTransform D_dryfield_night_trailer_coach_80187988;

/// Equipped-bank player idle request used by the nighttime trailer dialogue scripts.
///
/// Event playback copies the request and replaces its bank seed from the
/// current equipment. It resets clip 1 without blending and requests world
/// collision participation. The request is borrowed only for dispatch; the
/// selected bank must remain loaded throughout playback.
extern const AnimationPlayRequest gDryfieldNightTrailerCoachPlayerIdleAnimation;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_801879F8;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187A0C;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187A34;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187A98;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187AAC;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187AD4;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187B10;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187B24;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187B60;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187B74;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187B88;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187BC4;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187BD8;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187BEC;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187C3C;

extern AnimationPlayRequest D_dryfield_night_trailer_coach_80187CA0;

extern AnimationBankCopyRequest D_dryfield_night_trailer_coach_80187CE4;

extern EvsCommand D_dryfield_night_trailer_coach_80188708[14];

extern EvsCommand D_dryfield_night_trailer_coach_80188858[14];

extern EvsCommand D_dryfield_night_trailer_coach_801889A8[57];

extern EvsCommand D_dryfield_night_trailer_coach_80188F00[16];

extern EvsCommand D_dryfield_night_trailer_coach_80189080[24];

extern EvsCommand D_dryfield_night_trailer_coach_801892C0[13];

extern AreaApplyRec D_dryfield_night_trailer_coach_8018C208[2];

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalogue row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalogue must remain loaded throughout the session.
extern const EquipmentWeaponSupply* gShopRefillSupply;

extern Task* gRoomCutsceneSoundTask;

extern RoomCutsceneRec D_dryfield_night_trailer_coach_8018C21C;

#endif // SRC_ROOMS_DRYFIELD_NIGHT_TRAILER_COACH_DRYFIELD_NIGHT_TRAILER_COACH_PRIVATE_H
