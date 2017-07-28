library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ad7689_v0_1 is
	generic (
		-- Users to add parameters here

		-- User parameters ends
		-- Do not modify the parameters beyond this line


		-- Parameters of Axi Slave Bus Interface s_axi
		C_s_axi_DATA_WIDTH	: integer	:= 32;
		C_s_axi_ADDR_WIDTH	: integer	:= 7
	);
	port (
		-- Users to add ports here

        spi_ncs0 : out std_logic;
        spi_clk : out std_logic;
        spi_mosi : out std_logic;
        spi_miso : in std_logic;
        
        irq : out std_logic; -- Some threshold reached, check register

		-- User ports ends
		-- Do not modify the ports beyond this line


		-- Ports of Axi Slave Bus Interface s_axi
		s_axi_aclk	: in std_logic;
		s_axi_aresetn	: in std_logic;
		s_axi_awaddr	: in std_logic_vector(C_s_axi_ADDR_WIDTH-1 downto 0);
		s_axi_awprot	: in std_logic_vector(2 downto 0);
		s_axi_awvalid	: in std_logic;
		s_axi_awready	: out std_logic;
		s_axi_wdata	: in std_logic_vector(C_s_axi_DATA_WIDTH-1 downto 0);
		s_axi_wstrb	: in std_logic_vector((C_s_axi_DATA_WIDTH/8)-1 downto 0);
		s_axi_wvalid	: in std_logic;
		s_axi_wready	: out std_logic;
		s_axi_bresp	: out std_logic_vector(1 downto 0);
		s_axi_bvalid	: out std_logic;
		s_axi_bready	: in std_logic;
		s_axi_araddr	: in std_logic_vector(C_s_axi_ADDR_WIDTH-1 downto 0);
		s_axi_arprot	: in std_logic_vector(2 downto 0);
		s_axi_arvalid	: in std_logic;
		s_axi_arready	: out std_logic;
		s_axi_rdata	: out std_logic_vector(C_s_axi_DATA_WIDTH-1 downto 0);
		s_axi_rresp	: out std_logic_vector(1 downto 0);
		s_axi_rvalid	: out std_logic;
		s_axi_rready	: in std_logic
	);
end ad7689_v0_1;

architecture arch_imp of ad7689_v0_1 is
    type t_array16 is array (natural range <>) of std_logic_vector (15 downto 0);

    -- interface signals
    signal s_mreset : std_logic;
    signal s_cnv_done : std_logic;
    signal s_cnv_channel : std_logic_vector (3 downto 0);
    signal s_cnv_value : std_logic_vector (15 downto 0);
    signal s_cnv_error : std_logic;
    
    -- averager signals
    signal s_avg_din_valid : std_logic_vector (8 downto 0);
    signal s_avg_factor : std_logic_vector (17 downto 0);
    signal s_avg_out : t_array16 (0 to 8);
    signal s_avg_out_valid : std_logic_vector (8 downto 0);

    component ad7689_v0_1_s_axi
	generic (
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 7
	);
	port (
        u_reset_o : out std_logic; -- Auto-cleared reset from register
        u_avr_factor_o : out std_logic_vector (17 downto 0); -- Averaging factor per channel
        u_avg_channel0_i : in std_logic_vector (15 downto 0); -- ADC channel 0
        u_avg_channel1_i : in std_logic_vector (15 downto 0); -- ADC channel 1
        u_avg_channel2_i : in std_logic_vector (15 downto 0); -- ADC channel 2
        u_avg_channel3_i : in std_logic_vector (15 downto 0); -- ADC channel 3
        u_avg_channel4_i : in std_logic_vector (15 downto 0); -- ADC channel 4
        u_avg_channel5_i : in std_logic_vector (15 downto 0); -- ADC channel 5
        u_avg_channel6_i : in std_logic_vector (15 downto 0); -- ADC channel 6
        u_avg_channel7_i : in std_logic_vector (15 downto 0); -- ADC channel 7
        u_avg_channel8_i : in std_logic_vector (15 downto 0); -- ADC channel 8 (Temperature)
		S_AXI_ACLK	: in std_logic;
		S_AXI_ARESETN	: in std_logic;
		S_AXI_AWADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		S_AXI_AWPROT	: in std_logic_vector(2 downto 0);
		S_AXI_AWVALID	: in std_logic;
		S_AXI_AWREADY	: out std_logic;
		S_AXI_WDATA	: in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0); 
		S_AXI_WSTRB	: in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		S_AXI_WVALID	: in std_logic;
		S_AXI_WREADY	: out std_logic;
		S_AXI_BRESP	: out std_logic_vector(1 downto 0);
		S_AXI_BVALID	: out std_logic;
		S_AXI_BREADY	: in std_logic;
		S_AXI_ARADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		S_AXI_ARPROT	: in std_logic_vector(2 downto 0);
		S_AXI_ARVALID	: in std_logic;
		S_AXI_ARREADY	: out std_logic;
		S_AXI_RDATA	: out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		S_AXI_RRESP	: out std_logic_vector(1 downto 0);
		S_AXI_RVALID	: out std_logic;
		S_AXI_RREADY	: in std_logic
	);
    end component;
    
    component ad7689_spi_intf
    port ( mclk_i : in STD_LOGIC;
           mreset_i : in STD_LOGIC;
           spi_clk_o : out STD_LOGIC := '0';
           spi_ncs0_o : out STD_LOGIC := '0';
           spi_mosi_o : out STD_LOGIC := '0';
           spi_miso_i : in STD_LOGIC;
           cnv_done_o : out STD_LOGIC := '0';
           cnv_channel_o : out STD_LOGIC_VECTOR (3 downto 0) := (others => '0');
           cnv_value_o : out STD_LOGIC_VECTOR (15 downto 0) := (others => '0');
           cnv_error_o : out STD_LOGIC := '0'
         );
    end component;
    
    component averager
    port ( clk_i : in STD_LOGIC;
           reset_i : in STD_LOGIC;
           din_i : in STD_LOGIC_VECTOR (15 downto 0);
           din_valid_i : in STD_LOGIC;
           avg_factor_i : in STD_LOGIC_VECTOR (1 downto 0);
           avg_out_o : out STD_LOGIC_VECTOR (15 downto 0);
           avg_out_valid_o : out STD_LOGIC);
    end component;


