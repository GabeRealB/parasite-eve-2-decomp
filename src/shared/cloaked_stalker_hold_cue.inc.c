/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// While field_718 is armed, it queues the vocal cue at tick 0x14. From tick
/// 0x5F on, once the player accepts message 0x3ED, it sends 0x3F1 to release
/// the player and disarms.
void stalkerHoldCueTimer(Task* arg0)
{
    Actor402200Work* work;
    s16              timer;
    s32              sound;
    s32              pan;
    Task*            slot;
    GfxCoord*        coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    slot  = gameGetPtrSlot(3);
    if (work->field_718 != 0) {
        if (work->field_71A == 0x14) {
            sound = gStalkerHoldCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        }
        timer           = (u16)work->field_71A + 1;
        work->field_71A = timer;
        if ((timer >= 0x5F) && (Gp_DispatchMsg(slot, 0x3ED, 0, 0) == 0)) {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
            work->field_718 = 0;
        }
    }
}
