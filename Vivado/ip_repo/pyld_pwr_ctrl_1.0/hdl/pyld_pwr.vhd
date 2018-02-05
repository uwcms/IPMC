library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;



entity pyld_pwr is
    Port ( clk_i : in STD_LOGIC;
           pd_ext_en_i : in STD_LOGIC;
           pd_ext_req_i : in STD_LOGIC;
           pd_sw_en_i : in STD_LOGIC;
           pd_sw_req_i : in STD_LOGIC;
           grp_cfg_i : in STD_LOGIC_VECTOR (2 downto 0);
           pud_seq_tmr_cfg_i : in STD_LOGIC_VECTOR (31 downto 0);
           pd_seq_init_i : in STD_LOGIC_VECTOR (6 downto 0);
           pu_seq_init_i : in STD_LOGIC_VECTOR (6 downto 0);
           pe_o : out STD_LOGIC
           );
end pyld_pwr;

architecture pyld_pwr_arch of pyld_pwr is

    signal s_pud_seq_tmr :  STD_LOGIC_VECTOR (31 downto 0) := x"00000000";
    signal s_pu_seq_active :  STD_LOGIC := '0';
    signal s_pd_seq_active :  STD_LOGIC := '0'; 
    signal s_grp_cfg_onehot : STD_LOGIC_VECTOR (6 downto 0);

    function std_to_onehot ( input_std : std_logic_vector ) return std_logic_vector is
    
        variable input_int : unsigned(2 downto 0);
        variable tmp : unsigned(6 downto 0) := "0000001";
        variable res : unsigned(6 downto 0);
    
    begin
    
      input_int := unsigned(input_std);
     
       if (input_int = 0) then
          return "0000000";
       else 
        res := tmp sll to_integer(input_int -1);
        return std_logic_vector(res);
       end if;
    
    end function;

begin

s_grp_cfg_onehot <= std_to_onehot(grp_cfg_i);

process(clk_i) is
begin
   if (rising_edge(clk_i)) then
   
      if ((pd_ext_en_i ='1' and pd_ext_req_i = '1' ) or (pd_sw_en_i ='1' and pd_sw_req_i='1')) then
          pe_o <= '0';
          s_pd_seq_active <= '0';
          s_pu_seq_active <= '0';
      elsif ((pd_seq_init_i and s_grp_cfg_onehot) /= "0000000") then
          s_pd_seq_active <= '1';
          s_pud_seq_tmr <= pud_seq_tmr_cfg_i;
      elsif ((pu_seq_init_i and s_grp_cfg_onehot ) /= "0000000") then
          s_pu_seq_active <= '1';
          s_pud_seq_tmr <= x"00000000";      
      elsif (s_pd_seq_active = '1') then
           s_pud_seq_tmr <= std_logic_vector(unsigned(s_pud_seq_tmr) - 1);
           if(s_pud_seq_tmr = x"00000000") then
               pe_o <= '0';
               s_pd_seq_active <= '0';
               s_pu_seq_active <= '0';
           end if;
       elsif (s_pu_seq_active = '1') then
            s_pud_seq_tmr <= std_logic_vector(unsigned(s_pud_seq_tmr) + 1);
            if(s_pud_seq_tmr = pud_seq_tmr_cfg_i) then
               pe_o <= '1';
               s_pd_seq_active <= '0';
               s_pu_seq_active <= '0';
            end if;   
       end if;
    end if;
   
end process;

end pyld_pwr_arch;
