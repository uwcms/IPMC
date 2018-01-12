----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 19.07.2013 16:52:04
-- Design Name: 
-- Module Name: pwm - Behavioral
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
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_misc.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity pwm is
    Port ( CLK : in STD_LOGIC;
           EN : in STD_LOGIC;
           I : in STD_LOGIC_VECTOR (7 downto 0);
           O : out STD_LOGIC);
end pwm;

architecture Behavioral of pwm is
	signal pwm_counter : std_logic_vector (7 downto 0);
begin
	process (CLK, EN) begin
		if EN = '0' then
			pwm_counter <= (others => '0');
		elsif CLK'event and CLK = '1' then
			pwm_counter <= pwm_counter + 1;
		end if;
	end process;
	
	O <= '1' when pwm_counter <= I else '0';
end Behavioral;
