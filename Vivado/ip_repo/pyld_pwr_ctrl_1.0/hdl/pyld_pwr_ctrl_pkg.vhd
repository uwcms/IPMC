
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

package pyld_pwr_ctrl_pkg is

    type t_slv_arr_3 is array(integer range <>) of std_logic_vector(2 downto 0);
    type t_slv_arr_6 is array(integer range <>) of std_logic_vector(5 downto 0);
    type t_slv_arr_32 is array(integer range <>) of std_logic_vector(31 downto 0);
    
    function addr_encode(ch_base_reg_addr, ch_base_addr_offset, ch_num, addr_width : integer) return std_logic_vector;
    
    constant C_CH_to_CH_ADDR_OFFSET : integer := 16#8#;

end package pyld_pwr_ctrl_pkg;

package body pyld_pwr_ctrl_pkg is

  function addr_encode(ch_base_reg_addr, ch_base_addr_offset, ch_num, addr_width : integer) return std_logic_vector is
    variable v_tmp_slv : std_logic_vector(0 to addr_width-1);
    variable v_tmp_int : integer;
  begin
    v_tmp_int := ch_base_reg_addr + ch_base_addr_offset * ch_num;
    v_tmp_slv := std_logic_vector(to_unsigned(v_tmp_int, addr_width));
    return v_tmp_slv;
  end function addr_encode;
  
end pyld_pwr_ctrl_pkg;
