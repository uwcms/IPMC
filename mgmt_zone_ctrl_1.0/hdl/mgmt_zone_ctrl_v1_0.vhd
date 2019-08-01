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

library work;
use work.mgmt_zone_ctrl_pkg.all;  

entity mgmt_zone_ctrl_v1_0 is
	generic (  
        C_MZ_CNT     : integer range 1 to 32 := 2;
        C_HF_CNT     : integer range 1 to 64 := 64;
        C_PWREN_CNT  : integer range 1 to 32 := 7;
        C_VIO_INCLUDE : boolean := true;

		-- Parameters of Axi Slave Bus Interface S_AXI
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 13
	);
	port (

        hard_fault : in std_logic_vector(C_HF_CNT - 1 downto 0);
        pwr_en_i : in std_logic_vector(C_PWREN_CNT - 1 downto 0);
        pwr_en_o : out std_logic_vector(C_PWREN_CNT - 1 downto 0);
        pwr_en_t : out std_logic_vector(C_PWREN_CNT - 1 downto 0);
        pwr_en_debug : out std_logic_vector(C_PWREN_CNT - 1 downto 0);
        mz_enabled : out std_logic_vector(C_MZ_CNT - 1 downto 0);
		irq	: out std_logic;

		-- Ports of Axi Slave Bus Interface S_AXI
		s_axi_aclk	    : in std_logic;
		s_axi_aresetn	: in std_logic;
		s_axi_awaddr	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		s_axi_awprot	: in std_logic_vector(2 downto 0);
		s_axi_awvalid	: in std_logic;
		s_axi_awready	: out std_logic;
		s_axi_wdata	    : in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		s_axi_wstrb	    : in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		s_axi_wvalid	: in std_logic;
		s_axi_wready	: out std_logic;
		s_axi_bresp	    : out std_logic_vector(1 downto 0);
		s_axi_bvalid	: out std_logic;
		s_axi_bready	: in std_logic;
		s_axi_araddr	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		s_axi_arprot	: in std_logic_vector(2 downto 0);
		s_axi_arvalid	: in std_logic;
		s_axi_arready	: out std_logic;
		s_axi_rdata	    : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		s_axi_rresp	    : out std_logic_vector(1 downto 0);
		s_axi_rvalid	: out std_logic;
		s_axi_rready	: in std_logic
	);
end mgmt_zone_ctrl_v1_0;

