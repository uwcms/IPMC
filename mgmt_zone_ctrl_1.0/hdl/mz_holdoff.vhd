-- This file is part of the ZYNQ-IPMC Framework.
--
-- The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;


entity mz_holdoff is
port (
    clk_i    : in std_logic;

    MZ_pwr_on_seq_init_i    : in STD_LOGIC;
    MZ_hard_fault_holdoff_i : in  STD_LOGIC_VECTOR(31 downto 0);

    MZ_holdoff_o            : out  STD_LOGIC
  
  );
end mz_holdoff;

architecture mz_holdoff_arch of mz_holdoff is

signal s_MZ_holdoff_tmr :   unsigned(31 downto 0);
signal s_MZ_holdoff            :   STD_LOGIC := '0';


begin

process(clk_i) is
begin
    if (rising_edge(clk_i)) then
        if (MZ_pwr_on_seq_init_i = '1') then
             s_MZ_holdoff <= '1';
             s_MZ_holdoff_tmr <= unsigned(MZ_hard_fault_holdoff_i);
        elsif(s_MZ_holdoff_tmr /= 0) then
            s_MZ_holdoff_tmr <= s_MZ_holdoff_tmr - 1;
        else
            s_MZ_holdoff <= '0';
        end if;
    end if;

end process;

MZ_holdoff_o <= s_MZ_holdoff;

end mz_holdoff_arch;
