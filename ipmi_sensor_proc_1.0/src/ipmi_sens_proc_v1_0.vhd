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

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.ipmi_sensor_proc_pkg.all;

entity ipmi_sens_proc_v1_0 is
  generic (

    C_SENSOR_CNT        : integer range 1 to 64 := 18;
    C_SENSOR_DATA_WIDTH : integer := 16;

    -- Parameters of Axi Slave Bus Interface S_AXI
    C_S_AXI_DATA_WIDTH : integer := 32;
    C_S_AXI_ADDR_WIDTH : integer := 20
    );
  port (

    sensor_data_i : in std_logic_vector(C_SENSOR_DATA_WIDTH - 1 downto 0);
    sensor_id_i : in std_logic_vector(5 downto 0);
    sensor_vld_i  : in std_logic;

    MZ_hard_fault_o : out std_logic_vector(C_SENSOR_CNT-1 downto 0);

    irq_o : out std_logic;
    
    debug : out std_logic_vector(22 downto 0);

    -- Ports of Axi Slave Bus Interface S_AXI
    s_axi_aclk    : in  std_logic;
    s_axi_aresetn : in  std_logic;
    s_axi_awaddr  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    s_axi_awprot  : in  std_logic_vector(2 downto 0);
    s_axi_awvalid : in  std_logic;
    s_axi_awready : out std_logic;
    s_axi_wdata   : in  std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    s_axi_wstrb   : in  std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
    s_axi_wvalid  : in  std_logic;
    s_axi_wready  : out std_logic;
    s_axi_bresp   : out std_logic_vector(1 downto 0);
    s_axi_bvalid  : out std_logic;
    s_axi_bready  : in  std_logic;
    s_axi_araddr  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    s_axi_arprot  : in  std_logic_vector(2 downto 0);
    s_axi_arvalid : in  std_logic;
    s_axi_arready : out std_logic;
    s_axi_rdata   : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    s_axi_rresp   : out std_logic_vector(1 downto 0);
    s_axi_rvalid  : out std_logic;
    s_axi_rready  : in  std_logic
    );
end ipmi_sens_proc_v1_0;

