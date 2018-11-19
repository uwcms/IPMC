
library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;


entity sensor_arbit_engine is
  generic (

    C_SENSOR_CNT        : integer := 35;
    C_SENSOR_DATA_WIDTH : integer := 16
    );
  port (
    clk_i : in std_logic;
    rst_i : in std_logic;

    sensor_data_i : in std_logic_vector(C_SENSOR_CNT * C_SENSOR_DATA_WIDTH - 1 downto 0);
    sensor_vld_i  : in std_logic_vector(C_SENSOR_CNT-1 downto 0);

    sensor_data_o : out std_logic_vector(C_SENSOR_DATA_WIDTH - 1 downto 0);
    sensor_vld_o  : out std_logic;
    sensor_cnt_o  : out unsigned(5 downto 0)
    );
end sensor_arbit_engine;

architecture sensor_arbit_engine_arch of sensor_arbit_engine is

  signal s_sensor_data : std_logic_vector(C_SENSOR_CNT * C_SENSOR_DATA_WIDTH - 1 downto 0);
  signal s_sensor_vld  : std_logic_vector(C_SENSOR_CNT-1 downto 0);

  signal s_sensor_cnt : unsigned(5 downto 0);

begin

  process(clk_i) is
  begin
    if rising_edge(clk_i) then
      if (rst_i = '1') then
        s_sensor_cnt <= (others => '0');
      elsif (s_sensor_cnt = C_SENSOR_CNT-1) then
        s_sensor_cnt <= (others => '0');
      else
        s_sensor_cnt <= s_sensor_cnt + 1;
      end if;

    end if;
  end process;

  gen_latch_sensor_data : for idx in 0 to C_SENSOR_CNT-1 generate

    process(clk_i) is
    begin
      if rising_edge(clk_i) then
        if (rst_i = '1') then
          s_sensor_data((idx + 1) * C_SENSOR_DATA_WIDTH - 1 downto idx * C_SENSOR_DATA_WIDTH) <= (others => '0');
          s_sensor_vld(idx)                                                                   <= '0';
        elsif (sensor_vld_i(idx) = '1') then
          s_sensor_data((idx + 1) * C_SENSOR_DATA_WIDTH - 1 downto idx * C_SENSOR_DATA_WIDTH) <= sensor_data_i((idx + 1) * C_SENSOR_DATA_WIDTH - 1 downto idx * C_SENSOR_DATA_WIDTH);
          s_sensor_vld(idx)                                                                   <= '1';
        elsif ((to_integer(s_sensor_cnt)) = idx) then
          s_sensor_vld(idx) <= '0';
        end if;
      end if;
    end process;
  end generate;

  process(clk_i) is
  begin
    if rising_edge(clk_i) then
      if (rst_i = '1') then
        sensor_data_o <= (others => '0');
        sensor_vld_o  <= '0';
      elsif (s_sensor_vld(to_integer(s_sensor_cnt)) = '1') then
        sensor_data_o <= s_sensor_data((to_integer(s_sensor_cnt) + 1) * C_SENSOR_DATA_WIDTH - 1 downto to_integer(s_sensor_cnt) * C_SENSOR_DATA_WIDTH);
        sensor_vld_o  <= '1';
      else
        sensor_vld_o <= '0';
      end if;

    end if;
  end process;

  sensor_cnt_o <= s_sensor_cnt when rising_edge(clk_i);

end sensor_arbit_engine_arch;
