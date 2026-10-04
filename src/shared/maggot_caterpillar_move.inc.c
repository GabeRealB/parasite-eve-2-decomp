/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Steps the actor's coordinate, saving the previous translation in
/// `prevPos` first. The horizontal step follows the coordinate's forward
/// axis (`coord.m[*][2]`, a 4.12 direction) by `forwardSpeed` world units;
/// `fallSpeed` is added to the height as it is.
void maggotCaterpillarMoveStep(Task* arg0)
{
    GfxCoord*              coord;
    MaggotCaterpillarWork* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->prevPos.vx = coord->coord.t[0];
    work->prevPos.vy = coord->coord.t[1];
    work->prevPos.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1] += work->fallSpeed;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}
