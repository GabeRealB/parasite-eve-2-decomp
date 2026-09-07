#include "common.h"

#include "actors/actors_shared_801326f0.h"

void Gp_ClearRec18Occupied(void*);
s32  Gp_GetObjDepth(s32);
s32  Gp_GetObjPan(s32);
void SndEvt_EnqueueType6(s32, s32, s32);
void ActorsShared801326f0_Fn328DC(ActorShared801326f0*);
void ActorsShared801326f0_Fn32B7C(ActorShared801326f0*);
void ActorsShared801326f0_Fn32DA8(ActorShared801326f0*);
void ActorsShared801326f0_Fn32FD4(ActorShared801326f0*);
void ActorsShared801326f0_Fn33370(ActorShared801326f0*);
void ActorsShared801326f0_Fn336E8(ActorShared801326f0*);
void ActorsShared801326f0_Fn33AB4(ActorShared801326f0*);
void ActorsShared801326f0_Fn343E8(ActorShared801326f0*);
void ActorsShared801326f0_Fn345FC(ActorShared801326f0*);
void ActorsShared801326f0_Fn347E0(ActorShared801326f0*);
void ActorsShared801326f0_Fn3537C(ActorShared801326f0*);

s32 ActorsShared801326f0(ActorShared801326f0* arg0)
{
    s16                      state;
    s32                      object;
    s32                      ret;
    u16                      timer;
    ActorShared801326f0Work* soundWork;
    ActorShared801326f0Work* work;

    work  = arg0->field_1C;
    state = work->field_24E;
    ret   = 0;
    /* Each sound block needs separate locals to preserve the call scheduling. */
    switch (state) {
        case 0:
            ActorsShared801326f0_Fn328DC(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 1:
            ActorsShared801326f0_Fn32B7C(arg0);
            return ret;
        case 2:
            ActorsShared801326f0_Fn32DA8(arg0);
            return ret;
        case 3:
            ActorsShared801326f0_Fn32FD4(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 4:
            ActorsShared801326f0_Fn33370(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 5:
            ActorsShared801326f0_Fn336E8(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 6:
            work->field_250 = 0;
            ret             = 1;
            break;
        case 7:
            Gp_ClearRec18Occupied(&work->field_1C4);
            if (work->field_266 != 0) {
                ActorsShared801326f0_Fn3537C(arg0);
            }
            ActorsShared801326f0_Fn343E8(arg0);
            ret = 1;
            break;
        case 8:
            if (work->field_266 != 0) {
                ActorsShared801326f0_Fn3537C(arg0);
            }
            ActorsShared801326f0_Fn345FC(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 9:
            ActorsShared801326f0_Fn345FC(arg0);
            soundWork            = arg0->field_1C;
            object               = arg0->field_2C->field_8;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = arg0->field_20->field_8;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
            }
            return ret;
        case 10:
            ActorsShared801326f0_Fn347E0(arg0);
            if (work->field_250 != 0) {
                soundWork            = arg0->field_1C;
                object               = arg0->field_2C->field_8;
                timer                = soundWork->field_260 + 1;
                soundWork->field_260 = timer;
                if ((s16)timer < 0x10) {
                    return ret;
                }
                soundWork->field_260 = 0;
                {
                    u32 soundId;
                    s32 pan;
                    soundId   = arg0->field_20->field_8;
                    soundId >>= 0xC;
                    soundId <<= 8;
                    soundId  |= 0x40250005;
                    pan       = (s8)Gp_GetObjPan(object);
                    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(object));
                }
            }
            return ret;
        case 11:
            ActorsShared801326f0_Fn33AB4(arg0);
            break;
    }
    return ret;
}
