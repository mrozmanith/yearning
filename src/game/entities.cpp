#include "entities.h"
#include "data/animator.h"
#include "render/skinned_model.h"
#include "walker.h"
#include "asset/armature.h"
#include "asset/animation.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "asset/mesh.h"
#include "asset/font.h"
#include "recast/Detour/Include/DetourNavMeshQuery.h"
#include "mersenne/mersenne-twister.h"
#include "game.h"
#include "audio.h"
#include "asset/Wwise_IDs.h"
#include "render/views.h"
#include "awk.h"
#include "bullet/src/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h"
#include "menu.h"
#include "data/ragdoll.h"
#include "usernames.h"
#include "console.h"
#include "minion.h"

namespace VI
{


Entity* VisionCone::create(Transform* parent, r32 fov, r32 range, AI::Team team)
{
	const Vec3 color = AI::colors[(s32)team].xyz();
	Entity* vision_cone = World::create<Empty>();
	vision_cone->get<Transform>()->parent = parent;
	{
		SpotLight* light = vision_cone->add<SpotLight>();
		light->fov = fov;
		light->radius = range;
		light->color = color;
		light->type = SpotLight::Type::Override;
		light->mask = ~(1 << (s32)team); // don't display to fellow teammates
	}

	{
		PointLight* light = vision_cone->add<PointLight>();
		light->type = PointLight::Type::Shockwave;
		light->radius = AWK_SHOCKWAVE_RADIUS;
		light->color = color - Vec3(0.1f, 0.1f, 0.1f);
		light->mask = ~(1 << (s32)team); // don't display to fellow teammates
		light->type = PointLight::Type::Override;
	}

	View* cone_model = vision_cone->add<View>();
	cone_model->alpha(true);
	cone_model->mesh = Asset::Mesh::vision_cone;
	cone_model->texture = Asset::Texture::gradient;
	cone_model->color = Vec4(color, 0.25f);
	cone_model->mask = ~(1 << (s32)team); // don't display to fellow teammates

	r32 width_scale = sinf(fov) * 2.0f * 2.0f;
	Vec3 light_model_scale
	(
		width_scale,
		width_scale,
		cosf(fov) * 2.0f * 2.0f
	);
	cone_model->offset.scale(light_model_scale);
	cone_model->shader = Asset::Shader::flat_texture;

	return vision_cone;
}

AwkEntity::AwkEntity(AI::Team team)
{
	create<Audio>();
	Transform* transform = create<Transform>();
	create<Awk>();
	create<AIAgent>()->team = team;

	Health* health = create<Health>(100);

	SkinnedModel* model = create<SkinnedModel>();
	model->mesh = Asset::Mesh::awk;
	model->shader = Asset::Shader::armature;
	model->color = AI::colors[(s32)team];

	Animator* anim = create<Animator>();
	anim->armature = Asset::Armature::awk;

	create<Target>();

	const r32 radius = 0.5f;

	Vec3 abs_pos;
	Quat abs_quat;
	get<Transform>()->absolute(&abs_pos, &abs_quat);
	create<RigidBody>(RigidBody::Type::Sphere, Vec3(radius), 0.0f, CollisionTarget, CollisionNothing);
}

Health::Health(u16 start_value)
	: hp(start_value), total(start_value)
{
}

void Health::damage(Entity* e, u16 damage)
{
	if (hp > 0 && damage > 0)
	{
		if (damage > hp)
			hp = 0;
		else
			hp -= damage;
		damaged.fire(e);
		if (hp == 0)
			killed.fire(e);
	}
}

CreditsPickupEntity::CreditsPickupEntity(const Vec3& abs_pos, const Quat& abs_quat)
{
	create<Transform>()->absolute(abs_pos, abs_quat);
	View* model = create<View>();
	model->mesh = Asset::Mesh::target;
	model->color = Vec4(1, 1, 0, 0);
	model->shader = Asset::Shader::standard;

	PointLight* light = create<PointLight>();
	light->color = Vec3(1, 1, 0);
	light->radius = 8.0f;

	Target* target = create<Target>();
	create<CreditsPickup>()->link_arg<Entity*, &CreditsPickup::hit_by>(target->hit_by);

	const r32 radius = 0.25f;

	model->offset.scale(Vec3(radius));

	RigidBody* body = create<RigidBody>(RigidBody::Type::Sphere, Vec3(radius), 1.0f, CollisionTarget, btBroadphaseProxy::AllFilter);
	body->set_damping(0.5f, 0.5f);
}

SocketEntity::SocketEntity(const Vec3& abs_pos, const Quat& abs_rot, const b8 permanent_powered)
	: StaticGeom(Asset::Mesh::socket, abs_pos, abs_rot)
{
	create<Socket>(permanent_powered);

	PointLight* light = create<PointLight>();
	light->color = Vec3::zero;
	light->offset = Vec3(-0.5f, 0, 0);
	light->radius = 5.0f;

	create<Audio>();
}

Socket::Socket(const b8 permanent_powered)
	: permanent_powered(permanent_powered), links(), powered(permanent_powered), target()
{
}

void Socket::awake()
{
	if (permanent_powered)
	{
		get<View>()->shader = Asset::Shader::flat;
		get<View>()->color = Vec4(1, 0, 1, 0);
		get<PointLight>()->color = Vec3(1, 0, 1);
	}
	else
	{
		if (powered)
			get<View>()->color = Vec4(1, 1, 1, 0);
		refresh();
	}
}

void Socket::refresh_all()
{
	StaticArray<b8, MAX_ENTITIES> visited(MAX_ENTITIES);
	Array<ID> stack;
	for (auto i = Socket::list.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->permanent_powered)
		{
			if (!visited[i.index])
				stack.add(i.index);
		}
		else
			i.item()->powered = false;
	}

