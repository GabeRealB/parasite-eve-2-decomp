/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Answers message 0x2C00 with low nibble 1 (latched in `command`): shows
/// the model and arms its hit bodies, places the root at the spawn point
/// bits 8..11 pick from the current map's table (0x427 or 0x428, playing
/// sound 6 on 0x427), requests animation 7 with an upward launch, and starts
/// state 1, 4 or 7 by bits 4..7.
void madChaserEmergeAtSpot(Task* arg0)
{
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    TmdObject*     obj   = arg0->extra.tmd;
    Enemy*         enemy = arg0->spawnArg2.pointer;
    GfxCoord*      coord = obj->coords;
    MadChaserWork* w2;
    s32            id;
    s32            pan;
    u32            stageAreaKey;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_EMERGE) {
        work->shadowHidden    = 1;
        work->pairBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        obj->flags           &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if ((arg0->spawnArg1.value & 0xF) != 2) {
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        enemy->node.state.parts.flags = 0;
        stageAreaKey                  = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey == GAME_LOCATION_KEY(4, 39, 0, 0)) {
            // The 7C store follows 7A here; written first, it schedules
            // ahead of the heading load.
            work->rotation.vx = 0;
            work->rotation.vy = (D_shelter_b3_dumping_hole_8018B74C[(work->command >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->rotation.vz = 0;
            coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[(work->command >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[(work->command >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[(work->command >> 8) & 0xF].z;
            id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54270006;
            pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        } else if (stageAreaKey == GAME_LOCATION_KEY(4, 40, 0, 0)) {
            work->rotation.vx = 0;
            work->rotation.vy = (D_shelter_b3_garbage_incinerator_801874C4[(work->command >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->rotation.vz = 0;
            coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[(work->command >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[(work->command >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[(work->command >> 8) & 0xF].z;
        }
        work->moveAccel = 0;
        work->moveSpeed = 100;
        w2              = (MadChaserWork*)arg0->work;
        w2->animRate    = ANIMATION_RATE_ONE;
        w2->animId      = 7;
        w2->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        switch ((work->command >> 4) & 0xF) {
            case 0:
                _madChaserSetBehaviorStateS16(arg0, MAD_CHASER_EMERGE_STATE_BACKFLIP);
                break;
            case 1:
                _madChaserSetBehaviorStateS16(arg0, MAD_CHASER_EMERGE_STATE_ARC_BACK);
                break;
            default:
                _madChaserSetBehaviorStateS16(arg0, MAD_CHASER_EMERGE_STATE_HIGH_ARC);
                break;
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->command = 0;
    }
}
