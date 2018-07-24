library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ad7689_v0_1_s_axi is
	generic (
		-- Width of S_AXI data bus
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		-- Width of S_AXI address bus
		C_S_AXI_ADDR_WIDTH	: integer	:= 7
	);
	port (

        reset : out std_logic; 
        ch2ch_sample_period : out std_logic_vector (31 downto 0); -- Channel to channel sample delay (-> sampling frequency)
        measured_sample_freq : in std_logic_vector (31 downto 0); -- Per channel actual sample frequency
        measured_sample_cnt_ch0 : in std_logic_vector (31 downto 0); -- number of ch0 conv samples (w/ reset, counter waraps around)

        adc_reading_ch0 : in std_logic_vector (15 downto 0); -- ADC channel 0
        adc_reading_ch1 : in std_logic_vector (15 downto 0); -- ADC channel 1
        adc_reading_ch2 : in std_logic_vector (15 downto 0); -- ADC channel 2
        adc_reading_ch3 : in std_logic_vector (15 downto 0); -- ADC channel 3
        adc_reading_ch4 : in std_logic_vector (15 downto 0); -- ADC channel 4
        adc_reading_ch5 : in std_logic_vector (15 downto 0); -- ADC channel 5
        adc_reading_ch6 : in std_logic_vector (15 downto 0); -- ADC channel 6
        adc_reading_ch7 : in std_logic_vector (15 downto 0); -- ADC channel 7
        adc_reading_ch8 : in std_logic_vector (15 downto 0); -- ADC channel 8 (Temperature)
        
        ovrrd_enables : out std_logic_vector (8 downto 0); -- override enables

        ovrrd_reading_ch0 : out std_logic_vector (15 downto 0); -- ovrrd channel 0
        ovrrd_reading_ch1 : out std_logic_vector (15 downto 0); -- ovrrd channel 1
        ovrrd_reading_ch2 : out std_logic_vector (15 downto 0); -- ovrrd channel 2
        ovrrd_reading_ch3 : out std_logic_vector (15 downto 0); -- ovrrd channel 3
        ovrrd_reading_ch4 : out std_logic_vector (15 downto 0); -- ovrrd channel 4
        ovrrd_reading_ch5 : out std_logic_vector (15 downto 0); -- ovrrd channel 5
        ovrrd_reading_ch6 : out std_logic_vector (15 downto 0); -- ovrrd channel 6
        ovrrd_reading_ch7 : out std_logic_vector (15 downto 0); -- ovrrd channel 7
        ovrrd_reading_ch8 : out std_logic_vector (15 downto 0); -- ovrrd channel 8 (Temperature)

		-- Global Clock Signal
		S_AXI_ACLK	: in std_logic;
		-- Global Reset Signal. This Signal is Active LOW
		S_AXI_ARESETN	: in std_logic;
		-- Write address (issued by master, acceped by Slave)
		S_AXI_AWADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		-- Write channel Protection type. This signal indicates the
    		-- privilege and security level of the transaction, and whether
    		-- the transaction is a data access or an instruction access.
		S_AXI_AWPROT	: in std_logic_vector(2 downto 0);
		-- Write address valid. This signal indicates that the master signaling
    		-- valid write address and control information.
		S_AXI_AWVALID	: in std_logic;
		-- Write address ready. This signal indicates that the slave is ready
    		-- to accept an address and associated control signals.
		S_AXI_AWREADY	: out std_logic;
		-- Write data (issued by master, acceped by Slave) 
		S_AXI_WDATA	: in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		-- Write strobes. This signal indicates which byte lanes hold
    		-- valid data. There is one write strobe bit for each eight
    		-- bits of the write data bus.    
		S_AXI_WSTRB	: in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		-- Write valid. This signal indicates that valid write
    		-- data and strobes are available.
		S_AXI_WVALID	: in std_logic;
		-- Write ready. This signal indicates that the slave
    		-- can accept the write data.
		S_AXI_WREADY	: out std_logic;
		-- Write response. This signal indicates the status
    		-- of the write transaction.
		S_AXI_BRESP	: out std_logic_vector(1 downto 0);
		-- Write response valid. This signal indicates that the channel
    		-- is signaling a valid write response.
		S_AXI_BVALID	: out std_logic;
		-- Response ready. This signal indicates that the master
    		-- can accept a write response.
		S_AXI_BREADY	: in std_logic;
		-- Read address (issued by master, acceped by Slave)
		S_AXI_ARADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		-- Protection type. This signal indicates the privilege
    		-- and security level of the transaction, and whether the
    		-- transaction is a data access or an instruction access.
		S_AXI_ARPROT	: in std_logic_vector(2 downto 0);
		-- Read address valid. This signal indicates that the channel
    		-- is signaling valid read address and control information.
		S_AXI_ARVALID	: in std_logic;
		-- Read address ready. This signal indicates that the slave is
    		-- ready to accept an address and associated control signals.
		S_AXI_ARREADY	: out std_logic;
		-- Read data (issued by slave)
		S_AXI_RDATA	: out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		-- Read response. This signal indicates the status of the
    		-- read transfer.
		S_AXI_RRESP	: out std_logic_vector(1 downto 0);
		-- Read valid. This signal indicates that the channel is
    		-- signaling the required read data.
		S_AXI_RVALID	: out std_logic;
		-- Read ready. This signal indicates that the master can
    		-- accept the read data and response information.
		S_AXI_RREADY	: in std_logic
	);
