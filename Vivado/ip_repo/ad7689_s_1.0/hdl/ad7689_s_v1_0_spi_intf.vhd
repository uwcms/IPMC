library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- account for 3 undefined conversation results from power-up/reset?
-- CPOL = CPHA = 0

entity ad7689_spi_intf is
    Port ( mclk_i : in STD_LOGIC;
           mreset_i : in STD_LOGIC;
           
           -- SPI interface to the ADC
           spi_clk_o : out STD_LOGIC := '0';
           spi_ncs0_o : out STD_LOGIC := '0';
           spi_mosi_o : out STD_LOGIC := '0';
           spi_miso_i : in STD_LOGIC;
           
           u_req_sample_freq : in std_logic_vector (31 downto 0); -- Averaging factor per channel
           
           -- User interface
           cnv_done_o : out STD_LOGIC := '0';
           cnv_channel_o : out STD_LOGIC_VECTOR (3 downto 0) := (others => '0');
           cnv_value_o : out STD_LOGIC_VECTOR (15 downto 0) := (others => '0');
           cnv_error_o : out STD_LOGIC := '0'
         );
end ad7689_spi_intf;

architecture Behavioral of ad7689_spi_intf is

    -- Bit and timeout counters
    signal r_bit_counter : unsigned (4 downto 0) := (others => '0');
    signal r_timeout_counter : unsigned (9 downto 0) := (others => '0');
    constant C_TIMEOUT_LIMIT : unsigned (9 downto 0) := (others => '1');
    
    signal r_clk : std_logic := '0';
    signal r_channel_value : std_logic_vector (16 downto 0);
    
    signal r_dly_value : unsigned (31 downto 0);
    
    signal s_ad76xx_cfg_reg : std_logic_vector (16 downto 0) := (others => '0');
    
    -- Conversions have a certain delay, keep track of the channels being requested
    signal r_channel_number_1 : unsigned (3 downto 0) := (others => '0');
    signal r_channel_number_2 : unsigned (3 downto 0) := (others => '0');
    signal r_channel_number_3 : unsigned (3 downto 0) := (others => '0');
    signal r_cnv_valid_1 : std_logic := '0';
    signal r_cnv_valid_2 : std_logic := '0';
    signal r_cnv_valid_3 : std_logic := '0';
    
    signal init : std_logic := '1';
    
     signal r_dly_value_rst : std_logic;   
     signal r_dly_value_dec : std_logic;   
     signal s_load  : std_logic; 
     signal s_cnv_done : std_logic;
    
    -- State machine
    type t_state is (ST_ASSERT, ST_HOLD, ST_RW, ST_WAIT, ST_DEASSERT, ST_DLY, ST_TIMEOUT);
    signal r_state : t_state := ST_ASSERT;
    
    constant c_bw_cfg : std_logic := '1'; 
    constant c_dummy_filler : std_logic_vector(2 downto 0) := "000";

    -- Generation of the configuration register
    function f_ad76xx_cfg_reg (channel_sel: unsigned(3 downto 0); bw_sel: std_logic) return std_logic_vector is
        variable reg : std_logic_vector (13 downto 0) := (others => '0');
    begin
        reg(13) := '1'; -- Overwrite contents of register
        if (channel_sel = 8) then
            reg(12 downto 10) := "011";
        else
            reg(12 downto 10) := "111";
        end if;
        reg(9 downto 7) := std_logic_vector(channel_sel(2 downto 0));
        reg(6) := bw_sel; -- Full BW
        reg(5 downto 3) := "000"; -- Internal reference, REF = 2.5 V output, temperature enabled
        reg(2 downto 1) := "00"; -- Disable equencer
        reg(0) := '0'; -- Do not read back contents of configuration
    
        return reg & c_dummy_filler; 
    end f_ad76xx_cfg_reg;    
     
