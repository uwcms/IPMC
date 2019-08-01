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

-----------------------------------
-- Legend:
-- UNR: Upper Non-Recoverable 
-- UCR: Upper Critical
-- UNC: Upper Non-Critical
-- LNR: Lower Non-Recoverable 
-- LCR: Lower Critical
-- LNC: Lower Non-Critical

entity ipmi_sensor_event_proc is
  generic (
    C_WIDTH : integer := 16
    );
  port (
    clk_i : in std_logic;

    raw_i : in unsigned (C_WIDTH-1 downto 0);

    sensor_event_prev_i : in std_logic_vector (11 downto 0);

    thr_UNR_i : in unsigned (C_WIDTH-1 downto 0);
    thr_UCR_i : in unsigned (C_WIDTH-1 downto 0);
    thr_UNC_i : in unsigned (C_WIDTH-1 downto 0);
    thr_LNR_i : in unsigned (C_WIDTH-1 downto 0);
    thr_LCR_i : in unsigned (C_WIDTH-1 downto 0);
    thr_LNC_i : in unsigned (C_WIDTH-1 downto 0);

    hyst_pos_i : in unsigned (C_WIDTH-1 downto 0);
    hyst_neg_i : in unsigned (C_WIDTH-1 downto 0);

    sensor_event_next_o : out std_logic_vector (11 downto 0)
    );
end ipmi_sensor_event_proc;

architecture ipmi_sensor_event_proc_arch of ipmi_sensor_event_proc is

begin

  process(clk_i) is
    variable s_sensor_event : std_logic_vector (11 downto 0) := x"000";

  begin
    if rising_edge(clk_i) then

      -- latch previous sensor event state
      s_sensor_event := sensor_event_prev_i;

      -- re-evaualte event conditions based on new reading
      if (raw_i >= thr_UNR_i) then s_sensor_event(C_UNR_H) := '1'; elsif (raw_i < (thr_UNR_i - hyst_pos_i)) then s_sensor_event(C_UNR_H) := '0'; end if;
      if (raw_i <= thr_UNR_i) then s_sensor_event(C_UNR_L) := '1'; elsif (raw_i > (thr_UNR_i + hyst_neg_i)) then s_sensor_event(C_UNR_L) := '0'; end if;

      if (raw_i >= thr_UCR_i) then s_sensor_event(C_UCR_H) := '1'; elsif (raw_i < (thr_UCR_i - hyst_pos_i)) then s_sensor_event(C_UCR_H) := '0'; end if;
      if (raw_i <= thr_UCR_i) then s_sensor_event(C_UCR_L) := '1'; elsif (raw_i > (thr_UCR_i + hyst_neg_i)) then s_sensor_event(C_UCR_L) := '0'; end if;

      if (raw_i >= thr_UNC_i) then s_sensor_event(C_UNC_H) := '1'; elsif (raw_i < (thr_UNC_i - hyst_pos_i)) then s_sensor_event(C_UNC_H) := '0'; end if;
      if (raw_i <= thr_UNC_i) then s_sensor_event(C_UNC_L) := '1'; elsif (raw_i > (thr_UNC_i + hyst_neg_i)) then s_sensor_event(C_UNC_L) := '0'; end if;

      if (raw_i >= thr_LNR_i) then s_sensor_event(C_LNR_H) := '1'; elsif (raw_i < (thr_LNR_i - hyst_pos_i)) then s_sensor_event(C_LNR_H) := '0'; end if;
      if (raw_i <= thr_LNR_i) then s_sensor_event(C_LNR_L) := '1'; elsif (raw_i > (thr_LNR_i + hyst_neg_i)) then s_sensor_event(C_LNR_L) := '0'; end if;

      if (raw_i >= thr_LCR_i) then s_sensor_event(C_LCR_H) := '1'; elsif (raw_i < (thr_LCR_i - hyst_pos_i)) then s_sensor_event(C_LCR_H) := '0'; end if;
      if (raw_i <= thr_LCR_i) then s_sensor_event(C_LCR_L) := '1'; elsif (raw_i > (thr_LCR_i + hyst_neg_i)) then s_sensor_event(C_LCR_L) := '0'; end if;

      if (raw_i >= thr_LNC_i) then s_sensor_event(C_LNC_H) := '1'; elsif (raw_i < (thr_LNC_i - hyst_pos_i)) then s_sensor_event(C_LNC_H) := '0'; end if;
      if (raw_i <= thr_LNC_i) then s_sensor_event(C_LNC_L) := '1'; elsif (raw_i > (thr_LNC_i + hyst_neg_i)) then s_sensor_event(C_LNC_L) := '0'; end if;

      -- assign new event status with updated conditions
      sensor_event_next_o <= s_sensor_event;

    end if;

  end process;

end ipmi_sensor_event_proc_arch;
