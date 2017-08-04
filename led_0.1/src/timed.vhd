----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 19.07.2013 17:37:48
-- Design Name: 
-- Module Name: timed - Behavioral
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

library IEEE;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_misc.all;

entity timed is
	Generic (BITS : integer := 15);
    Port ( CLK : in STD_LOGIC;
           EN : in STD_LOGIC;
           I : in STD_LOGIC;
           O : out STD_LOGIC);
end timed;

architecture Behavioral of timed is
	signal counter : std_logic_vector (BITS downto 0) := (others => '1');
begin
	process (CLK, EN, I) begin
		if EN = '0' then
			counter <= (others => '0');
			O <= '0';
		elsif I = '1' then
			counter <= (others => '0');
			O <= '1';
		elsif CLK'event and CLK = '1' then
			if counter /= 2**(BITS-1)-1 then
				counter <= counter + 1;
				O <= '1';
			else
				O <= '0';
			end if;
		end if;
	end process;

end Behavioral;
