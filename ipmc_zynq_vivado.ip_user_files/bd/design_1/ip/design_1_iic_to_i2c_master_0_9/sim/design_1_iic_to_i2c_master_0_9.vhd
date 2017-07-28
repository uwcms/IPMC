-- (c) Copyright 1995-2017 Xilinx, Inc. All rights reserved.
-- 
-- This file contains confidential and proprietary information
-- of Xilinx, Inc. and is protected under U.S. and
-- international copyright and other intellectual property
-- laws.
-- 
-- DISCLAIMER
-- This disclaimer is not a license and does not grant any
-- rights to the materials distributed herewith. Except as
-- otherwise provided in a valid license issued to you by
-- Xilinx, and to the maximum extent permitted by applicable
-- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
-- WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
-- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
-- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
-- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
-- (2) Xilinx shall not be liable (whether in contract or tort,
-- including negligence, or under any other theory of
-- liability) for any loss or damage of any kind or nature
-- related to, arising under or in connection with these
-- materials, including for any direct, or any indirect,
-- special, incidental, or consequential loss or damage
-- (including loss of data, profits, goodwill, or any type of
-- loss or damage suffered as a result of any action brought
-- by a third party) even if such damage or loss was
-- reasonably foreseeable or Xilinx had been advised of the
-- possibility of the same.
-- 
-- CRITICAL APPLICATIONS
-- Xilinx products are not designed or intended to be fail-
-- safe, or for use in any application requiring fail-safe
-- performance, such as life-support or safety devices or
-- systems, Class III medical devices, nuclear facilities,
-- applications related to the deployment of airbags, or any
-- other applications that could lead to death, personal
-- injury, or severe property or environmental damage
-- (individually and collectively, "Critical
-- Applications"). Customer assumes the sole risk and
-- liability of any use of Xilinx products in Critical
-- Applications, subject only to applicable laws and
-- regulations governing limitations on product liability.
-- 
-- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
-- PART OF THIS FILE AT ALL TIMES.
-- 
-- DO NOT MODIFY THIS FILE.

-- IP VLNV: user.org:module_ref:iic_to_i2c_master:1.0
-- IP Revision: 1

LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;

ENTITY design_1_iic_to_i2c_master_0_9 IS
  PORT (
    s_iic_scl_i : OUT STD_LOGIC;
    s_iic_scl_o : IN STD_LOGIC;
    s_iic_scl_t : IN STD_LOGIC;
    s_iic_sda_i : OUT STD_LOGIC;
    s_iic_sda_o : IN STD_LOGIC;
    s_iic_sda_t : IN STD_LOGIC;
    s_i2c_scl_o : OUT STD_LOGIC;
    s_i2c_sda_io : INOUT STD_LOGIC
  );
END design_1_iic_to_i2c_master_0_9;

ARCHITECTURE design_1_iic_to_i2c_master_0_9_arch OF design_1_iic_to_i2c_master_0_9 IS
  ATTRIBUTE DowngradeIPIdentifiedWarnings : STRING;
  ATTRIBUTE DowngradeIPIdentifiedWarnings OF design_1_iic_to_i2c_master_0_9_arch: ARCHITECTURE IS "yes";
  COMPONENT iic_to_i2c_master IS
    PORT (
      s_iic_scl_i : OUT STD_LOGIC;
      s_iic_scl_o : IN STD_LOGIC;
      s_iic_scl_t : IN STD_LOGIC;
      s_iic_sda_i : OUT STD_LOGIC;
      s_iic_sda_o : IN STD_LOGIC;
      s_iic_sda_t : IN STD_LOGIC;
      s_i2c_scl_o : OUT STD_LOGIC;
      s_i2c_sda_io : INOUT STD_LOGIC
    );
  END COMPONENT iic_to_i2c_master;
  ATTRIBUTE X_INTERFACE_INFO : STRING;
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_scl_i: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SCL_I";
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_scl_o: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SCL_O";
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_scl_t: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SCL_T";
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_sda_i: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SDA_I";
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_sda_o: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SDA_O";
  ATTRIBUTE X_INTERFACE_INFO OF s_iic_sda_t: SIGNAL IS "xilinx.com:interface:iic:1.0 IIC SDA_T";
  ATTRIBUTE X_INTERFACE_INFO OF s_i2c_scl_o: SIGNAL IS "uw:uw:i2c:1.0 I2C scl";
  ATTRIBUTE X_INTERFACE_INFO OF s_i2c_sda_io: SIGNAL IS "uw:uw:i2c:1.0 I2C sda";
BEGIN
  U0 : iic_to_i2c_master
    PORT MAP (
      s_iic_scl_i => s_iic_scl_i,
      s_iic_scl_o => s_iic_scl_o,
      s_iic_scl_t => s_iic_scl_t,
      s_iic_sda_i => s_iic_sda_i,
      s_iic_sda_o => s_iic_sda_o,
      s_iic_sda_t => s_iic_sda_t,
      s_i2c_scl_o => s_i2c_scl_o,
      s_i2c_sda_io => s_i2c_sda_io
    );
END design_1_iic_to_i2c_master_0_9_arch;
