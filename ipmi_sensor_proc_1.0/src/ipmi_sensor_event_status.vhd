library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;
use IEEE.std_logic_misc.all;

use work.ipmi_sensor_proc_pkg.all;

entity ipmi_sensor_event_status is
  generic (

    C_SENSOR_CNT        : integer := 40;
    C_SENSOR_DATA_WIDTH : integer := 16
    );
  port (
    clk_i : in std_logic;
    rst_i : in std_logic;

    event_status_next_i : in  std_logic_vector(11 downto 0);
    event_status_prev_o : out std_logic_vector(11 downto 0);

    sensor_cnt_i : in unsigned(5 downto 0);
    event_rd_i   : in std_logic;
    event_wr_i   : in std_logic;

    event_assert_en_i   : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
    event_deassert_en_i : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

    event_assert_rearm_i   : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
    event_deassert_rearm_i : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

    event_current_status_o  : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
    event_assert_status_o   : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
    event_deassert_status_o : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

    MZ_hard_fault_i : out std_logic_vector(C_SENSOR_CNT-1 downto 0);
    -- MZ hard fault clear per sensor?
    event_irq_ack_i : in  std_logic;
    event_irq_req_o : out std_logic

    );
end ipmi_sensor_event_status;

architecture ipmi_sensor_event_status_arch of ipmi_sensor_event_status is

  signal s_event_assert_status   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_deassert_status : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_event_status    : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_status_d1 : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_event_status_rise : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_status_fall : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_irq_latch : std_logic := '0';

  signal s_MZ_hard_fault_UNRH : std_logic_vector(C_SENSOR_CNT-1 downto 0);
  signal s_MZ_hard_fault_LNRL : std_logic_vector(C_SENSOR_CNT-1 downto 0);
  
  signal s_event_assert_en_d1   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_deassert_en_d1   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_status_rst : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  
  signal s_event_status_rst_d1 : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_status_rst_d2 : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_status_rst_d3 : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  
  signal s_event_status_rst_4c : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  function or_reduce_2d (input : in t_slv_arr_12; size : in integer) return std_logic is
    variable v_or : std_logic := '0';
  begin

    for idx in 0 to size-1 loop
      v_or := v_or or or_reduce(input(idx));
    end loop;
    return v_or;

  end function;

