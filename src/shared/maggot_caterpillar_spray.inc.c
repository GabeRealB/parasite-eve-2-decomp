/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Holds the enemy still while its spray clip emits seven puff children.
///
/// Requires initialized work, a body/puff descriptor run covering index 1,
/// and a placement row in 0..7 for the idle-delay table. Sound plays at tick
/// 40; ticks 43..49 each request a puff, counting attempts even on allocation
/// failure. At clip tick 60 plus its blend length, the enemy returns to roam
/// with the row's idle delay plus a random 0..15 ticks.
static void _maggotCaterpillarSprayState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SPRAY_SOUND_FRAME      = 40,
        MAGGOT_CATERPILLAR_SPRAY_FIRST_PUFF_FRAME = 43,
        MAGGOT_CATERPILLAR_SPRAY_PUFF_END_FRAME   = 50,
        MAGGOT_CATERPILLAR_SPRAY_CLIP_FRAMES      = 60,
        MAGGOT_CATERPILLAR_SPRAY_SOUND            = 0x401A0003,
        MAGGOT_CATERPILLAR_PUFF_DESCRIPTOR_INDEX  = 1
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    soundKey;
    s32                    audioPan;
    u32                    randomDelay;

    work               = actor->work;
    coord              = actor->extra.tmd->coords;
    work->forwardSpeed = 0;
    work->turnRate     = 0;
    if (work->animFrame == MAGGOT_CATERPILLAR_SPRAY_SOUND_FRAME) {
        soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_SPRAY_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if ((work->animFrame >= MAGGOT_CATERPILLAR_SPRAY_FIRST_PUFF_FRAME) && (work->animFrame < MAGGOT_CATERPILLAR_SPRAY_PUFF_END_FRAME)) {
        enemySpawnFromTable(work->taskTable, MAGGOT_CATERPILLAR_PUFF_DESCRIPTOR_INDEX, 0, actor->spawnArg2.pointer);
        work->puffCount++;
    }
    if (work->animFrame >= (gMaggotCaterpillarSprayTail + MAGGOT_CATERPILLAR_SPRAY_CLIP_FRAMES)) {
        work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
        work->step         = 0;
        work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
        randomDelay        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
        gRandomLcgState    = randomDelay;
    }
}
