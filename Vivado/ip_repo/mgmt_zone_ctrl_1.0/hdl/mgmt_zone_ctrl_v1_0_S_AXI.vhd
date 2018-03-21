library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.mgmt_zone_ctrl_pkg.all;  

entity mgmt_zone_ctrl_v1_0_S_AXI is
	generic (
        C_MZ_CNT  : integer := 16;
        C_HF_CNT  : integer := 16;
        C_PWREN_CNT  : integer := 16;

		-- Width of S_AXI data bus
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		-- Width of S_AXI address bus
		C_S_AXI_ADDR_WIDTH	: integer	:= 13
	);
	port (
        MZ_soft_fault_o         : out STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
        MZ_pwr_off_seq_init_o   : out STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
        MZ_pwr_on_seq_init_o    : out STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
        MZ_hard_fault_holdoff_o : out  t_slv_arr_32(C_MZ_CNT-1 downto 0);
        MZ_hard_fault_mask_o : out  t_slv_arr_64(C_MZ_CNT-1 downto 0);

        pwr_en_MZ_cfg_o         : out t_slv_arr_16(C_PWREN_CNT-1 downto 0);
        
        pwr_en_tmr_cfg_o        : out t_slv_arr_32(C_PWREN_CNT-1 downto 0);
        pwr_en_active_lvl_o     : out STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);
        pwr_en_drive_o          : out STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);
        pwr_en_state_i          : in t_slv_arr_2(C_PWREN_CNT-1 downto 0);
        pwr_en_i                : in STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);

        hard_fault_i            : in  STD_LOGIC_VECTOR(C_HF_CNT-1 downto 0);

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
end mgmt_zone_ctrl_v1_0_S_AXI;

architecture arch_imp of mgmt_zone_ctrl_v1_0_S_AXI is


    constant HARD_FAULT_STATUS_0_REG        : integer := 0;
    constant HARD_FAULT_STATUS_1_REG        : integer := 4;
    
    constant IRQ_STATUS_REG                 : integer := 8;
    constant IRQ_EN_REG                     : integer := 12;
    constant IRQ_ACK_REG                    : integer := 16;
   
    constant PWR_EN_AGGR_STATUS             : integer := 20;
    
    constant PEN_2_PEN_ADDR_OFFSET          : integer := 16;

    constant PWR_EN_0_CFG_0_REG             : integer := 32;
    constant PWR_EN_0_CFG_1_REG             : integer := 36;
    constant PWR_EN_0_INDIV_STATUS_REG      : integer := 40;

    constant MZ_0_ADDR_OFFSET               : integer := 1024;
            
    constant MZ_0_PWR_STATUS_REG            : integer := (MZ_0_ADDR_OFFSET +  0);
    constant MZ_0_HARD_FAULT_MASK_0_REG     : integer := (MZ_0_ADDR_OFFSET +  4);
    constant MZ_0_HARD_FAULT_MASK_1_REG     : integer := (MZ_0_ADDR_OFFSET +  8);
    constant MZ_0_HARD_FAULT_HOLDOFF_REG    : integer := (MZ_0_ADDR_OFFSET + 12);
    constant MZ_0_SOFT_FAULT_REG            : integer := (MZ_0_ADDR_OFFSET + 16);
    constant MZ_0_PWR_ON_INIT_REG           : integer := (MZ_0_ADDR_OFFSET + 20);
    constant MZ_0_PWR_OFF_INIT_REG          : integer := (MZ_0_ADDR_OFFSET + 24);
    
    constant MZ_2_MZ_ADDR_OFFSET            : integer := 32;

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


	constant ADDR_LSB  : integer := (C_S_AXI_DATA_WIDTH/32)+ 1;
	------------------------------------------------
	---- Signals for user logic register space example
	--------------------------------------------------

	signal slv_reg_rden	: std_logic;
	signal slv_reg_wren	: std_logic;
	signal reg_data_out	:std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal aw_en	: std_logic;