	while (stack.length > 0)
	{
		ID socket_id = stack[stack.length - 1];
		stack.length--;

		if (!visited[socket_id])
		{
			visited[socket_id] = true;
			Socket* socket = &Socket::list[socket_id];
			socket->powered = true;
			for (s32 i = 0; i < socket->links.length; i++)
			{
				Socket* link = socket->links[i].ref();
				ID link_id = link->id();
				if (!visited[link_id])
					stack.add(link_id);
			}
		}
	}

	for (auto i = Socket::list.iterator(); !i.is_last(); i.next())
		i.item()->refresh();
}

void Socket::refresh()
{
	if (target.ref())
	{
		b8 on = permanent_powered || powered;
		Mover* mover;
		if ((mover = target.ref()->get<Mover>()))
			mover->target = on ? 1.0f : 0.0f;
	}

	if (!permanent_powered)
	{
		if (powered)
		{
			if (Game::time.total > 0.0f && get<View>()->color.w > 0.0f) // turning from off to on
				get<Audio>()->post_event(AK::EVENTS::PLAY_SWITCH_ON);
			get<View>()->color = Vec4(1, 1, 1, 0);
			get<PointLight>()->color = Vec3(1);
		}
		else
		{
			get<View>()->color = Vec4(0.5f, 0.5f, 0.5f, 1);
			get<PointLight>()->color = Vec3::zero;
		}
	}
}

void CreditsPickup::hit_by(Entity* hit_by)
{
	World::remove_deferred(entity());
}

PlayerSpawn::PlayerSpawn(AI::Team team)
{
	create<Transform>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn;
	view->shader = Asset::Shader::standard;
	view->color = AI::colors[(s32)team];

	create<RigidBody>(RigidBody::Type::CapsuleY, Vec3(0.8f, 3.75f, 0.8f), 0.0f, CollisionInaccessible, CollisionInaccessibleMask);
}

MinionSpawn::MinionSpawn(AI::Team team)
{
	create<Transform>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn;
	view->shader = Asset::Shader::standard;
	view->color = AI::colors[(s32)team];

	create<RigidBody>(RigidBody::Type::CapsuleY, Vec3(0.8f, 3.75f, 0.8f), 0.0f, CollisionInaccessible, CollisionInaccessibleMask);
}

Turret::Turret(AI::Team team)
{
	create<Transform>();

	View* view = create<View>();
	view->mesh = Asset::Mesh::spawn;
	view->shader = Asset::Shader::standard;
	view->color = AI::colors[(s32)team];
	
	create<TurretControl>();

	create<AIAgent>()->team = team;

	create<Health>(300);

	create<RigidBody>(RigidBody::Type::CapsuleY, Vec3(0.8f, 3.75f, 0.8f), 0.0f, CollisionInaccessible, CollisionInaccessibleMask);
}

void Target::hit(Entity* e)
{
	hit_by.fire(e);
	hit_this.fire(entity());
}

PlayerTrigger::PlayerTrigger()
	: entered(), exited(), triggered(), radius(1.0f)
{

}

b8 PlayerTrigger::is_triggered(const Entity* e) const
{
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (e == triggered[i].ref())
			return true;
	}
	return false;
}

