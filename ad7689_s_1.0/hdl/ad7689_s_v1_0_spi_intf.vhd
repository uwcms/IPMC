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

-- account for 3 undefined conversation results from power-up/reset?
-- CPOL = CPHA = 0

entity ad7689_spi_intf is
    Generic ( C_SLAVES : integer range 1 to 4 := 1);
    Port ( mclk_i : in STD_LOGIC;
           mreset_i : in STD_LOGIC;
           
           -- SPI interface to the ADC
           spi_clk_o : out STD_LOGIC := '0';
           spi_ncs_o : out STD_LOGIC_VECTOR (C_SLAVES-1 downto 0) := (others => '1');
           spi_mosi_o : out STD_LOGIC := '0';
           spi_miso_i : in STD_LOGIC;
           
           u_req_sample_freq : in std_logic_vector (31 downto 0); -- Averaging factor per channel
           
           -- User interface
           cnv_done_o : out STD_LOGIC := '0';
           cnv_slave_o : out STD_LOGIC_VECTOR (1 downto 0) := (others => '0');
           cnv_channel_o : out STD_LOGIC_VECTOR (3 downto 0) := (others => '0');
           cnv_value_o : out STD_LOGIC_VECTOR (15 downto 0) := (others => '0');
           cnv_error_o : out STD_LOGIC := '0'
         );
end ad7689_spi_intf;

architecture Behavioral of ad7689_spi_intf is
    signal r_clk : std_logic := '0';
    signal r_in : std_logic_vector (15 downto 0) := (others => '0');
    signal r_out : std_logic_vector (15 downto 0) := (others => '0');
    
    signal r_dly_value : unsigned (31 downto 0);
    signal r_dly_value_rst : std_logic;   
    signal r_dly_value_maxed : std_logic;
    
    signal r_assert_dly : integer range 0 to 3;
    
    signal r_bit_counter : integer range 0 to 16;
    
    -- State machine
    type t_state is (ST_ASSERT, ST_RW, ST_DEASSERT, ST_NXT_SLAVE, ST_DLY);
    signal r_state : t_state := ST_ASSERT;
    
    constant c_bw_cfg : std_logic := '1'; 

    -- Generation of the configuration register
    function f_ad76xx_cfg_reg (channel_sel: integer range 0 to 8; bw_sel: std_logic) return std_logic_vector is
        variable reg : std_logic_vector (13 downto 0) := (others => '0');
    begin
        reg(13) := '1'; -- Overwrite contents of register
        if (channel_sel = 8) then
            reg(12 downto 10) := "011";
        else
            reg(12 downto 10) := "111";
        end if;
        reg(9 downto 7) := std_logic_vector(to_unsigned(channel_sel, 3));
        reg(6) := bw_sel; -- Full BW
        reg(5 downto 3) := "000"; -- Internal reference, REF = 2.5 V output, temperature enabled
        reg(2 downto 1) := "00"; -- Disable sequencer
        reg(0) := '0'; -- Do not read back contents of configuration
    
        return reg;
    end f_ad76xx_cfg_reg;
     
