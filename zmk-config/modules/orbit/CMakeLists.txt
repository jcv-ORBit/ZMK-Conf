if(CONFIG_ORBIT_INPUT_PROCESSOR_CHORD OR CONFIG_ORBIT_HAPTIC_FEEDBACK
   OR CONFIG_ORBIT_GLOW OR CONFIG_ORBIT_WAKE)
  zephyr_library()
  zephyr_library_include_directories(include)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_INPUT_PROCESSOR_CHORD
    src/input_processor_chord.c)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_HAPTIC_FEEDBACK
    src/haptic_feedback.c)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_GLOW
    src/glow.c)
  zephyr_library_sources_ifdef(CONFIG_ORBIT_WAKE
    src/wake.c)
  if(CONFIG_ORBIT_GLOW OR CONFIG_ORBIT_WAKE)
    # ZMK event headers (<zmk/...>). ZMK is the *application* (west build
    # -s zmk/app), not a Zephyr module, so its include/ is reached through
    # the application source dir rather than a ZEPHYR_*_MODULE_DIR.
    zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
  endif()
endif()
