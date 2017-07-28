
################################################################
# This is a generated script based on design: amc
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2016.4
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_msg_id "BD_TCL-109" "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source amc_script.tcl


# The design that will be created by this Tcl script contains the following 
# module references:
# iic_to_i2c_master

# Please add the sources of those modules before sourcing this Tcl script.

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xc7z020clg400-1
}


# CHANGE DESIGN NAME HERE
set design_name amc

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_msg_id "BD_TCL-001" "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_msg_id "BD_TCL-002" "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_msg_id "BD_TCL-004" "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_msg_id "BD_TCL-005" "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_msg_id "BD_TCL-114" "ERROR" $errMsg}
   return $nRet
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set I2C [ create_bd_intf_port -mode Master -vlnv uw:uw:i2c_rtl:1.0 I2C ]
  set S00_AXI [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S00_AXI ]
  set_property -dict [ list \
CONFIG.ADDR_WIDTH {32} \
CONFIG.ARUSER_WIDTH {0} \
CONFIG.AWUSER_WIDTH {0} \
CONFIG.BUSER_WIDTH {0} \
CONFIG.DATA_WIDTH {32} \
CONFIG.HAS_BRESP {1} \
CONFIG.HAS_BURST {1} \
CONFIG.HAS_CACHE {1} \
CONFIG.HAS_LOCK {1} \
CONFIG.HAS_PROT {1} \
CONFIG.HAS_QOS {1} \
CONFIG.HAS_REGION {0} \
CONFIG.HAS_RRESP {1} \
CONFIG.HAS_WSTRB {1} \
CONFIG.ID_WIDTH {0} \
CONFIG.MAX_BURST_LENGTH {256} \
CONFIG.NUM_READ_OUTSTANDING {2} \
CONFIG.NUM_READ_THREADS {1} \
CONFIG.NUM_WRITE_OUTSTANDING {2} \
CONFIG.NUM_WRITE_THREADS {1} \
CONFIG.PROTOCOL {AXI4} \
CONFIG.READ_WRITE_MODE {READ_WRITE} \
CONFIG.RUSER_BITS_PER_BYTE {0} \
CONFIG.RUSER_WIDTH {0} \
CONFIG.SUPPORTS_NARROW_BURST {1} \
CONFIG.WUSER_BITS_PER_BYTE {0} \
CONFIG.WUSER_WIDTH {0} \
 ] $S00_AXI
  set TPS2358 [ create_bd_intf_port -mode Master -vlnv uw:uw:tps2358_rtl:1.0 TPS2358 ]

  # Create ports
  set S00_ACLK [ create_bd_port -dir I -type clk S00_ACLK ]
  set S00_ARESETN [ create_bd_port -dir I -type rst S00_ARESETN ]
  set iic2intc_irpt [ create_bd_port -dir O -type intr iic2intc_irpt ]
  set tps2358_fault_irq [ create_bd_port -dir O -from 1 -to 0 -type intr tps2358_fault_irq ]

  # Create instance: axi_iic_0, and set properties
  set axi_iic_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_iic:2.0 axi_iic_0 ]

  # Create instance: axi_interconnect_0, and set properties
  set axi_interconnect_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0 ]

  # Create instance: iic_to_i2c_master_0, and set properties
  set block_name iic_to_i2c_master
  set block_cell_name iic_to_i2c_master_0
  if { [catch {set iic_to_i2c_master_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $iic_to_i2c_master_0 eq "" } {
     catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: tps2358_0, and set properties
  set tps2358_0 [ create_bd_cell -type ip -vlnv user.org:user:tps2358:0.1 tps2358_0 ]

  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_ports S00_AXI] [get_bd_intf_pins axi_interconnect_0/S00_AXI]
  connect_bd_intf_net -intf_net axi_iic_0_IIC [get_bd_intf_pins axi_iic_0/IIC] [get_bd_intf_pins iic_to_i2c_master_0/IIC]
  connect_bd_intf_net -intf_net axi_interconnect_0_M00_AXI [get_bd_intf_pins axi_iic_0/S_AXI] [get_bd_intf_pins axi_interconnect_0/M00_AXI]
  connect_bd_intf_net -intf_net axi_interconnect_0_M01_AXI [get_bd_intf_pins axi_interconnect_0/M01_AXI] [get_bd_intf_pins tps2358_0/S00_AXI]
  connect_bd_intf_net -intf_net iic_to_i2c_master_0_I2C [get_bd_intf_ports I2C] [get_bd_intf_pins iic_to_i2c_master_0/I2C]
  connect_bd_intf_net -intf_net tps2358_0_TPS2358 [get_bd_intf_ports TPS2358] [get_bd_intf_pins tps2358_0/TPS2358]

  # Create port connections
  connect_bd_net -net S00_ACLK_1 [get_bd_ports S00_ACLK] [get_bd_pins axi_iic_0/s_axi_aclk] [get_bd_pins axi_interconnect_0/ACLK] [get_bd_pins axi_interconnect_0/M00_ACLK] [get_bd_pins axi_interconnect_0/M01_ACLK] [get_bd_pins axi_interconnect_0/S00_ACLK] [get_bd_pins tps2358_0/s00_axi_aclk]
  connect_bd_net -net S00_ARESETN_1 [get_bd_ports S00_ARESETN] [get_bd_pins axi_iic_0/s_axi_aresetn] [get_bd_pins axi_interconnect_0/ARESETN] [get_bd_pins axi_interconnect_0/M00_ARESETN] [get_bd_pins axi_interconnect_0/M01_ARESETN] [get_bd_pins axi_interconnect_0/S00_ARESETN] [get_bd_pins tps2358_0/s00_axi_aresetn]
  connect_bd_net -net axi_iic_0_iic2intc_irpt [get_bd_ports iic2intc_irpt] [get_bd_pins axi_iic_0/iic2intc_irpt]
  connect_bd_net -net tps2358_0_tps2358_fault_irq [get_bd_ports tps2358_fault_irq] [get_bd_pins tps2358_0/tps2358_fault_irq]

  # Create address segments
  create_bd_addr_seg -range 0x00010000 -offset 0x40800000 [get_bd_addr_spaces S00_AXI] [get_bd_addr_segs axi_iic_0/S_AXI/Reg] SEG_axi_iic_0_Reg
  create_bd_addr_seg -range 0x00010000 -offset 0x44A00000 [get_bd_addr_spaces S00_AXI] [get_bd_addr_segs tps2358_0/S00_AXI/S00_AXI_reg] SEG_tps2358_0_S00_AXI_reg

  # Perform GUI Layout
  regenerate_bd_layout -layout_string {
   guistr: "# # String gsaved with Nlview 6.6.5b  2016-09-06 bk=1.3687 VDI=39 GEI=35 GUI=JA:1.6
#  -string -flagsOSRD
preplace port TPS2358 -pg 1 -y 180 -defaultsOSRD
preplace port iic2intc_irpt -pg 1 -y 120 -defaultsOSRD
preplace port S00_AXI -pg 1 -y 80 -defaultsOSRD
preplace port I2C -pg 1 -y 70 -defaultsOSRD
preplace port S00_ARESETN -pg 1 -y 30 -defaultsOSRD
preplace port S00_ACLK -pg 1 -y 10 -defaultsOSRD
preplace portBus tps2358_fault_irq -pg 1 -y 200 -defaultsOSRD
preplace inst axi_iic_0 -pg 1 -lvl 2 -y 90 -defaultsOSRD
preplace inst iic_to_i2c_master_0 -pg 1 -lvl 3 -y 70 -defaultsOSRD
preplace inst tps2358_0 -pg 1 -lvl 3 -y 190 -defaultsOSRD
preplace inst axi_interconnect_0 -pg 1 -lvl 1 -y 160 -defaultsOSRD
preplace netloc axi_iic_0_iic2intc_irpt 1 2 2 560J 120 NJ
preplace netloc iic_to_i2c_master_0_I2C 1 3 1 NJ
preplace netloc S00_ACLK_1 1 0 3 20 10 330 10 570
preplace netloc S00_AXI_1 1 0 1 NJ
preplace netloc tps2358_0_tps2358_fault_irq 1 3 1 NJ
preplace netloc axi_interconnect_0_M00_AXI 1 1 1 320
preplace netloc axi_iic_0_IIC 1 2 1 NJ
preplace netloc axi_interconnect_0_M01_AXI 1 1 2 N 170 NJ
preplace netloc tps2358_0_TPS2358 1 3 1 NJ
preplace netloc S00_ARESETN_1 1 0 3 30 20 310 20 550J
levelinfo -pg 1 0 170 440 720 890 -top -10 -bot 290
",
}

  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


