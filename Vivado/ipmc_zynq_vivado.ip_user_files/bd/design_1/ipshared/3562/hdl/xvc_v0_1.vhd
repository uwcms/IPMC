library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity xvc_v0_1 is
	generic (
		-- Users to add parameters here

		-- User parameters ends
		-- Do not modify the parameters beyond this line


		-- Parameters of Axi Slave Bus Interface S_AXI
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 12
	);
	port (
		-- Users to add ports here
		
        zjc_trst : out STD_LOGIC;
        zjc_tdi  : out STD_LOGIC;
        zjc_tms  : out STD_LOGIC;
        zjc_tck : out STD_LOGIC;
        zjc_tdo : in STD_LOGIC;

		-- User ports ends
		-- Do not modify the ports beyond this line


		-- Ports of Axi Slave Bus Interface S_AXI
		s_axi_aclk	: in std_logic;
		s_axi_aresetn	: in std_logic;
		s_axi_awaddr	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		s_axi_awprot	: in std_logic_vector(2 downto 0);
		s_axi_awvalid	: in std_logic;
		s_axi_awready	: out std_logic;
		s_axi_wdata	: in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		s_axi_wstrb	: in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		s_axi_wvalid	: in std_logic;
		s_axi_wready	: out std_logic;
		s_axi_bresp	: out std_logic_vector(1 downto 0);
		s_axi_bvalid	: out std_logic;
		s_axi_bready	: in std_logic;
		s_axi_araddr	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		s_axi_arprot	: in std_logic_vector(2 downto 0);
		s_axi_arvalid	: in std_logic;
		s_axi_arready	: out std_logic;
		s_axi_rdata	: out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		s_axi_rresp	: out std_logic_vector(1 downto 0);
		s_axi_rvalid	: out std_logic;
		s_axi_rready	: in std_logic
	);
end xvc_v0_1;

architecture arch_imp of xvc_v0_1 is

    COMPONENT axi_bram_ctrl_0
      PORT (
        s_axi_aclk : IN STD_LOGIC;
        s_axi_aresetn : IN STD_LOGIC;
        s_axi_awaddr : IN STD_LOGIC_VECTOR(11 DOWNTO 0);
        s_axi_awprot : IN STD_LOGIC_VECTOR(2 DOWNTO 0);
        s_axi_awvalid : IN STD_LOGIC;
        s_axi_awready : OUT STD_LOGIC;
        s_axi_wdata : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        s_axi_wstrb : IN STD_LOGIC_VECTOR(3 DOWNTO 0);
        s_axi_wvalid : IN STD_LOGIC;
        s_axi_wready : OUT STD_LOGIC;
        s_axi_bresp : OUT STD_LOGIC_VECTOR(1 DOWNTO 0);
        s_axi_bvalid : OUT STD_LOGIC;
        s_axi_bready : IN STD_LOGIC;
        s_axi_araddr : IN STD_LOGIC_VECTOR(11 DOWNTO 0);
        s_axi_arprot : IN STD_LOGIC_VECTOR(2 DOWNTO 0);
        s_axi_arvalid : IN STD_LOGIC;
        s_axi_arready : OUT STD_LOGIC;
        s_axi_rdata : OUT STD_LOGIC_VECTOR(31 DOWNTO 0);
        s_axi_rresp : OUT STD_LOGIC_VECTOR(1 DOWNTO 0);
        s_axi_rvalid : OUT STD_LOGIC;
        s_axi_rready : IN STD_LOGIC;
        bram_rst_a : OUT STD_LOGIC;
        bram_clk_a : OUT STD_LOGIC;
        bram_en_a : OUT STD_LOGIC;
        bram_we_a : OUT STD_LOGIC_VECTOR(3 DOWNTO 0);
        bram_addr_a : OUT STD_LOGIC_VECTOR(11 DOWNTO 0);
        bram_wrdata_a : OUT STD_LOGIC_VECTOR(31 DOWNTO 0);
        bram_rddata_a : IN STD_LOGIC_VECTOR(31 DOWNTO 0)
      );
    END COMPONENT;
    
    signal din : STD_LOGIC_VECTOR (31 downto 0);
    signal dout : STD_LOGIC_VECTOR (31 downto 0);
    signal addrin : STD_LOGIC_VECTOR (11 downto 0);
    signal psclkin : STD_LOGIC;
    signal psrstin : STD_LOGIC;
    signal psenain : STD_LOGIC;
    signal pswenin : STD_LOGIC_VECTOR (3 downto 0);
    
    --address decode constants
    constant CFGADDR : std_logic_vector(11 downto 0) := X"000";
    constant TMSADDR : std_logic_vector(11 downto 0) := X"004";
    constant TDIADDR : std_logic_vector(11 downto 0) := X"008";
    constant TDOADDR : std_logic_vector(11 downto 0) := X"00c";
    constant CMDADDR : std_logic_vector(11 downto 0) := X"010";
    
    signal Doutsample_ev, Doutshift_ev, Dincapture_ev, Dinshift_ev : STD_LOGIC;
    signal tckDout, tdiDout, tmsDout, tdoDin : STD_LOGIC;
    signal avalid_tdo, avalid_tdi, avalid_tms, avalid_cfg, avalid_cmd : STD_LOGIC;
    signal tmsrdata, tdirdata, tdordata, cfgrdata, cmdrdata : STD_LOGIC_VECTOR(31 downto 0);
    signal zjc_enable_b_sig, zjc_tdi_sig, zjc_tms_sig, zjc_tck_sig, zjc_tdo_sig : STD_LOGIC;
    signal ckdown_width, ckup_width, bit_count : STD_LOGIC_VECTOR(7 downto 0);
    signal output_rst, rststb : STD_LOGIC;