begin

-- Instantiation of Axi Bus Interface s_axi
ad7689_v0_1_s_axi_inst : entity work.ad7689_v0_1_s_axi
	generic map (
		C_S_AXI_DATA_WIDTH	=> C_s_axi_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_s_axi_ADDR_WIDTH
	)
	port map (
	
        u_reset_o => s_mreset,
        u_avr_factor_o => s_avg_factor,
        u_avg_channel0_i => s_avg_out(0),
        u_avg_channel1_i => s_avg_out(1),
        u_avg_channel2_i => s_avg_out(2),
        u_avg_channel3_i => s_avg_out(3),
        u_avg_channel4_i => s_avg_out(4),
        u_avg_channel5_i => s_avg_out(5),
        u_avg_channel6_i => s_avg_out(6),
        u_avg_channel7_i => s_avg_out(7),
        u_avg_channel8_i => s_avg_out(8),
        
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

	-- Add user logic here
	
	ad7689_spi_intf_inst : entity work.ad7689_spi_intf
	port map (
        mclk_i => S_AXI_ACLK,
        mreset_i => s_mreset,
        
        -- SPI interface to the ADC
        spi_clk_o => spi_clk,
        spi_ncs0_o => spi_ncs0,
        spi_mosi_o => spi_mosi,
        spi_miso_i => spi_miso,
        
        -- User interface
        cnv_done_o => s_cnv_done,
        cnv_channel_o => s_cnv_channel,
        cnv_value_o => s_cnv_value,
        cnv_error_o => s_cnv_error
	);
	
	GEN_AVERAGER : for I in 0 to 8 generate begin
	    s_avg_din_valid (I) <= '1' when (s_cnv_done = '1') and (to_integer(unsigned(s_cnv_channel)) = I) else '0';
	
        averager_inst : entity work.averager
        port map (
            clk_i => S_AXI_ACLK,
            reset_i => s_mreset,
            din_i => s_cnv_value,
            din_valid_i => s_avg_din_valid(I),
            avg_factor_i => s_avg_factor((I+1)*2-1 downto I*2),
            avg_out_o => s_avg_out(I),
            avg_out_valid_o => s_avg_out_valid(I)
        );
	end generate GEN_AVERAGER;
	
	GEN_THRESHOLDS : for I in 0 to 8 generate begin
	   -- TODO
	end generate GEN_THRESHOLDS;

    irq <= '0'; -- TODO

	-- User logic ends

end arch_imp;