begin

  s_event_assert_en_d1 <= event_assert_en_i when rising_edge(clk_i);
  s_event_deassert_en_d1 <= event_deassert_en_i when rising_edge(clk_i);
  
  gen_event_status_rst_L1 : for idx in 0 to C_SENSOR_CNT-1 generate
    gen_event_status_rst_L2 : for idx2 in 0 to 11 generate
      s_event_status_rst(idx)(idx2) <= (event_assert_en_i(idx)(idx2) and not s_event_assert_en_d1(idx)(idx2)) or (event_deassert_en_i(idx)(idx2) and not s_event_deassert_en_d1(idx)(idx2));
      s_event_status_rst_d1(idx)(idx2) <= s_event_status_rst(idx)(idx2) when rising_edge(clk_i);
      s_event_status_rst_d2(idx)(idx2) <= s_event_status_rst_d1(idx)(idx2) when rising_edge(clk_i);
      s_event_status_rst_d3(idx)(idx2) <= s_event_status_rst_d2(idx)(idx2) when rising_edge(clk_i);
      s_event_status_rst_4c(idx)(idx2) <= '1' when s_event_status_rst(idx)(idx2) = '1' or s_event_status_rst_d1(idx)(idx2) = '1' or s_event_status_rst_d2(idx)(idx2) = '1' or s_event_status_rst_d3(idx)(idx2) = '1' else '0';
    end generate gen_event_status_rst_L2;
  end generate gen_event_status_rst_L1;

  process(clk_i) is
  begin
    if rising_edge(clk_i) then
      if (event_rd_i = '1') then
        event_status_prev_o <= s_event_status(to_integer(sensor_cnt_i));
      end if;
    end if;
  end process;
  
  gen_event_status_L1 : for idx in 0 to C_SENSOR_CNT-1 generate
    gen_event_status_L2 : for idx2 in 0 to 11 generate
    process (clk_i) begin
        if rising_edge(clk_i) then
          if (rst_i = '1') then
            s_event_status(idx)(idx2) <= '0';
          elsif (s_event_status_rst_4c(idx)(idx2) = '1') then
            s_event_status(idx)(idx2) <= '0';
          elsif (event_wr_i = '1' and to_integer(sensor_cnt_i) = idx) then
            s_event_status(idx)(idx2) <= event_status_next_i(idx2);
          else
            s_event_status(idx)(idx2) <= s_event_status(idx)(idx2); -- No change
          end if;
        end if;
      end process;
    
      process (clk_i) begin
        if rising_edge(clk_i) then
          if (rst_i = '1') then
            s_event_status_d1(idx)(idx2) <= '0';
          elsif (s_event_status_rst_4c(idx)(idx2) = '1') then
            s_event_status_d1(idx)(idx2) <= '0';
          else
            s_event_status_d1(idx)(idx2) <= s_event_status(idx)(idx2);
          end if;
        end if;
      end process;
    
    end generate gen_event_status_L2;
  end generate gen_event_status_L1;

  gen_assert_L1 : for idx in 0 to C_SENSOR_CNT-1 generate
    gen_assert_L2 : for idx2 in 0 to 11 generate
      process(clk_i) is
      begin
        if rising_edge(clk_i) then
          s_event_status_rise(idx)(idx2) <= (s_event_status(idx)(idx2) and not s_event_status_d1(idx)(idx2))
                                            and s_event_assert_en_d1(idx)(idx2); 

          if(event_assert_rearm_i(idx)(idx2) = '1') then
            s_event_assert_status(idx)(idx2) <= '0';
          elsif (s_event_status_rise(idx)(idx2) = '1') then
            s_event_assert_status(idx)(idx2) <= '1';
          end if;
        end if;
      end process;
    end generate;
  end generate;

  gen_assert_UNRH : for idx in 0 to C_SENSOR_CNT-1 generate
    process(clk_i) is
    begin
      if rising_edge(clk_i) then

        if(event_assert_rearm_i(idx)(C_UNR_H) = '1') then
          s_MZ_hard_fault_UNRH(idx) <= '0';
        elsif (s_event_status_rise(idx)(C_UNR_H) = '1') then
          s_MZ_hard_fault_UNRH(idx) <= '1';
        end if;
      end if;
    end process;

  end generate;

  gen_assert_LNRL : for idx in 0 to C_SENSOR_CNT-1 generate
    process(clk_i) is
    begin
      if rising_edge(clk_i) then

        if(event_assert_rearm_i(idx)(C_LNR_L) = '1') then
          s_MZ_hard_fault_LNRL(idx) <= '0';
        elsif (s_event_status_rise(idx)(C_LNR_L) = '1') then
          s_MZ_hard_fault_LNRL(idx) <= '1';
        end if;
      end if;
    end process;
  end generate;

  gen_assert_hard_fault : for idx in 0 to C_SENSOR_CNT-1 generate
    process(clk_i) is
    begin
      if rising_edge(clk_i) then

        --MZ_hard_fault_i(idx) <= s_MZ_hard_fault_UNRH(idx) or s_MZ_hard_fault_LNRL(idx);
        MZ_hard_fault_i(idx) <= (event_assert_en_i(idx)(C_UNR_H) and s_event_status(idx)(C_UNR_H)) or
                                (event_assert_en_i(idx)(C_LNR_L) and s_event_status(idx)(C_LNR_L));
      end if;
    end process;
  end generate;

  gen_deassert_L1 : for idx in 0 to C_SENSOR_CNT-1 generate
    gen_deassert_L2 : for idx2 in 0 to 11 generate
      process(clk_i) is
      begin
        if rising_edge(clk_i) then
          s_event_status_fall(idx)(idx2) <= (not s_event_status(idx)(idx2) and s_event_status_d1(idx)(idx2))
                                            and s_event_deassert_en_d1(idx)(idx2);

          if(event_deassert_rearm_i(idx)(idx2) = '1') then
            s_event_deassert_status(idx)(idx2) <= '0';
          elsif (s_event_status_fall(idx)(idx2) = '1') then
            s_event_deassert_status(idx)(idx2) <= '1';
          end if;
        end if;
      end process;
    end generate;
  end generate;

  event_assert_status_o   <= s_event_assert_status;
  event_deassert_status_o <= s_event_deassert_status;

  event_current_status_o <= s_event_status;

  process(clk_i) is
  begin
    if rising_edge(clk_i) then
      if (event_irq_ack_i = '1') then
        s_irq_latch <= '0';
      else
        if((or_reduce_2d(s_event_assert_status, C_SENSOR_CNT) or or_reduce_2d(s_event_deassert_status, C_SENSOR_CNT)) = '1') then
          s_irq_latch <= '1';
        end if;
      end if;
    end if;
  end process;

  event_irq_req_o <= s_irq_latch;

end ipmi_sensor_event_status_arch;
