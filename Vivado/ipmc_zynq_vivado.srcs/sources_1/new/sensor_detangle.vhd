----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 02/06/2019 02:29:46 PM
-- Design Name: 
-- Module Name: sensor_detangle - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
-- 
----------------------------------------------------------------------------------


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity sensor_detangle is
    Port ( adc_0_value_i : in STD_LOGIC_VECTOR (143 downto 0);
           adc_0_valid_i : in STD_LOGIC_VECTOR (8 downto 0);
    
           adc_1_value_i : in STD_LOGIC_VECTOR (143 downto 0);
           adc_1_valid_i : in STD_LOGIC_VECTOR (8 downto 0);
           
           adc_2_value_i : in STD_LOGIC_VECTOR (431 downto 0);
           adc_2_valid_i : in STD_LOGIC_VECTOR (26 downto 0);
           
           value_o : out STD_LOGIC_VECTOR (527 downto 0);
           valid_o : out STD_LOGIC_VECTOR (32 downto 0));
end sensor_detangle;

architecture Behavioral of sensor_detangle is

begin
    -- adc0, ch0 to ch7, 8 channels
    value_o (127 downto   0) <= adc_0_value_i (127 downto   0);
    valid_o (  7 downto   0) <= adc_0_valid_i (  7 downto   0);
    
    -- adc1, ch0 to ch6, 7 channels 
    value_o (239 downto 128) <= adc_1_value_i (111 downto   0);
    valid_o ( 14 downto   8) <= adc_1_valid_i (  6 downto   0);
    
    -- adc2, ch0 to ch6, 7 channels
    value_o (351 downto 240) <= adc_2_value_i (111 downto   0);
    valid_o ( 21 downto  15) <= adc_2_valid_i (  6 downto   0);
    
    -- adc2, ch9 to ch16, 8 channels
    value_o (479 downto 352) <= adc_2_value_i (271 downto 144);
    valid_o ( 29 downto  22) <= adc_2_valid_i ( 16 downto   9);
    
    -- adc2, ch18 to ch20, 3 channels
    value_o (527 downto 480) <= adc_2_value_i (335 downto 288);
    valid_o ( 32 downto  30) <= adc_2_valid_i ( 20 downto  18);
    

end Behavioral;
