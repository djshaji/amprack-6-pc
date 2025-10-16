#ifndef LILY_H
#define LILY_H
#include <lilv/lilv.h>
#include "json.hpp"
#include "log.h"
#include <string>
#include <cstring>

using json = nlohmann::json;

void generateLV2Info (std::string homedir) ;
json lily_getPluginInfo (const char* plugin_uri) ;
std::string lily_getEffectType (const LilvPlugin* plugin) ;

#endif
