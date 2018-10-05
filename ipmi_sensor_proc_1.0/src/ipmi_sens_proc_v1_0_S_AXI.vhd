library ieee;
use ieee.std_logic_1164.all;
--use ieee.std_logic_arith.all;
--use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;

use work.ipmi_sensor_proc_pkg.all;

entity ipmi_sens_proc_v1_0_S_AXI is
  generic
    (

      C_S_AXI_DATA_WIDTH : integer := 32;
      C_S_AXI_ADDR_WIDTH : integer := 20;

      C_SENSOR_CNT : integer := 25
      );
  port
    (
      ipmi_lgc_rst : out std_logic;

      sensor_raw_reading : in t_slv_arr_16(C_SENSOR_CNT-1 downto 0);
      sensor_thr_status  : in t_slv_arr_6(C_SENSOR_CNT-1 downto 0);

      event_irq_ack : out std_logic;
      event_irq_req : in  std_logic;

      event_assert_en   : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
      event_deassert_en : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

      event_assert_rearm   : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
      event_deassert_rearm : out t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

      event_current_status  : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
      event_assert_status   : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);
      event_deassert_status : in t_slv_arr_12(C_SENSOR_CNT-1 downto 0);

      cap_bram_addr : out t_slv_arr_10(C_SENSOR_CNT-1 downto 0);
      cap_bram_we   : out std_logic_vector(C_SENSOR_CNT-1 downto 0);
      cap_bram_din  : out t_slv_arr_16(C_SENSOR_CNT-1 downto 0);
      cap_bram_dout : in  t_slv_arr_16(C_SENSOR_CNT-1 downto 0);

      cap_bram_addr_sensor     : in t_slv_arr_10(C_SENSOR_CNT-1 downto 0);
      cap_bram_wr_in_progress  : in std_logic_vector(C_SENSOR_CNT-1 downto 0);
      cap_bram_wr_start        : out std_logic;
      cap_bram_wr_stop         : out std_logic;

      dpram_isp_addr : out t_slv_arr_6(7 downto 0);
      dpram_isp_we   : out std_logic_vector(7 downto 0);
      dpram_isp_din  : out t_slv_arr_16(7 downto 0);
      dpram_isp_dout : in  t_slv_arr_16(7 downto 0);

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
end entity ipmi_sens_proc_v1_0_S_AXI;

------------------------------------------------------------------------------
-- Architecture section
------------------------------------------------------------------------------

architecture ipmi_sens_proc_v1_0_S_AXI_arch of ipmi_sens_proc_v1_0_S_AXI is


-- Type declarations
  type main_fsm_type is (reset, idle, read_transaction_in_progress, write_transaction_in_progress, complete);

-- Signal declarations
  signal local_address_slv : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
  signal local_address     : integer range 0 to 2 ** (C_S_AXI_ADDR_WIDTH);
--signal local_address : integer range 0 to 1024;

-- pipelined responses
  signal s_axi_rresp_r   : std_logic_vector(1 downto 0);
  signal s_axi_rvalid_r : std_logic;

  signal local_address_valid : std_logic;

  signal s_ipmi_lgc_rst  : std_logic := '0';
  signal s_event_irq_ack : std_logic := '0';

  signal s_ipmi_lgc_rst_wr  : std_logic;
  signal s_event_irq_ack_wr : std_logic;

  signal s_cap_bram_ctrl_wr : std_logic;

  signal s_dpram_isp_dout_r : t_slv_arr_16(7 downto 0);

  signal combined_S_AXI_AWVALID_S_AXI_ARVALID : std_logic_vector(1 downto 0);
  signal Local_Reset                          : std_logic;
  signal current_state, next_state            : main_fsm_type;
  signal write_enable_registers               : std_logic;
  signal send_read_data_to_AXI                : std_logic;

  signal s_event_assert_en      : t_slv_arr_12(63 downto 0);
  signal s_event_deassert_en    : t_slv_arr_12(63 downto 0);
  signal s_event_assert_rearm   : t_slv_arr_12(63 downto 0);
  signal s_event_deassert_rearm : t_slv_arr_12(63 downto 0);

  signal s_event_assert_en_wr      : std_logic_vector(63 downto 0);
  signal s_event_deassert_en_wr    : std_logic_vector(63 downto 0);
  signal s_event_assert_rearm_wr   : std_logic_vector(63 downto 0);
  signal s_event_deassert_rearm_wr : std_logic_vector(63 downto 0);

  signal s_event_current_status  : t_slv_arr_12(63 downto 0);
  signal s_event_assert_status   : t_slv_arr_12(63 downto 0);
  signal s_event_deassert_status : t_slv_arr_12(63 downto 0);

  signal s_cap_bram_dout         : t_slv_arr_16(63 downto 0)     := (others => (others => '0'));
  signal s_cap_bram_we           : std_logic_vector(63 downto 0) := (others => '0');
  signal s_send_read_data_to_AXI : std_logic;

  signal s_cap_bram_ctrl : std_logic_vector(1 downto 0);

