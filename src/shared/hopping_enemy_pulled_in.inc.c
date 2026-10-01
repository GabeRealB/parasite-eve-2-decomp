/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Death: plays sound 3 unless the enemy's HP is already negative, releases
/// `Gp_StateF0`'s hold if it points at this enemy, unlinks the enemy node and
/// its three hit bodies, moves the task to state 5, tells slot-4 task 0 with
/// message 0x13F4, and hides the model.
void hopperPulledIn(Task* arg0)
{
    Actor341700Work* objs;
    Enemy*           enemy;
    TmdObject*       tmd;
    Actor341700Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (Enemy*)arg0->spawnArg2.pointer;
    tmd             = arg0->extra.tmd;
    work->field_438 = 1;
    if (enemy->hp >= 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    objs        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    hopperEnterState(arg0, 5);
    Gp_DispatchMsg(Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}
