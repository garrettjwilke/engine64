#ifndef ENGINE64_MATRIX3_H
#define ENGINE64_MATRIX3_H

#include "math/e64_vector3.h"

namespace e64 {

typedef struct Matrix3 {
	Vector3 ex;
	Vector3 ey;
	Vector3 ez;
} Matrix3;


namespace matrix3 {

/* Nine stores each, inline: as a call the 36-byte result would be written
   once by the callee and copied again by the caller. */

static inline Matrix3 identity(void)
{
	return (Matrix3){
		.ex = {1.0f, 0.0f, 0.0f},
		.ey = {0.0f, 1.0f, 0.0f},
		.ez = {0.0f, 0.0f, 1.0f},
	};
}

static inline Matrix3 transposed(const Matrix3 *m)
{
	return (Matrix3){
		.ex = {m->ex.x, m->ey.x, m->ez.x},
		.ey = {m->ex.y, m->ey.y, m->ez.y},
		.ez = {m->ex.z, m->ey.z, m->ez.z},
	};
}

Matrix3 create(float a, float b, float c, float d, float e, float f, float g, float h, float i);
Matrix3 fromColumns(const Vector3 *ex, const Vector3 *ey, const Vector3 *ez);
Matrix3 zero(void);
Matrix3 diagonal(float x, float y, float z);
Matrix3 fromAxisAngle(const Vector3 *axis, float angle);
Matrix3 outerProduct(const Vector3 *u, const Vector3 *v);
Matrix3 inverse(const Matrix3 *m);
Matrix3 difference(const Matrix3 *a, const Matrix3 *b);

void setIdentity(Matrix3 *m);
void setZero(Matrix3 *m);
void setDiagonal(Matrix3 *m, float x, float y, float z);
void setFromEuler(Matrix3 *m, float pitch, float yaw, float roll);
void setRows(Matrix3 *m, const Vector3 *ex, const Vector3 *ey, const Vector3 *ez);
Vector3 toEuler(const Matrix3 *m);

void transpose(Matrix3 *m);
void scale(Matrix3 *m, float scalar);
void add(Matrix3 *m, const Matrix3 *n);
void sub(Matrix3 *m, const Matrix3 *n);

Matrix3 scaled(const Matrix3 *m, float scalar);
Matrix3 sum(const Matrix3 *a, const Matrix3 *b);
Matrix3 product(const Matrix3 *a, const Matrix3 *b);

Vector3 transformVector(const Matrix3 *m, const Vector3 *v);
Vector3 transformVectorTransposed(const Matrix3 *m, const Vector3 *v);

/* Element access is inline: a couple of loads, smaller than a call. With
   literal indices, as the box-box solver uses them, matrix3::get folds to a
   single load. */

static inline Vector3 column0(const Matrix3 *m) { return (Vector3){m->ex.x, m->ey.x, m->ez.x}; }
static inline Vector3 column1(const Matrix3 *m) { return (Vector3){m->ex.y, m->ey.y, m->ez.y}; }
static inline Vector3 column2(const Matrix3 *m) { return (Vector3){m->ex.z, m->ey.z, m->ez.z}; }

static inline float get(const Matrix3 *m, int i, int j)
{
	const Vector3 *col = (i == 0) ? &m->ex : (i == 1) ? &m->ey : &m->ez;
	return (j == 0) ? col->x : (j == 1) ? col->y : col->z;
}

}

}

#endif
