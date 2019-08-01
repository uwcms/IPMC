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
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;

use work.ipmi_sensor_proc_pkg.all;

entity ipmi_sens_thr is
  port (clk_i     : in std_logic;
        raw_i     : in unsigned (15 downto 0);
        thr_UNR_i : in unsigned (15 downto 0);
        thr_UCR_i : in unsigned (15 downto 0);
        thr_UNC_i : in unsigned (15 downto 0);
        thr_LNR_i : in unsigned (15 downto 0);
        thr_LCR_i : in unsigned (15 downto 0);
        thr_LNC_i : in unsigned (15 downto 0);

        thr_reading_o : out std_logic_vector (5 downto 0));
end ipmi_sens_thr;

architecture ipmi_sens_thr_arch of ipmi_sens_thr is

  signal s_thr : std_logic_vector (5 downto 0);

begin

  s_thr(C_UNR) <= '1' when (raw_i) >= (thr_UNR_i) else '0';
  s_thr(C_UCR) <= '1' when (raw_i) >= (thr_UCR_i) else '0';
  s_thr(C_UNC) <= '1' when (raw_i) >= (thr_UNC_i) else '0';
  s_thr(C_LNR) <= '1' when (raw_i) <= (thr_LNR_i) else '0';
  s_thr(C_LCR) <= '1' when (raw_i) <= (thr_LCR_i) else '0';
  s_thr(C_LNC) <= '1' when (raw_i) <= (thr_LNC_i) else '0';

  thr_reading_o <= s_thr when rising_edge(clk_i);

end ipmi_sens_thr_arch;