end ad7689_v0_1_s_axi;

architecture arch_imp of ad7689_v0_1_s_axi is

	-- AXI4LITE signals
	signal axi_awaddr	: std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
	signal axi_awready	: std_logic;
	signal axi_wready	: std_logic;
	signal axi_bresp	: std_logic_vector(1 downto 0);
	signal axi_bvalid	: std_logic;
	signal axi_araddr	: std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
	signal axi_arready	: std_logic;
	signal axi_rdata	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal axi_rresp	: std_logic_vector(1 downto 0);
	signal axi_rvalid	: std_logic;

	-- Example-specific design signals
	-- local parameter for addressing 32 bit / 64 bit C_S_AXI_DATA_WIDTH
	-- ADDR_LSB is used for addressing 32/64 bit registers/memories
	-- ADDR_LSB = 2 for 32 bits (n downto 2)
	-- ADDR_LSB = 3 for 64 bits (n downto 3)
	constant ADDR_LSB  : integer := (C_S_AXI_DATA_WIDTH/32)+ 1;
	constant OPT_MEM_ADDR_BITS : integer := 4;
	------------------------------------------------
	---- Signals for user logic register space example
	--------------------------------------------------
	
	constant C_ovrrd_MASTER_UNLOCK_VAL : std_logic_vector(31 downto 0) := x"C0DE1357";

    signal s_reset : std_logic;
    
    signal s_ch2ch_sample_period : std_logic_vector (31 downto 0) := x"00000738";  -- default 3kHz
    
   
    signal s_ovrrd_master_reg : std_logic_vector(31 downto 0) ;
    signal s_ovrrd_master_enable :  std_logic;
    
    signal s_ovrrd_enables_reg :  std_logic_vector (8 downto 0); -- ovrrd enables

    signal s_ovrrd_reading_ch0 :  std_logic_vector (15 downto 0); -- ovrrd channel 0
    signal s_ovrrd_reading_ch1 :  std_logic_vector (15 downto 0); -- ovrrd channel 1
    signal s_ovrrd_reading_ch2 :  std_logic_vector (15 downto 0); -- ovrrd channel 2
    signal s_ovrrd_reading_ch3 :  std_logic_vector (15 downto 0); -- ovrrd channel 3
    signal s_ovrrd_reading_ch4 :  std_logic_vector (15 downto 0); -- ovrrd channel 4
    signal s_ovrrd_reading_ch5 :  std_logic_vector (15 downto 0); -- ovrrd channel 5
    signal s_ovrrd_reading_ch6 :  std_logic_vector (15 downto 0); -- ovrrd channel 6
    signal s_ovrrd_reading_ch7 :  std_logic_vector (15 downto 0); -- ovrrd channel 7
    signal s_ovrrd_reading_ch8 :  std_logic_vector (15 downto 0); -- ovrrd channel 8 (Temperature)    

	signal slv_reg_rden	: std_logic;
	signal slv_reg_wren	: std_logic;
	signal reg_data_out	:std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal byte_index	: integer;

