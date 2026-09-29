#ifndef GAMEPLAY_PLAYER_ACTOR_H
#define GAMEPLAY_PLAYER_ACTOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

struct GfxCoord;

/// Argument for `func_801052B8`. `field_0` is copied onto
/// `GameActor.field_93E`; `field_4` is copied onto `GameActor.field_934`.
typedef struct _GpCountArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
} GpCountArg;
STATIC_ASSERT_SIZEOF(GpCountArg, 8);

/// 2-wide rows indexed by `Mc_SaveData[0].state.characterId`. `Gp_PlayerMode2StateB` passes
/// `D_80112E04[field_22][1]` to `func_80105894`.
extern u8 D_80112E04[][2];

/// u16 table indexed by `Gp_AttachActorObj` arg1: the reach a weapon of that
/// attach id adds to the shape's `end1` to give its `end0`.
extern u16 D_80112F60[];

extern u16 Gp_WeaponIdBase[2];

extern GpAnimBlk* Gp_PlayerAnimBlkTbl[34];

/// Queues sound event `sfx` from the object's world position, panned and
/// depth-attenuated by `Gp_GetObjPan` / `gpGetObjDepth`. A third argument of
/// 1 raises the mid-action bit alongside it; the role of that argument at the
/// call sites is not established.
void Gp_PlayObjSfx(GfxCoord* coord, s32 sfx, s32 arg2);

void Gp_PulseState1C80(void);

void func_800FDB18(s32 arg0, struct GfxCoord* arg1, SVECTOR* arg2, GpEffArg* arg3);

s32 func_801011D0(struct GfxCoord* arg0, WorldCollisionContact* arg1, s32 arg2, s32* arg3);

void Gp_AttachActorObj(Task* arg0, s32 arg1, s32 arg2);

void Gp_AnimPlayChildSlotsEx(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

Task* func_80104258(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

Task* Gp_SpawnWeaponEff(void);

void func_80106350(Task* arg0, s32 arg1, s32 arg2);

/// Message 1006; the fourth dispatch argument is unused.
s32 func_80104E00(Task* arg0, s32 arg1, GpXformArg* arg2, s32 unusedArg3);

s32 Gp_PickNearestRec18(WorldCollisionContact* arg0, struct GfxCoord* arg1, struct GfxCoord* arg2);

s32 func_80105894(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_80106238(Task* arg0, s32 arg1, s32 arg2);

void Gp_AnimResetChildSlots(Task* arg0, s32 arg1);

void func_80106550(Task* arg0);

void Gp_AnimTickChildSlots(Task* arg0);

s32 func_80106264(s32 arg0);

void func_801066DC(Task* arg0, s16 arg1);

s16 func_80103E7C(s16 arg0, s16 arg1);

void Gp_StepPlayerMove(Task* arg0);

s32 Gp_KillPlayerEffs(void);

void Gp_TurnPlayer(Task* arg0);

s32 func_801060E0(Task* arg0);

void func_80103C74(GfxCoord* arg0, VECTOR3* arg1, VECTOR3* arg2);

s32 func_80103D8C(s32 arg0, s32 arg1);

void Gp_AnimPlayChildSlots(Task* arg0, s32 arg1, s32 arg2);

void Gp_TickActorAnimState(Task* arg0);

void Gp_TrackLockTarget(Task* arg0);

Task* func_80104490(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_80106518(s32 arg0);

Task* func_80104364(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_801041B4(Task* arg0);

void Gp_PlayerMode2State4(Task* arg0);

/// Message 1009; the fourth dispatch argument is unused.
s32 Gp_EnterActorMode2(Task* arg0, s32 arg1, s32 arg2, s32 unusedArg3);

void func_80105B74(VECTOR3* arg0);

void Gp_PlayerWorkTask(Task* arg0);

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1);

void Gp_PlaceCoordOffset(GfxCoord* arg0, GfxCoord* arg1, SVECTOR* arg2);

s32 func_80105ED4(Task* arg0);

void Gp_PlayerMode2State0(Task* arg0);

void Gp_PlayerMode2State1(Task* arg0);

void Gp_PlayerMode2State2(Task* arg0);

void Gp_PlayerMode2State6(Task* arg0);

s32 func_80104684(Task* arg0, s32 arg1, s32 arg2);
s32 func_80104D68(Task* arg0, s32 arg1, GpXformArg* arg2);
s32 func_801052B8(Task* arg0, s32 arg1, GpCountArg* arg2);
s32 func_80105828(Task* arg0);
s32 func_8010583C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
s32 func_801058BC(Task* arg0, s32 arg1, s32 arg2);
s32 func_80105A60(Task* arg0, s32 arg1, GfxCoord* arg2);
s32 func_80105AB0(Task* arg0, s32 arg1, s32 arg2);

#endif // GAMEPLAY_PLAYER_ACTOR_H
