----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 04/07/2017 04:04:40 PM
-- Design Name: 
-- Module Name: spi_intf - Behavioral
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

-- CPOL = CPHA = 0

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity ad7689_spi_intf is
    Port ( mclk_i : in STD_LOGIC;
           mreset_i : in STD_LOGIC;
           
           -- SPI interface to the ADC
           spi_clk_o : out STD_LOGIC := '0';
           spi_ncs0_o : out STD_LOGIC := '0';
           spi_mosi_o : out STD_LOGIC := '0';
           spi_miso_i : in STD_LOGIC;
           
           -- User interface
           cnv_done_o : out STD_LOGIC := '0';
           cnv_channel_o : out STD_LOGIC_VECTOR (3 downto 0) := (others => '0');
           cnv_value_o : out STD_LOGIC_VECTOR (15 downto 0) := (others => '0');
           cnv_error_o : out STD_LOGIC := '0'
         );
end ad7689_spi_intf;

architecture Behavioral of ad7689_spi_intf is
    -- Bit and timeout counters
    signal r_bit_counter : unsigned (3 downto 0) := (others => '0');
    signal r_timeout_counter : unsigned (9 downto 0) := (others => '0');
    constant C_TIMEOUT_LIMIT : unsigned (9 downto 0) := (others => '1');
    
    signal r_clk : std_logic := '0';
    signal r_channel_value : std_logic_vector (16 downto 0);
    
    signal s_ad76xx_cfg_reg : std_logic_vector (15 downto 0) := (others => '0');
    
    -- Conversions have a certain delay, keep track of the channels being requested
    signal r_channel_number_1 : unsigned (3 downto 0) := (others => '0');
    signal r_channel_number_2 : unsigned (3 downto 0) := (others => '0');
    signal r_channel_number_3 : unsigned (3 downto 0) := (others => '0');
    signal r_cnv_valid_1 : std_logic := '0';
    signal r_cnv_valid_2 : std_logic := '0';
    signal r_cnv_valid_3 : std_logic := '0';
    
    -- State machine
    type t_state is (ST_ASSERT, ST_RW, ST_WAIT, ST_DEASSERT, ST_TIMEOUT);
    signal r_state : t_state := ST_ASSERT;
    
    -- Generation of the configuration register
    function ad76xx_cfg_reg_gen (channel_sel: unsigned(2 downto 0); is_temp: std_logic) return std_logic_vector is
        variable reg : std_logic_vector (13 downto 0) := (others => '0');
    begin
        reg(13) := '1'; -- Overwrite contents of register
        if (is_temp = '1') then
            reg(12 downto 10) := "011";
        else
            reg(12 downto 10) := "111";
        end if;
        reg(9 downto 7) := std_logic_vector(channel_sel);
        reg(6) := '1'; -- Full BW
        reg(5 downto 3) := "000"; -- Internal reference, REF = 2.5 V output, temperature enabled
        reg(2 downto 1) := "00"; -- Disable sequencer
        reg(0) := '0'; -- Do not read back contents of configuration
    
        return reg & "XX";
    end ad76xx_cfg_reg_gen;
begin
    -- Generate the configuration register
    s_ad76xx_cfg_reg <= ad76xx_cfg_reg_gen(r_channel_number_1(2 downto 0), r_channel_number_1(3));

    -- Master-out is only the configuration register
    spi_mosi_o <= s_ad76xx_cfg_reg(15 - to_integer(r_bit_counter(3 downto 0)));

    process (mclk_i) begin
        if (rising_edge(mclk_i)) then
            if (mreset_i = '1') then
                -- Sync reset
                r_clk <= '0';
                spi_ncs0_o <= '1';
                r_bit_counter <= (others => '0');
                r_timeout_counter <= (others => '0');
                r_channel_value <= (others => '0');
                cnv_done_o <= '0';
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
                
                r_channel_number_1 <= (others => '0');
                r_channel_number_2 <= (others => '0');
                r_channel_number_3 <= (others => '0');
                r_cnv_valid_1 <= '0';
                r_cnv_valid_2 <= '0';
                r_cnv_valid_3 <= '0';
                
                r_state <= ST_ASSERT;
            else
                -- Default values
                r_clk <= '0';
                spi_ncs0_o <= '1';
                r_bit_counter <= (others => '0');
                r_timeout_counter <= (others => '0');
                cnv_done_o <= '0';
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
            
                case (r_state) is
                    when ST_ASSERT =>
                        -- Assert the /CS line, accessing the slave ADC
                        spi_ncs0_o <= '0';
                        
                        r_cnv_valid_2 <= r_cnv_valid_1;
                        r_cnv_valid_3 <= r_cnv_valid_2;
                        
                        r_state <= ST_RW;
                        
                    when ST_RW =>
                        -- Start bit banging the MOSI and MISO ports
                        -- Data from MISO will be the ADC values of the configuration
                        -- requested 2 words ago
                        spi_ncs0_o <= '0';
                    
                        if (r_clk = '0') then
                            -- CLK LOW to HIGH transition
                            r_clk <= '1';
                            r_channel_value <= spi_miso_i & r_channel_value (16 downto 1);
                            r_bit_counter <= r_bit_counter;
                        else
                            -- CLK HIGH to LOW transition
                            r_clk <= '0';
                            r_bit_counter <= r_bit_counter + 1;
                        end if;
                        
                        if (r_bit_counter >= 16) then -- 17 CLKs required for SDO release
                            r_state <= ST_DEASSERT;
                        end if;
                        
                    when ST_DEASSERT =>
                        -- Deassert the /CS line, causing the ADC to start the new conversion
                        spi_ncs0_o <= '1';
                        cnv_done_o <= r_cnv_valid_3;
                        cnv_channel_o <= std_logic_vector(r_channel_number_3);
                        
                        -- Cycle through the channels where channel 9 (temperature) is the last
                        if (r_channel_number_1(3) = '1') then
                            r_channel_number_1 <= (others => '0');
                        else
                            r_channel_number_1 <= r_channel_number_1 + 1;
                        end if;
                        r_channel_number_2 <= r_channel_number_1;
                        r_channel_number_3 <= r_channel_number_2;
                        
                        r_state <= ST_WAIT;
                        
                    when ST_WAIT =>
                        -- Wait for the conversion to complete or timeout
                        spi_ncs0_o <= '0'; -- Needed for conversion done
                        r_cnv_valid_1 <= '0'; -- Default
                        
                        r_timeout_counter <= r_timeout_counter + 1;
                        
                        if (spi_miso_i = '0') then
                            -- Conversion done
                            r_cnv_valid_1 <= '1';
                            
                            r_state <= ST_ASSERT;
                        elsif (r_timeout_counter = C_TIMEOUT_LIMIT) then
                            -- Timeout on the conversion
                            r_state <= ST_TIMEOUT;
                        end if;
                            
                    when ST_TIMEOUT =>
                        -- Conversion timed out, flag an error
                        cnv_error_o <= '1';
                        r_state <= ST_ASSERT;
                    
                    when others =>
                        r_state <= ST_ASSERT;
                end case;
            end if;
        end if;
    end process;
    
    spi_clk_o <= r_clk;
    cnv_value_o <= r_channel_value (15 downto 0);

end Behavioral;
