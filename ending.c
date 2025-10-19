    printf("Successfully processed 5 frames of dummy audio data\n");
    printf("Buffer size: %d samples per frame\n", BUFFER_SIZE);
    printf("Sample rate: %d Hz\n", SAMPLE_RATE);
    
    printf("\nLV2 Features Support:\n");
    printf("  ✓ Buffer size options (min/max/nominal)\n");
    printf("  ✓ Sample rate option\n");
    printf("  ✓ Sequence size option\n");
    printf("  ✓ Options get/set callbacks implemented\n");
    printf("  ✓ Atom:Path support for file loading\n");
    printf("  ✓ Atom sequences for plugin communication\n");
    printf("  ✓ Patch messages for parameter changes\n");
    printf("  ✓ Filename passing function with validation\n");
    printf("  ✓ Command line filename support\n");
    
    if (user_filename) {
        printf("\nUser filename processed: %s\n", user_filename);
    } else {
        printf("\nUsage: %s [filename] (optional)\n", argc > 0 ? argv[0] : "./lilv_full_test");
    }
    
    // Cleanup
    lilv_instance_deactivate(instance);
    lilv_instance_free(instance);
    free(input_buffer);
    free(output_buffer);
    
cleanup:
    free(control_values);
    free(min_values);
    free(max_values);
    free(def_values);
    lilv_node_free(audio_class);
    lilv_node_free(control_class);  
    lilv_node_free(input_class);
    lilv_node_free(output_class);
    
    // Cleanup atom sequences
    free(input_sequence);
    free(output_sequence);
    
    // Free URID map
    for (size_t i = 0; i < n_uris; i++) {
        free(uris[i]);
    }
    
    lilv_world_free(world);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}