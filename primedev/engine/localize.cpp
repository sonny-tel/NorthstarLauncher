#include <string>
#include <vector>
#include "localize.h"

static void (*EngineClient__Localize)(
	const char* key, const char** args, unsigned int num_args, char* output_buffer, unsigned int outputSize, unsigned int* a6) = nullptr;


std::string Localize(std::string key, ...) {
	va_list args;
	va_start(args, key);

	std::vector<const char*> arg_list;
	auto arg = va_arg(args, const char*);
	while (arg)
	{
		arg_list.push_back(arg);
		arg = va_arg(args, const char*);
	}
	va_end(args);

	char output_buffer[4096];

	EngineClient__Localize(key.c_str(), arg_list.data(), arg_list.size(), output_buffer, sizeof(output_buffer), 0);

	return std::string(output_buffer);

}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", LocalizeEngine, ConCommand, (CModule module)) {
	EngineClient__Localize = module.Offset(0xF8CA0).RCast<decltype(EngineClient__Localize)>();
}
