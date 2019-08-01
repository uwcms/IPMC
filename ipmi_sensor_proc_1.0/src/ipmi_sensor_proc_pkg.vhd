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


package ipmi_sensor_proc_pkg is

---------------------------------------------------------------
-- IPMI Bit Definitions
---------------------------------------------------------------

  -- Sensor Event Bit Definitions
  constant C_UNR_H : integer := 11;     -- UNR High
  constant C_UNR_L : integer := 10;     -- UNR Low
  constant C_UCR_H : integer := 9;      -- UCR High
  constant C_UCR_L : integer := 8;      -- UCR Low
  constant C_UNC_H : integer := 7;      -- UNC High
  constant C_UNC_L : integer := 6;      -- UNC Low
  constant C_LNR_H : integer := 5;      -- LNC High 
  constant C_LNR_L : integer := 4;      -- LNC Low
  constant C_LCR_H : integer := 3;      -- LCR High
  constant C_LCR_L : integer := 2;      -- LCR Low
  constant C_LNC_H : integer := 1;      -- LNC High
  constant C_LNC_L : integer := 0;      -- LNC Low


  constant C_HYST_NEG : integer := 7;
  constant C_HYST_POS : integer := 6;
  constant C_UNR : integer := 5;
  constant C_UCR : integer := 4;
  constant C_UNC : integer := 3;
  constant C_LNR : integer := 2;
  constant C_LCR : integer := 1;
  constant C_LNC : integer := 0;

---------------------------------------------------------------
-- Register Map
---------------------------------------------------------------

  constant C_IPMI_LGC_RST_ADDR   : integer := 0;
  constant C_EV_IRQ_REQ_ADDR     : integer := 4;
  constant C_EV_IRQ_ACK_ADDR     : integer := 8;
  constant C_BRAM_CAP_CTRL_ADDR  : integer := 12;


  constant C_CH_CTRL_STAT_BASE_ADDR : integer := 256;

  constant C_RAW_READING_ADDR       : integer := C_CH_CTRL_STAT_BASE_ADDR + 0 * 256;
  constant C_THR_STATUS_ADDR        : integer := C_CH_CTRL_STAT_BASE_ADDR + 1 * 256;
  constant C_EV_ASSERT_EN_ADDR      : integer := C_CH_CTRL_STAT_BASE_ADDR + 2 * 256;
  constant C_EV_DEASSERT_EN_ADDR    : integer := C_CH_CTRL_STAT_BASE_ADDR + 3 * 256;
  constant C_EV_ASSERT_REARM_ADDR   : integer := C_CH_CTRL_STAT_BASE_ADDR + 4 * 256;
  constant C_EV_DEASSERT_REARM_ADDR : integer := C_CH_CTRL_STAT_BASE_ADDR + 5 * 256;
  constant C_EV_ASSERT_CUR_ST_ADDR  : integer := C_CH_CTRL_STAT_BASE_ADDR + 6 * 256;
  constant C_EV_ASSERT_ST_ADDR      : integer := C_CH_CTRL_STAT_BASE_ADDR + 7 * 256;
  constant C_EV_DEASSERT_ST_ADDR    : integer := C_CH_CTRL_STAT_BASE_ADDR + 8 * 256;
  constant C_BRAM_CAP_STAT_ADDR     : integer := C_CH_CTRL_STAT_BASE_ADDR + 9 * 256;
  constant C_BRAM_CAP_ADDR_ADDR     : integer := C_CH_CTRL_STAT_BASE_ADDR + 10 * 256;

  constant C_DPRAM_BASE_ADDR : integer := 4096;

  constant C_HYST_POS_ADDR : integer := C_DPRAM_BASE_ADDR + C_HYST_POS * 256;
  constant C_HYST_NEG_ADDR : integer := C_DPRAM_BASE_ADDR + C_HYST_NEG * 256;
  constant C_UNR_ADDR      : integer := C_DPRAM_BASE_ADDR + C_UNR * 256;
  constant C_UCR_ADDR      : integer := C_DPRAM_BASE_ADDR + C_UCR * 256;
  constant C_UNC_ADDR      : integer := C_DPRAM_BASE_ADDR + C_UNC * 256;
  constant C_LNR_ADDR      : integer := C_DPRAM_BASE_ADDR + C_LNR * 256;
  constant C_LCR_ADDR      : integer := C_DPRAM_BASE_ADDR + C_LCR * 256;
  constant C_LNC_ADDR      : integer := C_DPRAM_BASE_ADDR + C_LNC * 256;

  constant C_BRAM_CAP_BASE : integer := 8192;

---------------------------------------------------------------
-- Utility/helper declarations
---------------------------------------------------------------

  type t_slv_arr_2 is array(integer range <>) of std_logic_vector(1 downto 0);
  type t_slv_arr_3 is array(integer range <>) of std_logic_vector(2 downto 0);
  type t_slv_arr_4 is array(integer range <>) of std_logic_vector(3 downto 0);
  type t_slv_arr_5 is array(integer range <>) of std_logic_vector(4 downto 0);
  type t_slv_arr_6 is array(integer range <>) of std_logic_vector(5 downto 0);
  type t_slv_arr_7 is array(integer range <>) of std_logic_vector(6 downto 0);
  type t_slv_arr_8 is array(integer range <>) of std_logic_vector(7 downto 0);
  type t_slv_arr_9 is array(integer range <>) of std_logic_vector(8 downto 0);
  type t_slv_arr_10 is array(integer range <>) of std_logic_vector(9 downto 0);
  type t_slv_arr_11 is array(integer range <>) of std_logic_vector(10 downto 0);
  type t_slv_arr_12 is array(integer range <>) of std_logic_vector(11 downto 0);
  type t_slv_arr_13 is array(integer range <>) of std_logic_vector(12 downto 0);
  type t_slv_arr_14 is array(integer range <>) of std_logic_vector(13 downto 0);
  type t_slv_arr_15 is array(integer range <>) of std_logic_vector(14 downto 0);
  type t_slv_arr_16 is array(integer range <>) of std_logic_vector(15 downto 0);

  type t_slv_arr_32 is array(integer range <>) of std_logic_vector(31 downto 0);

end ipmi_sensor_proc_pkg;
