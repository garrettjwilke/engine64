#include <fmath.h>
#include <math.h>

#include "math/e64_matrix3.h"

namespace e64 {
namespace matrix3 {

Matrix3 create(float a, float b, float c, float d, float e, float f, float g, float h, float i)
{
	return (Matrix3){
		.ex = {a, b, c},
		.ey = {d, e, f},
		.ez = {g, h, i},
	};
}

Matrix3 fromColumns(const Vector3 *ex, const Vector3 *ey, const Vector3 *ez)
{
	return (Matrix3){ .ex = *ex, .ey = *ey, .ez = *ez };
}

Matrix3 zero(void)
{
	return (Matrix3){};
}

Matrix3 diagonal(float x, float y, float z)
{
	return (Matrix3){
		.ex = {x, 0.0f, 0.0f},
		.ey = {0.0f, y, 0.0f},
		.ez = {0.0f, 0.0f, z},
	};
}

Matrix3 fromAxisAngle(const Vector3 *axis, float angle)
{
	float s, c;
	fm_sincosf(angle, &s, &c);
	float x = axis->x, y = axis->y, z = axis->z;
	float xy = x * y, yz = y * z, zx = z * x;
	float t = 1.0f - c;
	return (Matrix3){
		.ex = { x*x*t + c, xy*t + z*s, zx*t - y*s },
		.ey = { xy*t - z*s, y*y*t + c, yz*t + x*s },
		.ez = { zx*t + y*s, yz*t - x*s, z*z*t + c },
	};
}

Matrix3 outerProduct(const Vector3 *u, const Vector3 *v)
{
	return (Matrix3){
		.ex = { v->x * u->x, v->y * u->x, v->z * u->x },
		.ey = { v->x * u->y, v->y * u->y, v->z * u->y },
		.ez = { v->x * u->z, v->y * u->z, v->z * u->z },
	};
}

Matrix3 inverse(const Matrix3 *m)
{
	Vector3 t0 = vector3::cross(&m->ey, &m->ez);
	Vector3 t1 = vector3::cross(&m->ez, &m->ex);
	Vector3 t2 = vector3::cross(&m->ex, &m->ey);
	float detinv = 1.0f / vector3::dot(&m->ez, &t2);
	return (Matrix3){
		.ex = { t0.x * detinv, t1.x * detinv, t2.x * detinv },
		.ey = { t0.y * detinv, t1.y * detinv, t2.y * detinv },
		.ez = { t0.z * detinv, t1.z * detinv, t2.z * detinv },
	};
}

Matrix3 difference(const Matrix3 *a, const Matrix3 *b)
{
	return (Matrix3){
		.ex = vector3::difference(&a->ex, &b->ex),
		.ey = vector3::difference(&a->ey, &b->ey),
		.ez = vector3::difference(&a->ez, &b->ez),
	};
}

void sub(Matrix3 *m, const Matrix3 *n)
{
	vector3::sub(&m->ex, &n->ex);
	vector3::sub(&m->ey, &n->ey);
	vector3::sub(&m->ez, &n->ez);
}

void setRows(Matrix3 *m, const Vector3 *ex, const Vector3 *ey, const Vector3 *ez)
{
	m->ex = *ex;
	m->ey = *ey;
	m->ez = *ez;
}


void setIdentity(Matrix3 *m)
{
	m->ex = (Vector3){1.0f, 0.0f, 0.0f};
	m->ey = (Vector3){0.0f, 1.0f, 0.0f};
	m->ez = (Vector3){0.0f, 0.0f, 1.0f};
}

void setZero(Matrix3 *m)
{
	m->ex = (Vector3){0.0f, 0.0f, 0.0f};
	m->ey = (Vector3){0.0f, 0.0f, 0.0f};
	m->ez = (Vector3){0.0f, 0.0f, 0.0f};
}

void setDiagonal(Matrix3 *m, float x, float y, float z)
{
	m->ex = (Vector3){x, 0.0f, 0.0f};
	m->ey = (Vector3){0.0f, y, 0.0f};
	m->ez = (Vector3){0.0f, 0.0f, z };
}

void setFromEuler(Matrix3 *m, float pitch, float yaw, float roll)
{
	float sp, cp, sy, cy, sr, cr;
	fm_sincosf(pitch, &sp, &cp);
	fm_sincosf(yaw, &sy, &cy);
	fm_sincosf(roll, &sr, &cr);

	m->ex = (Vector3){
		 cy * cr,
		 cy * sr,
		-sy
	};
	m->ey = (Vector3){
		sp * sy * cr - cp * sr,
		sp * sy * sr + cp * cr,
		sp * cy
	};
	m->ez = (Vector3){
		cp * sy * cr + sp * sr,
		cp * sy * sr - sp * cr,
		cp * cy
	};
}

Vector3 toEuler(const Matrix3 *m)
{
	Vector3 euler;
	float sy_p = -m->ex.z;
	float cy_p = sqrtf(m->ex.x * m->ex.x + m->ex.y * m->ex.y);

	euler.y = fm_atan2f(sy_p, cy_p);
	if (cy_p > 1e-4f) {
		euler.x = fm_atan2f(m->ey.z, m->ez.z);
		euler.z = fm_atan2f(m->ex.y, m->ex.x);
	} else {
		euler.x = 0.0f;
		euler.z = fm_atan2f(-m->ey.x, m->ey.y);
	}
	return euler;
}


void transpose(Matrix3 *m)
{
	Matrix3 t = transposed(m);
	*m = t;
}

void scale(Matrix3 *m, float scalar)
{
	vector3::scale(&m->ex, scalar);
	vector3::scale(&m->ey, scalar);
	vector3::scale(&m->ez, scalar);
}

void add(Matrix3 *m, const Matrix3 *n)
{
	vector3::add(&m->ex, &n->ex);
	vector3::add(&m->ey, &n->ey);
	vector3::add(&m->ez, &n->ez);
}

Matrix3 scaled(const Matrix3 *m, float scalar)
{
	return (Matrix3){
		.ex = vector3::scaled(&m->ex, scalar),
		.ey = vector3::scaled(&m->ey, scalar),
		.ez = vector3::scaled(&m->ez, scalar),
	};
}

Matrix3 sum(const Matrix3 *a, const Matrix3 *b)
{
	return (Matrix3){
		.ex = vector3::sum(&a->ex, &b->ex),
		.ey = vector3::sum(&a->ey, &b->ey),
		.ez = vector3::sum(&a->ez, &b->ez),
	};
}

Matrix3 product(const Matrix3 *a, const Matrix3 *b)
{
	return (Matrix3){
		.ex = transformVector(a, &b->ex),
		.ey = transformVector(a, &b->ey),
		.ez = transformVector(a, &b->ez),
	};
}

Vector3 transformVector(const Matrix3 *m, const Vector3 *v)
{
	return (Vector3){
		m->ex.x * v->x + m->ey.x * v->y + m->ez.x * v->z,
		m->ex.y * v->x + m->ey.y * v->y + m->ez.y * v->z,
		m->ex.z * v->x + m->ey.z * v->y + m->ez.z * v->z,
	};
}

Vector3 transformVectorTransposed(const Matrix3 *m, const Vector3 *v)
{
	return (Vector3){
		vector3::dot(&m->ex, v),
		vector3::dot(&m->ey, v),
		vector3::dot(&m->ez, v),
	};
}

}
}
