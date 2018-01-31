library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.pyld_pwr_ctrl_pkg.all;  

entity pyld_pwr_ctrl_v1_0_S_AXI is
	generic (
		C_CORE_VER	:  std_logic_vector(31 downto 0) := x"00000000";
        C_PE_PIN_CNT : integer    := 16;  -- number of Power Enable output pins
        C_PG_PIN_CNT : integer    := 16;  -- number of Power Good input pins

		C_S_AXI_DATA_WIDTH	: integer	:= 32; 		-- Width of S_AXI data bus
		C_S_AXI_ADDR_WIDTH	: integer	:= 8		-- Width of S_AXI address bus

	);
	port (
         pd_sw_req_o : out std_logic;
    
         pe_pin_cfg_o : out t_slv_arr_6(C_PE_PIN_CNT-1 downto 0);
        
         pud_seq_tmr_cfg_o : out t_slv_arr_32(C_PE_PIN_CNT-1 downto 0);
         pd_seq_init_o : out std_logic_vector(6 downto 0);
         pu_seq_init_o : out std_logic_vector(6 downto 0);
        
         pe_i : in std_logic_vector(C_PE_PIN_CNT-1 downto 0);
         pg_i : in std_logic_vector(C_PG_PIN_CNT-1 downto 0);

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
end pyld_pwr_ctrl_v1_0_S_AXI;

architecture arch_imp of pyld_pwr_ctrl_v1_0_S_AXI is

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
	constant OPT_MEM_ADDR_BITS : integer := 2;
	------------------------------------------------
	---- Signals for user logic register space example
	--------------------------------------------------

	signal slv_reg_rden	: std_logic;
	signal slv_reg_wren	: std_logic;
	signal reg_data_out	:std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal aw_en	: std_logic;

    signal s_pd_sw_req :  std_logic;
    signal s_pd_sw_reg :  std_logic_vector(31 downto 0);

    signal s_pe_pin_cfg_reg :  t_slv_arr_6(C_PE_PIN_CNT-1 downto 0);
    signal s_pud_seq_tmr_cfg_reg :  t_slv_arr_32(C_PE_PIN_CNT-1 downto 0);
    signal s_pd_seq_init_reg :  std_logic_vector(6 downto 0);
    signal s_pu_seq_init_reg :  std_logic_vector(6 downto 0);
    
    constant c_gnd : std_logic_vector(31 downto 0) := (others => '0');

begin

         s_pd_sw_req <= '1' when s_pd_sw_reg = x"c0dea0ff" else '0';
         pd_sw_req_o <= s_pd_sw_req;
    
         pe_pin_cfg_o  <= s_pe_pin_cfg_reg ;
        
         pud_seq_tmr_cfg_o  <= s_pud_seq_tmr_cfg_reg ;
         pd_seq_init_o  <=  s_pd_seq_init_reg;
         pu_seq_init_o  <=  s_pu_seq_init_reg;


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
	      aw_en <= '1';
	    else
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1' and aw_en = '1') then
	        -- slave is ready to accept write address when
	        -- there is a valid write address and write data
	        -- on the write address and data bus. This design 
	        -- expects no outstanding transactions. 
	        axi_awready <= '1';
	        elsif (S_AXI_BREADY = '1' and axi_bvalid = '1') then
	            aw_en <= '1';
	        	axi_awready <= '0';
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
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1' and aw_en = '1') then
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
	      if (axi_wready = '0' and S_AXI_WVALID = '1' and S_AXI_AWVALID = '1' and aw_en = '1') then
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

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then 
	    
	     s_pd_seq_init_reg <= (others => '0');
	  	 s_pu_seq_init_reg <= (others => '0');
	  	     
	    if S_AXI_ARESETN = '0' then

	    else
	      if (slv_reg_wren = '1') then

             if ( axi_awaddr = addr_encode(4, 0, 0, C_S_AXI_ADDR_WIDTH))     then s_pd_sw_reg <=  S_AXI_WDATA; end if;
             if ( axi_awaddr = addr_encode(8, 0, 0, C_S_AXI_ADDR_WIDTH))     then s_pd_seq_init_reg <=  S_AXI_WDATA(6 downto 0); end if;
             if ( axi_awaddr = addr_encode(12, 0, 0, C_S_AXI_ADDR_WIDTH))    then s_pu_seq_init_reg <=  S_AXI_WDATA(6 downto 0); end if;

             for idx in 0 to C_PE_PIN_CNT-1 loop if (  axi_awaddr = addr_encode(32, C_CH_to_CH_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then s_pe_pin_cfg_reg(idx) <= S_AXI_WDATA(5 downto 0); end if; end loop;
             for idx in 0 to C_PE_PIN_CNT-1 loop if (  axi_awaddr = addr_encode(36, C_CH_to_CH_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then s_pud_seq_tmr_cfg_reg(idx) <= S_AXI_WDATA; end if; end loop;
	            
          end if;
	    end if;
	  end if;                   
	end process; 
	
    
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

	process ( axi_araddr, S_AXI_ARESETN, slv_reg_rden)
		variable v_dout :std_logic_vector(31 downto 0);

	begin
	v_dout := (others => '0');

    if (  axi_araddr = addr_encode(0, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))          then v_dout := v_dout or C_CORE_VER; end if;
    if (  axi_araddr = addr_encode(4, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))          then v_dout := v_dout or s_pd_sw_reg; end if;
    if (  axi_araddr = addr_encode(8, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))          then v_dout := v_dout or c_gnd(31 downto 7) & s_pd_seq_init_reg; end if;
    if (  axi_araddr = addr_encode(12, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))         then v_dout := v_dout or c_gnd(31 downto 7) & s_pu_seq_init_reg; end if;
    if (  axi_araddr = addr_encode(16, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))         then v_dout := v_dout or c_gnd(31 downto C_PE_PIN_CNT) & pe_i; end if;
    if (  axi_araddr = addr_encode(20, C_CH_to_CH_ADDR_OFFSET, 0, C_S_AXI_ADDR_WIDTH))         then v_dout := v_dout or c_gnd(31 downto C_PG_PIN_CNT) & pg_i; end if;

	 for idx in 0 to C_PE_PIN_CNT-1 loop if (  axi_araddr = addr_encode(32, C_CH_to_CH_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then v_dout := v_dout or c_gnd(31 downto 6) & s_pe_pin_cfg_reg(idx); end if; end loop;
	 for idx in 0 to C_PE_PIN_CNT-1 loop if (  axi_araddr = addr_encode(36, C_CH_to_CH_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then v_dout := v_dout or s_pud_seq_tmr_cfg_reg(idx); end if; end loop;

	reg_data_out  <= v_dout;

	end process; 

	-- Output register or memory read data
	process( S_AXI_ACLK ) is
	begin
	  if (rising_edge (S_AXI_ACLK)) then
	    if ( S_AXI_ARESETN = '0' ) then
	      axi_rdata  <= (others => '0');
	    else
	      if (slv_reg_rden = '1') then
	        -- When there is a valid read address (S_AXI_ARVALID) with 
	        -- acceptance of read address by the slave (axi_arready), 
	        -- output the read dada 
	        -- Read address mux
	          axi_rdata <= reg_data_out;     -- register read data
	      end if;   
	    end if;
	  end if;
	end process;

end arch_imp;