begin

    process(mclk_i) is
    begin
      if (rising_edge(mclk_i)) then
         if (r_dly_value_rst = '1') then
             r_dly_value <= unsigned(u_req_sample_freq);
         elsif (r_dly_value_dec = '1' and r_dly_value /= 0) then
    
            r_dly_value <= r_dly_value - 1;
         end if;
         
      end if;
    end process;

    -- Generate the configuration register

    s_ad76xx_cfg_reg <= f_ad76xx_cfg_reg(r_channel_number_1, c_bw_cfg);

    -- Master-out is only the configuration register
    spi_mosi_o <= s_ad76xx_cfg_reg(16 - to_integer(r_bit_counter(4 downto 0))); -- when init = '1' else '0';
    
        process (mclk_i) begin
        if (rising_edge(mclk_i)) then
            if (mreset_i = '1') then
                -- Sync reset
                r_clk <= '0';
                spi_ncs0_o <= '0';
                r_bit_counter <= "10000";
                r_timeout_counter <= (others => '0');
                r_channel_value <= (others => '0');
                s_cnv_done <= '0';
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
                
                init <= '1';
                
                r_channel_number_1 <= (others => '0');
                r_channel_number_2 <= (others => '0');
                r_channel_number_3 <= (others => '0');
                r_cnv_valid_1 <= '0';
                r_cnv_valid_2 <= '0';
                r_cnv_valid_3 <= '0';
                
                r_state <= ST_ASSERT;
                r_dly_value_rst <= '1';
                r_dly_value_dec <= '0';

            else
                -- Default values
                r_clk <= '0';
                spi_ncs0_o <= '0';
                r_bit_counter <= "10000";
                r_timeout_counter <= (others => '0');
                s_cnv_done <= '0';
                cnv_channel_o <= (others => '0');
                cnv_error_o <= '0';
                r_dly_value_rst <= '0';
                r_dly_value_dec <= '0';
                                        s_load <= '0';

                case (r_state) is
                    when ST_ASSERT =>
                        -- Assert the /CS line, accessing the slave ADC
                        spi_ncs0_o <= '1';
                        
                        r_cnv_valid_2 <= r_cnv_valid_1;
                        r_cnv_valid_3 <= r_cnv_valid_2;
                        
                        r_state <= ST_WAIT;
                        r_bit_counter <= "10000";
                        r_dly_value_rst <= '1';

                    when ST_WAIT =>
                        r_clk <= '0';
                        -- Wait for the conversion to complete or timeout
                        spi_ncs0_o <= '0'; -- Needed for conversion done
                        r_cnv_valid_1 <= '0'; -- Default
               
                         r_dly_value_dec <= '1';

                        --r_timeout_counter <= r_timeout_counter + 1;
                        
                        if (spi_miso_i = '0') then
                            -- Conversion done
                            r_cnv_valid_1 <= '1';
                            r_bit_counter <= "00000";
                            r_state <= ST_RW;
                        else
                            r_state <= ST_WAIT;
                            r_bit_counter <= "10000";
                        end if;
                        
                        r_channel_value <= (others => '0');

                    when ST_RW =>
                        -- Start bit banging the MOSI and MISO ports
                        -- Data from MISO will be the ADC values of the configuration
                        -- requested 2 words ago
                        spi_ncs0_o <= '0';
                        
                        r_dly_value_dec <= '1';
                    
                        if (r_clk = '0') then
                            -- CLK LOW to HIGH transition
                            r_clk <= '1';
                            --if (r_bit_counter > 1) then
                                r_channel_value <=  r_channel_value (15 downto 0) & spi_miso_i;
                            --end if;   
                            s_load <= '1';
                            r_bit_counter <= r_bit_counter;
                        else
                            -- CLK HIGH to LOW transition
                            r_clk <= '0';
                         
                            r_bit_counter <= r_bit_counter + 1;
                        end if;
                        
                        if (r_bit_counter >= 16) then -- 17 CLKs required for SDO release
                            r_state <= ST_DLY;
                            s_cnv_done <= '1';
                            init <= '0';
                            cnv_channel_o <= std_logic_vector(r_channel_number_3);
--                        -- Cycle through the channels where channel 9 (temperature) is the last
                        
                             if (r_channel_number_1(3) = '1') then
                                   r_channel_number_1 <= (others => '0');
                             else
                                    r_channel_number_1 <= r_channel_number_1 + 1;
                             end if;
                         
                            r_channel_number_2 <= r_channel_number_1;
                            r_channel_number_3 <= r_channel_number_2;                            
                            
                        else
                             r_state <= ST_RW;
                        end if;
                       
                    when ST_DLY =>

                            spi_ncs0_o <= '0';
                        
                            r_dly_value_dec <= '1';
                        
                            if (r_dly_value = 0) then
                                r_state <= ST_ASSERT;
                            else
                                r_state <= ST_DLY;
                            end if;
                           
                    when others =>
                        r_state <= ST_ASSERT;
                        r_clk <= '0';
                end case;
            end if;
        end if;
    end process;
    
    cnv_done_o <= s_cnv_done;
    
    spi_clk_o <= r_clk;
    cnv_value_o <= r_channel_value (14 downto 0) & '0';

end Behavioral;
