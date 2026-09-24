#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"
#include "main/sound.h"

/// The tick's mode dispatcher: runs the handler for the work block's mode
/// `field_24E`. Modes 0, 3, 4, 5, 8 and 9 (and 10 while `field_250` is set)
/// also count `field_260` up and replay the ambient cue from the placement's
/// sound bank every 16 frames. Mode 6 clears `field_250`, mode 7 releases the
/// contact records and runs the descent in `Actor03700_Fn025C8`; both report 1,
/// which tells the caller to skip this frame's movement.
s32 Actor03700_Fn008D0(Task* task)
{
    s16              state;
    GsCOORDINATE2*   object;
    s32              ret;
    u16              timer;
    Actor103700Work* soundWork;
    Actor103700Work* work;

    work  = (Actor103700Work*)task->work;
    state = work->field_24E;
    ret   = 0;
    /* Each sound block needs separate locals to preserve the call scheduling. */
    switch (state) {
        case 0:
            Actor03700_Fn00ABC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 1:
            Actor03700_Fn00D5C(task);
            return ret;
        case 2:
            Actor03700_Fn00F88(task);
            return ret;
        case 3:
            Actor03700_Fn011B4(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 4:
            Actor03700_Fn01550(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 5:
            Actor03700_Fn018C8(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 6:
            work->field_250 = 0;
            ret             = 1;
            break;
        case 7:
            Gp_ClearRec18Occupied(work->records);
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn025C8(task);
            ret = 1;
            break;
        case 8:
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 9:
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = ((TmdObject*)task->extra)->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 10:
            Actor03700_Fn029C0(task);
            if (work->field_250 != 0) {
                soundWork            = (Actor103700Work*)task->work;
                object               = ((TmdObject*)task->extra)->coords;
                timer                = soundWork->field_260 + 1;
                soundWork->field_260 = timer;
                if ((s16)timer < 0x10) {
                    return ret;
                }
                soundWork->field_260 = 0;
                {
                    u32 soundId;
                    s32 pan;
                    soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                    soundId >>= 0xC;
                    soundId <<= 8;
                    soundId  |= 0x40250005;
                    pan       = (s8)Gp_GetObjPan(object);
                    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
                }
            }
            return ret;
        case 11:
            Actor03700_Fn01C94(task);
            break;
    }
    return ret;
}
