library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.pyld_pwr_ctrl_pkg.all;  
 
entity pyld_pwr_ctrl_v1_0 is
	generic (

		C_CORE_VER	:  std_logic_vector(31 downto 0) := x"00000002";
        C_PE_PIN_CNT : integer	:= 16;  -- number of Power Enable output pins
        C_PG_PIN_CNT : integer	:= 16;  -- number of Power Good input pins

		-- Parameters of Axi Slave Bus Interface S_AXI
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 8
	);
	port (

       PE_pin_o : out std_logic_vector(C_PE_PIN_CNT - 1  downto 0);
       PG_pin_i : in std_logic_vector(C_PG_PIN_CNT - 1  downto 0);
       PD_ext_req_i : in STD_LOGIC;

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
end pyld_pwr_ctrl_v1_0;

architecture arch_imp of pyld_pwr_ctrl_v1_0 is

    signal s_pd_sw_req : std_logic;

    signal s_pd_ext_en : std_logic_vector(C_PE_PIN_CNT-1 downto 0);
    signal s_pd_sw_en : std_logic_vector(C_PE_PIN_CNT-1 downto 0);
    
    signal s_grp_cfg : t_slv_arr_3(C_PE_PIN_CNT-1 downto 0);
    signal s_pud_seq_tmr_cfg : t_slv_arr_32(C_PE_PIN_CNT-1 downto 0);
    signal s_pd_seq_init : std_logic_vector(6 downto 0);
    signal s_pu_seq_init : std_logic_vector(6 downto 0);

    signal s_pe_pin_cfg :  t_slv_arr_6(C_PE_PIN_CNT-1 downto 0);

    signal s_pe : std_logic_vector(C_PE_PIN_CNT-1 downto 0);
    
begin 

-- Instantiation of Axi Bus Interface S_AXI
i_pyld_pwr_ctrl_v1_0_S_AXI : entity work.pyld_pwr_ctrl_v1_0_S_AXI
	generic map (
		C_CORE_VER	=> C_CORE_VER,
        C_PE_PIN_CNT => C_PE_PIN_CNT,
        C_PG_PIN_CNT => C_PG_PIN_CNT,
        C_S_AXI_DATA_WIDTH	=> C_S_AXI_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_S_AXI_ADDR_WIDTH
	)
	port map (
	
        pd_sw_req_o => s_pd_sw_req,
        pe_pin_cfg_o => s_pe_pin_cfg,
        pud_seq_tmr_cfg_o => s_pud_seq_tmr_cfg,
        pd_seq_init_o => s_pd_seq_init,
        pu_seq_init_o => s_pu_seq_init,
        pe_i => s_pe,
        pg_i => PG_pin_i,
    	
		S_AXI_ACLK	=> s_axi_aclk,
		S_AXI_ARESETN	=> s_axi_aresetn,
		S_AXI_AWADDR	=> s_axi_awaddr,
		S_AXI_AWPROT	=> s_axi_awprot,
		S_AXI_AWVALID	=> s_axi_awvalid,
		S_AXI_AWREADY	=> s_axi_awready,
		S_AXI_WDATA	=> s_axi_wdata,
		S_AXI_WSTRB	=> s_axi_wstrb,
		S_AXI_WVALID	=> s_axi_wvalid,
		S_AXI_WREADY	=> s_axi_wready,
		S_AXI_BRESP	=> s_axi_bresp,
		S_AXI_BVALID	=> s_axi_bvalid,
		S_AXI_BREADY	=> s_axi_bready,
		S_AXI_ARADDR	=> s_axi_araddr,
		S_AXI_ARPROT	=> s_axi_arprot,
		S_AXI_ARVALID	=> s_axi_arvalid,
		S_AXI_ARREADY	=> s_axi_arready,
		S_AXI_RDATA	=> s_axi_rdata,
		S_AXI_RRESP	=> s_axi_rresp,
		S_AXI_RVALID	=> s_axi_rvalid,
		S_AXI_RREADY	=> s_axi_rready
	);
	
  gen_pyld_pwr : for idx in 0 to C_PE_PIN_CNT-1 generate
  
        s_grp_cfg(idx)   <= s_pe_pin_cfg(idx)(2 downto 0);
        s_pd_sw_en(idx)  <= s_pe_pin_cfg(idx)(4);
        s_pd_ext_en(idx) <= s_pe_pin_cfg(idx)(5);

        i_pyld_pwr : entity work.pyld_pwr
           port map( 
               clk_i => s_axi_aclk,
               pd_ext_en_i  => s_pd_ext_en(idx),
               pd_ext_req_i  => PD_ext_req_i,
               pd_sw_en_i  => s_pd_sw_en(idx),
               pd_sw_req_i  => s_pd_sw_req,
               grp_cfg_i  => s_grp_cfg(idx),
               pud_seq_tmr_cfg_i  => s_pud_seq_tmr_cfg(idx),
               pd_seq_init_i  => s_pd_seq_init,
               pu_seq_init_i  => s_pu_seq_init,
               pe_o  => s_pe(idx)
               );
   end generate;
   
   PE_pin_o <= s_pe;
   
end arch_imp;
