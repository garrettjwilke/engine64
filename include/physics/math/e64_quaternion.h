#ifndef ENGINE64_QUATERNION_H
#define ENGINE64_QUATERNION_H

#include "physics/math/e64_vector3.h"
#include "physics/math/e64_matrix3.h"

namespace e64 {

typedef struct Quaternion {
	float x, y, z, w;
} Quaternion;


Quaternion quaternion_create(float x, float y, float z, float w);
Quaternion quaternion_identity(void);
Quaternion quaternion_fromAxisAngle(const Vector3 *axis, float radians);

/* Euler angles in radians, in the x, y, z order of RenderTransform.rotation:
   the rotation t3d_mat4_from_srt_euler builds, straight to a quaternion, so
   t3d_mat4_from_srt writes the same matrix. t3d's Euler matrix is the
   transpose of matrix3_setFromEuler's, and this follows t3d. */
Quaternion quaternion_fromEuler(float pitch, float yaw, float roll);
Quaternion quaternion_product(const Quaternion *a, const Quaternion *b);
Quaternion quaternion_normalized(const Quaternion *q);
Quaternion quaternion_nlerp(const Quaternion *a, const Quaternion *b, float t);
Vector3 quaternion_rotateVector(const Quaternion *q, const Vector3 *v);

void quaternion_setAxisAngle(Quaternion *q, const Vector3 *axis, float radians);
void quaternion_toAxisAngle(const Quaternion *q, Vector3 *axis, float *angle);
void quaternion_integrate(Quaternion *q, const Vector3 *omega, float dt);

Matrix3 quaternion_toMatrix3(const Quaternion *q);
Quaternion quaternion_fromMatrix3(const Matrix3 *m);


}

#endif
