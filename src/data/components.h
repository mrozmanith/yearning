#pragma once

#include "entity.h"
#include "lmath.h"
#include "LinearMath/btTransform.h"

namespace VI
{

struct Transform : public ComponentType<Transform>
{
	Transform* parent;
	Vec3 pos;
	Quat rot;

	Transform();

	void awake();
	void get_bullet(btTransform&) const;
	void set_bullet(const btTransform&);

	void mat(Mat4*) const;

	Vec3 to_world(const Vec3&) const;
	Vec3 to_local(const Vec3&) const;

	void absolute(Quat*, Vec3*) const;
	void absolute(const Quat&, const Vec3&);
	Vec3 absolute_pos() const;
	void absolute_pos(const Vec3&);
	Quat absolute_rot() const;
	void absolute_rot(const Quat&);
	void reparent(Transform*);
};

}