void PlayerTrigger::update(const Update& u)
{
	Vec3 pos = get<Transform>()->absolute_pos();
	r32 radius_squared = radius * radius;
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
		{
			if ((triggered[i].ref()->get<Transform>()->absolute_pos() - pos).length_squared() > radius_squared)
			{
				Entity* e = triggered[i].ref();
				triggered[i] = nullptr;
				exited.fire(e);
			}
		}
	}

	for (auto i = PlayerCommon::list.iterator(); !i.is_last(); i.next())
	{
		Entity* e = i.item()->entity();
		if ((e->get<Transform>()->absolute_pos() - pos).length_squared() < radius_squared)
		{
			b8 already_triggered = false;
			s32 free_slot = -1;
			for (s32 i = 0; i < max_trigger; i++)
			{
				if (free_slot == -1 && !triggered[i].ref())
					free_slot = i;

				if (triggered[i].ref() == e)
				{
					already_triggered = true;
					break;
				}
			}

			if (!already_triggered && free_slot != -1)
			{
				triggered[free_slot] = e;
				entered.fire(e);
			}
		}
	}
}

s32 PlayerTrigger::count() const
{
	s32 count = 0;
	for (s32 i = 0; i < max_trigger; i++)
	{
		if (triggered[i].ref())
			count++;
	}
	return count;
}

RopeEntity::RopeEntity(const Vec3& pos, const Vec3& normal, RigidBody* start, const r32 slack)
{
	create<Rope>(pos, normal, start, slack);
}

#define rope_segment_length 1.0f
#define rope_radius 0.05f

Rope::Rope(const Vec3& pos, const Vec3& normal, RigidBody* start, const r32 slack)
	: last_segment(start),
	last_segment_relative_pos(start->get<Transform>()->to_local(pos + normal * rope_radius)),
	segments(),
	slack(slack),
	socket2()
{
	socket1 = start->has<Socket>() ? start->get<Socket>() : nullptr;
}

Array<Mat4> Rope::instances = Array<Mat4>();

void Rope::draw_opaque(const RenderParams& params)
{
	static Mat4 scale = Mat4::make_scale(Vec3(rope_radius, rope_radius, rope_segment_length * 0.5f));
	Mesh* mesh_data = Loader::mesh_instanced(Asset::Mesh::tri_tube);

	instances.length = 0;

	Vec3 radius = (Vec4(mesh_data->bounds_radius, mesh_data->bounds_radius, mesh_data->bounds_radius, 0)).xyz();
	r32 f_radius = fmax(radius.x, fmax(radius.y, radius.z));
	for (auto i = Rope::list.iterator(); !i.is_last(); i.next())
	{
		Rope* rope = i.item();
		for (s32 j = 0; j < rope->segments.length; j++)
		{
			Mat4 m;
			rope->segments[j].ref()->get<Transform>()->mat(&m);

			if (params.camera->visible_sphere(m.translation(), rope_segment_length * f_radius))
				instances.add(scale * m);
		}
	}

	if (instances.length == 0)
		return;

	Loader::shader(Asset::Shader::standard_instanced);

	RenderSync* sync = params.sync;
	sync->write(RenderOp::Shader);
	sync->write(Asset::Shader::standard_instanced);
	sync->write(params.technique);

	Mat4 vp = params.view_projection;

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::vp);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(vp);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::v);
	sync->write(RenderDataType::Mat4);
	sync->write<s32>(1);
	sync->write<Mat4>(params.view);

	sync->write(RenderOp::Uniform);
	sync->write(Asset::Uniform::diffuse_color);
	sync->write(RenderDataType::Vec4);
	sync->write<s32>(1);
	sync->write<Vec4>(Vec4(1, 1, 1, 0));

	sync->write(RenderOp::Instances);
	sync->write(Asset::Mesh::tri_tube);
	sync->write(instances.length);
	sync->write<Mat4>(instances.data, instances.length);
}

void Rope::segment_hit(Entity* segment_entity)
{
	RigidBody* segment = segment_entity->get<RigidBody>();
	if (segment != last_segment.ref())
	{
		btRigidBody* btBody = segment->btBody;
		for (s32 i = 0; i < segments.length; i++)
		{
			if (segments[i].ref() == segment)
				segments.remove(i);
			else
				segments[i].ref()->btBody->activate(true);
		}
		World::remove_deferred(segment_entity);

		if (socket1.ref() && socket2.ref())
		{
			for (s32 i = 0; i < socket1.ref()->links.length; i++)
			{
				if (socket1.ref()->links[i].ref() == socket2.ref())
				{
					socket1.ref()->links.remove(i);
					break;
				}
			}
			for (s32 i = 0; i < socket2.ref()->links.length; i++)
			{
				if (socket2.ref()->links[i].ref() == socket1.ref())
				{
					socket2.ref()->links.remove(i);
					break;
				}
			}
			socket1 = socket2 = nullptr;
			Socket::refresh_all();
		}

		if (segments.length == 0)
			World::remove_deferred(entity());
	}
}

