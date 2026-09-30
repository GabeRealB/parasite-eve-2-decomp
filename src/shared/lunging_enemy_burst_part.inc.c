/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Spawns the effect burst for the owner's coordinate, hands that coordinate
/// to the pan/depth sound cue, then parks the work block in state 2.
void lungerBurstPartTick(GpEnemy* arg0, Task* arg1)
{
    Task*            owner;
    TmdObject*       obj;
    TmdObject*       ownerObj;
    Actor105600Work* work;
    GfxCoord*        coord;
    s16              state;
    s32              snd;
    s32              pan;

    owner      = arg1->parent;
    obj        = arg1->extra.tmd;
    ownerObj   = owner->extra.tmd;
    work       = owner->work;
    coord      = obj->coords;
    obj->flags = ownerObj->flags;
    state      = work->field_6D2;

    switch (state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        case 1:
            Gp_SpawnEff(0x6005C, coord, 0x10002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
            work->field_6D2 = 2;
            snd             = gLungerBurstCue |
                  ((((GpEnemy*)arg1->spawnArg2.pointer)->placeKey >> 0xC) << 8);
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}