begin

    -- Delay counter
    process(mclk_i) is begin
        if (rising_edge(mclk_i)) then
            if (r_dly_value_rst = '1') then
                r_dly_value <= (others => '0');
                r_dly_value_maxed <= '0';
            else
                r_dly_value_maxed <= '0';
                if (r_dly_value /= unsigned(u_req_sample_freq)) then
                    r_dly_value <= r_dly_value + 1;
                else
                    r_dly_value_maxed <= '1';
                end if;
            end if;
        end if;
    end process;
    
    process (mclk_i)
        variable v_cur_slave : integer range 0 to C_SLAVES-1 := 0;
        variable v_channel_req : integer range 0 to 8 := 2; -- Requested channel
        variable v_channel_cnv : integer range 0 to 8 := 0; -- Conversion channel
    begin
        if (rising_edge(mclk_i)) then
            if (mreset_i = '1') then
                -- Sync reset
                r_clk <= '0';
                r_in <= (others => '0');
                r_out <= (others => '0');
                spi_ncs_o <= (others => '1');
                
                cnv_done_o <= '0';
                cnv_slave_o <= (others => '0');
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
                
                r_dly_value_rst <= '1';
                
                r_state <= ST_ASSERT;
                
                r_assert_dly <= 0;
                r_bit_counter <= 0;
                
                v_cur_slave := 0;
                v_channel_req := 2;
                v_channel_cnv := 0;
            else
                -- Default values
                r_clk <= '0';
                r_out <= (others => '0');
                spi_ncs_o <= (others => '1');
                
                cnv_done_o <= '0';
                cnv_slave_o <= (others => '0');
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
                
                r_dly_value_rst <= '1';
                
                r_assert_dly <= 0;
                r_bit_counter <= 0;

                case (r_state) is
                    when ST_ASSERT =>
                        spi_ncs_o(v_cur_slave) <= '0';
                        
                        -- Generate the configuration register
                        r_out <= f_ad76xx_cfg_reg(v_channel_req, c_bw_cfg) & "00";
                        
                        r_assert_dly <= r_assert_dly + 1;
                        if (r_assert_dly = 3) then
                            r_state <= ST_RW;
                        else
                            r_state <= ST_ASSERT;
                        end if;
                    
                    when ST_RW =>
                        -- Start bit banging the MOSI and MISO ports
                        -- Data from MISO will be the ADC values of the configuration
                        -- requested 2 words ago
                        spi_ncs_o(v_cur_slave) <= '0';
                        r_out <= r_out;
                    
                        -- Clock toggling
                        if (r_clk = '0') then
                            -- Latch at CLK rising edge transition
                            r_clk <= '1';
                            r_in <= r_in (14 downto 0) & spi_miso_i;
                            r_bit_counter <= r_bit_counter + 1;
                        else
                            -- Shift MOSI at falling edge transition
                            r_clk <= '0';
                            r_out <= r_out(14 downto 0) & '0';
                            r_bit_counter <= r_bit_counter;
                        end if;
                        
                        if (r_bit_counter >= 16) then -- 16 CLKs required for SDO release
                            r_state <= ST_DEASSERT;
                        else
                            r_state <= ST_RW; -- Not done yet
                        end if;
                        
                    when ST_DEASSERT =>
                        -- Due to input registers there is still a bit to read from MISO before deasserting
                        r_in <= r_in (14 downto 0) & spi_miso_i;
                        
                        cnv_done_o <= '1';
                        cnv_slave_o <= std_logic_vector(to_unsigned(v_cur_slave, 2));
                        cnv_channel_o <= std_logic_vector(to_unsigned(v_channel_cnv, 4));
                        
                        if (v_cur_slave /= C_SLAVES-1) then
                            r_state <= ST_NXT_SLAVE; -- Move on to next slave
                        else
                            r_state <= ST_DLY; -- Last slave, now wait
                        end if;
                        
                    when ST_NXT_SLAVE =>
                        if (C_SLAVES /= 1) then
                            v_cur_slave := v_cur_slave + 1;
                        end if;
                        
                        r_state <= ST_ASSERT;
                        
                     when ST_DLY =>
                        v_cur_slave := 0;
                        
                        r_dly_value_rst <= '0';
                        
                        if (r_dly_value_maxed = '1') then
                            if (v_channel_req = 8) then
                                v_channel_req := 0;
                            else
                                v_channel_req := v_channel_req + 1;
                            end if;
                            
                            if (v_channel_cnv = 8) then
                                v_channel_cnv := 0; 
                            else 
                                v_channel_cnv := v_channel_cnv + 1;
                            end if;
                                                
                            r_state <= ST_ASSERT;
                        else
                            r_state <= ST_DLY;
                        end if;
                           
                    when others =>
                        -- Fallback
                        r_state <= ST_ASSERT;
                end case;
            end if;
        end if;
    end process;
    
    spi_clk_o <= r_clk;
    spi_mosi_o <= r_out(15);

    cnv_value_o <= r_in;

end Behavioral;
