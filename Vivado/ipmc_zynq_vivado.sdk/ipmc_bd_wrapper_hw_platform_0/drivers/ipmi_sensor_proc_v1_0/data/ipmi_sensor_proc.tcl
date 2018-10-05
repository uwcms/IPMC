

proc generate {drv_handle} {
	::hsi::utils::define_include_file $drv_handle "xparameters.h" "ipmi_sensor_proc" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_SENSOR_CNT" "C_SENSOR_DATA_WIDTH"
    ::hsi::utils::define_config_file $drv_handle "ipmi_sensor_proc_g.c" "IPMI_SENSOR_PROC" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_SENSOR_CNT" "C_SENSOR_DATA_WIDTH"
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "ipmi_sensor_proc" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_SENSOR_CNT" "C_SENSOR_DATA_WIDTH"
 }
