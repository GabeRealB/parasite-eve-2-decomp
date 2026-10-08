/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Distributes the look-around yaw across the three spine parts.
///
/// Requires live work and a model with at least six coordinates. Visits parts
/// 5, 4 and 3 in that order, extracts each local Euler rotation and adds one
/// third of signed spineYaw (4096 angle units per turn, division toward zero).
/// Narrows the added yaw back to a halfword and replaces the nine Q12 basis
/// coefficients, retaining translations and alignment bytes. Dirties each part;
/// SDK rotation routines change GTE state.
static void _madChaserTwistSpine(Task* task)
{
    /// Adds one third of look yaw to one joint and replaces its local basis.
    ///
    /// jointCoord is a writable GfxCoord lvalue, jointMatrix a MATRIX pointer
    /// lvalue, angles an SVECTOR lvalue, rotation a MATRIX lvalue and workBlock
    /// a live MadChaserWork pointer. angles and rotation must be separate from
    /// joint/work storage; jointMatrix receives the joint matrix address.
    /// Arguments occur repeatedly and must be stable and free of side effects.
    /// Expands to a compound statement, captures the local joint-count constant,
    /// and is undefined after this function's three calls. Retains translation
    /// and alignment bytes, narrows yaw to s16 and dirties joint composition.
#define MAD_CHASER_TWIST_SPINE_JOINT(jointCoord, jointMatrix, angles, rotation, workBlock)     \
    {                                                                                          \
        gfxSetRotIdentity(&(rotation));                                                        \
        (jointMatrix) = &(jointCoord).coord;                                                   \
        gfxExtractEulerAngles((jointMatrix), &(angles));                                       \
        (angles).vy = (u16)(angles).vy + (workBlock)->spineYaw / MAD_CHASER_SPINE_JOINT_COUNT; \
        RotMatrix(&(angles), &(rotation));                                                     \
        (jointMatrix)->m[0][0]    = (rotation).m[0][0];                                        \
        (jointMatrix)->m[0][1]    = (rotation).m[0][1];                                        \
        (jointMatrix)->m[0][2]    = (rotation).m[0][2];                                        \
        (jointMatrix)->m[1][0]    = (rotation).m[1][0];                                        \
        (jointMatrix)->m[1][1]    = (rotation).m[1][1];                                        \
        (jointMatrix)->m[1][2]    = (rotation).m[1][2];                                        \
        (jointMatrix)->m[2][0]    = (rotation).m[2][0];                                        \
        (jointMatrix)->m[2][1]    = (rotation).m[2][1];                                        \
        (jointMatrix)->m[2][2]    = (rotation).m[2][2];                                        \
        (jointCoord).composeStamp = GRAPHICS_COORD_DIRTY;                                      \
    }
    enum { MAD_CHASER_SPINE_JOINT_COUNT = 3 };

    SVECTOR        eulerAngles;
    MATRIX         rotationMatrix;
    MadChaserWork* work;
    GfxCoord*      coords;
    MATRIX*        part5Matrix;
    MATRIX*        part4Matrix;
    MATRIX*        part3Matrix;

    work   = task->work;
    coords = task->extra.tmd->coords;

    MAD_CHASER_TWIST_SPINE_JOINT(coords[5], part5Matrix, eulerAngles, rotationMatrix, work);
    MAD_CHASER_TWIST_SPINE_JOINT(coords[4], part4Matrix, eulerAngles, rotationMatrix, work);
    MAD_CHASER_TWIST_SPINE_JOINT(coords[3], part3Matrix, eulerAngles, rotationMatrix, work);
#undef MAD_CHASER_TWIST_SPINE_JOINT
}
