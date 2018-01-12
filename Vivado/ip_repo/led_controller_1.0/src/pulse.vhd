----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 22.07.2013 10:25:56
-- Design Name: 
-- Module Name: pulse - Behavioral
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

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity pulse is
	Generic (BITS : integer := 16);
    Port ( CLK : in STD_LOGIC;
           EN : in STD_LOGIC;
           I : in STD_LOGIC_VECTOR (7 downto 0);
           O : out STD_LOGIC_VECTOR (7 downto 0);
           D : out STD_LOGIC);
end pulse;

architecture Behavioral of pulse is
	signal counter : std_logic_vector (BITS+1 downto 0) := (others => '0');
	signal i_c : std_logic_vector (BITS+1 downto 8) := (others => '0');
	signal i_i, i_r : std_logic_vector (BITS+1 downto 0);
	signal dec : std_logic := '0';
	
	signal gnd_v : std_logic_vector (BITS+1 downto 0) := (others => '0');
begin
	i_i <= i_c (BITS+1 downto 8) & I (7 downto 0);
	i_r <=	counter + i_i when dec = '0' else
			counter - i_i;

	process (CLK, EN) begin
		if EN = '0' then
			counter <= (others => '0');
			dec <= '0';
			D <= '0';
		elsif CLK'event and CLK = '1' then
			D <= '0';
			if i_r(BITS+1) = '1' then
				if dec = '1' then
					dec <= '0';
					counter <= (others => '0');
					D <= '1';
				else
					dec <= '1';
					counter (BITS downto 0) <= (others => '1');
					counter (BITS+1) <= '0';
				end if;
			else
				counter <= i_r;
			end if;
		end if;
	end process;
	
	O <= counter (BITS downto BITS-7);
end Behavioral;
