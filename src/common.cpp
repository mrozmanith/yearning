#include "common.h"
#include "render/views.h"
#include "input.h"
#include "console.h"
#include "data/components.h"
#include "data/animator.h"
#include "asset/shader.h"
#include "asset/mesh.h"
#include "asset/armature.h"
#include "render/skinned_model.h"
#include "load.h"
#include "game/game.h"

#include <bullet/src/BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <bullet/src/btBulletDynamicsCommon.h>

namespace VI
{

#define fov_initial (PI * 0.25f)
#define speed_mouse 0.0025f

Empty::Empty()
{
	create<Transform>();
}

Prop::Prop(const AssetID mesh_id, const AssetID armature, const AssetID animation)
{
	Transform* transform = create<Transform>();
	if (armature == AssetNull)
	{
		View* model = create<View>();
		model->mesh = mesh_id;
		model->shader = Asset::Shader::standard;
	}
	else
	{
		SkinnedModel* model = create<SkinnedModel>();
		model->mesh = mesh_id;
		model->shader = Asset::Shader::armature;
		Animator* anim = create<Animator>();
		anim->armature = armature;
		anim->layers[0].animation = animation;
	}
}

StaticGeom::StaticGeom(AssetID mesh_id, const Vec3& absolute_pos, const Quat& absolute_rot, short group, short mask)
{
	Transform* transform = create<Transform>();
	View* model = create<View>();

	model->mesh = mesh_id;
	model->shader = Game::level.mode == Game::Mode::Pvp ? Asset::Shader::culled : Asset::Shader::standard;

	const Mesh* mesh = Loader::mesh(model->mesh);

	get<Transform>()->absolute(absolute_pos, absolute_rot);
	RigidBody* body = create<RigidBody>(RigidBody::Type::Mesh, Vec3::zero, 0.0f, btBroadphaseProxy::StaticFilter | group, ~btBroadphaseProxy::StaticFilter & mask, mesh_id);
}

PhysicsEntity::PhysicsEntity(AssetID mesh, const Vec3& pos, const Quat& quat, RigidBody::Type type, const Vec3& scale, r32 mass, short filter_group, short filter_mask)
{
	Transform* transform = create<Transform>();
	transform->pos = pos;
	transform->rot = quat;

	if (mesh != AssetNull)
	{
		View* model = create<View>();
		model->offset = Mat4::make_scale(scale);
		model->mesh = mesh;
		model->shader = Asset::Shader::standard;
	}
	
	create<RigidBody>(type, scale, mass, filter_group, filter_mask, mesh);
}


}