void Rope::add(const Vec3& pos, const Quat& rot)
{
	Vec3 forward = rot * Vec3(0, 0, 1);
	while (true)
	{
		if (last_segment.ref())
		{
			Vec3 last_segment_pos = last_segment.ref()->get<Transform>()->to_world(last_segment_relative_pos);
			Vec3 diff = pos - last_segment_pos;
			r32 length = diff.dot(forward);
			r32 rope_s32erval = rope_segment_length / (1.0f + slack);
			Vec3 scale = Vec3(rope_radius, rope_radius, rope_segment_length * 0.5f);

			if (length > rope_s32erval * 0.5f)
			{
				Vec3 spawn_pos = last_segment_pos + (diff / length) * rope_s32erval * 0.5f;
				Entity* box = World::create<PhysicsEntity>(AssetNull, spawn_pos, rot, RigidBody::Type::CapsuleZ, Vec3(rope_radius, rope_segment_length - rope_radius * 2.0f, 0.0f), 1.0f, CollisionTarget, ~CollisionTarget);
				link_arg<Entity*, &Rope::segment_hit>(box->add<Target>()->hit_this);

				static Quat rotation_a = Quat::look(Vec3(0, 0, 1)) * Quat::euler(0, PI * -0.5f, 0);
				static Quat rotation_b = Quat::look(Vec3(0, 0, -1)) * Quat::euler(PI, PI * -0.5f, 0);

				RigidBody::Constraint constraint;
				constraint.type = segments.length > 0 ? RigidBody::Constraint::Type::ConeTwist : RigidBody::Constraint::Type::PointToPoint;
				constraint.frame_a = btTransform(rotation_a, last_segment_relative_pos),
				constraint.frame_b = btTransform(rotation_b, Vec3(0, 0, rope_segment_length * -0.5f));
				constraint.limits = Vec3(PI, PI, 0);
				constraint.a = last_segment;
				constraint.b = box->get<RigidBody>();
				RigidBody::add_constraint(constraint);

				box->get<RigidBody>()->set_damping(0.5f, 0.5f);
				segments.add(box->get<RigidBody>());
				last_segment = box->get<RigidBody>();
				last_segment_relative_pos = Vec3(0, 0, rope_segment_length * 0.5f);
			}
			else
				break;
		}
		else
			break;
	}
}

void Rope::end(const Vec3& pos, const Vec3& normal, RigidBody* end)
{
	Vec3 abs_pos = pos + normal * rope_radius;
	add(abs_pos, Quat::look(Vec3::normalize(abs_pos - last_segment.ref()->get<Transform>()->to_world(last_segment_relative_pos))));

	RigidBody::Constraint constraint;
	constraint.type = RigidBody::Constraint::Type::PointToPoint;
	constraint.frame_a = btTransform(Quat::identity, last_segment_relative_pos);
	constraint.frame_b = btTransform(Quat::identity, end->get<Transform>()->to_local(abs_pos));
	constraint.a = last_segment;
	constraint.b = end;
	RigidBody::add_constraint(constraint);

	if (end->has<Socket>())
	{
		socket2 = end->get<Socket>();
		if (socket1.ref() && socket2.ref())
		{
			socket1.ref()->links.add(socket2);
			socket2.ref()->links.add(socket1);
			Socket::refresh_all();
		}
	}

	last_segment = nullptr;
}

void Rope::spawn(const Vec3& pos, const Vec3& dir, const r32 max_distance, const r32 slack)
{
	Vec3 dir_normalized = Vec3::normalize(dir);
	Vec3 start = pos;
	Vec3 end = start + dir_normalized * max_distance;
	btCollisionWorld::ClosestRayResultCallback ray_callback(start, end);
	ray_callback.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces | btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
	ray_callback.m_collisionFilterGroup = ray_callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	Physics::btWorld->rayTest(start, end, ray_callback);
	if (ray_callback.hasHit())
	{
		Vec3 end2 = start + dir_normalized * -max_distance;

		btCollisionWorld::ClosestRayResultCallback ray_callback2(start, end2);
		ray_callback2.m_flags = btTriangleRaycastCallback::EFlags::kF_FilterBackfaces | btTriangleRaycastCallback::EFlags::kF_KeepUnflippedNormal;
		ray_callback2.m_collisionFilterGroup = ray_callback2.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
		Physics::btWorld->rayTest(start, end2, ray_callback2);

		if (ray_callback2.hasHit())
		{
			RigidBody* a = Entity::list[ray_callback.m_collisionObject->getUserIndex()].get<RigidBody>();
			RigidBody* b = Entity::list[ray_callback2.m_collisionObject->getUserIndex()].get<RigidBody>();

			Transform* a_trans = a->get<Transform>();
			Transform* b_trans = b->get<Transform>();
			RopeEntity* rope_entity = World::create<RopeEntity>(ray_callback.m_hitPointWorld, ray_callback.m_hitNormalWorld, a, slack);
			rope_entity->get<Rope>()->end(ray_callback2.m_hitPointWorld, ray_callback2.m_hitNormalWorld, b);
		}
	}
}

