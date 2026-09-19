#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace anxdoid {

bool should_redirect_path(const char* path);
std::string redirect_path(const char* path);

void init_shim();

} // namespace anxdoid
