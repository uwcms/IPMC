----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 11/12/2014 10:26:22 AM
-- Design Name: 
-- Module Name: rwshiftreg - Behavioral
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

entity rwshiftreg is
Port (--register read/write ports
      din : in STD_LOGIC_VECTOR (31 downto 0);
      dout : out STD_LOGIC_VECTOR (31 downto 0);
      avalidin : in STD_LOGIC;
      psclkin : in STD_LOGIC;
      psrstin : in STD_LOGIC;
      psenain : in STD_LOGIC;
      pswenin : in STD_LOGIC_VECTOR (3 downto 0);
      --register shift ports
      shift_ev : in STD_LOGIC;
      sdatain : in STD_LOGIC;
      sdataout : out STD_LOGIC);
end rwshiftreg;

architecture Behavioral of rwshiftreg is

  signal regbits : STD_LOGIC_VECTOR (31 downto 0);

begin
  process (psclkin) begin
    if (rising_edge(psclkin)) then
      if (psrstin = '1') then
        regbits <= (others => '0');
      elsif ((avalidin = '1') and (psenain = '1')) then
        --individual byte writes
        if (pswenin(3) = '1') then
          regbits(31 downto 24) <= din(31 downto 24);
        end if;
        if (pswenin(2) = '1') then
          regbits(23 downto 16) <= din(23 downto 16);
        end if;
        if (pswenin(1) = '1') then
          regbits(15 downto 8) <= din(15 downto 8);
        end if;
        if (pswenin(0) = '1') then
          regbits(7 downto 0) <= din(7 downto 0);
        end if;
    
      elsif (shift_ev = '1') then
        --right shift, out of lsb, into msb
        regbits(30 downto 0) <= regbits(31 downto 1);
        regbits(31) <= sdatain;
      end if;
    end if;
  end process;

  sdataout <= regbits(0);           --shift out is LSb of register
  dout <= regbits;                  --read data port

end Behavioral;
