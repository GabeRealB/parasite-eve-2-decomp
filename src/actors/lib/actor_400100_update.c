#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actor_400100_damage.h"
#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_update.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"
#include <psyq/inline_c.h>

void                         Actor00100_Fn001FC(GsCOORDINATE2*, s16);
void                         Actor00100_Fn01D74(Actor00100*);
s32                          Actor00100_Fn01EEC(Actor00100*, Actor00100Work*);
void                         Gp_UpdateCoord(GsCOORDINATE2*);
void                         func_800B4114(s8*, s32, s16, s32, s32);
extern s8                    Actor00100_D1B6D0;
extern GpEnemyTaskFuncTable4 Actor00100_D001A0;

#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

extern GsCOORDINATE2 Gfx_ViewCoord;
extern s8            D_80114C12;
extern u8            D_80071075;

struct _GpObj20E;
void Gp_ReleaseStateF0Add(struct _GpObj20E* arg0, s32 arg1);
s32  Gp_DispatchMsg(void* arg0, s32 arg1, s32 arg2, s32 arg3);

void Actor00100_Fn0A288(GpEnemy* enemy, Actor00100* actor)
{
    PlayerStatus*        config;
    s32                  excludedState;
    VECTOR               pos;
    Actor00100StateTable states;
    GsCOORDINATE2*       actorcoord;

    SVECTOR**              scratchHead;
    SVECTOR*               scratch;
    s32                    state;
    s16                    modeState;
    s16                    height;
    s16                    modeHeight;
    s16                    initialState;
    s16                    finalState;
    s16                    i;
    GsCOORDINATE2*         playerCoord;
    s32                    sound;
    s32                    sound2;
    s32                    depth;
    s32                    result;
    s32                    action;
    s32                    nextAction;
    s32                    pan2;
    s32                    pan;
    Actor00100AnimCommand* command2;
    Actor00100Work*        actorWork;
    Actor00100*            playerSlot;
    Actor00100Work*        work;
    Actor00100*            player;
    Actor00100AnimCommand* command;
    Actor00100*            slot;
    GsCOORDINATE2*         coord;
    void*                  message;
    void*                  nextMessage;

    work                          = actor->field_1C;
    player                        = Game_GetPtrSlot(3);
    config                        = &Player_Status;
    states                        = Actor00100_D000F0;
    actor->field_2C->field_8->flg = 0;
    Gp_UpdateCoord(actor->field_2C->field_8);
    pos.vx = actor->field_2C->field_8->workm.t[0];
    pos.vy = actor->field_2C->field_8->workm.t[1];
    pos.vz = actor->field_2C->field_8->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->field_2C->field_8->flg = 0;
    switch (D_801153F4) {
        case 0:
            initialState = work->field_0;
            if (initialState != 21 && initialState != 0 && initialState != 6 && initialState != 3) {
                actor->field_2C->field_C = 0;
                height                   = actor->field_2C->field_8->coord.t[1];
                Actor00100_Fn01900(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
            }
            break;
        case 1:
            modeState = work->field_0;
            if ((modeState != 0x15) && (modeState != 0) && (modeState != 6) && (modeState != 3)) {
                actor->field_2C->field_C = 0;
                modeHeight               = actor->field_2C->field_8->coord.t[1];
                Actor00100_Fn01900(actor, 1, 3, 0x12C, (s32)modeHeight, 0xFF);
                Actor00100_Fn01900(actor, 3, 4, 0xC8, (s32)modeHeight, 0xFF);
                Actor00100_Fn01900(actor, 1, 0xB, 0xFA, (s32)modeHeight, 0xFF);
            }
            return;
        case 2:
            actor->field_2C->field_C = 0x80;
            return;
    }
    scratchHead = (SVECTOR**)G_SCRATCH_HEAD;
    scratch     = Actor00100_AllocVector(scratchHead);
    if (work->field_BE0 > 0) {
        work->field_BE0 = (s16)((u16)work->field_BE0 - 1);
    } else if (work->field_0 != 5) {
        Actor00100_Fn0375C(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (s16)(u16)work->field_0;
    excludedState = 21;
    state         = work->field_0;
    if (work->field_C18 == 1) {
        if ((state != excludedState) && (state != 0) && (state != 6) && (state != 3)) {
            actorWork = actor->field_1C;
            slot      = Game_GetPtrSlot(3);
            if ((slot != NULL) && (actorWork->field_8E8 == 7)) {
                playerCoord = slot->field_2C->field_8;
                actorcoord  = actor->field_2C->field_8;
                if (abs(playerCoord->coord.t[1] - actorcoord->coord.t[1]) >= 0x321) {
                    playerCoord->coord.t[1]      = actorcoord->coord.t[1];
                    slot->field_2C->field_8->flg = 0;
                }
            }
        }
        action          = work->field_BFC;
        work->field_C28 = (u16)(work->field_C28 + 1);
        switch (action) {
            case 4:
                break;
            case 1:
                if (work->field_8E8 == 0x38) {
                    Gp_DispatchMsg(player, 0x3FE, (s32)(&work->field_8D8), 0);
                    if ((s16)work->field_C28 == 0xF) {
                        if ((Gp_GetViewIndex() & 0xFF) == 8) {
                            sound = (((u16)enemy->field_8 >> 0xC) << 8) | 0x54010004;
                            pan   = (s8)Gp_GetObjPan((GpObj38*)actor->field_2C->field_8);
                            depth = Gp_GetObjDepth((GpObj38*)actor->field_2C->field_8);
                            SndEvt_EnqueueType6(sound, (s8)pan, (s8)(depth + abs(Gp_GetObjPan((GpObj38*)actor->field_2C->field_8)) / 2));
                        } else {
                            sound2 = (((u16)enemy->field_8 >> 0xC) << 8) | 0x54010004;
                            pan2   = (s8)Gp_GetObjPan((GpObj38*)actor->field_2C->field_8);
                            SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)actor->field_2C->field_8));
                        }
                    }
                    if ((s16)work->field_C28 >= 0xF) {
                        coord = player->field_2C->field_8;
                        if (coord->coord.t[1] < 0x1770) {
                            work->field_8DC = (s32)(work->field_8DC + 0x21);
                        } else {
                            coord->coord.t[1] = 0x1770;
                            work->field_8D8   = 0;
                            work->field_8DC   = 0;
                            work->field_8E0   = 0;
                        }
                    }
                    if (player->field_2C->field_8->coord.t[1] >= 0x1770) {
                        if (config->hp > 0) {
                            for (i = 0; i < 10; i++) {
                                gGameSession->areaBgmCountdown = 0x7F;
                                playerSlot                     = Game_GetPtrSlot(3);
                                if (Gp_DispatchMsg(playerSlot, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 4), 0) == 1)
                                    break;
                            }
                        }
                    }
                } else if (work->field_8D8 == 0) {
                    if (work->field_8E0 != 0) {
                        goto dispatchMotion;
                    }
                } else {
                dispatchMotion:
                    if ((s16)Gp_DispatchMsg(player, 0x3FE, (s32)(&work->field_8D8), 0) != 1) {
                        if ((s16)work->field_C28 >= 0xF) {
                            work->field_8D8 = (s32)((s32)work->field_8D8 >> 1);
                            work->field_8DC = (s32)((s32)work->field_8DC >> 1);
                            work->field_8E0 = (s32)((s32)work->field_8E0 >> 1);
                        }
                    } else if (((*(u32*)&gGameSession->loc & 0xFFFF0000) == 0x04010000) && (work->field_8D8 != 0) && (work->field_8E0 != 0) && ((s16)work->field_C28 < 6)) {
                        if (Actor00100_InRegion(player)) {
                            if (Actor00100_InDirection(player, (VECTOR*)&work->field_8D8)) {
                                work->field_8E8     = 0x38;
                                Gp_StateC08.field_6 = (u8)(Gp_StateC08.field_6 | 1);
                                Gp_PulseState1C();
                                scratch->vx = (u16)work->field_8D8;
                                scratch->vy = 0;
                                scratch->vz = (u16)work->field_8E0;
                                VectorNormalSS(scratch, scratch);
                                gte_lddp(250);
                                gte_ldsv(scratch);
                                __asm__ volatile("nop; nop; .word 0x4B98003D");
                                gte_stsv(scratch);
                                work->field_8D8 = (s32)(s16)scratch->vx;
                                work->field_8DC = 0x3C;
                                work->field_8E0 = (s32)scratch->vz;
                            } else {
                                goto clearMotion;
                            }
                        } else {
                            goto clearMotion;
                        }
                    } else {
                    clearMotion:
                        work->field_8D8 = 0;
                        work->field_8DC = 0;
                        work->field_8E0 = 0;
                    }
                }
                break;
            case 2:
                command = work->field_BF8;
                if (command == &Actor00100_D1B9D0) {
                    if ((config->hp > 0) && ((s16)work->field_C28 >= 0x17)) {
                        message           = &work->field_BF8;
                        command->field_10 = (s32)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9]->field_1C;
                        work->field_BFC   = 4;
                        work->field_C00   = 1;
                        work->field_C04   = 3;
                        Gp_DispatchMsg(player, 0x3FF, (s32)message, 0);
                        work->field_C28 = 0U;
                    }
                } else if ((config->hp > 0) && ((s16)work->field_C28 >= 0x22)) {
                    message                 = &work->field_BF8;
                    Actor00100_D1B9BC.value = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9]->field_1C;
                    work->field_BFC         = 4;
                    work->field_C00         = 1;
                    work->field_C04         = 3;
                    Gp_DispatchMsg(player, 0x3FF, (s32)message, 0);
                    work->field_C28 = 0U;
                }
                break;
            case 3:
                if (((s16)work->field_C28 < 6) && (config->hp > 0) && ((work->field_8D8 != 0) || (work->field_8E0 != 0))) {
                    result = Gp_DispatchMsg(player, 0x3FE, (s32)(&work->field_8D8), 0);
                    if (result == 1) {
                        work->field_8D8 = 0;
                        work->field_8DC = 0;
                        work->field_8E0 = 0;
                        work->field_8EA = (s8)result;
                    }
                }
                break;
            case 5:
                if ((config->hp > 0) && ((s16)work->field_C28 >= 7)) {
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
                    work->field_C18 = 0;
                }
                break;
        }
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            nextAction = work->field_BFC;
            switch (nextAction) {
                case 1:
                    if ((((*(u32*)&gGameSession->loc & 0xFFFF0000) != 0x04010000) || (work->field_8E8 != 0x38)) && (config->hp > 0)) {
                        nextMessage     = &work->field_BF8;
                        work->field_C00 = 0;
                        work->field_C04 = 0;
                        work->field_BFC = 2;
                        Gp_DispatchMsg(player, 0x3FF, (s32)nextMessage, 0);
                        work->field_C28 = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->field_C00 = 1;
                        work->field_C04 = 6;
                        work->field_BFC = 5;
                        command2        = work->field_BF8;
                        if (command2 == &Actor00100_D1B9D0) {
                            command2->field_14 = (s32)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9]->field_24;
                        } else {
                            Actor00100_D1B9C0.value = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9]->field_24;
                        }
                        nextMessage = &work->field_BF8;
                        Gp_DispatchMsg(player, 0x3FF, (s32)nextMessage, 0);
                        work->field_C28 = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
                        work->field_C18 = 0;
                    }
                    break;
            }
        }
    }
    states.fn[work->field_0](actor);
    finalState = work->field_0;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 6) && (finalState != 5) && (finalState != 3)) {
        work->objs[0].flags |= 0x8000;
        work->objs[1].flags |= 0x8000;
    } else {
        work->objs[0].flags &= 0x7FFF;
        work->objs[1].flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied(&work->objs[2].field_20);
    Gp_ClearRec18Occupied(&work->objs[0].field_20);
    Gp_ClearRec18Occupied(&work->objs[1].field_20);
    Gp_ClearRec18Occupied(&work->objs[3].field_38);
    if ((work->field_C2A == 1) && (work->field_C18 == 0)) {
        work->field_C2A = 0;
        Gp_ReleaseStateF0Add(actor, 1);
    }
    if (work->field_C26 > 0) {
        work->field_C26 = (s16)((u16)work->field_C26 - 1);
    }
    scratch->vx = 0;
    scratch->vy = 0;
    scratch->vz = 0;
    Actor00100_TransformToView(actor->field_2C->field_8 + 2, scratch);
    enemy->field_1C.vx     = (s32)(s16)scratch->vx;
    enemy->field_1C.vy     = (s32)scratch->vy;
    enemy->field_1C.vz     = (s32)scratch->vz;
    enemy->field_18        = &Gfx_ViewCoord;
    *(s32*)G_SCRATCH_HEAD += 8;
}
