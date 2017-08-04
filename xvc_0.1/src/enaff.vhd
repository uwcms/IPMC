----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 11/11/2014 09:57:29 AM
-- Design Name: 
-- Module Name: enaff - Behavioral
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

entity enaff is
    Generic (resetvalue : STD_LOGIC := '0');
    Port ( sck : in STD_LOGIC;
           srst : in STD_LOGIC;
           din : in STD_LOGIC;
           senain : in STD_LOGIC;
           qout : out STD_LOGIC);
end enaff;

architecture Behavioral of enaff is

signal ffbit : std_logic := resetvalue;

begin
  process (sck) begin
    if (rising_edge(sck)) then
      if (srst = '1') then
        -- reset state
        ffbit <= resetvalue;
      elsif (senain = '1') then
        -- update input, otherwise hold value
        ffbit <= din;
      end if;
    end if;
  end process;

  qout <= ffbit;

end Behavioral;
