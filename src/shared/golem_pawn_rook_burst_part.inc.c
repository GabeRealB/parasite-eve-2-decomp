/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawns the effect burst for the owner's coordinate, hands that coordinate
/// to the pan/depth sound cue, then parks the work block in state 2.
void golemPawnRookBurstPartTick(Enemy* arg0, Task* arg1)
{
    Task*              owner;
    TmdObject*         obj;
    TmdObject*         ownerObj;
    GolemPawnRookWork* work;
    GfxCoord*          coord;
    s16                state;
    s32                snd;
    s32                pan;

    owner      = arg1->parent;
    obj        = arg1->extra.tmd;
    ownerObj   = owner->extra.tmd;
    work       = owner->work;
    coord      = obj->coords;
    obj->flags = ownerObj->flags;
    state      = work->shieldBreakStep;

    switch (state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        case 1:
            Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x10002600, NULL);
            Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x01002600, NULL);
            Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x01002600, NULL);
            Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x02002600, NULL);
            work->shieldBreakStep = 2;
            snd                   = gGolemPawnRookBurstCue |
                  ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}
