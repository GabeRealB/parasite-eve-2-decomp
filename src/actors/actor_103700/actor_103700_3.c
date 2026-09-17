#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from; see `func_actor_102600_80135A6C`. */
extern void* D_80067704[1];

extern u8 D_801153F4;

/* The two model streams the death effect picks between, in this overlay's data. */
extern u8 D_actor_103700_8013621C[];
extern u8 D_actor_103700_80136420[];

/// Death handler. Mode 1 of `D_801153F4` only refreshes the actor colour and
/// mode 2 hides the model; otherwise it steps `field_250`: unlink the enemy and
/// play the death cue (releasing the player's hold if `field_262` is set), wait
/// out a short delay, spawn the `field_268` death effect, let the player go,
/// count 60 frames and destroy the enemy.
void func_actor_103700_80133EF4(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    GsCOORDINATE2*   obj;
    Actor103700Work* work;
    Task*            player;
    GpAnimArg        arg;
    /* One 12-byte frame slot serves both the colour vector and the area key. */
    union {
        VECTOR    color;
        GpAreaKey key;
    } buf;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    u8             areaByte0;
    GpAreaRec*     rec;
    GpCdRec10*     entry;
    GpEffWork*     eff;
    TmdObject*     effModel;
    s32            sound;
    s32            sound2;
    s32            idx;
    u32            raw;
    GsCOORDINATE2* coord;

    work   = (Actor103700Work*)task->idMap;
    obj    = ((TmdObject*)task->extra)->field_8;
    model  = (TmdObject*)task->extra;
    player = Game_GetPtrSlot(3);

    switch (D_801153F4) {
        case 1:
            coord        = ((TmdObject*)task->extra)->field_8;
            buf.color.vx = coord->workm.t[0];
            buf.color.vy = coord->workm.t[1];
            buf.color.vz = coord->workm.t[2];
            Gp_UpdateActorColor((GpEnemy*)task->spawnArg2, &buf.color, 0, 0);
            return;
        case 2:
            ((TmdObject*)task->extra)->field_C |= 0x80;
            return;
        case 0:
        default:
            if (work->field_266 != 0) {
                func_actor_103700_8013537C(task);
            }
            switch (work->field_250) {
                case 0:
                    enemy->field_54 = 0;
                    Gp_UnlinkObj(&work->obj);
                    Gp_UnlinkNode(&enemy->node);
                    Gp_ReleaseStateF0Add((GpObj20E*)task, 0x25);
                    model->field_C = 0x80;
                    sound          = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 0x40250003;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)obj), (s8)Gp_GetObjDepth((GpObj38*)obj));
                    if (work->field_262 != 0) {
                        arg.field_0  = D_actor_103700_80139F1C;
                        arg.field_4  = 2;
                        arg.field_8  = 0;
                        arg.field_C  = 0;
                        arg.field_10 = 1;
                        Gp_DispatchMsg(player, 0x3F4, (s32)&arg, 0);
                        sound2 = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 6;
                        SndEvt_EnqueueType6(sound2, (s8)Gp_GetObjPan((GpObj38*)obj), (s8)Gp_GetObjDepth((GpObj38*)obj));
                    }
                    work->field_250 = 1;
                    work->field_258 = (((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) % 6 + 2;
                    break;
                case 1:
                    if ((s16)--work->field_258 > 0) {
                        break;
                    }
                    switch (work->field_268) {
                        case 0:
                            Tmd_FreeBuffers(model);
                            model->field_C |= 4;
                            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                            if ((Gp_LcgState >> 16) & 1) {
                                D_80067704[0] = D_actor_103700_8013621C;
                            } else {
                                D_80067704[0] = D_actor_103700_80136420;
                            }
                            eff = Gp_SpawnEff(0x40007, &((TmdObject*)task->extra)->field_8[4], 0x80, NULL);
                            if (eff != NULL) {
                                sessionKey      = (GpAreaKey*)&Game_Session->field_4;
                                raw             = ((Actor103700Spawn*)task->spawnArg2)->field_8;
                                effModel        = (TmdObject*)eff->field_0->extra;
                                buf.key.field_3 = sessionKey->field_3;
                                buf.key.field_2 = sessionKey->field_2;
                                buf.key.field_1 = sessionKey->field_1;
                                keyPtr          = &buf.key;
                                TOUCH_REG(keyPtr);
                                areaByte0       = Game_Session->field_4;
                                idx             = raw >> 12;
                                buf.key.field_0 = areaByte0;
                                Gp_SyncAreaKeyIndex(keyPtr);
                                rec                = Gp_GetNestedAreaRec(&buf.key);
                                entry              = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
                                effModel->field_24 = entry->field_D;
                                effModel->field_25 = entry->field_E;
                                if (effModel->field_18 != NULL) {
                                    Tmd_ProcessStream(effModel);
                                    Tmd_ProcessStream(effModel);
                                }
                            }
                            break;
                        case 1:
                            Gp_SpawnEff(0x60080, obj, 0x10280, NULL);
                            break;
                        case 2:
                            Gp_SpawnEff(0x60055, obj, 0x10013380, NULL);
                            Gp_SpawnEff(0x60055, obj, 0x10111300, NULL);
                            break;
                    }
                    work->field_250 = 2;
                    break;
                case 2:
                    if (work->field_262 != 0) {
                        if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                            Gp_DispatchMsg(player, 0x3F1, 0, 0);
                            work->field_250 = 3;
                            work->field_256 = 60;
                            work->field_262 = 0;
                        }
                    } else {
                        work->field_250 = 3;
                        work->field_256 = 60;
                    }
                    break;
                case 3:
                    if ((s16)--work->field_256 < 0) {
                        work->field_250 = 4;
                    }
                    break;
                case 4:
                    if (work->field_266 == 0 || work->field_266 == 2) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    break;
            }
            break;
    }
}

/* Closes this unit's .rodata after the jump table above, so actor_103700_7's
   table starts at 0x80131EAC. Nothing reads it. */
const u32 D_actor_103700_80131EA8 = 0;
