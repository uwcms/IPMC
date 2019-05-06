library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity sensor_serializer_v1_0 is
	generic (
	    C_SENSOR_PORTS : integer range 1 to 8 := 4;
	    
	    C_SENSOR_0_CH_COUNT : integer := 8;
	    C_SENSOR_1_CH_COUNT : integer := 8;
	    C_SENSOR_2_CH_COUNT : integer := 24;
	    C_SENSOR_3_CH_COUNT : integer := 8;
	    C_SENSOR_4_CH_COUNT : integer := 0;
	    C_SENSOR_5_CH_COUNT : integer := 0;
	    C_SENSOR_6_CH_COUNT : integer := 0;
	    C_SENSOR_7_CH_COUNT : integer := 0;
	    
	    C_DATA_WIDTH : integer := 16
	);
	port (
	    rstn : in std_logic;
		clk : in std_logic;
		
		sensor_data_0 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_0_CH_COUNT - 1 downto 0);
		sensor_valid_0 : in std_logic_vector(C_SENSOR_0_CH_COUNT - 1 downto 0);
		
		sensor_data_1 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_1_CH_COUNT - 1 downto 0);
        sensor_valid_1 : in std_logic_vector(C_SENSOR_1_CH_COUNT - 1 downto 0);
        
        sensor_data_2 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_2_CH_COUNT - 1 downto 0);
        sensor_valid_2 : in std_logic_vector(C_SENSOR_2_CH_COUNT - 1 downto 0);
        
        sensor_data_3 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_3_CH_COUNT - 1 downto 0);
        sensor_valid_3 : in std_logic_vector(C_SENSOR_3_CH_COUNT - 1 downto 0);
        
        sensor_data_4 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_4_CH_COUNT - 1 downto 0);
        sensor_valid_4 : in std_logic_vector(C_SENSOR_4_CH_COUNT - 1 downto 0);
        
        sensor_data_5 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_5_CH_COUNT - 1 downto 0);
        sensor_valid_5 : in std_logic_vector(C_SENSOR_5_CH_COUNT - 1 downto 0);
        
        sensor_data_6 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_6_CH_COUNT - 1 downto 0);
        sensor_valid_6 : in std_logic_vector(C_SENSOR_6_CH_COUNT - 1 downto 0);
        
        sensor_data_7 : in std_logic_vector(C_DATA_WIDTH * C_SENSOR_7_CH_COUNT - 1 downto 0);
        sensor_valid_7 : in std_logic_vector(C_SENSOR_7_CH_COUNT - 1 downto 0);
		
		sensor_data_out : out std_logic_vector(C_DATA_WIDTH-1 downto 0);
		sensor_id_out : out std_logic_vector(5 downto 0);
        sensor_valid_out  : out std_logic
	);
end sensor_serializer_v1_0;

architecture arch_imp of sensor_serializer_v1_0 is
    constant CHANNEL_COUNT : integer := C_SENSOR_0_CH_COUNT + C_SENSOR_1_CH_COUNT + C_SENSOR_2_CH_COUNT + C_SENSOR_3_CH_COUNT +
                                        C_SENSOR_4_CH_COUNT + C_SENSOR_5_CH_COUNT + C_SENSOR_6_CH_COUNT + C_SENSOR_7_CH_COUNT;
    
    constant CHANNEL_OFFSET_1 : integer := C_SENSOR_0_CH_COUNT;
    constant CHANNEL_OFFSET_2 : integer := CHANNEL_OFFSET_1 + C_SENSOR_1_CH_COUNT;
    constant CHANNEL_OFFSET_3 : integer := CHANNEL_OFFSET_2 + C_SENSOR_2_CH_COUNT;
    constant CHANNEL_OFFSET_4 : integer := CHANNEL_OFFSET_3 + C_SENSOR_3_CH_COUNT;
    constant CHANNEL_OFFSET_5 : integer := CHANNEL_OFFSET_4 + C_SENSOR_4_CH_COUNT;
    constant CHANNEL_OFFSET_6 : integer := CHANNEL_OFFSET_5 + C_SENSOR_5_CH_COUNT;
    constant CHANNEL_OFFSET_7 : integer := CHANNEL_OFFSET_6 + C_SENSOR_6_CH_COUNT;

    signal sensor_data_bus : std_logic_vector(CHANNEL_COUNT * C_DATA_WIDTH - 1 downto 0);
    signal sensor_valid_bus : std_logic_vector(CHANNEL_COUNT - 1 downto 0);
    
    signal sensor_data_bus_l : std_logic_vector(CHANNEL_COUNT * C_DATA_WIDTH - 1 downto 0);
    signal sensor_valid_bus_l : std_logic_vector(CHANNEL_COUNT - 1 downto 0);
    
    signal counter : integer range 0 to CHANNEL_COUNT-1;
