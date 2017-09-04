connect -url tcp:gradl.hep.wisc.edu:3121
source /tmp/IPMC/Vivado/ipmc_zynq_vivado.sdk/ipmc_bd_wrapper_hw_platform_0/ps7_init.tcl
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Platform Cable USB 000011768c1e01"} -index 0
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Platform Cable USB 000011768c1e01" && level==0} -index 1
fpga -file /tmp/IPMC/Vivado/ipmc_zynq_vivado.sdk/ipmc_bd_wrapper_hw_platform_0/ipmc_bd_wrapper.bit
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Platform Cable USB 000011768c1e01"} -index 0
loadhw -hw /tmp/IPMC/Vivado/ipmc_zynq_vivado.sdk/ipmc_bd_wrapper_hw_platform_0/system.hdf -mem-ranges [list {0x40000000 0xbfffffff}]
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Platform Cable USB 000011768c1e01"} -index 0
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Platform Cable USB 000011768c1e01"} -index 0
dow /tmp/IPMC/Vivado/ipmc_zynq_vivado.sdk/freertos_cli_example/Debug/freertos_cli_example.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Platform Cable USB 000011768c1e01"} -index 0
con
