

proc generate {drv_handle} {
    ::hsi::utils::define_include_file $drv_handle "xparameters.h" "ad7689_s" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" 
    ::hsi::utils::define_config_file $drv_handle "ad7689_s_g.c" "AD7689_S" "DEVICE_ID"  "C_S_AXI_BASEADDR" 
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "ad7689_s" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" 
}
