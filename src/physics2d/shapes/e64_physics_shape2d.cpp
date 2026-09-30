/*
	Narrowphase dispatchers. Each function routes to the concrete shape
	implementation based on shape->type, with the cached shape->world.
*/
#include "physics2d/shapes/e64_physics_shape2d.h"

namespace e64 {
namespace physics2d {
namespace shape2d {

int testPoint(const Shape2D *shape, const Vector2 *p)
{
	switch (shape->type) {
	case Shape2D::SHAPE_CIRCLE: return circle2d::testPoint(&shape->circle, &shape->world, p);
	case Shape2D::SHAPE_RECTANGLE: return rectangle2d::testPoint(&shape->rectangle, &shape->world, p);
	case Shape2D::SHAPE_CAPSULE: return capsule2d::testPoint(&shape->capsule, &shape->world, p);
	case Shape2D::SHAPE_SEGMENT: return segment2d::testPoint(&shape->segment, &shape->world, p);
	}
	return 0;
}


int raycast(const Shape2D *shape, RaycastData2D *raycast)
{
	switch (shape->type) {
	case Shape2D::SHAPE_CIRCLE: return circle2d::raycast(&shape->circle, &shape->world, raycast);
	case Shape2D::SHAPE_RECTANGLE: return rectangle2d::raycast(&shape->rectangle, &shape->world, raycast);
	case Shape2D::SHAPE_CAPSULE: return capsule2d::raycast(&shape->capsule, &shape->world, raycast);
	case Shape2D::SHAPE_SEGMENT: return segment2d::raycast(&shape->segment, &shape->world, raycast);
	}
	return 0;
}


void computeAABB(const Shape2D *shape, AABB2D *aabb)
{
	switch (shape->type) {
	case Shape2D::SHAPE_CIRCLE: circle2d::computeAABB(&shape->circle, &shape->world, aabb); break;
	case Shape2D::SHAPE_RECTANGLE: rectangle2d::computeAABB(&shape->rectangle, &shape->world, aabb); break;
	case Shape2D::SHAPE_CAPSULE: capsule2d::computeAABB(&shape->capsule, &shape->world, aabb); break;
	case Shape2D::SHAPE_SEGMENT: segment2d::computeAABB(&shape->segment, &shape->world, aabb); break;
	}
}

}
}
}
