#ifndef ENGINE64_QUATERNION_H
#define ENGINE64_QUATERNION_H

#include "math/e64_vector3.h"
#include "math/e64_matrix3.h"

namespace e64 {

typedef struct Quaternion {
	float x, y, z, w;
} Quaternion;


namespace quaternion {

Quaternion create(float x, float y, float z, float w);
Quaternion identity(void);
Quaternion fromAxisAngle(const Vector3 *axis, float radians);

/* Euler angles in radians, in the x, y, z order of Render::Transform.rotation:
   the rotation t3d_mat4_from_srt_euler builds, straight to a quaternion, so
   t3d_mat4_from_srt writes the same matrix. t3d's Euler matrix is the
   transpose of matrix3::setFromEuler's, and this follows t3d. */
Quaternion fromEuler(float pitch, float yaw, float roll);
Quaternion product(const Quaternion *a, const Quaternion *b);
Quaternion normalized(const Quaternion *q);
Quaternion nlerp(const Quaternion *a, const Quaternion *b, float t);
Vector3 rotateVector(const Quaternion *q, const Vector3 *v);

void setAxisAngle(Quaternion *q, const Vector3 *axis, float radians);
void toAxisAngle(const Quaternion *q, Vector3 *axis, float *angle);
void integrate(Quaternion *q, const Vector3 *omega, float dt);

Matrix3 toMatrix3(const Quaternion *q);
Quaternion fromMatrix3(const Matrix3 *m);

}

}

#endif
