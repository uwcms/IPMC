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

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

library work;
use work.mgmt_zone_ctrl_pkg.all;  

entity pwr_en is
	generic (
        C_MZ_CNT  : integer := 16;
        C_PWREN_CNT  : integer := 16
    );
    Port ( clk_i : in STD_LOGIC;
           MZ_hard_fault_i       : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           MZ_soft_fault_i       : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           MZ_holdoff_i          : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           MZ_pwr_off_seq_init_i : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           MZ_pwr_on_seq_init_i  : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           pwr_en_MZ_cfg_i       : in STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0);
           pwr_en_tmr_cfg_i      : in STD_LOGIC_VECTOR(31 downto 0);
           pwr_en_state_o        : out STD_LOGIC_VECTOR(1 downto 0);
           pwr_en_o              : out STD_LOGIC
           );
end pwr_en;

architecture pwr_en_arch of pwr_en is

    constant C_zero : STD_LOGIC_VECTOR(C_MZ_CNT-1 downto 0) := (others => '0');
    
    signal s_seq_tmr :  STD_LOGIC_VECTOR (31 downto 0) := x"00000000";
    signal s_on_seq_active :  STD_LOGIC := '0';
    signal s_off_seq_active :  STD_LOGIC := '0'; 
    
    signal s_pwr_en_state :  STD_LOGIC_VECTOR (1 downto 0) := C_PWR_EN_OFF;

begin

process(clk_i) is
begin
   if (rising_edge(clk_i)) then
   
     if ((((MZ_hard_fault_i and pwr_en_MZ_cfg_i) /= c_zero) and (MZ_holdoff_i and pwr_en_MZ_cfg_i) = c_zero)
            or ((MZ_soft_fault_i and pwr_en_MZ_cfg_i) /= c_zero)) then
          pwr_en_o <= '0';
          s_off_seq_active <= '0';
          s_on_seq_active <= '0';
          s_pwr_en_state <= C_PWR_EN_OFF;
      elsif ((MZ_pwr_off_seq_init_i and pwr_en_MZ_cfg_i) /= c_zero) then
          s_off_seq_active <= '1';
          s_on_seq_active <= '0';
          s_seq_tmr <= pwr_en_tmr_cfg_i;
          s_pwr_en_state <= C_PWR_EN_SEQ_OFF;
      elsif ((MZ_pwr_on_seq_init_i and pwr_en_MZ_cfg_i ) /= c_zero) then
          s_off_seq_active <= '0';
          s_on_seq_active <= '1';
          s_seq_tmr <= x"00000000";    
          s_pwr_en_state <= C_PWR_EN_SEQ_ON;
      elsif (s_off_seq_active = '1') then
           s_seq_tmr <= std_logic_vector(unsigned(s_seq_tmr) - 1);
           s_pwr_en_state <= C_PWR_EN_SEQ_OFF;
           if(s_seq_tmr = x"00000000") then
               pwr_en_o <= '0';
               s_off_seq_active <= '0';
               s_on_seq_active <= '0';
               s_pwr_en_state <= C_PWR_EN_OFF;
           end if;
       elsif (s_on_seq_active = '1') then
            s_seq_tmr <= std_logic_vector(unsigned(s_seq_tmr) + 1);
            s_pwr_en_state <= C_PWR_EN_SEQ_ON;
            if(s_seq_tmr = pwr_en_tmr_cfg_i) then
               pwr_en_o <= '1';
               s_off_seq_active <= '0';
               s_on_seq_active <= '0';
               s_pwr_en_state <= C_PWR_EN_ON;
            end if;   
       end if;
    end if;
   
end process;

pwr_en_state_o <= s_pwr_en_state;

end pwr_en_arch;