begin

  Local_Reset                          <= not S_AXI_ARESETN;
  combined_S_AXI_AWVALID_S_AXI_ARVALID <= S_AXI_AWVALID & S_AXI_ARVALID;

  s_dpram_isp_dout_r <= dpram_isp_dout;  -- when rising_edge(S_AXI_ACLK);

  state_machine_update : process (S_AXI_ACLK)
  begin
    if S_AXI_ACLK'event and S_AXI_ACLK = '1' then
      if Local_Reset = '1' then
        current_state <= reset;
      else
        current_state <= next_state;
      end if;
    end if;
  end process;

  s_axi_rresp  <= s_axi_rresp_r;        -- when rising_edge(S_AXI_ACLK);
  s_axi_rvalid <= s_axi_rvalid_r;       -- when rising_edge(S_AXI_ACLK);

  s_send_read_data_to_AXI <= send_read_data_to_AXI;

  state_machine_decisions : process (current_state,
                                     combined_S_AXI_AWVALID_S_AXI_ARVALID,
                                     S_AXI_ARVALID,
                                     S_AXI_RREADY,
                                     S_AXI_AWVALID,
                                     S_AXI_WVALID,
                                     S_AXI_BREADY,
                                     local_address,
                                     local_address_valid)
  begin
    S_AXI_ARREADY          <= '0';
    s_axi_rresp_r          <= "--";
    s_axi_rvalid_r         <= '0';
    S_AXI_WREADY           <= '0';
    S_AXI_BRESP            <= "--";
    S_AXI_BVALID           <= '0';
    S_AXI_WREADY           <= '0';
    S_AXI_AWREADY          <= '0';
    write_enable_registers <= '0';
    send_read_data_to_AXI  <= '0';

    case current_state is
      when reset =>
        next_state <= idle;

      when idle =>
        next_state <= idle;
        case combined_S_AXI_AWVALID_S_AXI_ARVALID is
          when "01"   => next_state <= read_transaction_in_progress;
          when "10"   => next_state <= write_transaction_in_progress;
          when others => null;
        end case;

      when read_transaction_in_progress =>
        next_state            <= read_transaction_in_progress;
        S_AXI_ARREADY         <= S_AXI_ARVALID;
        s_axi_rvalid_r        <= '1';
        s_axi_rresp_r         <= "00";
        send_read_data_to_AXI <= '1';
        if S_AXI_RREADY = '1' then
          next_state <= complete;
        end if;

      when write_transaction_in_progress =>
        next_state             <= write_transaction_in_progress;
        write_enable_registers <= '1';
        S_AXI_AWREADY          <= S_AXI_AWVALID;
        S_AXI_WREADY           <= S_AXI_WVALID;
        S_AXI_BRESP            <= "00";
        S_AXI_BVALID           <= '1';
        if S_AXI_BREADY = '1' then
          next_state <= complete;
        end if;

      when complete =>
        case combined_S_AXI_AWVALID_S_AXI_ARVALID is
          when "00"   => next_state <= idle;
          when others => next_state <= complete;
        end case;

      when others =>
        next_state <= reset;
    end case;
  end process;

  send_data_to_AXI_RDATA : process (send_read_data_to_AXI,
                                    local_address,
                                    local_address_valid,
                                    s_event_assert_en,
                                    s_event_deassert_en,
                                    s_event_assert_rearm,
                                    s_event_deassert_rearm,
                                    s_event_current_status,
                                    s_event_assert_status,
                                    s_event_deassert_status,
                                    s_ipmi_lgc_rst,
                                    event_irq_req,
                                    s_event_irq_ack,
                                    sensor_raw_reading,
                                    sensor_thr_status,
                                    s_dpram_isp_dout_r,
                                    s_cap_bram_dout,
                                    s_cap_bram_ctrl,
                                    cap_bram_wr_in_progress,
                                    cap_bram_addr_sensor
                                    )
  begin
    S_AXI_RDATA <= (others => '0');
    if (local_address_valid = '1' and send_read_data_to_AXI = '1') then
      case (local_address) is

        when C_IPMI_LGC_RST_ADDR    => S_AXI_RDATA(0) <= s_ipmi_lgc_rst;
        when C_EV_IRQ_REQ_ADDR      => S_AXI_RDATA(0) <= event_irq_req;
        when C_EV_IRQ_ACK_ADDR      => S_AXI_RDATA(0) <= s_event_irq_ack;
        when C_BRAM_CAP_CTRL_ADDR   => S_AXI_RDATA(1 downto 0) <= s_cap_bram_ctrl;

        when C_RAW_READING_ADDR to C_RAW_READING_ADDR+(64-1)*4 => S_AXI_RDATA(15 downto 0) <= sensor_raw_reading((local_address-C_RAW_READING_ADDR)/4);
        when C_THR_STATUS_ADDR to C_THR_STATUS_ADDR+(64-1)*4   => S_AXI_RDATA(7 downto 0)  <= "11" &sensor_thr_status((local_address-C_THR_STATUS_ADDR)/4);

        when C_EV_ASSERT_EN_ADDR to C_EV_ASSERT_EN_ADDR+(64-1)*4           => S_AXI_RDATA(11 downto 0) <= s_event_assert_en((local_address-C_EV_ASSERT_EN_ADDR)/4);
        when C_EV_DEASSERT_EN_ADDR to C_EV_DEASSERT_EN_ADDR+(64-1)*4       => S_AXI_RDATA(11 downto 0) <= s_event_deassert_en((local_address-C_EV_DEASSERT_EN_ADDR)/4);
        when C_EV_ASSERT_REARM_ADDR to C_EV_ASSERT_REARM_ADDR+(64-1)*4     => S_AXI_RDATA(11 downto 0) <= s_event_assert_rearm((local_address-C_EV_ASSERT_REARM_ADDR)/4);
        when C_EV_DEASSERT_REARM_ADDR to C_EV_DEASSERT_REARM_ADDR+(64-1)*4 => S_AXI_RDATA(11 downto 0) <= s_event_deassert_rearm((local_address-C_EV_DEASSERT_REARM_ADDR)/4);
        when C_EV_ASSERT_CUR_ST_ADDR to C_EV_ASSERT_CUR_ST_ADDR+(64-1)*4   => S_AXI_RDATA(11 downto 0) <= s_event_current_status((local_address-C_EV_ASSERT_CUR_ST_ADDR)/4);
        when C_EV_ASSERT_ST_ADDR to C_EV_ASSERT_ST_ADDR+(64-1)*4           => S_AXI_RDATA(11 downto 0) <= s_event_assert_status((local_address-C_EV_ASSERT_ST_ADDR)/4);
        when C_EV_DEASSERT_ST_ADDR to C_EV_DEASSERT_ST_ADDR+(64-1)*4       => S_AXI_RDATA(11 downto 0) <= s_event_deassert_status((local_address-C_EV_DEASSERT_ST_ADDR)/4);

        when C_BRAM_CAP_STAT_ADDR to C_BRAM_CAP_STAT_ADDR+(64-1)*4           => S_AXI_RDATA(0) <= cap_bram_wr_in_progress((local_address-C_BRAM_CAP_STAT_ADDR)/4);
        when C_BRAM_CAP_ADDR_ADDR to C_BRAM_CAP_ADDR_ADDR+(64-1)*4       => S_AXI_RDATA(9 downto 0) <= cap_bram_addr_sensor((local_address-C_BRAM_CAP_ADDR_ADDR)/4);

        when C_HYST_POS_ADDR to C_HYST_POS_ADDR+256-4 => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_HYST_POS);
        when C_HYST_NEG_ADDR to C_HYST_NEG_ADDR+256-4 => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_HYST_NEG);
        when C_UNR_ADDR to C_UNR_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_UNR);
        when C_UCR_ADDR to C_UCR_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_UCR);
        when C_UNC_ADDR to C_UNC_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_UNC);
        when C_LNR_ADDR to C_LNR_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_LNR);
        when C_LCR_ADDR to C_LCR_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_LCR);
        when C_LNC_ADDR to C_LNC_ADDR+256-4           => S_AXI_RDATA(15 downto 0) <= s_dpram_isp_dout_r(C_LNC);

        when C_BRAM_CAP_BASE+0*4096 to C_BRAM_CAP_BASE+1*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(0);
        when C_BRAM_CAP_BASE+1*4096 to C_BRAM_CAP_BASE+2*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(1);
        when C_BRAM_CAP_BASE+2*4096 to C_BRAM_CAP_BASE+3*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(2);
        when C_BRAM_CAP_BASE+3*4096 to C_BRAM_CAP_BASE+4*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(3);
        when C_BRAM_CAP_BASE+4*4096 to C_BRAM_CAP_BASE+5*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(4);
        when C_BRAM_CAP_BASE+5*4096 to C_BRAM_CAP_BASE+6*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(5);
        when C_BRAM_CAP_BASE+6*4096 to C_BRAM_CAP_BASE+7*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(6);
        when C_BRAM_CAP_BASE+7*4096 to C_BRAM_CAP_BASE+8*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(7);
        when C_BRAM_CAP_BASE+8*4096 to C_BRAM_CAP_BASE+9*4096-4  => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(8);
        when C_BRAM_CAP_BASE+9*4096 to C_BRAM_CAP_BASE+10*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(9);

        when C_BRAM_CAP_BASE+10*4096 to C_BRAM_CAP_BASE+11*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(10);
        when C_BRAM_CAP_BASE+11*4096 to C_BRAM_CAP_BASE+12*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(11);
        when C_BRAM_CAP_BASE+12*4096 to C_BRAM_CAP_BASE+13*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(12);
        when C_BRAM_CAP_BASE+13*4096 to C_BRAM_CAP_BASE+14*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(13);
        when C_BRAM_CAP_BASE+14*4096 to C_BRAM_CAP_BASE+15*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(14);
        when C_BRAM_CAP_BASE+15*4096 to C_BRAM_CAP_BASE+16*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(15);
        when C_BRAM_CAP_BASE+16*4096 to C_BRAM_CAP_BASE+17*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(16);
        when C_BRAM_CAP_BASE+17*4096 to C_BRAM_CAP_BASE+18*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(17);
        when C_BRAM_CAP_BASE+18*4096 to C_BRAM_CAP_BASE+19*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(18);
        when C_BRAM_CAP_BASE+19*4096 to C_BRAM_CAP_BASE+20*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(19);

        when C_BRAM_CAP_BASE+20*4096 to C_BRAM_CAP_BASE+21*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(20);
        when C_BRAM_CAP_BASE+21*4096 to C_BRAM_CAP_BASE+22*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(21);
        when C_BRAM_CAP_BASE+22*4096 to C_BRAM_CAP_BASE+23*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(22);
        when C_BRAM_CAP_BASE+23*4096 to C_BRAM_CAP_BASE+24*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(23);
        when C_BRAM_CAP_BASE+24*4096 to C_BRAM_CAP_BASE+25*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(24);
        when C_BRAM_CAP_BASE+25*4096 to C_BRAM_CAP_BASE+26*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(25);
        when C_BRAM_CAP_BASE+26*4096 to C_BRAM_CAP_BASE+27*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(26);
        when C_BRAM_CAP_BASE+27*4096 to C_BRAM_CAP_BASE+28*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(27);
        when C_BRAM_CAP_BASE+28*4096 to C_BRAM_CAP_BASE+29*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(28);
        when C_BRAM_CAP_BASE+29*4096 to C_BRAM_CAP_BASE+30*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(29);

        when C_BRAM_CAP_BASE+30*4096 to C_BRAM_CAP_BASE+31*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(30);
        when C_BRAM_CAP_BASE+31*4096 to C_BRAM_CAP_BASE+32*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(31);
        when C_BRAM_CAP_BASE+32*4096 to C_BRAM_CAP_BASE+33*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(32);
        when C_BRAM_CAP_BASE+33*4096 to C_BRAM_CAP_BASE+34*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(33);
        when C_BRAM_CAP_BASE+34*4096 to C_BRAM_CAP_BASE+35*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(34);
        when C_BRAM_CAP_BASE+35*4096 to C_BRAM_CAP_BASE+36*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(35);
        when C_BRAM_CAP_BASE+36*4096 to C_BRAM_CAP_BASE+37*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(36);
        when C_BRAM_CAP_BASE+37*4096 to C_BRAM_CAP_BASE+38*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(37);
        when C_BRAM_CAP_BASE+38*4096 to C_BRAM_CAP_BASE+39*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(38);
        when C_BRAM_CAP_BASE+39*4096 to C_BRAM_CAP_BASE+40*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(39);

        when C_BRAM_CAP_BASE+40*4096 to C_BRAM_CAP_BASE+41*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(40);
        when C_BRAM_CAP_BASE+41*4096 to C_BRAM_CAP_BASE+42*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(41);
        when C_BRAM_CAP_BASE+42*4096 to C_BRAM_CAP_BASE+43*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(42);
        when C_BRAM_CAP_BASE+43*4096 to C_BRAM_CAP_BASE+44*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(43);
        when C_BRAM_CAP_BASE+44*4096 to C_BRAM_CAP_BASE+45*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(44);
        when C_BRAM_CAP_BASE+45*4096 to C_BRAM_CAP_BASE+46*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(45);
        when C_BRAM_CAP_BASE+46*4096 to C_BRAM_CAP_BASE+47*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(46);
        when C_BRAM_CAP_BASE+47*4096 to C_BRAM_CAP_BASE+48*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(47);
        when C_BRAM_CAP_BASE+48*4096 to C_BRAM_CAP_BASE+49*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(48);
        when C_BRAM_CAP_BASE+49*4096 to C_BRAM_CAP_BASE+50*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(49);

        when C_BRAM_CAP_BASE+50*4096 to C_BRAM_CAP_BASE+51*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(50);
        when C_BRAM_CAP_BASE+51*4096 to C_BRAM_CAP_BASE+52*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(51);
        when C_BRAM_CAP_BASE+52*4096 to C_BRAM_CAP_BASE+53*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(52);
        when C_BRAM_CAP_BASE+53*4096 to C_BRAM_CAP_BASE+54*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(53);
        when C_BRAM_CAP_BASE+54*4096 to C_BRAM_CAP_BASE+55*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(54);
        when C_BRAM_CAP_BASE+55*4096 to C_BRAM_CAP_BASE+56*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(55);
        when C_BRAM_CAP_BASE+56*4096 to C_BRAM_CAP_BASE+57*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(56);
        when C_BRAM_CAP_BASE+57*4096 to C_BRAM_CAP_BASE+58*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(57);
        when C_BRAM_CAP_BASE+58*4096 to C_BRAM_CAP_BASE+59*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(58);
        when C_BRAM_CAP_BASE+59*4096 to C_BRAM_CAP_BASE+60*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(59);

        when C_BRAM_CAP_BASE+60*4096 to C_BRAM_CAP_BASE+61*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(60);
        when C_BRAM_CAP_BASE+61*4096 to C_BRAM_CAP_BASE+62*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(61);
        when C_BRAM_CAP_BASE+62*4096 to C_BRAM_CAP_BASE+63*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(62);
        when C_BRAM_CAP_BASE+63*4096 to C_BRAM_CAP_BASE+64*4096-4 => S_AXI_RDATA(15 downto 0) <= s_cap_bram_dout(63);

        when others => null;
      end case;
    end if;
  end process;

  local_address_capture_register : process (S_AXI_ACLK)
  begin
    if (S_AXI_ACLK'event and S_AXI_ACLK = '1') then
      if Local_Reset = '1' then
        local_address_slv <= (others => '0');
      else
        if local_address_valid = '1' then
          case (combined_S_AXI_AWVALID_S_AXI_ARVALID) is
            when "10"   => local_address_slv <= S_AXI_AWADDR(C_S_AXI_ADDR_WIDTH-1 downto 0);
            when "01"   => local_address_slv <= S_AXI_ARADDR(C_S_AXI_ADDR_WIDTH-1 downto 0);
            when others => local_address_slv <= local_address_slv;
          end case;
        end if;
      end if;
    end if;
  end process;

  local_address <= to_integer(unsigned(local_address_slv));

  p_ev_regs : process (S_AXI_ACLK)
  begin
    for i in 0 to C_SENSOR_CNT-1 loop
      if rising_edge(S_AXI_ACLK) then
        if Local_Reset = '1' then
        else
          if (s_event_assert_en_wr(i) = '1') then
            s_event_assert_en(i) <= S_AXI_WDATA(11 downto 0);
          end if;
          if (s_event_deassert_en_wr(i) = '1') then
            s_event_deassert_en(i) <= S_AXI_WDATA(11 downto 0);
          end if;
          if (s_event_assert_rearm_wr(i) = '1') then
            s_event_assert_rearm(i) <= S_AXI_WDATA(11 downto 0);
          end if;
          if (s_event_deassert_rearm_wr(i) = '1') then
            s_event_deassert_rearm(i) <= S_AXI_WDATA(11 downto 0);
          end if;
        end if;
      end if;
    end loop;
  end process;

  p_misc_wr_reg : process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then
      if Local_Reset = '1' then
      else
        if (s_ipmi_lgc_rst_wr = '1') then
          s_ipmi_lgc_rst <= S_AXI_WDATA(0);
        end if;

        if (s_event_irq_ack_wr = '1') then
          s_event_irq_ack <= S_AXI_WDATA(0);
        end if;

        if (s_cap_bram_ctrl_wr = '1') then
          s_cap_bram_ctrl <= S_AXI_WDATA(1 downto 0);
        end if;


      end if;
    end if;
  end process;

  address_range_analysis : process (local_address, write_enable_registers)
  begin

    local_address_valid <= '1';

    dpram_isp_we  <= (others => '0');
    s_cap_bram_we <= (others => '0');

    s_event_assert_en_wr      <= (others => '0');
    s_event_deassert_en_wr    <= (others => '0');
    s_event_assert_rearm_wr   <= (others => '0');
    s_event_deassert_rearm_wr <= (others => '0');
    s_ipmi_lgc_rst_wr         <= '0';
    s_event_irq_ack_wr        <= '0';
    s_cap_bram_ctrl_wr        <= '0';

    if write_enable_registers = '1' then
      case (local_address) is

        when C_IPMI_LGC_RST_ADDR => s_ipmi_lgc_rst_wr  <= '1';
        when C_EV_IRQ_ACK_ADDR   => s_event_irq_ack_wr <= '1';
        when C_BRAM_CAP_CTRL_ADDR   => s_cap_bram_ctrl_wr <= '1';

        when C_EV_ASSERT_EN_ADDR to C_EV_ASSERT_EN_ADDR+(64-1)*4           => s_event_assert_en_wr((local_address-C_EV_ASSERT_EN_ADDR)/4)           <= '1';
        when C_EV_DEASSERT_EN_ADDR to C_EV_DEASSERT_EN_ADDR+(64-1)*4       => s_event_deassert_en_wr((local_address-C_EV_DEASSERT_EN_ADDR)/4)       <= '1';
        when C_EV_ASSERT_REARM_ADDR to C_EV_ASSERT_REARM_ADDR+(64-1)*4     => s_event_assert_rearm_wr((local_address-C_EV_ASSERT_REARM_ADDR)/4)     <= '1';
        when C_EV_DEASSERT_REARM_ADDR to C_EV_DEASSERT_REARM_ADDR+(64-1)*4 => s_event_deassert_rearm_wr((local_address-C_EV_DEASSERT_REARM_ADDR)/4) <= '1';

        when C_HYST_POS_ADDR to C_HYST_POS_ADDR+256-4 => dpram_isp_we(C_HYST_POS) <= '1';
        when C_HYST_NEG_ADDR to C_HYST_NEG_ADDR+256-4 => dpram_isp_we(C_HYST_NEG) <= '1';
        when C_UNR_ADDR to C_UNR_ADDR+256-4           => dpram_isp_we(C_UNR)      <= '1';
        when C_UCR_ADDR to C_UCR_ADDR+256-4           => dpram_isp_we(C_UCR)      <= '1';
        when C_UNC_ADDR to C_UNC_ADDR+256-4           => dpram_isp_we(C_UNC)      <= '1';
        when C_LNR_ADDR to C_LNR_ADDR+256-4           => dpram_isp_we(C_LNR)      <= '1';
        when C_LCR_ADDR to C_LCR_ADDR+256-4           => dpram_isp_we(C_LCR)      <= '1';
        when C_LNC_ADDR to C_LNC_ADDR+256-4           => dpram_isp_we(C_LNC)      <= '1';

        when C_BRAM_CAP_BASE+0*4096 to C_BRAM_CAP_BASE+1*4096-4  => s_cap_bram_we(0) <= '1';
        when C_BRAM_CAP_BASE+1*4096 to C_BRAM_CAP_BASE+2*4096-4  => s_cap_bram_we(1) <= '1';
        when C_BRAM_CAP_BASE+2*4096 to C_BRAM_CAP_BASE+3*4096-4  => s_cap_bram_we(2) <= '1';
        when C_BRAM_CAP_BASE+3*4096 to C_BRAM_CAP_BASE+4*4096-4  => s_cap_bram_we(3) <= '1';
        when C_BRAM_CAP_BASE+4*4096 to C_BRAM_CAP_BASE+5*4096-4  => s_cap_bram_we(4) <= '1';
        when C_BRAM_CAP_BASE+5*4096 to C_BRAM_CAP_BASE+6*4096-4  => s_cap_bram_we(5) <= '1';
        when C_BRAM_CAP_BASE+6*4096 to C_BRAM_CAP_BASE+7*4096-4  => s_cap_bram_we(6) <= '1';
        when C_BRAM_CAP_BASE+7*4096 to C_BRAM_CAP_BASE+8*4096-4  => s_cap_bram_we(7) <= '1';
        when C_BRAM_CAP_BASE+8*4096 to C_BRAM_CAP_BASE+9*4096-4  => s_cap_bram_we(8) <= '1';
        when C_BRAM_CAP_BASE+9*4096 to C_BRAM_CAP_BASE+10*4096-4 => s_cap_bram_we(9) <= '1';

        when C_BRAM_CAP_BASE+10*4096 to C_BRAM_CAP_BASE+11*4096-4 => s_cap_bram_we(10) <= '1';
        when C_BRAM_CAP_BASE+11*4096 to C_BRAM_CAP_BASE+12*4096-4 => s_cap_bram_we(11) <= '1';
        when C_BRAM_CAP_BASE+12*4096 to C_BRAM_CAP_BASE+13*4096-4 => s_cap_bram_we(12) <= '1';
        when C_BRAM_CAP_BASE+13*4096 to C_BRAM_CAP_BASE+14*4096-4 => s_cap_bram_we(13) <= '1';
        when C_BRAM_CAP_BASE+14*4096 to C_BRAM_CAP_BASE+15*4096-4 => s_cap_bram_we(14) <= '1';
        when C_BRAM_CAP_BASE+15*4096 to C_BRAM_CAP_BASE+16*4096-4 => s_cap_bram_we(15) <= '1';
        when C_BRAM_CAP_BASE+16*4096 to C_BRAM_CAP_BASE+17*4096-4 => s_cap_bram_we(16) <= '1';
        when C_BRAM_CAP_BASE+17*4096 to C_BRAM_CAP_BASE+18*4096-4 => s_cap_bram_we(17) <= '1';
        when C_BRAM_CAP_BASE+18*4096 to C_BRAM_CAP_BASE+19*4096-4 => s_cap_bram_we(18) <= '1';
        when C_BRAM_CAP_BASE+19*4096 to C_BRAM_CAP_BASE+20*4096-4 => s_cap_bram_we(19) <= '1';

        when C_BRAM_CAP_BASE+20*4096 to C_BRAM_CAP_BASE+21*4096-4 => s_cap_bram_we(20) <= '1';
        when C_BRAM_CAP_BASE+21*4096 to C_BRAM_CAP_BASE+22*4096-4 => s_cap_bram_we(21) <= '1';
        when C_BRAM_CAP_BASE+22*4096 to C_BRAM_CAP_BASE+23*4096-4 => s_cap_bram_we(22) <= '1';
        when C_BRAM_CAP_BASE+23*4096 to C_BRAM_CAP_BASE+24*4096-4 => s_cap_bram_we(23) <= '1';
        when C_BRAM_CAP_BASE+24*4096 to C_BRAM_CAP_BASE+25*4096-4 => s_cap_bram_we(24) <= '1';
        when C_BRAM_CAP_BASE+25*4096 to C_BRAM_CAP_BASE+26*4096-4 => s_cap_bram_we(25) <= '1';
        when C_BRAM_CAP_BASE+26*4096 to C_BRAM_CAP_BASE+27*4096-4 => s_cap_bram_we(26) <= '1';
        when C_BRAM_CAP_BASE+27*4096 to C_BRAM_CAP_BASE+28*4096-4 => s_cap_bram_we(27) <= '1';
        when C_BRAM_CAP_BASE+28*4096 to C_BRAM_CAP_BASE+29*4096-4 => s_cap_bram_we(28) <= '1';
        when C_BRAM_CAP_BASE+29*4096 to C_BRAM_CAP_BASE+30*4096-4 => s_cap_bram_we(29) <= '1';

        when C_BRAM_CAP_BASE+30*4096 to C_BRAM_CAP_BASE+31*4096-4 => s_cap_bram_we(30) <= '1';
        when C_BRAM_CAP_BASE+31*4096 to C_BRAM_CAP_BASE+32*4096-4 => s_cap_bram_we(31) <= '1';
        when C_BRAM_CAP_BASE+32*4096 to C_BRAM_CAP_BASE+33*4096-4 => s_cap_bram_we(32) <= '1';
        when C_BRAM_CAP_BASE+33*4096 to C_BRAM_CAP_BASE+34*4096-4 => s_cap_bram_we(33) <= '1';
        when C_BRAM_CAP_BASE+34*4096 to C_BRAM_CAP_BASE+35*4096-4 => s_cap_bram_we(34) <= '1';
        when C_BRAM_CAP_BASE+35*4096 to C_BRAM_CAP_BASE+36*4096-4 => s_cap_bram_we(35) <= '1';
        when C_BRAM_CAP_BASE+36*4096 to C_BRAM_CAP_BASE+37*4096-4 => s_cap_bram_we(36) <= '1';
        when C_BRAM_CAP_BASE+37*4096 to C_BRAM_CAP_BASE+38*4096-4 => s_cap_bram_we(37) <= '1';
        when C_BRAM_CAP_BASE+38*4096 to C_BRAM_CAP_BASE+39*4096-4 => s_cap_bram_we(38) <= '1';
        when C_BRAM_CAP_BASE+39*4096 to C_BRAM_CAP_BASE+40*4096-4 => s_cap_bram_we(39) <= '1';

        when C_BRAM_CAP_BASE+40*4096 to C_BRAM_CAP_BASE+41*4096-4 => s_cap_bram_we(40) <= '1';
        when C_BRAM_CAP_BASE+41*4096 to C_BRAM_CAP_BASE+42*4096-4 => s_cap_bram_we(41) <= '1';
        when C_BRAM_CAP_BASE+42*4096 to C_BRAM_CAP_BASE+43*4096-4 => s_cap_bram_we(42) <= '1';
        when C_BRAM_CAP_BASE+43*4096 to C_BRAM_CAP_BASE+44*4096-4 => s_cap_bram_we(43) <= '1';
        when C_BRAM_CAP_BASE+44*4096 to C_BRAM_CAP_BASE+45*4096-4 => s_cap_bram_we(44) <= '1';
        when C_BRAM_CAP_BASE+45*4096 to C_BRAM_CAP_BASE+46*4096-4 => s_cap_bram_we(45) <= '1';
        when C_BRAM_CAP_BASE+46*4096 to C_BRAM_CAP_BASE+47*4096-4 => s_cap_bram_we(46) <= '1';
        when C_BRAM_CAP_BASE+47*4096 to C_BRAM_CAP_BASE+48*4096-4 => s_cap_bram_we(47) <= '1';
        when C_BRAM_CAP_BASE+48*4096 to C_BRAM_CAP_BASE+49*4096-4 => s_cap_bram_we(48) <= '1';
        when C_BRAM_CAP_BASE+49*4096 to C_BRAM_CAP_BASE+50*4096-4 => s_cap_bram_we(49) <= '1';

        when C_BRAM_CAP_BASE+50*4096 to C_BRAM_CAP_BASE+51*4096-4 => s_cap_bram_we(50) <= '1';
        when C_BRAM_CAP_BASE+51*4096 to C_BRAM_CAP_BASE+52*4096-4 => s_cap_bram_we(51) <= '1';
        when C_BRAM_CAP_BASE+52*4096 to C_BRAM_CAP_BASE+53*4096-4 => s_cap_bram_we(52) <= '1';
        when C_BRAM_CAP_BASE+53*4096 to C_BRAM_CAP_BASE+54*4096-4 => s_cap_bram_we(53) <= '1';
        when C_BRAM_CAP_BASE+54*4096 to C_BRAM_CAP_BASE+55*4096-4 => s_cap_bram_we(54) <= '1';
        when C_BRAM_CAP_BASE+55*4096 to C_BRAM_CAP_BASE+56*4096-4 => s_cap_bram_we(55) <= '1';
        when C_BRAM_CAP_BASE+56*4096 to C_BRAM_CAP_BASE+57*4096-4 => s_cap_bram_we(56) <= '1';
        when C_BRAM_CAP_BASE+57*4096 to C_BRAM_CAP_BASE+58*4096-4 => s_cap_bram_we(57) <= '1';
        when C_BRAM_CAP_BASE+58*4096 to C_BRAM_CAP_BASE+59*4096-4 => s_cap_bram_we(58) <= '1';
        when C_BRAM_CAP_BASE+59*4096 to C_BRAM_CAP_BASE+60*4096-4 => s_cap_bram_we(59) <= '1';

        when C_BRAM_CAP_BASE+60*4096 to C_BRAM_CAP_BASE+61*4096-4 => s_cap_bram_we(60) <= '1';
        when C_BRAM_CAP_BASE+61*4096 to C_BRAM_CAP_BASE+62*4096-4 => s_cap_bram_we(61) <= '1';
        when C_BRAM_CAP_BASE+62*4096 to C_BRAM_CAP_BASE+63*4096-4 => s_cap_bram_we(62) <= '1';
        when C_BRAM_CAP_BASE+63*4096 to C_BRAM_CAP_BASE+64*4096-4 => s_cap_bram_we(63) <= '1';

        when others =>
          local_address_valid <= '0';
      end case;
    end if;
  end process;

  gen_bram_signals : for idx in 0 to C_SENSOR_CNT-1 generate

    cap_bram_we(idx)     <= s_cap_bram_we(idx);
    s_cap_bram_dout(idx) <= cap_bram_dout(idx);
    cap_bram_din(idx)    <= S_AXI_WDATA(15 downto 0);
    cap_bram_addr(idx)   <= local_address_slv(11 downto 2);
  end generate;


  gen_dpram_signals : for idx in 0 to 7 generate
    dpram_isp_din(idx)  <= S_AXI_WDATA(15 downto 0);
    dpram_isp_addr(idx) <= local_address_slv(7 downto 2);
  end generate;

  event_assert_en      <= s_event_assert_en(C_SENSOR_CNT-1 downto 0);
  event_deassert_en    <= s_event_deassert_en(C_SENSOR_CNT-1 downto 0);
  event_assert_rearm   <= s_event_assert_rearm(C_SENSOR_CNT-1 downto 0);
  event_deassert_rearm <= s_event_deassert_rearm(C_SENSOR_CNT-1 downto 0);

  s_event_current_status(C_SENSOR_CNT-1 downto 0)  <= event_current_status;
  s_event_assert_status(C_SENSOR_CNT-1 downto 0)   <= event_assert_status;
  s_event_deassert_status(C_SENSOR_CNT-1 downto 0) <= event_deassert_status;

  ipmi_lgc_rst  <= s_ipmi_lgc_rst;
  event_irq_ack <= s_event_irq_ack;
  
  
  cap_bram_wr_start <= s_cap_bram_ctrl(0);
  cap_bram_wr_stop <= s_cap_bram_ctrl(1);


end ipmi_sens_proc_v1_0_S_AXI_arch;
