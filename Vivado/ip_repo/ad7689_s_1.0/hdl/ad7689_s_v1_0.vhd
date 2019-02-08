library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ad7689 is 
	generic (

        -- Parameters of Axi Slave Bus Interface s_axi
		C_s_axi_DATA_WIDTH	: integer	:= 32;
		C_s_axi_ADDR_WIDTH	: integer	:= 9;
		
		C_SLAVES : integer range 1 to 4 := 1;
		C_OUT_INTERFACE : string        := "PARALLEL"
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
        cnv_value_par : out std_logic_vector((16 * 9 * C_SLAVES)-1 downto 0);
        cnv_valid_par : out std_logic_vector((9 * C_SLAVES)-1 downto 0);

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
    
    signal s_ch_value_demuxed : t_array16 (0 to 8);
    signal s_ch_valid_demuxed : std_logic_vector (8 downto 0);
    
    signal s_measured_sample_freq :  std_logic_vector (31 downto 0); 
    signal s_ch2ch_sample_period :  std_logic_vector (31 downto 0);
    
    signal s_ovrrd_enables :  std_logic_vector (8 downto 0);
    
    signal s_ovrrd_reading_ch :  t_array16 (0 to 8);
    
    signal s_spi_ncs :  std_logic_vector (C_SLAVES-1 downto 0);
    signal s_spi_clk :  std_logic;
    signal s_spi_mosi :  std_logic;
    signal s_spi_miso :  std_logic;
        
    signal s_ref_freq_cnt : unsigned(31 downto 0);
    signal s_ref_freq_cnt_reset : std_logic;
    
    signal s_measured_freq_cnt : unsigned(31 downto 0);
    
    signal s_measured_sample_cnt_ch0 :  std_logic_vector (31 downto 0);
    
    
    --- NEW
    signal r_adc_reading_array : std_logic_vector ((16 * 9 * C_SLAVES)-1 downto 0);

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
	
    process (s_axi_aclk) begin
        if rising_edge(s_axi_aclk) then
            if (s_cnv_done = '1') then
                for i in 0 to C_SLAVES-1 loop
                    for k in 0 to 8 loop
                        if ((to_integer(unsigned(s_cnv_slave)) = i) and (to_integer(unsigned(s_cnv_channel)) = k)) then
                            r_adc_reading_array((i*9 + (k+1))*16-1 downto (i*9 + k)*16) <= s_cnv_value;
                        end if;
                    end loop;
                end loop;
            end if;
        end if;
    end process;
	
--	gen_ch_demux : for idx in 0 to 8 generate begin
	     
--	    process(s_axi_aclk) is
--	    begin
--	       if rising_edge(s_axi_aclk) then
--	          if ((s_cnv_done = '1') and (to_integer(unsigned(s_cnv_channel)) = idx)) then
	             
--	             s_ch_valid_demuxed (idx) <= '1';
	             
--	             if (s_ovrrd_enables(idx) = '0') then
--	               s_ch_value_demuxed(idx) <= s_cnv_value;
--	             else
--	               s_ch_value_demuxed(idx) <= s_ovrrd_reading_ch(idx);
--	             end if;
	             
--	          else
--	             s_ch_valid_demuxed (idx) <= '0';
--	          end if;
--	       end if;
--	    end process;

--	end generate gen_ch_demux;


--    process(s_axi_aclk) is
--    begin
    
--    if rising_edge(s_axi_aclk) then
--       s_ref_freq_cnt <= s_ref_freq_cnt + 1;
       
--       s_ref_freq_cnt_reset <= '0';
    
--       if (s_ref_freq_cnt = to_unsigned(c_ref_freq, 32)) then
--           s_ref_freq_cnt <= x"00000000";
--           s_ref_freq_cnt_reset <= '1';
--       end if;
--    end if;
--    end process;

--    process(s_axi_aclk) is
--    begin
    
--    if rising_edge(s_axi_aclk) then
    
--       if (s_ch_valid_demuxed(0) = '1') then
--            s_measured_freq_cnt <= s_measured_freq_cnt + 1;
--       end if;
       
--       if (s_ref_freq_cnt_reset = '1') then
--           s_measured_freq_cnt <= x"00000000";
--           s_measured_sample_freq <= std_logic_vector(s_measured_freq_cnt);
--       end if;
--    end if;
--    end process;
   
--    process(s_axi_aclk) is
--    begin
    
--        if rising_edge(s_axi_aclk) then
        
--           if (s_mreset = '1') then
--                s_measured_sample_cnt_ch0 <= (others => '0');
--           elsif (s_ch_valid_demuxed(0) = '1') then
--                s_measured_sample_cnt_ch0 <= std_logic_vector(unsigned(s_measured_sample_cnt_ch0) + 1);
--           end if;
      
--        end if;
--    end process; 
    
    OUT_BUS_GENERATE: if C_OUT_INTERFACE = "BUS" generate
        -- Conv interface
        cnv_done <= s_cnv_done;
        cnv_slave <= s_cnv_slave;
        cnv_channel <= s_cnv_channel;
        cnv_value <= s_cnv_value;
        cnv_error <= s_cnv_error;
    end generate OUT_BUS_GENERATE;
    
    OUT_PARALLEL_GENERATE: if C_OUT_INTERFACE = "PARALLEL" generate
        -- Parallel bus interface
        process (s_axi_aclk) begin
            if rising_edge(s_axi_aclk) then
                if (s_cnv_done = '1') then
                    cnv_value_par <= (others => '0');
                    cnv_valid_par <= (others => '0');
                    for i in 0 to C_SLAVES-1 loop
                        for k in 0 to 8 loop
                            if ((to_integer(unsigned(s_cnv_slave)) = i) and (to_integer(unsigned(s_cnv_channel)) = k)) then
                                cnv_value_par((i*9 + (k+1))*16-1 downto (i*9 + k)*16) <= s_cnv_value;
                                cnv_valid_par(i*9 + k) <= '1'; 
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
