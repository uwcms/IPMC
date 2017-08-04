----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 04/11/2017 02:01:47 PM
-- Design Name: 
-- Module Name: averager - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
-- 
----------------------------------------------------------------------------------


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity averager is
    Port ( clk_i : in STD_LOGIC;
           reset_i : in STD_LOGIC;
           din_i : in STD_LOGIC_VECTOR (15 downto 0);
           din_valid_i : in STD_LOGIC;
           avg_factor_i : in STD_LOGIC_VECTOR (1 downto 0);
           avg_out_o : out STD_LOGIC_VECTOR (15 downto 0);
           avg_out_valid_o : out STD_LOGIC);
end averager;

architecture Behavioral of averager is
    signal s_divided_din : unsigned (15 downto 0);
    signal s_avg_counter_max : unsigned (3 downto 0);
    
    signal r_avg_counter : unsigned (3 downto 0) := (others => '0');
    
    signal r_avg : unsigned (15 downto 0);
begin

--    process (all) begin
--        for I in 0 to 3 loop
--            if (to_integer(unsigned(avg_factor_i)) = I) then
--                if (I = 0) then
--                    s_divided_din <= unsigned(din_i);
--                else
--                    s_divided_din (15 downto 15-I) <= (others => '0');
--                    s_divided_din (15-I downto 0) <= unsigned(din_i (15 downto I));
--                end if;
                
--                s_avg_counter_max <= to_unsigned(2**I - 1, 4);
--            end if;
--        end loop;
--    end process;
    
    with to_integer(unsigned(avg_factor_i)) select
        s_divided_din <= unsigned(din_i) when 0,
                         "0" & unsigned(din_i(15 downto 1)) when 1,
                         "00" & unsigned(din_i(15 downto 2)) when 2,
                         "000" & unsigned(din_i(15 downto 3)) when others;
                         
    s_avg_counter_max <= to_unsigned(2**to_integer(unsigned(avg_factor_i)) - 1, 4);
    
--    s_divided_din <= unsigned(din_i) when to_integer(unsigned(avg_factor_i)) = 0 else
--                     "0" & unsigned(din_i(15 downto 1)) when to_integer(unsigned(avg_factor_i)) = 1 else
--                     "00" & unsigned(din_i(15 downto 1)) when to_integer(unsigned(avg_factor_i)) = 1 else
--                     "000" &

    process (clk_i) begin
        if (rising_edge(clk_i)) then
            if (reset_i = '1') then
                r_avg_counter <= (others => '0');
                
                avg_out_valid_o <= '0';
            else
                if (din_valid_i = '1') then
                    if (r_avg_counter = 0) then
                        r_avg <= s_divided_din;
                    else
                        r_avg <= r_avg + s_divided_din;
                    end if;
                    
                    if (r_avg_counter = s_avg_counter_max) then
                        r_avg_counter <= (others => '0');
                        avg_out_valid_o <= '1';
                    else
                        r_avg_counter <= r_avg_counter + 1;
                        avg_out_valid_o <= '0';
                    end if;
                end if;
            end if;
        end if;
    end process;
    
    avg_out_o <= std_logic_vector(r_avg);

end Behavioral;
