library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ad7689 is 
	generic (

        -- Parameters of Axi Slave Bus Interface s_axi
		C_s_axi_DATA_WIDTH	: integer	:= 32;
		C_s_axi_ADDR_WIDTH	: integer	:= 10;
		
		C_SLAVES : integer range 1 to 4 := 3;
		C_OUT_INTERFACE : string        := "PARALLEL";
		
		C_CHANNELS : integer            := 9;
		C_CHANNELS_OUT : integer        := 8
	);
	port (
        -- SPI interface
        spi_ncs : out std_logic_vector(C_SLAVES-1 downto 0);
        spi_clk : out std_logic;
        spi_mosi : out std_logic;
        spi_miso : in std_logic;
        
        -- ADC bus interface
        cnv_done : out std_logic;
        cnv_slave : out std_logic_vector(1 downto 0);
        cnv_channel : out std_logic_vector(3 downto 0);
        cnv_value : out std_logic_vector(15 downto 0);
        cnv_error : out std_logic;
        
        -- ADC parallel interface
        cnv_value_par : out std_logic_vector((16 * C_CHANNELS_OUT * C_SLAVES)-1 downto 0);
        cnv_valid_par : out std_logic_vector((C_CHANNELS_OUT * C_SLAVES)-1 downto 0);

		-- Ports of Axi Slave Bus Interface s_axi
		s_axi_aclk	    : in std_logic;
		s_axi_aresetn	: in std_logic;
		s_axi_awaddr	: in std_logic_vector(C_s_axi_ADDR_WIDTH-1 downto 0);
		s_axi_awprot	: in std_logic_vector(2 downto 0);
		s_axi_awvalid	: in std_logic;
		s_axi_awready	: out std_logic;
		s_axi_wdata	    : in std_logic_vector(C_s_axi_DATA_WIDTH-1 downto 0);
		s_axi_wstrb	    : in std_logic_vector((C_s_axi_DATA_WIDTH/8)-1 downto 0);
		s_axi_wvalid	: in std_logic;
		s_axi_wready	: out std_logic;
		s_axi_bresp	    : out std_logic_vector(1 downto 0);
		s_axi_bvalid	: out std_logic;
		s_axi_bready	: in std_logic;
		s_axi_araddr	: in std_logic_vector(C_s_axi_ADDR_WIDTH-1 downto 0);
		s_axi_arprot	: in std_logic_vector(2 downto 0);
		s_axi_arvalid	: in std_logic;
		s_axi_arready	: out std_logic;
		s_axi_rdata	    : out std_logic_vector(C_s_axi_DATA_WIDTH-1 downto 0);
		s_axi_rresp	    : out std_logic_vector(1 downto 0);
		s_axi_rvalid	: out std_logic;
		s_axi_rready	: in std_logic
	
	); 
end ad7689;
 
architecture arch_imp of ad7689 is
    type t_array16 is array (natural range <>) of std_logic_vector (15 downto 0);
    
    constant  c_ref_freq : integer := 50000000;
    
    -- interface signals
    signal s_mreset : std_logic;
    signal s_cnv_done : std_logic;
    signal s_cnv_slave : std_logic_vector (1 downto 0);
    signal s_cnv_channel : std_logic_vector (3 downto 0);
    signal s_cnv_value : std_logic_vector (15 downto 0);
    signal s_cnv_error : std_logic;
    
    signal d_cnv_value : std_logic_vector (15 downto 0);
    
    signal r_cnv_done : std_logic;
    signal r_cnv_slave : std_logic_vector (1 downto 0);
    signal r_cnv_channel : std_logic_vector (3 downto 0);
    signal r_cnv_value : std_logic_vector (15 downto 0);
    signal r_cnv_error : std_logic;
    
    signal s_measured_sample_freq :  std_logic_vector (31 downto 0); 
    signal s_ch2ch_sample_period :  std_logic_vector (31 downto 0);
    
    signal s_adc_override_en : std_logic_vector ((8 * C_SLAVES )-1 downto 0);
    signal s_adc_override_array : std_logic_vector ((16 * 8 * C_SLAVES )-1 downto 0);
    
    signal s_spi_ncs :  std_logic_vector (C_SLAVES-1 downto 0);
    signal s_spi_clk :  std_logic;
    signal s_spi_mosi :  std_logic;
    signal s_spi_miso :  std_logic;
    
    signal s_measured_freq_cnt : unsigned(31 downto 0);
    
    signal s_measured_sample_cnt_ch0 :  std_logic_vector (31 downto 0);
    
    
    --- NEW
    signal r_adc_reading_array : std_logic_vector ((16 * C_CHANNELS * C_SLAVES)-1 downto 0);