----
    signal s_hard_fault_reg          :  STD_LOGIC_VECTOR(63 downto 0) := (others => '0');

    signal s_irq_status_reg          :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
    signal s_irq_enable_reg          :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
    signal s_irq_ack_reg             :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
    
    signal s_pwr_en_aggr_status_reg       :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');

     signal s_mz_pwr_status_reg               :  t_slv_arr_32(C_MZ_CNT-1 downto 0);
     signal s_mz_hard_fault_mask_0_reg        :  t_slv_arr_32(C_MZ_CNT-1 downto 0);
     signal s_mz_hard_fault_mask_1_reg        :  t_slv_arr_32(C_MZ_CNT-1 downto 0);
     signal s_mz_hard_fault_holdoff_reg       :  t_slv_arr_32(C_MZ_CNT-1 downto 0);
     signal s_mz_soft_fault_reg               :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
     signal s_mz_pwr_on_init_reg              :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
     signal s_mz_pwr_off_init_reg             :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);

    
     signal s_mz_pwr_en_cfg_0_reg        :  t_slv_arr_32(C_PWREN_CNT-1 downto 0);
     signal s_mz_pwr_en_cfg_1_reg        :  t_slv_arr_32(C_PWREN_CNT-1 downto 0);
     signal s_pwr_en_status_reg          :  t_slv_arr_32(C_PWREN_CNT-1 downto 0);
    
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
	  
	  s_mz_pwr_on_init_reg <= (others => '0');
	  s_mz_pwr_off_init_reg <= (others => '0');
	  s_mz_soft_fault_reg <= (others => '0');

	    if S_AXI_ARESETN = '0' then

	    else
	      if (slv_reg_wren = '1') then

        	    if ( axi_awaddr = addr_encode(IRQ_EN_REG, 0, 0, C_S_AXI_ADDR_WIDTH))    then 
        	           s_irq_enable_reg <=  S_AXI_WDATA(31 downto 0); end if;
        	           
        	    if ( axi_awaddr = addr_encode(IRQ_ACK_REG, 0, 0, C_S_AXI_ADDR_WIDTH))    
        	           then s_irq_ack_reg <=  S_AXI_WDATA(31 downto 0); end if;

                for idx in 0 to C_PWREN_CNT-1 loop if (  axi_awaddr = addr_encode(PWR_EN_0_CFG_0_REG, PEN_2_PEN_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_pwr_en_cfg_0_reg(idx) <= S_AXI_WDATA(31 downto 0); end if; end loop;
                
                for idx in 0 to C_PWREN_CNT-1 loop if (  axi_awaddr = addr_encode(PWR_EN_0_CFG_1_REG, PEN_2_PEN_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_pwr_en_cfg_1_reg(idx) <= S_AXI_WDATA(31 downto 0); end if; end loop;

                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_HARD_FAULT_MASK_0_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_hard_fault_mask_0_reg(idx) <= S_AXI_WDATA(31 downto 0); end if; end loop;
                        
                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_HARD_FAULT_MASK_1_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_hard_fault_mask_1_reg(idx) <= S_AXI_WDATA(31 downto 0); end if; end loop;

                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_HARD_FAULT_HOLDOFF_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_hard_fault_holdoff_reg(idx) <= S_AXI_WDATA(31 downto 0); end if; end loop;
                        
                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_SOFT_FAULT_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_soft_fault_reg(idx) <= S_AXI_WDATA(idx); end if; end loop;
                        
                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_PWR_ON_INIT_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_pwr_on_init_reg(idx) <= S_AXI_WDATA(idx); end if; end loop;
                        
                for idx in 0 to C_MZ_CNT-1 loop if (  axi_awaddr = addr_encode(MZ_0_PWR_OFF_INIT_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))     then 
                        s_mz_pwr_off_init_reg(idx) <= S_AXI_WDATA(idx); end if; end loop;

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
    
        if (  axi_araddr = addr_encode(HARD_FAULT_STATUS_0_REG, 0, 0, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_hard_fault_reg(31 downto  0); end if;
                
        if (  axi_araddr = addr_encode(HARD_FAULT_STATUS_1_REG, 0, 0, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_hard_fault_reg(63 downto 32); end if;

        if (  axi_araddr = addr_encode(IRQ_STATUS_REG, 0, 0, C_S_AXI_ADDR_WIDTH))                   then 
                v_dout := v_dout or s_irq_status_reg; end if;
        if (  axi_araddr = addr_encode(IRQ_EN_REG, 0, 0, C_S_AXI_ADDR_WIDTH))                       then 
                v_dout := v_dout or s_irq_enable_reg; end if;
        if (  axi_araddr = addr_encode(IRQ_ACK_REG, 0, 0, C_S_AXI_ADDR_WIDTH))                      then 
                v_dout := v_dout or s_irq_ack_reg; end if;
       
        if (  axi_araddr = addr_encode(PWR_EN_AGGR_STATUS, 0, 0, C_S_AXI_ADDR_WIDTH))               then 
                v_dout := v_dout or s_pwr_en_aggr_status_reg; end if;

        for idx in 0 to C_PWREN_CNT-1 loop if (  axi_araddr = addr_encode(PWR_EN_0_CFG_0_REG, PEN_2_PEN_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_pwr_en_cfg_0_reg(idx); end if; end loop;
                
        for idx in 0 to C_PWREN_CNT-1 loop if (  axi_araddr = addr_encode(PWR_EN_0_CFG_1_REG, PEN_2_PEN_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_pwr_en_cfg_1_reg(idx); end if; end loop;
                
        for idx in 0 to C_PWREN_CNT-1 loop if (  axi_araddr = addr_encode(PWR_EN_0_INDIV_STATUS_REG, PEN_2_PEN_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_pwr_en_status_reg(idx); end if; end loop;

        for idx in 0 to C_MZ_CNT-1 loop if (  axi_araddr = addr_encode(MZ_0_PWR_STATUS_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_pwr_status_reg(idx); end if; end loop;
                
        for idx in 0 to C_MZ_CNT-1 loop if (  axi_araddr = addr_encode(MZ_0_HARD_FAULT_MASK_0_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_hard_fault_mask_0_reg(idx); end if; end loop;
                
        for idx in 0 to C_MZ_CNT-1 loop if (  axi_araddr = addr_encode(MZ_0_HARD_FAULT_MASK_1_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_hard_fault_mask_1_reg(idx); end if; end loop;
                
        for idx in 0 to C_MZ_CNT-1 loop if (  axi_araddr = addr_encode(MZ_0_HARD_FAULT_HOLDOFF_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout := v_dout or s_mz_hard_fault_holdoff_reg(idx); end if; end loop;
                
        for idx in 0 to C_MZ_CNT-1 loop if (  axi_araddr = addr_encode(MZ_0_SOFT_FAULT_REG, MZ_2_MZ_ADDR_OFFSET, idx, C_S_AXI_ADDR_WIDTH))          then 
                v_dout(idx) := v_dout(idx) or s_mz_soft_fault_reg(idx); end if; end loop;

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

----

    s_hard_fault_reg(C_HF_CNT-1 downto 0) <= hard_fault_i;
    s_hard_fault_reg(63 downto C_HF_CNT) <= (others => '0');


    s_pwr_en_aggr_status_reg(C_PWREN_CNT-1 downto 0) <= pwr_en_i;
    s_pwr_en_aggr_status_reg(31 downto C_PWREN_CNT) <= (others => '0');

  --  signal s_irq_status_reg          :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
  --  signal s_irq_enable_reg          :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
  --  signal s_irq_ack_reg             :  STD_LOGIC_VECTOR(31 downto 0) := (others => '0');
    

      MZ_soft_fault_o        <= s_mz_soft_fault_reg;
        
      MZ_pwr_off_seq_init_o   <= s_mz_pwr_off_init_reg;
      MZ_pwr_on_seq_init_o    <= s_mz_pwr_on_init_reg;
      MZ_hard_fault_holdoff_o <= s_MZ_hard_fault_holdoff_reg; 

      gen_MZ_signals : for idx in 0 to C_MZ_CNT-1 generate
        MZ_hard_fault_mask_o(idx)(31 downto 0) <= s_mz_hard_fault_mask_0_reg(idx); 
        MZ_hard_fault_mask_o(idx)(63 downto 32) <= s_mz_hard_fault_mask_1_reg(idx); 
      end generate;
       
      gen_pwr_en_signals : for idx in 0 to C_PWREN_CNT-1 generate
             pwr_en_tmr_cfg_o(idx) <= s_mz_pwr_en_cfg_0_reg(idx);
    
             pwr_en_MZ_cfg_o(idx) <= s_mz_pwr_en_cfg_1_reg(idx)(15 downto 0);
             pwr_en_active_lvl_o(idx) <= s_mz_pwr_en_cfg_1_reg(idx)(16);
             pwr_en_drive_o(idx) <= s_mz_pwr_en_cfg_1_reg(idx)(17);
             s_pwr_en_status_reg(idx)(1 downto 0) <= pwr_en_state_i(idx);
      end generate;   
             
end arch_imp;
