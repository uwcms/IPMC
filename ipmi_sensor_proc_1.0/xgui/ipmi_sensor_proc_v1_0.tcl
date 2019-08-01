# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  set C_SENSOR_CNT [ipgui::add_param $IPINST -name "C_SENSOR_CNT" -parent ${Page_0}]
  set_property tooltip {Total number of input sensors that will be received at its inputs.} ${C_SENSOR_CNT}
  set C_SENSOR_DATA_WIDTH [ipgui::add_param $IPINST -name "C_SENSOR_DATA_WIDTH" -parent ${Page_0}]
  set_property tooltip {Maximum number of data bits each sensor will send out} ${C_SENSOR_DATA_WIDTH}


}

proc update_PARAM_VALUE.C_SENSOR_CNT { PARAM_VALUE.C_SENSOR_CNT } {
	# Procedure called to update C_SENSOR_CNT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_SENSOR_CNT { PARAM_VALUE.C_SENSOR_CNT } {
	# Procedure called to validate C_SENSOR_CNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_DATA_WIDTH { PARAM_VALUE.C_SENSOR_DATA_WIDTH } {
	# Procedure called to update C_SENSOR_DATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_SENSOR_DATA_WIDTH { PARAM_VALUE.C_SENSOR_DATA_WIDTH } {
	# Procedure called to validate C_SENSOR_DATA_WIDTH
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

proc update_MODELPARAM_VALUE.C_SENSOR_CNT { MODELPARAM_VALUE.C_SENSOR_CNT PARAM_VALUE.C_SENSOR_CNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_CNT}] ${MODELPARAM_VALUE.C_SENSOR_CNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_DATA_WIDTH { MODELPARAM_VALUE.C_SENSOR_DATA_WIDTH PARAM_VALUE.C_SENSOR_DATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_DATA_WIDTH}] ${MODELPARAM_VALUE.C_SENSOR_DATA_WIDTH}
}

