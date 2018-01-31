
proc generate {drv_handle} {
    ::hsi::utils::define_include_file $drv_handle "xparameters.h" "pyld_pwr_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_PE_PIN_CNT" "C_PG_PIN_CNT"
    ::hsi::utils::define_config_file $drv_handle "pyld_pwr_ctrl_g.c" "Pyld_Pwr_Ctrl" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_PE_PIN_CNT" "C_PG_PIN_CNT"
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "pyld_pwr_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_PE_PIN_CNT" "C_PG_PIN_CNT"
}
