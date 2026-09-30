/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Answers message 0x2C00 with low nibble 1 (latched in `field_44C`): shows
/// the model and arms its hit bodies, places the root at the spawn point
/// bits 8..11 pick from the current map's table (0x427 or 0x428, playing
/// sound 6 on 0x427), requests animation 7 with an upward launch, and starts
/// state 1, 4 or 7 by bits 4..7.
void hopperEmergeAtSpot(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    TmdObject*       obj   = arg0->extra.tmd;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    GfxCoord*        coord = obj->coords;
    Actor341700Work* w2;
    s32              id;
    s32              pan;
    u32              stageAreaKey;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_2CC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        obj->flags          &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if ((arg0->spawnArg1.value & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        enemy->node.state.parts.flags = 0;
        stageAreaKey                  = GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
        if (stageAreaKey == GAME_LOCATION_KEY(4, 39, 0, 0)) {
            // The 7C store follows 7A here; written first, it schedules
            // ahead of the heading load.
            work->field_78    = 0;
            work->field_7A    = (D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        } else if (stageAreaKey == GAME_LOCATION_KEY(4, 40, 0, 0)) {
            work->field_78    = 0;
            work->field_7A    = (D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[(work->field_44C >> 8) & 0xF].z;
        }
        work->field_428 = 0;
        work->field_42A = 100;
        w2              = (Actor341700Work*)arg0->work;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                hopperSetStateS16(arg0, 1);
                break;
            case 1:
                hopperSetStateS16(arg0, 4);
                break;
            default:
                hopperSetStateS16(arg0, 7);
                break;
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}
