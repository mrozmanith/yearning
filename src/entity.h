#pragma once

#include "types.h"
#include "array.h"
#include "vi_assert.h"
#include "exec.h"

typedef unsigned int Family;
typedef size_t ID;
typedef unsigned long ComponentMask;
const Family MAX_FAMILIES = sizeof(ComponentMask) * 8;

struct Entity;

struct ComponentBase
{
	ID id;
	Entity* entity;
};

struct Entities;

// If you inherit this, do not add any data
// Create a component instead
struct Entity : public Exec<GameTime>
{
	static Family families;
	ID id;
	ComponentBase* components[MAX_FAMILIES];
	Entity(ID i)
	{
		id = i;
	}
	virtual ~Entity();
	virtual void exec(GameTime);
	template<typename T> T* get()
	{
		return (T*)components[T::family()];
	}
};

template<typename Derived>
struct Component : public ComponentBase
{
	static Family family()
	{
		static Family f = Entity::families++;
		vi_assert(f <= MAX_FAMILIES);
		return f;
	}
};

struct ComponentPoolBase;

typedef void (*ComponentPoolRemove)(ComponentPoolBase*, size_t);

struct ComponentPoolBase
{
	ComponentPoolRemove remove_function;

	// This gets reinterpreted as an ArrayNonRelocating<T> in ComponentPool.
	// Embrace the madness.
	ArrayNonRelocating<char> data;
};

template<typename T>
struct ComponentPool : public ComponentPoolBase
{
	size_t add()
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->add();
	}

	T* get(size_t id)
	{
		return reinterpret_cast<ArrayNonRelocating<T>*>(&data)->get(id);
	}

	static void remove(ComponentPoolBase* pool, size_t id)
	{
		reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data)->get(id)->~T();
		reinterpret_cast<ArrayNonRelocating<T>*>(&pool->data)->remove(id);
	}
};

struct Entities
{
	ArrayNonRelocating<Entity> all;
	Array<ComponentPoolBase> component_pools;

	Entities()
		: all(), component_pools()
	{
		component_pools.reserve(MAX_FAMILIES);
		component_pools.length = MAX_FAMILIES;
	}

	template<typename T> T* create()
	{
		ID id = all.add();
		T* e = (T*)all.get(id);
		new (e) T(this, id);
		return e;
	}

	template<typename T> T* add(Entity* e)
	{
		Family f = T::family();
		ComponentPool<T>* pool = (ComponentPool<T>*)&component_pools.data[f];
		if (!pool->remove_function)
		{
			new (pool) ComponentPool<T>();
			pool->remove_function = &ComponentPool<T>::remove;
		}
		size_t id = pool->add();
		T* t = pool->get(id);
		new(t) T();
		t->id = id;
		t->entity = e;
		e->components[f] = t;
		return t;
	}

	void remove(Entity* e)
	{
		e->~Entity();
		for (int i = 0; i < Entity::families; i++)
		{
			if (e->components[i])
			{
				ComponentPoolBase* pool = &component_pools.data[i];
				pool->remove_function(pool, e->components[i]->id);
			}
		}
		all.remove(e->id);
	}
};
