#include "lilv/lilv.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);

    const char * uri = "http://calf.sourceforge.net/plugins/Compressor";
    const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
    const LilvNode* uri_node = lilv_new_uri(world, uri);
    const LilvPlugin* lilv_plugin = lilv_plugins_get_by_uri(plugins, uri_node);
    if (!lilv_plugin) {
        printf("Plugin not found: %s\n", uri);
        lilv_node_free((LilvNode*)uri_node);
        lilv_world_free(world);
        return 1;
    }
    const char * name = lilv_node_as_string(lilv_plugin_get_name(lilv_plugin));
    printf("Found plugin: %s\n", name);

    /* Use separate input and output buffers; many plugins don't support in-place processing */
    float input_buffer[512] = {0};
    float output_buffer[512] = {0};

    LilvInstance* instance = lilv_plugin_instantiate(lilv_plugin, 48000, NULL);
    if (instance) {
        printf("Successfully instantiated plugin: %s\n", name);
        /* If your Lilv build exposes a function to free instances, call it here (e.g. lilv_instance_free) */

        const uint32_t n_ports = lilv_plugin_get_num_ports(lilv_plugin);
        float* min_values = (float*)calloc(n_ports, sizeof(float));
        float* max_values = (float*)calloc(n_ports, sizeof(float));
        float* def_values = (float*)calloc(n_ports, sizeof(float));

        float * dummy_output_control_port = (float *) malloc (sizeof (float));

        // Get the port ranges using the convenience function
        lilv_plugin_get_port_ranges_float(lilv_plugin, min_values, max_values, def_values);
        LilvNode* lv2_InputPort   = lilv_new_uri(world, LV2_CORE__InputPort);
        LilvNode* lv2_OutputPort  = lilv_new_uri(world, LV2_CORE__OutputPort);
        LilvNode* lv2_AudioPort   = lilv_new_uri(world, LV2_CORE__AudioPort);
        LilvNode* lv2_ControlPort = lilv_new_uri(world, LV2_CORE__ControlPort);

        float inputPort = -1;
        float inputPort2 = -1;
        float outputPort = -1;
        float outputPort2 = -1;

        for (uint32_t i = 0; i < n_ports; ++i) {
            const LilvPort* port = lilv_plugin_get_port_by_index(lilv_plugin, i);
            if (lilv_port_is_a(lilv_plugin, port, lv2_AudioPort)) {
                if (lilv_port_is_a(lilv_plugin, port, lv2_OutputPort)) {
                    //~ LOGD("[%s %d]: found output port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                    if (outputPort == -1)
                        outputPort = i;
                    else if (outputPort2 == -1)
                        outputPort2 = i;
                    else
                        printf("[%s %d]: is third output port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                } else if (lilv_port_is_a(lilv_plugin, port, lv2_InputPort)) {
                    //~ LOGD("[%s %d]: found input port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                    if (inputPort == -1)
                        inputPort = i;
                    else if (inputPort2 == -1)
                        inputPort2 = i;
                    else
                        printf("[%s %d]: is third input port", lilv_node_as_string(lilv_plugin_get_name(lilv_plugin)), i);
                }

                // dummy connect audio ports
                lilv_instance_connect_port(instance, i, dummy_output_control_port);
                continue;            
            }

            if (lilv_port_is_a(lilv_plugin, port, lv2_ControlPort) && !lilv_port_is_a(lilv_plugin, port, lv2_InputPort)) {
                lilv_instance_connect_port(instance, i, dummy_output_control_port);
                continue;
            }

            float *def = (float *) malloc (sizeof(float));
            lilv_instance_connect_port(instance, i, def);
            *def = def_values[i];
        }

        lilv_instance_connect_port(instance, inputPort, input_buffer);
        lilv_instance_connect_port(instance, outputPort, output_buffer);
        lilv_instance_connect_port(instance, inputPort2, input_buffer);
        lilv_instance_connect_port(instance, outputPort2, output_buffer);
        lilv_instance_activate(instance);
        printf("Processing 512 samples...\n");
        lilv_instance_run(instance, 512); // Process 512 samples
        lilv_instance_deactivate(instance);
      } else {
        printf("Failed to instantiate plugin: %s\n", name);
    }

    lilv_node_free((LilvNode*)uri_node);

    lilv_world_free(world);
    return 0;
}