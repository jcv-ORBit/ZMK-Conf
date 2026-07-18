if(CONFIG_ORBIT_INPUT_PROCESSOR_CHORD OR CONFIG_ORBIT_HAPTIC_FEEDBACK)
  zephyr_library()
  zephyr_library_include_directories(include)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_INPUT_PROCESSOR_CHORD
    src/input_processor_chord.c)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_HAPTIC_FEEDBACK
    src/haptic_feedback.c)
endif()