begin

-- Instantiation of Axi Bus Interface S_AXI
axi_bram_ctrl_inst : axi_bram_ctrl_0
PORT MAP (
    s_axi_aclk => s_axi_aclk,
    s_axi_aresetn => s_axi_aresetn,
    s_axi_awaddr => s_axi_awaddr,
    s_axi_awprot => s_axi_awprot,
    s_axi_awvalid => s_axi_awvalid,
    s_axi_awready => s_axi_awready,
    s_axi_wdata => s_axi_wdata,
    s_axi_wstrb => s_axi_wstrb,
    s_axi_wvalid => s_axi_wvalid,
    s_axi_wready => s_axi_wready,
    s_axi_bresp => s_axi_bresp,
    s_axi_bvalid => s_axi_bvalid,
    s_axi_bready => s_axi_bready,
    s_axi_araddr => s_axi_araddr,
    s_axi_arprot => s_axi_arprot,
    s_axi_arvalid => s_axi_arvalid,
    s_axi_arready => s_axi_arready,
    s_axi_rdata => s_axi_rdata,
    s_axi_rresp => s_axi_rresp,
    s_axi_rvalid => s_axi_rvalid,
    s_axi_rready => s_axi_rready,
    bram_rst_a => psrstin,
    bram_clk_a => psclkin,
    bram_en_a => psenain,
    bram_we_a => pswenin,
    bram_addr_a => addrin,
    bram_wrdata_a => din,
    bram_rddata_a => dout
);

