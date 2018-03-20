library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

library work;
use work.mgmt_zone_ctrl_pkg.all;  

entity mz_pwr is
	generic (
    C_MZ_CNT     : integer := 2;
    C_PWREN_CNT  : integer := 8
);
Port ( 
       pwr_en_MZ_cfg_i       : in t_2d_slv(C_MZ_CNT-1 downto 0, C_PWREN_CNT-1 downto 0);
       pwr_en_state_diag_o   : out t_slv_arr_4(C_PWREN_CNT-1 downto 0);
       MZ_pwr_state_o        : out t_slv_arr_4(C_MZ_CNT-1 downto 0)    
       );
end mz_pwr;

architecture mz_pwr_arch of mz_pwr is

begin

  gen_MZ : for idx_mz in 0 to C_MZ_CNT-1 generate
      gen_pwr_en_signals : for idx_pwr in 0 to C_PWREN_CNT-1 generate
    
       end generate;
   end generate;
   
  


end mz_pwr_arch;