begin

    -- Instantiation of Axi Bus Interface s_axi
    ad7689_v0_1_s_axi_inst : entity work.ad7689_v0_1_s_axi
	generic map (
		C_S_AXI_DATA_WIDTH	=> C_s_axi_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_s_axi_ADDR_WIDTH,
		C_SLAVES => C_SLAVES
	)
	port map (
	
        reset => s_mreset,
        ch2ch_sample_period => s_ch2ch_sample_period,
        
        measured_sample_freq => s_measured_sample_freq,
        measured_sample_cnt_ch0 => s_measured_sample_cnt_ch0,
        
        m_adc_reading_array => r_adc_reading_array,
        m_adc_override_en => s_adc_override_en,
        m_adc_override_array => s_adc_override_array,

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

	ad7689_spi_intf_inst : entity work.ad7689_spi_intf
	generic map (
        C_SLAVES => C_SLAVES
    )
	port map (
        mclk_i => S_AXI_ACLK,
        mreset_i => s_mreset,
        
        -- SPI interface to the ADC
        spi_clk_o => s_spi_clk,
        spi_ncs_o => s_spi_ncs,
        spi_mosi_o => s_spi_mosi,
        spi_miso_i => s_spi_miso,
        
        u_req_sample_freq => s_ch2ch_sample_period,
        
        cnv_done_o => s_cnv_done,
        cnv_slave_o => s_cnv_slave,
        cnv_channel_o => s_cnv_channel,
        cnv_value_o => s_cnv_value,
        cnv_error_o => s_cnv_error
	);
	
	d_cnv_value <= s_adc_override_array((to_integer(unsigned(s_cnv_slave))*8 + (to_integer(unsigned(s_cnv_channel(2 downto 0)))+1))*16-1 downto
	                                    (to_integer(unsigned(s_cnv_slave))*8 + to_integer(unsigned(s_cnv_channel(2 downto 0))))*16)
	               when s_adc_override_en(to_integer(unsigned(s_cnv_slave))*8 + to_integer(unsigned(s_cnv_channel(2 downto 0)))) = '1' and s_cnv_channel(3) = '0' else s_cnv_value;
	
	process (s_axi_aclk) begin
	   if (rising_edge(s_axi_aclk)) then
            r_cnv_done <= s_cnv_done;
            r_cnv_value <= d_cnv_value;
            r_cnv_slave <= s_cnv_slave;
            r_cnv_channel <= s_cnv_channel;
            r_cnv_error <= s_cnv_error;
        end if;
    end process;
	
	GEN_LOOP_SLAVES: for I in 0 to C_SLAVES-1 generate
       GEN_LOOP_CHANNELS: for K in 0 to 8 generate
           process (s_axi_aclk) begin
               if (rising_edge(s_axi_aclk)) then
                   if (r_cnv_done = '1') then
                       if ((to_integer(unsigned(r_cnv_slave)) = I) and (to_integer(unsigned(r_cnv_channel)) = K)) then
                           r_adc_reading_array((I*C_CHANNELS + (K+1))*16-1 downto (I*C_CHANNELS + K)*16) <= r_cnv_value;
                       end if;
                   end if;
               end if;
           end process;
       end generate GEN_LOOP_CHANNELS;
    end generate GEN_LOOP_SLAVES;
	
    OUT_BUS_GENERATE: if C_OUT_INTERFACE = "BUS" generate
        -- Conv interface
        cnv_done <= r_cnv_done;
        cnv_slave <= r_cnv_slave;
        cnv_channel <= r_cnv_channel;
        cnv_value <= r_cnv_value;
        cnv_error <= r_cnv_error;
    end generate OUT_BUS_GENERATE;
    
    OUT_PARALLEL_GENERATE: if C_OUT_INTERFACE = "PARALLEL" generate
        -- Parallel bus interface
        process (s_axi_aclk) begin
            if rising_edge(s_axi_aclk) then
                cnv_value_par <= (others => '0');
                cnv_valid_par <= (others => '0');
                if (r_cnv_done = '1') then
                    for i in 0 to C_SLAVES-1 loop
                        for k in 0 to C_CHANNELS_OUT-1 loop
                            if ((to_integer(unsigned(r_cnv_slave)) = i) and (to_integer(unsigned(r_cnv_channel)) = k)) then
                                cnv_value_par((i*C_CHANNELS_OUT + (k+1))*16-1 downto (i*C_CHANNELS_OUT + k)*16) <= r_cnv_value;
                                cnv_valid_par(i*C_CHANNELS_OUT + k) <= '1'; 
                            end if;
                        end loop;
                    end loop;
                end if;
            end if;
        end process;
    end generate OUT_PARALLEL_GENERATE;
    
    -- Registered IO interface 
    process (s_axi_aclk) begin
        if (rising_edge(s_axi_aclk)) then
            spi_ncs <= s_spi_ncs;
            spi_clk <= s_spi_clk;
            spi_mosi <= s_spi_mosi;
            s_spi_miso <= spi_miso;
        end if;
    end process;
    
end arch_imp;
