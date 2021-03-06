#pragma once

#include "data/entity.h"
#include "lmath.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"

namespace VI
{

struct RigidBody;

struct Walker : public ComponentType<Walker>
{
	Vec2 dir;
	r32 height,
		support_height,
		radius,
		mass,
		speed,
		max_speed,
		rotation,
		target_rotation,
		rotation_speed,
		air_control_accel,
		last_supported_speed,
		accel1,
		accel2,
		accel_threshold,
		deceleration,
		net_speed;
	Ref<RigidBody> support;
	LinkArg<r32> land;
	b8 auto_rotate;
	b8 enabled;
	u32 obstacle_id;
	Walker(r32 = 0.0f);
	~Walker();
	void awake();
	b8 slide(Vec2*, const Vec3&);
	btCollisionWorld::ClosestRayResultCallback check_support(r32 = 0.0f);

	Vec3 base_pos() const;
	void absolute_pos(const Vec3&);
	Vec3 forward() const;
	r32 capsule_height() const;

	void update(const Update&);
};

}
