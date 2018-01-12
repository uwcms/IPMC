

proc generate {drv_handle} {
    ::hsi::utils::define_include_file $drv_handle "xparameters.h" "led_controller" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_LED_INTERFACES"
    ::hsi::utils::define_config_file $drv_handle "led_controller_g.c" "LED_Controller" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_LED_INTERFACES"
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "led_controller" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_LED_INTERFACES"
}
