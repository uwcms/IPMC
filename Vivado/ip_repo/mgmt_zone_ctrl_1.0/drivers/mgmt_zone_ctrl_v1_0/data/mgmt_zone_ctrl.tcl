        
proc generate {drv_handle} {
    ::hsi::utils::define_include_file $drv_handle "xparameters.h" "mgmt_zone_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
    ::hsi::utils::define_config_file $drv_handle "mgmt_zone_ctrl_g.c" "Mgmt_Zone_Ctrl" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "mgmt_zone_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
}