begin
    
    GEN_SENSOR_0_MAPPING_2: for idx in 0 to C_SENSOR_0_CH_COUNT - 1 generate
        sensor_data_bus((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH) <= sensor_data_0((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
        sensor_valid_bus(idx) <= sensor_valid_0(idx);
    end generate GEN_SENSOR_0_MAPPING_2;
    
    GEN_SENSOR_1_MAPPING: if C_SENSOR_PORTS >= 2 generate
        GEN_SENSOR_1_MAPPING_2: for idx in 0 to C_SENSOR_1_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_1 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_1 + idx) * C_DATA_WIDTH) <= sensor_data_1((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_1 + idx) <= sensor_valid_1(idx);
        end generate GEN_SENSOR_1_MAPPING_2;
    end generate GEN_SENSOR_1_MAPPING;
    
    GEN_SENSOR_2_MAPPING: if C_SENSOR_PORTS >= 3 generate
        GEN_SENSOR_2_MAPPING_2: for idx in 0 to C_SENSOR_2_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_2 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_2 + idx) * C_DATA_WIDTH) <= sensor_data_2((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_2 + idx) <= sensor_valid_2(idx);
        end generate GEN_SENSOR_2_MAPPING_2;
    end generate GEN_SENSOR_2_MAPPING;
    
    GEN_SENSOR_3_MAPPING: if C_SENSOR_PORTS >= 4 generate
        GEN_SENSOR_3_MAPPING_2: for idx in 0 to C_SENSOR_3_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_3 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_3 + idx) * C_DATA_WIDTH) <= sensor_data_3((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_3 + idx) <= sensor_valid_3(idx);
        end generate GEN_SENSOR_3_MAPPING_2;
    end generate GEN_SENSOR_3_MAPPING;
    
    GEN_SENSOR_4_MAPPING: if C_SENSOR_PORTS >= 5 generate
        GEN_SENSOR_4_MAPPING_2: for idx in 0 to C_SENSOR_4_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_4 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_4 + idx) * C_DATA_WIDTH) <= sensor_data_4((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_4 + idx) <= sensor_valid_4(idx);
        end generate GEN_SENSOR_4_MAPPING_2;
    end generate GEN_SENSOR_4_MAPPING;
    
    GEN_SENSOR_5_MAPPING: if C_SENSOR_PORTS >= 6 generate
        GEN_SENSOR_5_MAPPING_2: for idx in 0 to C_SENSOR_5_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_5 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_5 + idx) * C_DATA_WIDTH) <= sensor_data_5((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_5 + idx) <= sensor_valid_5(idx);
        end generate GEN_SENSOR_5_MAPPING_2;
    end generate GEN_SENSOR_5_MAPPING;
    
    GEN_SENSOR_6_MAPPING: if C_SENSOR_PORTS >= 7 generate
        GEN_SENSOR_6_MAPPING_2: for idx in 0 to C_SENSOR_6_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_6 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_6 + idx) * C_DATA_WIDTH) <= sensor_data_6((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_6 + idx) <= sensor_valid_6(idx);
        end generate GEN_SENSOR_6_MAPPING_2;
    end generate GEN_SENSOR_6_MAPPING;
    
    GEN_SENSOR_7_MAPPING: if C_SENSOR_PORTS >= 8 generate
        GEN_SENSOR_7_MAPPING_2: for idx in 0 to C_SENSOR_7_CH_COUNT - 1 generate
            sensor_data_bus((CHANNEL_OFFSET_7 + idx + 1) * C_DATA_WIDTH - 1 downto (CHANNEL_OFFSET_7 + idx) * C_DATA_WIDTH) <= sensor_data_7((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
            sensor_valid_bus(CHANNEL_OFFSET_7 + idx) <= sensor_valid_7(idx);
        end generate GEN_SENSOR_7_MAPPING_2;
    end generate GEN_SENSOR_7_MAPPING;
    
    -- Round robin counter
    process (clk) begin
        if rising_edge(clk) then
            if (rstn = '0') then
                counter <= 0;
            else
                if (counter = (CHANNEL_COUNT - 1)) then
                    counter <= 0;
                else
                    counter <= counter + 1;
                end if;
            end if;
        end if;
    end process;


    GEN_LATCH_SENSOR_DATA : for idx in 0 to CHANNEL_COUNT - 1 generate
        process (clk) begin
            if rising_edge(clk) then
                if (rstn = '0') then
                    sensor_data_bus_l((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH) <= (others => '0');
                    sensor_valid_bus_l(idx) <= '0';
                else
                    if (sensor_valid_bus(idx) = '1') then
                        sensor_data_bus_l((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH) <= sensor_data_bus((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH);
                        sensor_valid_bus_l(idx) <= '1';
                    elsif (counter = idx) then
                        sensor_data_bus_l((idx + 1) * C_DATA_WIDTH - 1 downto idx * C_DATA_WIDTH) <= (others => '0');
                        sensor_valid_bus_l(idx) <= '0';
                    else
                        -- Keep value for later use
                    end if;
                end if;
            end if;
        end process;
    end generate GEN_LATCH_SENSOR_DATA;
    
    process (clk) begin
        if rising_edge(clk) then
            if (rstn = '0') then
                sensor_data_out <= (others => '0');
                sensor_id_out <= (others => '0');
                sensor_valid_out <= '0';
            else
                if (sensor_valid_bus_l(counter) = '1') then
                    sensor_data_out <= sensor_data_bus_l((counter + 1) * C_DATA_WIDTH - 1 downto counter * C_DATA_WIDTH);
                    sensor_id_out <= std_logic_vector(to_unsigned(counter, sensor_id_out'length));
                    sensor_valid_out <= '1';
                else
                    sensor_data_out <= (others => '0');
                    sensor_id_out <= (others => '0');
                    sensor_valid_out <= '0';
                end if;
            end if;
        
        end if;
    end process;

end arch_imp;
