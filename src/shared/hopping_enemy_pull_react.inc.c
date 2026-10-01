/* Part of the hopping enemy library; see hopping_enemy.h. */

/// In the flat stance requests animation 9, plays sound 2 and skips to the limp
/// drag; otherwise requests animation 7, seeds the speed ramp and advances to
/// the struggle.
void hopperPullReact(Task* arg0)
{
    Actor341700Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor341700Work*)arg0->work;
    if (gHopperAnimStance[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}
