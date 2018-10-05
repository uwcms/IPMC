library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;
use IEEE.std_logic_misc.all;

use work.ipmi_sensor_proc_pkg.all;

entity ipmi_sens_thr_status is
  generic (C_SENSOR_CNT : integer := 40
           );
  port (
    clk_i : in std_logic;
    rst_i : in std_logic;

    sensor_cnt_i : in unsigned(5 downto 0);
    event_wr_i   : in std_logic;

    sensor_thr_next_i : in std_logic_vector(5 downto 0);

    sensor_thr_status_o : out t_slv_arr_6(C_SENSOR_CNT-1 downto 0)
    );
end ipmi_sens_thr_status;

architecture ipmi_sens_thr_status_arch of ipmi_sens_thr_status is

begin

  process(clk_i) is
  begin
    if rising_edge(clk_i) then
      if (rst_i = '1') then
        sensor_thr_status_o <= (others => (others => '0'));
      elsif (event_wr_i = '1') then
        sensor_thr_status_o(to_integer(sensor_cnt_i)) <= sensor_thr_next_i;
      end if;
    end if;
  end process;

end ipmi_sens_thr_status_arch;