begin
	-- I/O Connections assignments

	S_AXI_AWREADY	<= axi_awready;
	S_AXI_WREADY	<= axi_wready;
	S_AXI_BRESP	<= axi_bresp;
	S_AXI_BVALID	<= axi_bvalid;
	S_AXI_ARREADY	<= axi_arready;
	S_AXI_RDATA	<= axi_rdata;
	S_AXI_RRESP	<= axi_rresp;
	S_AXI_RVALID	<= axi_rvalid;
	-- Implement axi_awready generation
	-- axi_awready is asserted for one S_AXI_ACLK clock cycle when both
	-- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_awready is
	-- de-asserted when reset is low.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    if S_AXI_ARESETN = '0' then
	      axi_awready <= '0';
	    else
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1') then
	        -- slave is ready to accept write address when
	        -- there is a valid write address and write data
	        -- on the write address and data bus. This design 
	        -- expects no outstanding transactions. 
	        axi_awready <= '1';
	      else
	        axi_awready <= '0';
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement axi_awaddr latching
	-- This process is used to latch the address when both 
	-- S_AXI_AWVALID and S_AXI_WVALID are valid. 

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    if S_AXI_ARESETN = '0' then
	      axi_awaddr <= (others => '0');
	    else
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1') then
	        -- Write Address latching
	        axi_awaddr <= S_AXI_AWADDR;
	      end if;
	    end if;
	  end if;                   
	end process; 

	-- Implement axi_wready generation
	-- axi_wready is asserted for one S_AXI_ACLK clock cycle when both
	-- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_wready is 
	-- de-asserted when reset is low. 

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    if S_AXI_ARESETN = '0' then
	      axi_wready <= '0';
	    else
	      if (axi_wready = '0' and S_AXI_WVALID = '1' and S_AXI_AWVALID = '1') then
	          -- slave is ready to accept write data when 
	          -- there is a valid write address and write data
	          -- on the write address and data bus. This design 
	          -- expects no outstanding transactions.           
	          axi_wready <= '1';
	      else
	        axi_wready <= '0';
	      end if;
	    end if;
	  end if;
	end process; 

	-- Implement memory mapped register select and write logic generation
	-- The write data is accepted and written to memory mapped registers when
	-- axi_awready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted. Write strobes are used to
	-- select byte enables of slave registers while writing.
	-- These registers are cleared when reset (active low) is applied.
	-- Slave register write enable is asserted when valid address and data are available
	-- and the slave is ready to accept the write address and write data.
	slv_reg_wren <= axi_wready and S_AXI_WVALID and axi_awready and S_AXI_AWVALID ;

	-- Implement write response logic generation
	-- The write response and response valid signals are asserted by the slave 
	-- when axi_wready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted.  
	-- This marks the acceptance of address and indicates the status of 
	-- write transaction.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    if S_AXI_ARESETN = '0' then
	      axi_bvalid  <= '0';
	      axi_bresp   <= "00"; --need to work more on the responses
	    else
	      if (axi_awready = '1' and S_AXI_AWVALID = '1' and axi_wready = '1' and S_AXI_WVALID = '1' and axi_bvalid = '0'  ) then
	        axi_bvalid <= '1';
	        axi_bresp  <= "00"; 
	      elsif (S_AXI_BREADY = '1' and axi_bvalid = '1') then   --check if bready is asserted while bvalid is high)
	        axi_bvalid <= '0';                                 -- (there is a possibility that bready is always asserted high)
	      end if;
	    end if;
	  end if;                   
	end process; 

	-- Implement axi_arready generation
	-- axi_arready is asserted for one S_AXI_ACLK clock cycle when
	-- S_AXI_ARVALID is asserted. axi_awready is 
	-- de-asserted when reset (active low) is asserted. 
	-- The read address is also latched when S_AXI_ARVALID is 
	-- asserted. axi_araddr is reset to zero on reset assertion.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    if S_AXI_ARESETN = '0' then
	      axi_arready <= '0';
	      axi_araddr  <= (others => '1');
	    else
	      if (axi_arready = '0' and S_AXI_ARVALID = '1') then
	        -- indicates that the slave has acceped the valid read address
	        axi_arready <= '1';
	        -- Read Address latching 
	        axi_araddr  <= S_AXI_ARADDR;           
	      else
	        axi_arready <= '0';
	      end if;
	    end if;
	  end if;                   
	end process; 

	-- Implement axi_arvalid generation
	-- axi_rvalid is asserted for one S_AXI_ACLK clock cycle when both 
	-- S_AXI_ARVALID and axi_arready are asserted. The slave registers 
	-- data are available on the axi_rdata bus at this instance. The 
	-- assertion of axi_rvalid marks the validity of read data on the 
	-- bus and axi_rresp indicates the status of read transaction.axi_rvalid 
	-- is deasserted on reset (active low). axi_rresp and axi_rdata are 
	-- cleared to zero on reset (active low).  
	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_rvalid <= '0';
	      axi_rresp  <= "00";
	    else
	      if (axi_arready = '1' and S_AXI_ARVALID = '1' and axi_rvalid = '0') then
	        -- Valid read data is available at the read data bus
	        axi_rvalid <= '1';
	        axi_rresp  <= "00"; -- 'OKAY' response
	      elsif (axi_rvalid = '1' and S_AXI_RREADY = '1') then
	        -- Read data is accepted by the master
	        axi_rvalid <= '0';
	      end if;            
	    end if;
	  end if;
	end process;

	-- Implement memory mapped register select and read logic generation
	-- Slave register read enable is asserted when valid address is available
	-- and the slave is ready to accept the read address.
	slv_reg_rden <= axi_arready and S_AXI_ARVALID and (not axi_rvalid) ;

	-- Output register or memory read data
	process( S_AXI_ACLK, axi_araddr) is
	   variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0);
	begin
	  -- Address decoding for reading registers
      loc_addr := axi_araddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
	
	  if (rising_edge (S_AXI_ACLK)) then
	    if ( S_AXI_ARESETN = '0' ) then
	      axi_rdata  <= (others => '0');
	    else
	      if (slv_reg_rden = '1') then
	        -- When there is a valid read address (S_AXI_ARVALID) with 
	        -- acceptance of read address by the slave (axi_arready), 
	        -- output the read dada 
	        -- Read address mux
	        axi_rdata <= (others => '0'); -- Default
	        case loc_addr is
              when b"00000" => axi_rdata(0) <= s_reset;
              when b"00001" => axi_rdata(31 downto 0) <= s_ch2ch_sample_period;
              when b"00010" => axi_rdata(31 downto 0) <= measured_sample_freq;
              when b"00011" => axi_rdata(31 downto 0) <= measured_sample_cnt_ch0;

              when b"00100" => axi_rdata(31 downto 0) <= s_ovrrd_master_reg;
              when b"00101" => axi_rdata(8 downto 0) <= s_ovrrd_enables_reg;
              
              when b"01000" => axi_rdata(15 downto 0) <= adc_reading_ch0;
              when b"01001" => axi_rdata(15 downto 0) <= adc_reading_ch1;
              when b"01010" => axi_rdata(15 downto 0) <= adc_reading_ch2;
              when b"01011" => axi_rdata(15 downto 0) <= adc_reading_ch3;
              when b"01100" => axi_rdata(15 downto 0) <= adc_reading_ch4;
              when b"01101" => axi_rdata(15 downto 0) <= adc_reading_ch5;
              when b"01110" => axi_rdata(15 downto 0) <= adc_reading_ch6;
              when b"01111" => axi_rdata(15 downto 0) <= adc_reading_ch7;
              when b"10000" => axi_rdata(15 downto 0) <= adc_reading_ch8;
            
              when b"10100" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch0;
              when b"10101" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch1;
              when b"10110" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch2;
              when b"10111" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch3;
              when b"11000" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch4;
              when b"11001" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch5;
              when b"11010" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch6;
              when b"11011" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch7;
              when b"11100" => axi_rdata(15 downto 0) <= s_ovrrd_reading_ch8;

              when others => axi_rdata  <= (others => '0');
            end case;
	      end if;   
	    end if;
	  end if;
	end process;
	
	process (S_AXI_ACLK)
    variable loc_addr : std_logic_vector(OPT_MEM_ADDR_BITS downto 0); 
    begin
      if rising_edge(S_AXI_ACLK) then 
        if S_AXI_ARESETN = '0' then

        else
          loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
          
          if (slv_reg_wren = '1') then
            case loc_addr is
              when b"00000" =>  s_reset <= S_AXI_WDATA (0);
              when b"00001" =>  s_ch2ch_sample_period <= S_AXI_WDATA (31 downto 0);                
           
              when b"00100" =>  s_ovrrd_master_reg <= S_AXI_WDATA (31 downto 0);                
              when b"00101" =>  s_ovrrd_enables_reg <= S_AXI_WDATA (8 downto 0);                
           
              when b"10100" =>  s_ovrrd_reading_ch0 <= S_AXI_WDATA (15 downto 0);                
              when b"10101" =>  s_ovrrd_reading_ch1 <= S_AXI_WDATA (15 downto 0);                
              when b"10110" =>  s_ovrrd_reading_ch2 <= S_AXI_WDATA (15 downto 0);                
              when b"10111" =>  s_ovrrd_reading_ch3 <= S_AXI_WDATA (15 downto 0);                
              when b"11000" =>  s_ovrrd_reading_ch4 <= S_AXI_WDATA (15 downto 0);                
              when b"11001" =>  s_ovrrd_reading_ch5 <= S_AXI_WDATA (15 downto 0);                
              when b"11010" =>  s_ovrrd_reading_ch6 <= S_AXI_WDATA (15 downto 0);                
              when b"11011" =>  s_ovrrd_reading_ch7 <= S_AXI_WDATA (15 downto 0);                
              when b"11100" =>  s_ovrrd_reading_ch8 <= S_AXI_WDATA (15 downto 0);                
           
              when others =>
    
            end case;
          end if;
        end if;
      end if;                   
    end process; 
    
    -------------------------------------------------------	
	
	s_ovrrd_master_enable <= '1' when s_ovrrd_master_reg = C_ovrrd_MASTER_UNLOCK_VAL else '0';

	gen_ovrrd_enable_out: for idx in 0 to 8 generate
	    ovrrd_enables(idx) <= s_ovrrd_enables_reg(idx) when s_ovrrd_master_enable = '1' else '0';
	end generate;
	
	ovrrd_reading_ch0 <= s_ovrrd_reading_ch0;
	ovrrd_reading_ch1 <= s_ovrrd_reading_ch1;
	ovrrd_reading_ch2 <= s_ovrrd_reading_ch2;
	ovrrd_reading_ch3 <= s_ovrrd_reading_ch3;
	ovrrd_reading_ch4 <= s_ovrrd_reading_ch4;
	ovrrd_reading_ch5 <= s_ovrrd_reading_ch5;
	ovrrd_reading_ch6 <= s_ovrrd_reading_ch6;
	ovrrd_reading_ch7 <= s_ovrrd_reading_ch7;
	ovrrd_reading_ch8 <= s_ovrrd_reading_ch8;
	
	reset <= s_reset;
    ch2ch_sample_period <= s_ch2ch_sample_period;

end arch_imp;
