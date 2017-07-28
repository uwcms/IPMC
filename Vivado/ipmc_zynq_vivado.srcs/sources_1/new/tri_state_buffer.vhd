----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 04/21/2017 11:26:16 AM
-- Design Name: 
-- Module Name: tri_state_buffer - Behavioral
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
library UNISIM;
use UNISIM.VComponents.all;

entity tri_state_buffer is
    Generic ( N : integer := 49);
    Port ( I : in STD_LOGIC_VECTOR(N-1 downto 0);
           O : out STD_LOGIC_VECTOR(N-1 downto 0);
           T : in STD_LOGIC_VECTOR(N-1 downto 0);
           IO : inout STD_LOGIC_VECTOR(N-1 downto 0));
end tri_state_buffer;

architecture Behavioral of tri_state_buffer is

begin
    GEN_BUFFERS : for X in 0 to N-1 generate
        IOBUF_inst : IOBUF
        port map (
            O => O(X),   -- 1-bit output: Buffer output
            I => I(X),   -- 1-bit input: Buffer input
            IO => IO(X), -- 1-bit inout: Buffer inout (connect directly to top-level port)
            T => T(X)    -- 1-bit input: 3-state enable input
        );
    end generate GEN_BUFFERS;

    

end Behavioral;
