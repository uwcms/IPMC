
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

package mgmt_zone_ctrl_pkg is

    type t_slv_arr_2 is array(integer range <>) of std_logic_vector(1 downto 0);
    type t_slv_arr_4 is array(integer range <>) of std_logic_vector(3 downto 0);
    type t_slv_arr_16 is array(integer range <>) of std_logic_vector(15 downto 0);
    type t_slv_arr_32 is array(integer range <>) of std_logic_vector(31 downto 0);
    type t_slv_arr_64 is array(integer range <>) of std_logic_vector(63 downto 0);
    
    type t_2d_slv  is array(integer range <>,integer range <>) of std_logic;

    function addr_encode(ch_base_reg_addr, ch_base_addr_offset, ch_num, addr_width : integer) return std_logic_vector;
    
    constant C_PWR_EN_OFF     : std_logic_vector(1 downto 0) := "00";
    constant C_PWR_EN_SEQ_OFF : std_logic_vector(1 downto 0) := "01";
    constant C_PWR_EN_SEQ_ON  : std_logic_vector(1 downto 0) := "10";
    constant C_PWR_EN_ON      : std_logic_vector(1 downto 0) := "11";

end package mgmt_zone_ctrl_pkg;

package body mgmt_zone_ctrl_pkg is

  function addr_encode(ch_base_reg_addr, ch_base_addr_offset, ch_num, addr_width : integer) return std_logic_vector is
    variable v_tmp_slv : std_logic_vector(0 to addr_width-1);
    variable v_tmp_int : integer;
  begin
    v_tmp_int := ch_base_reg_addr + ch_base_addr_offset * ch_num;
    v_tmp_slv := std_logic_vector(to_unsigned(v_tmp_int, addr_width));
    return v_tmp_slv;
  end function addr_encode;
  
end mgmt_zone_ctrl_pkg;
