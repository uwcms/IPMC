# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "C_MZ_CNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_HF_CNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_PWREN_CNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_VIO_INCLUDE" -parent ${Page_0}


}

proc update_PARAM_VALUE.C_HF_CNT { PARAM_VALUE.C_HF_CNT } {
	# Procedure called to update C_HF_CNT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_HF_CNT { PARAM_VALUE.C_HF_CNT } {
	# Procedure called to validate C_HF_CNT
	return true
}

proc update_PARAM_VALUE.C_MZ_CNT { PARAM_VALUE.C_MZ_CNT } {
	# Procedure called to update C_MZ_CNT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_MZ_CNT { PARAM_VALUE.C_MZ_CNT } {
	# Procedure called to validate C_MZ_CNT
	return true
}

proc update_PARAM_VALUE.C_PWREN_CNT { PARAM_VALUE.C_PWREN_CNT } {
	# Procedure called to update C_PWREN_CNT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_PWREN_CNT { PARAM_VALUE.C_PWREN_CNT } {
	# Procedure called to validate C_PWREN_CNT
	return true
}

proc update_PARAM_VALUE.C_VIO_INCLUDE { PARAM_VALUE.C_VIO_INCLUDE } {
	# Procedure called to update C_VIO_INCLUDE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_VIO_INCLUDE { PARAM_VALUE.C_VIO_INCLUDE } {
	# Procedure called to validate C_VIO_INCLUDE
	return true
}

proc update_PARAM_VALUE.C_S_AXI_DATA_WIDTH { PARAM_VALUE.C_S_AXI_DATA_WIDTH } {
	# Procedure called to update C_S_AXI_DATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S_AXI_DATA_WIDTH { PARAM_VALUE.C_S_AXI_DATA_WIDTH } {
	# Procedure called to validate C_S_AXI_DATA_WIDTH
	return true
}

proc update_PARAM_VALUE.C_S_AXI_ADDR_WIDTH { PARAM_VALUE.C_S_AXI_ADDR_WIDTH } {
	# Procedure called to update C_S_AXI_ADDR_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S_AXI_ADDR_WIDTH { PARAM_VALUE.C_S_AXI_ADDR_WIDTH } {
	# Procedure called to validate C_S_AXI_ADDR_WIDTH
	return true
}

proc update_PARAM_VALUE.C_S_AXI_BASEADDR { PARAM_VALUE.C_S_AXI_BASEADDR } {
	# Procedure called to update C_S_AXI_BASEADDR when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S_AXI_BASEADDR { PARAM_VALUE.C_S_AXI_BASEADDR } {
	# Procedure called to validate C_S_AXI_BASEADDR
	return true
}

proc update_PARAM_VALUE.C_S_AXI_HIGHADDR { PARAM_VALUE.C_S_AXI_HIGHADDR } {
	# Procedure called to update C_S_AXI_HIGHADDR when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S_AXI_HIGHADDR { PARAM_VALUE.C_S_AXI_HIGHADDR } {
	# Procedure called to validate C_S_AXI_HIGHADDR
	return true
}


proc update_MODELPARAM_VALUE.C_S_AXI_DATA_WIDTH { MODELPARAM_VALUE.C_S_AXI_DATA_WIDTH PARAM_VALUE.C_S_AXI_DATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S_AXI_DATA_WIDTH}] ${MODELPARAM_VALUE.C_S_AXI_DATA_WIDTH}
}

proc update_MODELPARAM_VALUE.C_S_AXI_ADDR_WIDTH { MODELPARAM_VALUE.C_S_AXI_ADDR_WIDTH PARAM_VALUE.C_S_AXI_ADDR_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S_AXI_ADDR_WIDTH}] ${MODELPARAM_VALUE.C_S_AXI_ADDR_WIDTH}
}

proc update_MODELPARAM_VALUE.C_MZ_CNT { MODELPARAM_VALUE.C_MZ_CNT PARAM_VALUE.C_MZ_CNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_MZ_CNT}] ${MODELPARAM_VALUE.C_MZ_CNT}
}

proc update_MODELPARAM_VALUE.C_HF_CNT { MODELPARAM_VALUE.C_HF_CNT PARAM_VALUE.C_HF_CNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_HF_CNT}] ${MODELPARAM_VALUE.C_HF_CNT}
}

proc update_MODELPARAM_VALUE.C_PWREN_CNT { MODELPARAM_VALUE.C_PWREN_CNT PARAM_VALUE.C_PWREN_CNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_PWREN_CNT}] ${MODELPARAM_VALUE.C_PWREN_CNT}
}

proc update_MODELPARAM_VALUE.C_VIO_INCLUDE { MODELPARAM_VALUE.C_VIO_INCLUDE PARAM_VALUE.C_VIO_INCLUDE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_VIO_INCLUDE}] ${MODELPARAM_VALUE.C_VIO_INCLUDE}
}