--I/O buffers for JTAG interface
  zjc_tdo_sig <= zjc_tdo;
    --  --TDO pin, input, LVCMOS18, Bank 12, Pin Y17
    --  ZJCTDO_i:  IBUF
    --     generic map (
    --        IBUF_LOW_PWR => TRUE, -- Low power (TRUE) vs. performance (FALSE) setting for referenced I/O standards
    --        IOSTANDARD => "LVCMOS18")
    --     port map (O => zjc_tdo_sig,
    --        I => zjc_tdo);
   zjc_tdi <= zjc_tdi_sig;
    --  --TDI pin, output, LVCMOS18, Bank 12, Pin AB17
    --  ZJCTDI_i: OBUF
    --    generic map (
    --      DRIVE => 12,
    --      IOSTANDARD => "LVCMOS18",
    --      SLEW => "SLOW")
    --    port map (O => zjc_tdi,
    --      I => zjc_tdi_sig);
   zjc_tms <= zjc_tms_sig;
    --  --TMS pin, output, LVCMOS18, Bank 12, Pin AC16
    --  ZJCTMS_i: OBUF
    --    generic map (
    --      DRIVE => 12,
    --      IOSTANDARD => "LVCMOS18",
    --      SLEW => "SLOW")
    --    port map (O => zjc_tms,
    --      I => zjc_tms_sig);
   zjc_tck <= zjc_tck_sig;
    --  --TCK pin, output, LVCMOS18, Bank 12, Pin AB16
    --  ZJCTCK_i: OBUF
    --    generic map (
    --      DRIVE => 12,
    --      IOSTANDARD => "LVCMOS18",
    --      SLEW => "SLOW")
    --    port map (O => zjc_tck,
    --      I => zjc_tck_sig);
    
   zjc_trst <= zjc_enable_b_sig;
    --  --Zynq-based JTAG Enable pin, output, LVCMOS33, Bank 13, Pin AC21    
    --  ZJC_ENABLE_B_i: OBUF
    --    generic map (
    --      DRIVE => 12,
    --      IOSTANDARD => "LVCMOS33",
    --      SLEW => "SLOW")
    --    port map (O => zjc_trst,
    --      I => zjc_enable_b_sig);

  --enabled registers for the JTAG pins
  TDIPINFF:entity work.enaff
    Port Map ( sck => psclkin,
           srst => output_rst,
           din => tdiDout,
           senain => Doutsample_ev,
           qout => zjc_tdi_sig);

  TMSPINFF:entity work.enaff
    Port Map ( sck => psclkin,
           srst => output_rst,
           din => tmsDout,
           senain => Doutsample_ev,
           qout => zjc_tms_sig);

  TDOPINFF:entity work.enaff
    Port Map ( sck => psclkin,
       srst => psrstin,
       din => zjc_tdo_sig, 
       senain => Dincapture_ev,
       qout => tdoDin);

  TDIREG:entity work.rwshiftreg
    Port Map (din => din,
          dout => tdirdata,
          avalidin => avalid_tdi,
          psclkin => psclkin,
          psrstin => psrstin,
          psenain => psenain,
          pswenin => pswenin,
          shift_ev => Doutshift_ev,
          sdatain => '0',
          sdataout => tdiDout);

  TMSREG:entity work.rwshiftreg
    Port Map (din => din,
          dout => tmsrdata,
          avalidin => avalid_tms,
          psclkin => psclkin,
          psrstin => psrstin,
          psenain => psenain,
          pswenin => pswenin,
          shift_ev => Doutshift_ev,
          sdatain => '0',
          sdataout => tmsDout);

  TDOREG:entity work.rwshiftreg
    Port Map (din => din,
          dout => tdordata,
          avalidin => avalid_tdo,
          psclkin => psclkin,
          psrstin => psrstin,
          psenain => psenain,
          pswenin => pswenin,
          shift_ev => Dinshift_ev,
          sdatain => tdoDin,
          sdataout => open);

  CFGREG:entity work.rwreg
    Port Map (din => din,
          dout => cfgrdata,
          avalidin => avalid_cfg,
          psclkin => psclkin,
          psrstin => psrstin,
          psenain => psenain,
          pswenin => pswenin);
  
  FSM:entity work.XVCfsm
    Port Map (
            din => din,
            dout => cmdrdata,
            avalidin => avalid_cmd, 
            psclkin => psclkin,
            psrstin => psrstin,
            psenain => psenain,
            pswenin => pswenin,
            --config register with params
            bit_count => bit_count,
            ckdown_width => ckdown_width,
            ckup_width => ckup_width,
            --interface signals to I/O FFs
            rststb => rststb,
            tck => zjc_tck_sig,
            Doutsample_ev => Doutsample_ev,
            Doutshift_ev => Doutshift_ev,
            Dincapture_ev => Dincapture_ev,
            Dinshift_ev => Dinshift_ev,
            DEBUG_PORT => open);

  output_rst <= rststb and psrstin;
  
  --config reg signals
  bit_count <= cfgrdata(7 downto 0);
  ckdown_width <= cfgrdata(15 downto 8);
  ckup_width <= cfgrdata(23 downto 16);
  zjc_enable_b_sig <= not cfgrdata(31);                         -- msb = enable, active-high
  
  --address valid decodes for write operations
  avalid_tdo <= '1' when (addrin = TDOADDR) else '0';
  avalid_tdi <= '1' when (addrin = TDIADDR) else '0';
  avalid_tms <= '1' when (addrin = TMSADDR) else '0';
  avalid_cfg <= '1' when (addrin = CFGADDR) else '0';
  avalid_cmd <= '1' when (addrin = CMDADDR) else '0';

  --read data mux
  with addrin select
    dout <= cfgrdata when CFGADDR,
             tmsrdata when TMSADDR,
             tdirdata when TDIADDR, 
             tdordata when TDOADDR,
             cmdrdata when CMDADDR,
             (others => '0') when others;

end arch_imp;
