#pragma once

#include "array.h"
#include "types.h"
#include "lmath.h"
#include <array>

struct cJSON;

namespace VI
{

#define MAX_BONE_WEIGHTS 4

enum RenderDataType
{
	RenderDataType_Float,
	RenderDataType_Vec2,
	RenderDataType_Vec3,
	RenderDataType_Vec4,
	RenderDataType_Int,
	RenderDataType_Mat4,
	RenderDataType_Texture,
};

namespace Json
{
	cJSON* load(const char*);
	void json_free(cJSON*);
	Vec3 get_vec3(cJSON*, const char*, const Vec3& = Vec3::zero);
	Vec4 get_vec4(cJSON*, const char*, const Vec4& = Vec4::zero);
	Quat get_quat(cJSON*, const char*, const Quat& = Quat::identity);
	const int get_int(cJSON*, const char*, const int = 0);
	const char* get_string(cJSON*, const char*, const char* = 0);
};

inline size_t render_data_type_size(RenderDataType type)
{
	switch (type)
	{
		case RenderDataType_Float:
			return sizeof(float);
		case RenderDataType_Vec2:
			return sizeof(Vec2);
		case RenderDataType_Vec3:
			return sizeof(Vec3);
		case RenderDataType_Vec4:
			return sizeof(Vec4);
		case RenderDataType_Int:
			return sizeof(int);
		case RenderDataType_Mat4:
			return sizeof(Mat4);
		case RenderDataType_Texture:
			return sizeof(int);
	}
	vi_assert(false);
	return 0;
}

// Can't have more than X meshes parented to a bone in a .arm file
struct Bone
{
	Quat rot;
	Vec3 pos;
};

struct Armature
{
	Array<int> hierarchy;
	Array<Bone> bind_pose;
	Array<Mat4> inverse_bind_pose;
	Armature()
		: hierarchy(), bind_pose(), inverse_bind_pose()
	{

	}
};

struct Mesh
{
	Array<int> indices;
	Array<Vec3> vertices;
	Array<Vec3> normals;
	Armature armature;
	Vec3 bounds_min;
	Vec3 bounds_max;
	Vec4 color;
	void reset()
	{
		indices.length = 0;
		vertices.length = 0;
		normals.length = 0;
		armature.hierarchy.length = 0;
		armature.bind_pose.length = 0;
		armature.inverse_bind_pose.length = 0;
	}
};

template<typename T>
struct Keyframe
{
	float time;
	T value;
};

struct Channel
{
	int bone_index;
	Array<Keyframe<Vec3> > positions;
	Array<Keyframe<Quat> > rotations;
	Array<Keyframe<Vec3> > scales;
};

struct Animation
{
	float duration;
	Array<Channel> channels;
};

struct Font
{
	struct Character
	{
		char code;
		int index_start;
		int index_count;
		int vertex_start;
		int vertex_count;
		Vec2 min;
		Vec2 max;
	};
	Array<Character> characters;
	Array<int> indices;
	Array<Vec3> vertices;
	Font()
		: characters(), indices(), vertices()
	{
	}
};

}