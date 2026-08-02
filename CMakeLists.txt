if(CONFIG_ZMK_IIDX_HID)
  zephyr_include_directories(include)
  target_sources(app PRIVATE
    src/iidx_hid.c
    src/behavior_iidx_boot_select.c
    src/behavior_iidx_button.c
    src/behavior_iidx_mode.c
  )
  target_sources_ifdef(CONFIG_ZMK_IIDX_BLE app PRIVATE src/iidx_ble.c)
endif()
