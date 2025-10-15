#include "lily.h"

json lily_getLv2Json () {
    IN
    json j = json::array ();

    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);

    const LilvPlugins* list = lilv_world_get_all_plugins(world);

    LILV_FOREACH (plugins, i, list) {
        const LilvPlugin* p = lilv_plugins_get(list, i);
        const char * name = lilv_node_as_string (lilv_plugin_get_name (p));
        const char * uri = lilv_node_as_string (lilv_plugin_get_uri (p));
        const LilvNodes* features = lilv_plugin_get_supported_features (p);
        // const LilvNodes* classes = lilv_plugin_get_supported_classes (p);
        LOGD ("[LV2] %s -> %s\n", name, lilv_node_as_uri(lilv_plugin_get_uri(p)));
    }

    lilv_world_free(world);

    return j ;
    OUT
}