architecture arch_imp of mgmt_zone_ctrl_v1_0 is

    COMPONENT vio_pwr_en
      PORT (
        clk : IN STD_LOGIC;
        probe_in0 : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
        probe_in1 : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
        probe_in2 : IN STD_LOGIC_VECTOR(63 DOWNTO 0);
        probe_in3 : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
        probe_in4 : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
        probe_in5 : IN STD_LOGIC_VECTOR(15 DOWNTO 0)        
      );
    END COMPONENT;

    signal s_pwr_en :  std_logic_vector(C_PWREN_CNT - 1 downto 0);
    signal s_pwr_lvl :  std_logic_vector(C_PWREN_CNT - 1 downto 0);
    --signal s_pwr_out :  std_logic_vector(C_PWREN_CNT - 1 downto 0);

    signal s_MZ_hard_fault         :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
    signal s_MZ_soft_fault         :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
    signal s_MZ_pwr_off_seq_init   :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
    signal s_MZ_pwr_on_seq_init    :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
    signal s_MZ_pwr_on_seq_init_d1 :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);

    signal s_pwr_en_MZ_cfg       :  t_slv_arr_16(C_PWREN_CNT-1 downto 0);
    
    signal s_pwr_en_tmr_cfg      :  t_slv_arr_32(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_state        :  t_slv_arr_2(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_active_lvl   :  STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_drive        :  STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_drive_mux    :  STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0);
    signal r_pwr_en_drive_mux    :  STD_LOGIC_VECTOR(C_PWREN_CNT-1 downto 0) := (others => '1');

    signal s_hard_fault          :  STD_LOGIC_VECTOR(63 downto 0) := (others => '0');
    signal s_hard_fault_mask_en  :  t_slv_arr_64(C_MZ_CNT-1 downto 0);
    signal s_MZ_hard_fault_holdoff: t_slv_arr_32(C_MZ_CNT-1 downto 0);

    signal s_MZ_holdoff          :  STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
    
    signal s_pwr_en_output_lvl_vio : std_logic_vector(15 downto 0):= (others => '0');
    signal s_pwr_en_output_drive_vio : std_logic_vector(15 downto 0) := (others => '0');
    signal s_hard_fault_vio : std_logic_vector(63 downto 0) := (others => '0');
    signal s_MZ_pwr_off_seq_init_vio   :  STD_LOGIC_VECTOR(15 downto 0):= (others => '0');
    signal s_MZ_pwr_on_seq_init_vio    :  STD_LOGIC_VECTOR(15 downto 0):= (others => '0');
    signal s_MZ_hard_fault_vio : std_logic_vector(15 downto 0) := (others => '0');
        
    signal s_pwr_en_ovrd           : std_logic_vector(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_ovrd_drive     : std_logic_vector(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_ovrd_lvl       : std_logic_vector(C_PWREN_CNT-1 downto 0);
    signal s_pwr_en_ovrd_in        : std_logic_vector(C_PWREN_CNT-1 downto 0);
        
    attribute KEEP : string;
    attribute DONT_TOUCH : string;
    attribute MARK_DEBUG : string;

    attribute KEEP of s_hard_fault_vio: signal is "TRUE";
    attribute KEEP of s_pwr_en_output_lvl_vio: signal is "TRUE";
    attribute KEEP of s_pwr_en_output_drive_vio: signal is "TRUE";
    attribute KEEP of s_MZ_hard_fault_vio: signal is "TRUE";
    attribute KEEP of s_MZ_pwr_off_seq_init_vio: signal is "TRUE";
    attribute KEEP of s_MZ_pwr_on_seq_init_vio: signal is "TRUE";

    attribute DONT_TOUCH of s_hard_fault_vio: signal is "TRUE";
    attribute DONT_TOUCH of s_pwr_en_output_lvl_vio: signal is "TRUE";
    attribute DONT_TOUCH of s_pwr_en_output_drive_vio: signal is "TRUE";
    attribute DONT_TOUCH of s_MZ_hard_fault_vio: signal is "TRUE";
    attribute DONT_TOUCH of s_MZ_pwr_off_seq_init_vio: signal is "TRUE";
    attribute DONT_TOUCH of s_MZ_pwr_on_seq_init_vio: signal is "TRUE";

    attribute MARK_DEBUG of s_hard_fault_vio: signal is "TRUE";
    attribute MARK_DEBUG of s_pwr_en_output_lvl_vio: signal is "TRUE";
    attribute MARK_DEBUG of s_pwr_en_output_drive_vio: signal is "TRUE";
    attribute MARK_DEBUG of s_MZ_hard_fault_vio: signal is "TRUE";
    attribute MARK_DEBUG of s_MZ_pwr_off_seq_init_vio: signal is "TRUE";
    attribute MARK_DEBUG of s_MZ_pwr_on_seq_init_vio: signal is "TRUE";
            
begin

-- Instantiation of Axi Bus Interface S_AXI
mgmt_zone_ctrl_v1_0_S_AXI_inst : entity work.mgmt_zone_ctrl_v1_0_S_AXI
	generic map (
		C_S_AXI_DATA_WIDTH	=> C_S_AXI_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_S_AXI_ADDR_WIDTH,
	    C_MZ_CNT 	=> C_MZ_CNT,
        C_HF_CNT  	=> C_HF_CNT,
        C_PWREN_CNT	=> C_PWREN_CNT
	)
	port map (
        MZ_soft_fault_o                	=> s_MZ_soft_fault,
        MZ_pwr_off_seq_init_o         	=> s_MZ_pwr_off_seq_init,
        MZ_pwr_on_seq_init_o        	=> s_MZ_pwr_on_seq_init,
        MZ_hard_fault_holdoff_o         => s_MZ_hard_fault_holdoff,
        MZ_hard_fault_mask_o            => s_hard_fault_mask_en,
    
        pwr_en_MZ_cfg_o             	=> s_pwr_en_MZ_cfg,
        pwr_en_tmr_cfg_o            	=> s_pwr_en_tmr_cfg,
        pwr_en_active_lvl_o         	=> s_pwr_en_active_lvl,
        pwr_en_drive_o              	=> s_pwr_en_drive,
        pwr_en_state_i               	=> s_pwr_en_state,
        pwr_en_i                    	=> s_pwr_en,

        hard_fault_i                	=> hard_fault,
        
        pwr_en_ovrd_o                   => s_pwr_en_ovrd,
        pwr_en_ovrd_drive_o             => s_pwr_en_ovrd_drive,
        pwr_en_ovrd_lvl_o               => s_pwr_en_ovrd_lvl,
        pwr_en_ovrd_lvl_i               => s_pwr_en_ovrd_in,
	
		S_AXI_ACLK    	=> s_axi_aclk,
		S_AXI_ARESETN	=> s_axi_aresetn,
		S_AXI_AWADDR	=> s_axi_awaddr,
		S_AXI_AWPROT	=> s_axi_awprot,
		S_AXI_AWVALID	=> s_axi_awvalid,
		S_AXI_AWREADY	=> s_axi_awready,
		S_AXI_WDATA	    => s_axi_wdata,
		S_AXI_WSTRB	    => s_axi_wstrb,
		S_AXI_WVALID	=> s_axi_wvalid,
		S_AXI_WREADY	=> s_axi_wready,
		S_AXI_BRESP	    => s_axi_bresp,
		S_AXI_BVALID	=> s_axi_bvalid,
		S_AXI_BREADY	=> s_axi_bready,
		S_AXI_ARADDR	=> s_axi_araddr,
		S_AXI_ARPROT	=> s_axi_arprot,
		S_AXI_ARVALID	=> s_axi_arvalid,
		S_AXI_ARREADY	=> s_axi_arready,
		S_AXI_RDATA    	=> s_axi_rdata,
		S_AXI_RRESP	    => s_axi_rresp,
		S_AXI_RVALID	=> s_axi_rvalid,
		S_AXI_RREADY	=> s_axi_rready
	);

    s_hard_fault(C_HF_CNT-1 downto 0) <= hard_fault;
    s_hard_fault(63 downto C_HF_CNT) <= (others => '0');
    
    gen_MZ : for idx in 0 to C_MZ_CNT-1 generate
        s_MZ_hard_fault(idx) <= '1' when ((s_hard_fault_mask_en(idx) and s_hard_fault ) /= x"0000000000000000") else '0';
        
        i_mz_holdoff : entity work.mz_holdoff
             port map( 
                   clk_i                     => s_axi_aclk,
              
                    MZ_pwr_on_seq_init_i    => s_MZ_pwr_on_seq_init(idx),
                    MZ_hard_fault_holdoff_i => s_MZ_hard_fault_holdoff(idx),
                
                    MZ_holdoff_o            => s_MZ_holdoff(idx)
                  
                  );
                end generate;
        
    s_MZ_pwr_on_seq_init_d1 <= s_MZ_pwr_on_seq_init when rising_edge(s_axi_aclk);
    
    gen_pwr_en : for idx in 0 to C_PWREN_CNT-1 generate
    
        i_pwr_en : entity work.pwr_en
            generic map (
                C_MZ_CNT    => C_MZ_CNT,
                C_PWREN_CNT	=> C_PWREN_CNT
            )
            port map( 
                   clk_i                     => s_axi_aclk,
                   MZ_hard_fault_i           => s_MZ_hard_fault,
                   MZ_soft_fault_i           => s_MZ_soft_fault,
                   MZ_holdoff_i              => s_MZ_holdoff,
                   MZ_pwr_off_seq_init_i     => s_MZ_pwr_off_seq_init,
                   MZ_pwr_on_seq_init_i      => s_MZ_pwr_on_seq_init_d1, -- 1 clk dly to prevent race condition
                   pwr_en_MZ_cfg_i           => s_pwr_en_MZ_cfg(idx)(C_MZ_CNT-1 downto 0),
                   pwr_en_tmr_cfg_i          => s_pwr_en_tmr_cfg(idx),
                   pwr_en_state_o            => s_pwr_en_state(idx),
                   pwr_en_o                  => s_pwr_en(idx)
             );
                   
             s_pwr_lvl(idx) <= s_pwr_en(idx) xnor s_pwr_en_active_lvl(idx) when s_pwr_en_ovrd(idx) = '0' else s_pwr_en_ovrd_lvl(idx);
             s_pwr_en_drive_mux(idx) <= s_pwr_en_drive(idx) when s_pwr_en_ovrd(idx) = '0' else s_pwr_en_ovrd_drive(idx);
             r_pwr_en_drive_mux(idx) <= not s_pwr_en_drive_mux(idx) when s_axi_aclk'event and s_axi_aclk = '1';
             --s_pwr_out(idx) <= s_pwr_lvl(idx) when r_pwr_en_drive_mux(idx) = '1' else 'Z';
             
             s_pwr_en_output_lvl_vio(idx) <= s_pwr_lvl(idx);
             s_pwr_en_output_drive_vio(idx) <= s_pwr_en_drive(idx);
             
             pwr_en_debug(idx) <= s_pwr_lvl(idx);

    end generate;

    s_hard_fault_vio(C_HF_CNT -1 downto 0) <= hard_fault;
    s_MZ_pwr_off_seq_init_vio(C_MZ_CNT -1 downto 0) <= s_MZ_pwr_off_seq_init;
    s_MZ_pwr_on_seq_init_vio(C_MZ_CNT -1 downto 0) <= s_MZ_pwr_on_seq_init;
    s_MZ_hard_fault_vio(C_MZ_CNT -1 downto 0) <= s_MZ_hard_fault;


    gen_vio: if C_VIO_INCLUDE = true generate
    
    i_vio_pwr_en : vio_pwr_en
          PORT MAP (
            clk => s_axi_aclk,
            probe_in0 => s_pwr_en_output_lvl_vio,
            probe_in1 => s_pwr_en_output_drive_vio,
            probe_in2 => s_hard_fault_vio,
            probe_in3 => s_MZ_pwr_off_seq_init_vio,
            probe_in4 => s_MZ_pwr_on_seq_init_vio,
            probe_in5 => s_MZ_hard_fault_vio
          );    
    end generate;
    
    pwr_en_o <= s_pwr_lvl;
    pwr_en_t <= r_pwr_en_drive_mux;
    s_pwr_en_ovrd_in <= pwr_en_i;
    irq <= '0'; --TODO

end arch_imp;
