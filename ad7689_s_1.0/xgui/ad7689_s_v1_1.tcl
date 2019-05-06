# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Configurations [ipgui::add_page $IPINST -name "Configurations"]
  set C_SLAVES [ipgui::add_param $IPINST -name "C_SLAVES" -parent ${Configurations}]
  set_property tooltip {Number of ADC slaves attached to the same interface} ${C_SLAVES}
  set C_OUT_INTERFACE [ipgui::add_param $IPINST -name "C_OUT_INTERFACE" -parent ${Configurations} -layout horizontal]
  set_property tooltip {The output type that will be available for use} ${C_OUT_INTERFACE}


}

proc update_PARAM_VALUE.C_CHANNELS { PARAM_VALUE.C_CHANNELS } {
	# Procedure called to update C_CHANNELS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_CHANNELS { PARAM_VALUE.C_CHANNELS } {
	# Procedure called to validate C_CHANNELS
	return true
}

proc update_PARAM_VALUE.C_CHANNELS_OUT { PARAM_VALUE.C_CHANNELS_OUT } {
	# Procedure called to update C_CHANNELS_OUT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_CHANNELS_OUT { PARAM_VALUE.C_CHANNELS_OUT } {
	# Procedure called to validate C_CHANNELS_OUT
	return true
}

proc update_PARAM_VALUE.C_OUT_INTERFACE { PARAM_VALUE.C_OUT_INTERFACE } {
	# Procedure called to update C_OUT_INTERFACE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_OUT_INTERFACE { PARAM_VALUE.C_OUT_INTERFACE } {
	# Procedure called to validate C_OUT_INTERFACE
	return true
}

proc update_PARAM_VALUE.C_SLAVES { PARAM_VALUE.C_SLAVES } {
	# Procedure called to update C_SLAVES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_SLAVES { PARAM_VALUE.C_SLAVES } {
	# Procedure called to validate C_SLAVES
	return true
}

proc update_PARAM_VALUE.C_s_axi_ADDR_WIDTH { PARAM_VALUE.C_s_axi_ADDR_WIDTH } {
	# Procedure called to update C_s_axi_ADDR_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_s_axi_ADDR_WIDTH { PARAM_VALUE.C_s_axi_ADDR_WIDTH } {
	# Procedure called to validate C_s_axi_ADDR_WIDTH
	return true
}

proc update_PARAM_VALUE.C_s_axi_DATA_WIDTH { PARAM_VALUE.C_s_axi_DATA_WIDTH } {
	# Procedure called to update C_s_axi_DATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_s_axi_DATA_WIDTH { PARAM_VALUE.C_s_axi_DATA_WIDTH } {
	# Procedure called to validate C_s_axi_DATA_WIDTH
	return true
}

proc update_PARAM_VALUE.c_bus { PARAM_VALUE.c_bus } {
	# Procedure called to update c_bus when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.c_bus { PARAM_VALUE.c_bus } {
	# Procedure called to validate c_bus
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


proc update_MODELPARAM_VALUE.C_s_axi_DATA_WIDTH { MODELPARAM_VALUE.C_s_axi_DATA_WIDTH PARAM_VALUE.C_s_axi_DATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_s_axi_DATA_WIDTH}] ${MODELPARAM_VALUE.C_s_axi_DATA_WIDTH}
}

proc update_MODELPARAM_VALUE.C_s_axi_ADDR_WIDTH { MODELPARAM_VALUE.C_s_axi_ADDR_WIDTH PARAM_VALUE.C_s_axi_ADDR_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_s_axi_ADDR_WIDTH}] ${MODELPARAM_VALUE.C_s_axi_ADDR_WIDTH}
}

proc update_MODELPARAM_VALUE.C_SLAVES { MODELPARAM_VALUE.C_SLAVES PARAM_VALUE.C_SLAVES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SLAVES}] ${MODELPARAM_VALUE.C_SLAVES}
}

proc update_MODELPARAM_VALUE.C_OUT_INTERFACE { MODELPARAM_VALUE.C_OUT_INTERFACE PARAM_VALUE.C_OUT_INTERFACE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_OUT_INTERFACE}] ${MODELPARAM_VALUE.C_OUT_INTERFACE}
}

proc update_MODELPARAM_VALUE.C_CHANNELS { MODELPARAM_VALUE.C_CHANNELS PARAM_VALUE.C_CHANNELS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_CHANNELS}] ${MODELPARAM_VALUE.C_CHANNELS}
}

proc update_MODELPARAM_VALUE.C_CHANNELS_OUT { MODELPARAM_VALUE.C_CHANNELS_OUT PARAM_VALUE.C_CHANNELS_OUT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_CHANNELS_OUT}] ${MODELPARAM_VALUE.C_CHANNELS_OUT}
}

