/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Runs the actor's fade sequence off `field_6DA`. States 1 / 3 fade the
/// display object's `field_2C` and the `field_6D8` / `field_6E2` shades up and
/// down, releasing the queued cues as they finish; state 4 fades to 0xB00 and
/// snapshots the root matrix into `field_674`, and state 6 winds `scale.vx` /
/// `scale.vy` down before resetting the root matrix to identity. States 7-9
/// flicker between two LCG-rolled timings, spawning effect 0x600E0 at the
/// fourth part on odd animation frames.
void stalkerCloakFade(Task* arg0)
{
    SVECTOR*         sc;
    Actor402200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GpMtxWords*      m;
    s32              snd;
    s32              pan;
    s32              v;
    s32              w;
    s32              sy;
    s32              y;
    u32              random;
    s16              t;

    sc    = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    coord = obj->coords;
    switch (work->field_6DA) {
        case 0:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_6E2        = -1;
            work->field_49A       &= 0x7FFF;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 1:
            obj->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->field_6DE;
            if (obj->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                t                       = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8         = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                    work->field_6DA = 2;
                    if (work->field_6B8 != 0) {
                        SndEvt_EnqueueType7(work->field_6B8, 1);
                        work->field_6B8 = 0;
                    }
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            break;
        case 2:
            work->field_6E2 = 0x80;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 3:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8          = 0xFF;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->field_6DE;
                if (obj->shading.colorBlend <= 0) {
                    obj->shading.colorBlend = 0;
                    work->field_6DA         = 0;
                    if (work->field_6BC != 0) {
                        SndEvt_EnqueueType7(work->field_6BC, 1);
                        work->field_6BC = 0;
                    }
                    snd = gStalkerFadeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            break;
        case 4:
            obj->shading.colorBlend += 0xB00 / work->field_6DE;
            if (obj->shading.colorBlend >= 0xB00) {
                obj->shading.colorBlend = 0xB00;
                t                       = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8         = t;
                if (t <= 0) {
                    work->field_6DA = 5;
                    work->field_6D8 = 0;
                    work->scale.vx  = 0x1000;
                    work->scale.vy  = 0x1000;
                    work->scale.vz  = 0x1000;
                    work->field_674 = arg0->extra.tmd->coords[0].coord;
                    work->field_6D0 = 0;
                }
            }
            work->field_6E2 = -1;
            break;
        case 5:
            work->field_6E2 = -1;
            break;
        case 6:
            work->field_49A &= 0x7FFF;
            switch (work->field_6D0) {
                case 0:
                    y = work->scale.vy;
                    if (work->field_6E8 != 0) {
                        sy = y - 0x400;
                    } else {
                        sy = y - 0x200;
                    }
                    work->scale.vy = sy;
                    if (sy <= 0x800) {
                        work->field_6D0 = 1;
                    }
                    break;
                case 1:
                    if (work->field_6E8 != 0) {
                        v              = work->scale.vx - 0x200;
                        w              = work->scale.vy + 0x400;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    } else {
                        v              = work->scale.vx - 0x100;
                        w              = work->scale.vy + 0x200;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    }
                    if (work->scale.vx <= 0x800) {
                        work->field_6D0 = 2;
                    }
                    break;
            }
            stalkerApplyScale(arg0);
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8          = 0xFF;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->field_6DE;
                if (obj->shading.colorBlend <= 0) {
                    obj->shading.colorBlend = 0;
                    work->field_6DA         = 0;
                    m                       = (GpMtxWords*)&arg0->extra.tmd->coords[0].coord;
                    m->m00_m01              = 0x1000;
                    m->m02_m10              = 0;
                    m->m11_m12              = 0x1000;
                    m->m20_m21              = 0;
                    m->m22                  = 0x1000;
                    arg0->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
            work->field_6E2 = -1;
            break;
        case 7:
            work->field_6DA = 8;
            work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
            t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->field_6DC = t;
            work->field_6DE = t;
            break;
        case 8:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0x80) {
                work->field_6D8          = 0x80;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->field_6DE;
                if (obj->shading.colorBlend <= 0x800) {
                    obj->shading.colorBlend = 0x800;
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 9;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6EA == 0) {
                work->field_6EA = 1;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
        case 9:
            obj->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->field_6DE;
            if (obj->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                t                       = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8         = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 8;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
