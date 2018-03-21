// Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2017.2 (lin64) Build 1909853 Thu Jun 15 18:39:10 MDT 2017
// Date        : Wed Mar 21 10:57:27 2018
// Host        : moonraker.cern.ch running 64-bit Scientific Linux CERN SLC release 6.9 (Carbon)
// Command     : write_verilog -force -mode synth_stub
//               /data/alex/tmp/ipmc_mgmt_zone_ctrl/IPMC/Vivado/ip_repo/mgmt_zone_ctrl_1.0/src/vio_pwr_en_1/vio_pwr_en_stub.v
// Design      : vio_pwr_en
// Purpose     : Stub declaration of top-level module interface
// Device      : xc7z020clg400-1
// --------------------------------------------------------------------------------

// This empty module with port declaration file causes synthesis tools to infer a black box for IP.
// The synthesis directives are for Synopsys Synplify support to prevent IO buffer insertion.
// Please paste the declaration into a Verilog source file or add the file as an additional source.
(* x_core_info = "vio,Vivado 2017.2" *)
module vio_pwr_en(clk, probe_in0, probe_in1, probe_in2, probe_in3)
/* synthesis syn_black_box black_box_pad_pin="clk,probe_in0[15:0],probe_in1[15:0],probe_in2[15:0],probe_in3[63:0]" */;
  input clk;
  input [15:0]probe_in0;
  input [15:0]probe_in1;
  input [15:0]probe_in2;
  input [63:0]probe_in3;
endmodule