architecture arch_imp of ipmi_sens_proc_v1_0 is

  component fifo_sensor_data
    port (
      clk          : in  std_logic;
      srst         : in  std_logic;
      din          : in  std_logic_vector(21 downto 0);
      wr_en        : in  std_logic;
      rd_en        : in  std_logic;
      dout         : out std_logic_vector(21 downto 0);
      full         : out std_logic;
      overflow     : out std_logic;
      empty        : out std_logic;
      almost_empty : out std_logic;
      underflow    : out std_logic
      );
  end component;

  component distram_dp_64x16
    port (
      a    : in  std_logic_vector(5 downto 0);
      d    : in  std_logic_vector(15 downto 0);
      dpra : in  std_logic_vector(5 downto 0);
      clk  : in  std_logic;
      we   : in  std_logic;
      spo  : out std_logic_vector(15 downto 0);
      dpo  : out std_logic_vector(15 downto 0);
      qspo : out std_logic_vector(15 downto 0);
      qdpo : out std_logic_vector(15 downto 0)
      );
  end component;

  component bram_sensor_data
    port (
      clka  : in  std_logic;
      ena   : in  std_logic;
      wea   : in  std_logic_vector(0 downto 0);
      addra : in  std_logic_vector(9 downto 0);
      dina  : in  std_logic_vector(15 downto 0);
      douta : out std_logic_vector(15 downto 0);
      clkb  : in  std_logic;
      enb   : in  std_logic;
      web   : in  std_logic_vector(0 downto 0);
      addrb : in  std_logic_vector(9 downto 0);
      dinb  : in  std_logic_vector(15 downto 0);
      doutb : out std_logic_vector(15 downto 0)
      );
  end component;

  type deser_fsm_type is (reset, wait_for_data, unload_fifo, push_data, hold_1, hold_2, hold_3);

  signal current_state, next_state : deser_fsm_type;

  signal s_ipmi_lgc_rst : std_logic := '0';

  signal s_fifo_sd_din  : std_logic_vector(21 downto 0);
  signal s_fifo_sd_dout : std_logic_vector(21 downto 0);

  signal s_fifo_sd_wren         : std_logic;
  signal s_fifo_sd_rden         : std_logic;
  signal s_fifo_sd_ovf          : std_logic;
  signal s_fifo_sd_udf          : std_logic;
  signal s_fifo_sd_empty        : std_logic;
  signal s_fifo_sd_not_empty    : std_logic;
  signal s_fifo_sd_almost_empty : std_logic;

  signal s_sensor_event_prev : std_logic_vector (11 downto 0);
  signal s_sensor_event_next : std_logic_vector (11 downto 0);

  signal s_sensor_event_status : t_slv_arr_12 (C_SENSOR_CNT-1 downto 0);

  signal s_thr_UNR : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_thr_UCR : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_thr_UNC : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_thr_LNR : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_thr_LCR : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_thr_LNC : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);

  signal s_hyst_pos : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_hyst_neg : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);

  signal s_sensor_data_deser : unsigned (C_SENSOR_DATA_WIDTH-1 downto 0);
  signal s_sensor_cnt_deser  : unsigned (C_SENSOR_DATA_WIDTH+6-1 downto C_SENSOR_DATA_WIDTH);


  signal s_isp_addr_p0 : std_logic_vector(5 downto 0);
  signal s_isp_addr_p1 : t_slv_arr_6(7 downto 0);

  signal s_isp_din_p0  : std_logic_vector(15 downto 0);
  signal s_isp_dout_p0 : t_slv_arr_16(7 downto 0);
  signal s_isp_dout_p1 : t_slv_arr_16(7 downto 0);

  signal s_isp_we_p0 : std_logic_vector(7 downto 0);

  signal s_cap_bram_addr_axi : t_slv_arr_10(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_we_axi   : std_logic_vector(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_din_axi  : t_slv_arr_16(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_dout_axi : t_slv_arr_16(C_SENSOR_CNT-1 downto 0);

  signal s_cap_bram_addr_sensor : t_slv_arr_10(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_we_sensor   : std_logic_vector(C_SENSOR_CNT-1 downto 0);

  signal s_cap_bram_wr_in_progress   : std_logic_vector(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_post_trig_sample : t_slv_arr_10(C_SENSOR_CNT-1 downto 0);
  signal s_cap_bram_wr_start   : std_logic;
  signal s_cap_bram_wr_start_d1   : std_logic;
  signal s_cap_bram_wr_start_edge   : std_logic;
  
  signal s_cap_bram_wr_stop   : std_logic;

---



---

  signal s_event_assert_en   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_deassert_en : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_event_assert_rearm   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_deassert_rearm : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_event_current_status  : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_assert_status   : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
  signal s_event_deassert_status : t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

  signal s_sensor_thr : std_logic_vector (5 downto 0);

  signal s_sensor_raw_reading : std_logic_vector(C_SENSOR_DATA_WIDTH-1 downto 0);

  signal s_sensor_thr_status : t_slv_arr_6(C_SENSOR_CNT-1 downto 0);


  signal s_ises_event_rd : std_logic;
  signal s_ises_event_wr : std_logic;

  signal s_event_irq_ack : std_logic;
  signal s_event_irq_req : std_logic;

begin

  process(s_axi_aclk) is
  begin
    if rising_edge(s_axi_aclk) then
      if (s_ipmi_lgc_rst = '1') then
        s_sensor_raw_reading <= (others => '0');
      elsif (sensor_vld_i = '1') then
          s_sensor_raw_reading <= sensor_data_i;
      end if;
    end if;
  end process;

  gen_bram : for idx in 0 to C_SENSOR_CNT-1 generate
    
   process(s_axi_aclk) is
     begin
       if rising_edge(s_axi_aclk) then
       
       s_cap_bram_wr_start_d1 <= s_cap_bram_wr_start;
       
       s_cap_bram_wr_start_edge <= not s_cap_bram_wr_start_d1 and s_cap_bram_wr_start;
       

          if ( s_ipmi_lgc_rst = '1' or (s_cap_bram_post_trig_sample(idx) = "1000000000") or s_cap_bram_wr_stop = '1') then
            s_cap_bram_wr_in_progress(idx) <= '0';
         elsif (s_cap_bram_wr_start_edge = '1')then
            s_cap_bram_wr_in_progress(idx) <= '1';
         end if;
         
         if (s_ipmi_lgc_rst = '1') then
             s_cap_bram_we_sensor(idx) <= '0';
             s_cap_bram_addr_sensor(idx) <= (others => '1');         
         elsif (s_cap_bram_wr_in_progress(idx) = '1' and sensor_vld_i = '1' and unsigned(sensor_id_i) = idx) then
            s_cap_bram_we_sensor(idx) <= '1';
            s_cap_bram_addr_sensor(idx) <= std_logic_vector(unsigned(s_cap_bram_addr_sensor(idx)) + 1);
            if (s_cap_bram_addr_sensor(idx) = "1111111111") then
                s_cap_bram_addr_sensor(idx) <= "0000000000";
            end if;
         else   
             s_cap_bram_we_sensor(idx) <= '0';
         end if;   
         
         if (s_ipmi_lgc_rst = '1' or s_cap_bram_wr_start = '1') then
                s_cap_bram_post_trig_sample(idx) <= "0000000000" ;      
         elsif (s_event_irq_req = '1' and sensor_vld_i = '1' and unsigned(sensor_id_i) = idx) then
            s_cap_bram_post_trig_sample(idx) <= std_logic_vector(unsigned(s_cap_bram_post_trig_sample(idx)) + 1);
             if (s_cap_bram_post_trig_sample(idx) = "1000000000") then
                 s_cap_bram_post_trig_sample(idx) <= "1000000000";
             end if;
         
         end if;               
          
       end if;
     end process;
    
    i_bram_sensor_data : bram_sensor_data
      port map (
        clka   => s_axi_aclk,
        ena    => '1',
        wea(0) => s_cap_bram_we_axi(idx),
        addra  => s_cap_bram_addr_axi(idx),
        dina   => s_cap_bram_din_axi(idx),
        douta  => s_cap_bram_dout_axi(idx),
        clkb   => s_axi_aclk,
        enb    => '1',
        web(0) => s_cap_bram_we_sensor(idx),
        addrb  => s_cap_bram_addr_sensor(idx),
        dinb   => s_sensor_raw_reading,
        doutb  => open
        );
     
  end generate; 

-- Instantiation of Axi Bus Interface S_AXI
  i_ipmi_sens_proc_v1_0_S_AXI : entity work.ipmi_sens_proc_v1_0_S_AXI
    generic map (
      C_S_AXI_DATA_WIDTH => C_S_AXI_DATA_WIDTH,
      C_S_AXI_ADDR_WIDTH => C_S_AXI_ADDR_WIDTH,
      C_SENSOR_CNT        => C_SENSOR_CNT
      )
    port map (
      ipmi_lgc_rst => s_ipmi_lgc_rst,

      --sensor_raw_reading => s_sensor_raw_reading,
      sensor_thr_status  => s_sensor_thr_status,

      event_irq_ack => s_event_irq_ack,
      event_irq_req => s_event_irq_req,

      event_assert_en   => s_event_assert_en,
      event_deassert_en => s_event_deassert_en,

      event_assert_rearm   => s_event_assert_rearm,
      event_deassert_rearm => s_event_deassert_rearm,

      event_current_status  => s_event_current_status,
      event_assert_status   => s_event_assert_status,
      event_deassert_status => s_event_deassert_status,

      cap_bram_addr => s_cap_bram_addr_axi,
      cap_bram_we   => s_cap_bram_we_axi,
      cap_bram_din  => s_cap_bram_din_axi,
      cap_bram_dout => s_cap_bram_dout_axi,

      cap_bram_addr_sensor     => s_cap_bram_addr_sensor,
      cap_bram_wr_in_progress  => s_cap_bram_wr_in_progress,
      cap_bram_wr_start        => s_cap_bram_wr_start,
      cap_bram_wr_stop         => s_cap_bram_wr_stop,

      dpram_isp_addr => s_isp_addr_p0,
      dpram_isp_we   => s_isp_we_p0,
      dpram_isp_din  => s_isp_din_p0,
      dpram_isp_dout => s_isp_dout_p0,

      S_AXI_ACLK    => s_axi_aclk,
      S_AXI_ARESETN => s_axi_aresetn,
      S_AXI_AWADDR  => s_axi_awaddr,
      S_AXI_AWPROT  => s_axi_awprot,
      S_AXI_AWVALID => s_axi_awvalid,
      S_AXI_AWREADY => s_axi_awready,
      S_AXI_WDATA   => s_axi_wdata,
      S_AXI_WSTRB   => s_axi_wstrb,
      S_AXI_WVALID  => s_axi_wvalid,
      S_AXI_WREADY  => s_axi_wready,
      S_AXI_BRESP   => s_axi_bresp,
      S_AXI_BVALID  => s_axi_bvalid,
      S_AXI_BREADY  => s_axi_bready,
      S_AXI_ARADDR  => s_axi_araddr,
      S_AXI_ARPROT  => s_axi_arprot,
      S_AXI_ARVALID => s_axi_arvalid,
      S_AXI_ARREADY => s_axi_arready,
      S_AXI_RDATA   => s_axi_rdata,
      S_AXI_RRESP   => s_axi_rresp,
      S_AXI_RVALID  => s_axi_rvalid,
      S_AXI_RREADY  => s_axi_rready
      );

  s_fifo_sd_din(C_SENSOR_DATA_WIDTH - 1 downto 0)                       <= sensor_data_i;
  s_fifo_sd_din(C_SENSOR_DATA_WIDTH + 6 - 1 downto C_SENSOR_DATA_WIDTH) <= sensor_id_i;
  s_fifo_sd_wren                                                        <= sensor_vld_i;

  i_fifo_sensor_data : fifo_sensor_data
    port map (
      clk          => s_axi_aclk,
      srst         => s_ipmi_lgc_rst,
      din          => s_fifo_sd_din,
      wr_en        => s_fifo_sd_wren,
      rd_en        => s_fifo_sd_rden,
      dout         => s_fifo_sd_dout,
      full         => open,
      overflow     => s_fifo_sd_ovf,
      empty        => s_fifo_sd_empty,
      almost_empty => s_fifo_sd_almost_empty,
      underflow    => s_fifo_sd_udf
      );

  s_sensor_data_deser <= unsigned(s_fifo_sd_dout(C_SENSOR_DATA_WIDTH-1 downto 0));
  s_sensor_cnt_deser  <= unsigned(s_fifo_sd_dout(C_SENSOR_DATA_WIDTH+6-1 downto C_SENSOR_DATA_WIDTH));
  
  debug(15 downto 0) <= std_logic_vector(s_sensor_data_deser);
  debug(21 downto 16) <= std_logic_vector(s_sensor_cnt_deser);
  debug(22) <= s_fifo_sd_rden;

  deser_fsm : process(s_axi_aclk) is
  begin
    if rising_edge(s_axi_aclk) then
      if s_ipmi_lgc_rst = '1' then
        current_state <= reset;
      else
        current_state <= next_state;
      end if;
    end if;
  end process;

  state_machine_decisions : process (current_state, s_fifo_sd_empty)
  begin

    s_fifo_sd_rden  <= '0';
    s_ises_event_rd <= '0';
    s_ises_event_wr <= '0';

    next_state <= current_state;

    case current_state is
      when reset =>
        next_state <= wait_for_data;

      when wait_for_data =>
        if (s_fifo_sd_empty = '0') then
          next_state     <= unload_fifo;
          s_fifo_sd_rden <= '1';
        end if;

      when unload_fifo =>
        next_state <= push_data;
        --s_ises_event_wr <= '1';

      when push_data =>
        next_state      <= hold_1;
        s_ises_event_rd <= '1';

      when hold_1 =>
        next_state <= hold_2;

      when hold_2 =>
        next_state <= hold_3;

      when hold_3 =>
        next_state      <= wait_for_data;
        s_ises_event_wr <= '1';

      when others =>
        next_state <= reset;
    end case;
  end process;

  g_distram_ipmi_sensor_params : for idx in 0 to 7 generate

    s_isp_addr_p1(idx) <= std_logic_vector(s_sensor_cnt_deser);

    i_distram_dp_64x16_ipmi_sensor_params : distram_dp_64x16
      port map (
        a    => s_isp_addr_p0,
        d    => s_isp_din_p0,
        dpra => s_isp_addr_p1(idx),
        clk  => s_axi_aclk,
        we   => s_isp_we_p0(idx),
        spo => s_isp_dout_p0(idx),
        dpo  => s_isp_dout_p1(idx),
        qspo => open,
        qdpo => open
        );
  end generate;

  s_thr_UNR <= unsigned(s_isp_dout_p1(C_UNR));
  s_thr_UCR <= unsigned(s_isp_dout_p1(C_UCR));
  s_thr_UNC <= unsigned(s_isp_dout_p1(C_UNC));
  s_thr_LNR <= unsigned(s_isp_dout_p1(C_LNR));
  s_thr_LCR <= unsigned(s_isp_dout_p1(C_LCR));
  s_thr_LNC <= unsigned(s_isp_dout_p1(C_LNC));

  s_hyst_pos <= unsigned(s_isp_dout_p1(C_HYST_POS));
  s_hyst_neg <= unsigned(s_isp_dout_p1(C_HYST_NEG));

  i_ipmi_sensor_event_proc : entity work.ipmi_sensor_event_proc
    generic map (
      C_WIDTH => C_SENSOR_DATA_WIDTH
      )
    port map (
      clk_i => s_axi_aclk,

      raw_i => s_sensor_data_deser,

      sensor_event_prev_i => s_sensor_event_prev,

      thr_UNR_i => s_thr_UNR,
      thr_UCR_i => s_thr_UCR,
      thr_UNC_i => s_thr_UNC,
      thr_LNR_i => s_thr_LNR,
      thr_LCR_i => s_thr_LCR,
      thr_LNC_i => s_thr_LNC,

      hyst_pos_i => s_hyst_pos,
      hyst_neg_i => s_hyst_neg,

      sensor_event_next_o => s_sensor_event_next
      );

  i_ipmi_sensor_event_status : entity work.ipmi_sensor_event_status
    generic map(

      C_SENSOR_CNT        => C_SENSOR_CNT,
      C_SENSOR_DATA_WIDTH => C_SENSOR_DATA_WIDTH
      )
    port map(
      clk_i => s_axi_aclk,
      rst_i => s_ipmi_lgc_rst,

      event_status_next_i => s_sensor_event_next,
      event_status_prev_o => s_sensor_event_prev,

      sensor_cnt_i => s_sensor_cnt_deser,
      event_rd_i   => s_ises_event_rd,
      event_wr_i   => s_ises_event_wr,

      event_assert_en_i   => s_event_assert_en,
      event_deassert_en_i => s_event_deassert_en,

      event_assert_rearm_i   => s_event_assert_rearm,
      event_deassert_rearm_i => s_event_deassert_rearm,

      event_current_status_o  => s_event_current_status,
      event_assert_status_o   => s_event_assert_status,
      event_deassert_status_o => s_event_deassert_status,

      MZ_hard_fault_i => MZ_hard_fault_o,

      event_irq_ack_i => s_event_irq_ack,
      event_irq_req_o => s_event_irq_req
      );

  i_ipmi_sens_reading : entity work.ipmi_sens_thr
    port map (
      clk_i     => s_axi_aclk,
      raw_i     => s_sensor_data_deser,
      thr_UNR_i => s_thr_UNR,
      thr_UCR_i => s_thr_UCR,
      thr_UNC_i => s_thr_UNC,
      thr_LNR_i => s_thr_LNR,
      thr_LCR_i => s_thr_LCR,
      thr_LNC_i => s_thr_LNC,

      thr_reading_o => s_sensor_thr
      );

  i_ipmi_sens_reading_status : entity work.ipmi_sens_thr_status
    generic map (

      C_SENSOR_CNT => C_SENSOR_CNT
      )
    port map (
      clk_i => s_axi_aclk,
      rst_i => s_ipmi_lgc_rst,

      sensor_cnt_i => s_sensor_cnt_deser,
      event_wr_i   => s_ises_event_wr,

      sensor_thr_next_i   => s_sensor_thr,
      sensor_thr_status_o => s_sensor_thr_status
      );

  irq_o <= s_event_irq_req;

end arch_imp;
