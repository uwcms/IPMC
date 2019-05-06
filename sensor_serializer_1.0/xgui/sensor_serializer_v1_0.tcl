
# Loading additional proc with user specified bodies to compute parameter values.
source [file join [file dirname [file dirname [info script]]] gui/sensor_serializer_v1_0.gtcl]

# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "C_DATA_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_PORTS" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_0_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_1_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_2_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_3_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_4_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_5_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_6_CH_COUNT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_SENSOR_7_CH_COUNT" -parent ${Page_0}


}

proc update_PARAM_VALUE.C_SENSOR_0_CH_COUNT { PARAM_VALUE.C_SENSOR_0_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_0_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_0_CH_COUNT ${PARAM_VALUE.C_SENSOR_0_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_0_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_0_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_0_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_0_CH_COUNT { PARAM_VALUE.C_SENSOR_0_CH_COUNT } {
	# Procedure called to validate C_SENSOR_0_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_1_CH_COUNT { PARAM_VALUE.C_SENSOR_1_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_1_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_1_CH_COUNT ${PARAM_VALUE.C_SENSOR_1_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_1_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_1_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_1_CH_COUNT
		set_property value [gen_USERPARAMETER_C_SENSOR_1_CH_COUNT_VALUE] $C_SENSOR_1_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_1_CH_COUNT { PARAM_VALUE.C_SENSOR_1_CH_COUNT } {
	# Procedure called to validate C_SENSOR_1_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_2_CH_COUNT { PARAM_VALUE.C_SENSOR_2_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_2_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_2_CH_COUNT ${PARAM_VALUE.C_SENSOR_2_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_2_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_2_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_2_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_2_CH_COUNT { PARAM_VALUE.C_SENSOR_2_CH_COUNT } {
	# Procedure called to validate C_SENSOR_2_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_3_CH_COUNT { PARAM_VALUE.C_SENSOR_3_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_3_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_3_CH_COUNT ${PARAM_VALUE.C_SENSOR_3_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_3_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_3_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_3_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_3_CH_COUNT { PARAM_VALUE.C_SENSOR_3_CH_COUNT } {
	# Procedure called to validate C_SENSOR_3_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_4_CH_COUNT { PARAM_VALUE.C_SENSOR_4_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_4_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_4_CH_COUNT ${PARAM_VALUE.C_SENSOR_4_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_4_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_4_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_4_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_4_CH_COUNT { PARAM_VALUE.C_SENSOR_4_CH_COUNT } {
	# Procedure called to validate C_SENSOR_4_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_5_CH_COUNT { PARAM_VALUE.C_SENSOR_5_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_5_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_5_CH_COUNT ${PARAM_VALUE.C_SENSOR_5_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_5_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_5_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_5_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_5_CH_COUNT { PARAM_VALUE.C_SENSOR_5_CH_COUNT } {
	# Procedure called to validate C_SENSOR_5_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_6_CH_COUNT { PARAM_VALUE.C_SENSOR_6_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_6_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_6_CH_COUNT ${PARAM_VALUE.C_SENSOR_6_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_6_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_6_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_6_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_6_CH_COUNT { PARAM_VALUE.C_SENSOR_6_CH_COUNT } {
	# Procedure called to validate C_SENSOR_6_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_SENSOR_7_CH_COUNT { PARAM_VALUE.C_SENSOR_7_CH_COUNT PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_7_CH_COUNT when any of the dependent parameters in the arguments change
	
	set C_SENSOR_7_CH_COUNT ${PARAM_VALUE.C_SENSOR_7_CH_COUNT}
	set C_SENSOR_PORTS ${PARAM_VALUE.C_SENSOR_PORTS}
	set values(C_SENSOR_PORTS) [get_property value $C_SENSOR_PORTS]
	if { [gen_USERPARAMETER_C_SENSOR_7_CH_COUNT_ENABLEMENT $values(C_SENSOR_PORTS)] } {
		set_property enabled true $C_SENSOR_7_CH_COUNT
	} else {
		set_property enabled false $C_SENSOR_7_CH_COUNT
	}
}

proc validate_PARAM_VALUE.C_SENSOR_7_CH_COUNT { PARAM_VALUE.C_SENSOR_7_CH_COUNT } {
	# Procedure called to validate C_SENSOR_7_CH_COUNT
	return true
}

proc update_PARAM_VALUE.C_DATA_WIDTH { PARAM_VALUE.C_DATA_WIDTH } {
	# Procedure called to update C_DATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_DATA_WIDTH { PARAM_VALUE.C_DATA_WIDTH } {
	# Procedure called to validate C_DATA_WIDTH
	return true
}

proc update_PARAM_VALUE.C_SENSOR_PORTS { PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to update C_SENSOR_PORTS when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_SENSOR_PORTS { PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to validate C_SENSOR_PORTS
	return true
}


proc update_MODELPARAM_VALUE.C_SENSOR_PORTS { MODELPARAM_VALUE.C_SENSOR_PORTS PARAM_VALUE.C_SENSOR_PORTS } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_PORTS}] ${MODELPARAM_VALUE.C_SENSOR_PORTS}
}

proc update_MODELPARAM_VALUE.C_DATA_WIDTH { MODELPARAM_VALUE.C_DATA_WIDTH PARAM_VALUE.C_DATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_DATA_WIDTH}] ${MODELPARAM_VALUE.C_DATA_WIDTH}
}

proc update_MODELPARAM_VALUE.C_SENSOR_0_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_0_CH_COUNT PARAM_VALUE.C_SENSOR_0_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_0_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_0_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_1_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_1_CH_COUNT PARAM_VALUE.C_SENSOR_1_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_1_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_1_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_2_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_2_CH_COUNT PARAM_VALUE.C_SENSOR_2_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_2_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_2_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_3_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_3_CH_COUNT PARAM_VALUE.C_SENSOR_3_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_3_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_3_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_4_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_4_CH_COUNT PARAM_VALUE.C_SENSOR_4_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_4_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_4_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_5_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_5_CH_COUNT PARAM_VALUE.C_SENSOR_5_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_5_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_5_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_6_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_6_CH_COUNT PARAM_VALUE.C_SENSOR_6_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_6_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_6_CH_COUNT}
}

proc update_MODELPARAM_VALUE.C_SENSOR_7_CH_COUNT { MODELPARAM_VALUE.C_SENSOR_7_CH_COUNT PARAM_VALUE.C_SENSOR_7_CH_COUNT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_SENSOR_7_CH_COUNT}] ${MODELPARAM_VALUE.C_SENSOR_7_CH_COUNT}
}

