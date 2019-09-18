-- IIC master to I2C master
-- 

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

Library UNISIM;
use UNISIM.vcomponents.all;

entity iic_to_i2c_master is
    port (
    
    s_iic_scl_i : out std_logic; -- IIC Serial Clock Input from 3-state buffer (required)
    s_iic_scl_o : in std_logic; -- IIC Serial Clock Output to 3-state buffer (required)
    s_iic_scl_t : in std_logic; -- IIC Serial Clock Output Enable to 3-state buffer (required)
    s_iic_sda_i : out std_logic; -- IIC Serial Data Input from 3-state buffer (required)
    s_iic_sda_o : in std_logic; -- IIC Serial Data Output to 3-state buffer (required)
    s_iic_sda_t : in std_logic; -- IIC Serial Data Output Enable to 3-state buffer (required)
    
    s_i2c_scl_o : out std_logic;
    s_i2c_sda_io : inout std_logic
    
    );
end iic_to_i2c_master;

architecture arch_impl of iic_to_i2c_master is

    ATTRIBUTE X_INTERFACE_INFO : STRING;
    ATTRIBUTE X_INTERFACE_INFO of s_iic_scl_i: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SCL_I";
    ATTRIBUTE X_INTERFACE_INFO of s_iic_scl_o: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SCL_O";
    ATTRIBUTE X_INTERFACE_INFO of s_iic_scl_t: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SCL_T";
    ATTRIBUTE X_INTERFACE_INFO of s_iic_sda_i: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SDA_I";
    ATTRIBUTE X_INTERFACE_INFO of s_iic_sda_o: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SDA_O";
    ATTRIBUTE X_INTERFACE_INFO of s_iic_sda_t: SIGNAL is "xilinx.com:interface:iic:1.0 IIC SDA_T";
    
    ATTRIBUTE X_INTERFACE_INFO of s_i2c_scl_o: SIGNAL is "uw:uw:i2c_rtl:1.0 I2C SCL";
    ATTRIBUTE X_INTERFACE_INFO of s_i2c_sda_io: SIGNAL is "uw:uw:i2c_rtl:1.0 I2C SDA";

begin
    s_i2c_scl_o <= s_iic_scl_o when s_iic_scl_t = '1' else 'Z';
    s_iic_scl_i <= '0';
    
    --s_i2c_sda_io <= s_iic_sda_o when s_iic_sda_t = '1' else 'Z';
    --s_iic_sda_i <= s_i2c_sda_io;
    
    IOBUF_inst : IOBUF
    port map (
        O => s_iic_sda_i,   -- 1-bit output: Buffer output
        I => s_iic_sda_o,   -- 1-bit input: Buffer input
        IO => s_i2c_sda_io, -- 1-bit inout: Buffer inout (connect directly to top-level port)
        T => s_iic_sda_t    -- 1-bit input: 3-state enable input
    );
    
end arch_impl;
			
			
