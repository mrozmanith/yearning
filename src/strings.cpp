#include "strings.h"
#include "asset/lookup.h"
#include <cstring>
#include "data/array.h"
#include "utf8/utf8.h"

namespace VI
{


const char* string_values[Asset::String::count];
Array<const char*> dynamic_string_names;
Array<const char*> dynamic_string_values;

void strings_set(AssetID id, const char* value)
{
	string_values[id] = value;
}

AssetID strings_get(const char* value)
{
	if (!value)
		return AssetNull;

	for (s32 i = 0; i < (s32)Asset::String::count; i++)
	{
		if (utf8cmp(AssetLookup::String::names[i], value) == 0)
			return i;
	}

	for (s32 j = 0; j < dynamic_string_names.length; j++)
	{
		if (utf8cmp(dynamic_string_names[j], value) == 0)
			return (s32)Asset::String::count + j;
	}

	return AssetNull;
}

AssetID strings_add_dynamic(const char* name, const char* value)
{
	dynamic_string_names.add(name);
	dynamic_string_values.add(value);
	return (s32)Asset::String::count + dynamic_string_names.length - 1;
}

const char* _(AssetID id)
{
	if (id < (s32)Asset::String::count)
		return string_values[id];
	else
		return dynamic_string_values[id - (s32)Asset::String::count];
}


};