MoverEntity::MoverEntity(const b8 reversed, const b8 trans, const b8 rot)
{
	create<Mover>(reversed, trans, rot);
}

Mover::Mover(const b8 reversed, const b8 trans, const b8 rot)
	: reversed(reversed), x(), translation(trans), rotation(rot), target(reversed ? 1.0f : 0.0f), speed(), object(), start_pos(), start_rot(), end_pos(), end_rot(), last_moving(), ease()
{
}

void Mover::update(const Update& u)
{
	if (object.ref())
	{
		r32 actual_target = reversed ? 1.0f - target : target;
		b8 moving;
		if (x == actual_target)
		{
			RigidBody* body = object.ref()->get<RigidBody>();
			if (body)
				body->btBody->setActivationState(ISLAND_SLEEPING);

			moving = false;
		}
		else
		{
			if (x < actual_target)
				x = fmin(actual_target, x + speed * u.time.delta);
			else
				x = fmax(actual_target, x - speed * u.time.delta);
			refresh();

			moving = true;
		}

		if (!object.ref()->has<Audio>())
			object.ref()->entity()->add<Audio>();
		if (moving && !last_moving)
			object.ref()->get<Audio>()->post_event(AK::EVENTS::MOVER_LOOP);
		else if (!moving && last_moving)
			object.ref()->get<Audio>()->post_event(AK::EVENTS::MOVER_STOP);
		last_moving = moving;
	}
	else
		World::remove(entity());
}

void Mover::setup(Transform* obj, Transform* end, const r32 _speed)
{
	object = obj;
	obj->absolute(&start_pos, &start_rot);
	end->absolute(&end_pos, &end_rot);
	if (translation)
		speed = _speed / (end_pos - start_pos).length();
	else
		speed = _speed / Quat::angle(start_rot, end_rot);
	refresh();
}

void Mover::refresh()
{
	if (object.ref())
	{
		r32 eased = Ease::ease(ease, x, 0.0f, 1.0f);
		if (translation && rotation)
			object.ref()->absolute(Vec3::lerp(eased, start_pos, end_pos), Quat::slerp(eased, start_rot, end_rot));
		else
		{
			if (rotation)
				object.ref()->absolute_rot(Quat::slerp(eased, start_rot, end_rot));
			if (translation)
				object.ref()->absolute_pos(Vec3::lerp(eased, start_pos, end_pos));
		}
		RigidBody* body = object.ref()->get<RigidBody>();
		if (body)
			body->btBody->activate(true);
	}
	else
		World::remove(entity());
}

ShockwaveEntity::ShockwaveEntity(Entity* owner, r32 max_radius)
{
	create<Transform>();

	PointLight* light = create<PointLight>();
	light->radius = 0.0f;
	light->type = PointLight::Type::Shockwave;

	create<Shockwave>(owner, max_radius);
}

Shockwave::Shockwave(Entity* owner, r32 max_radius)
	: timer(), owner(owner), max_radius(max_radius)
{
}

#define shockwave_duration 2.0f
#define shockwave_duration_multiplier (1.0f / shockwave_duration)

r32 Shockwave::radius() const
{
	return get<PointLight>()->radius;
}

r32 Shockwave::duration() const
{
	return max_radius * (2.0f / 15.0f);
}

void Shockwave::update(const Update& u)
{
	PointLight* light = get<PointLight>();
	timer += u.time.delta;
	r32 d = duration();
	if (timer > d)
		World::remove(entity());
	else
	{
		r32 fade_radius = max_radius * (2.0f / 15.0f);
		light->radius = Ease::cubic_out(timer * (1.0f / d), 0.0f, max_radius);
		r32 fade = 1.0f - fmax(0, ((light->radius - (max_radius - fade_radius)) / fade_radius));
		light->color = Vec3(fade);
	}
}


}
