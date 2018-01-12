----------------------------------------------------------------------------------
-- Engineer: Marcelo Vicente
-- Module Name: led_ctrl - Behavioral
-- Project Name: Optical Receiver Mezzanine
-- Target Devices: xc7k70tfbg676-1
-- Tool Versions: Vivado 2014.2
-- Description: LED controller
----------------------------------------------------------------------------------


library IEEE;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_misc.all;

entity led_ctrlr is
    Generic ( BIT_DIV : integer := 10;
              ACTIVE_HIGH : boolean := false );
    Port ( RST : in STD_LOGIC;
           CLK : in STD_LOGIC;
           MODE : in STD_LOGIC_VECTOR (1 downto 0);
           VAL : in STD_LOGIC_VECTOR (7 downto 0);
           LED : out STD_LOGIC);
end led_ctrlr;

architecture Behavioral of led_ctrlr is
	component pwm port (
		CLK : in STD_LOGIC;
		EN : in STD_LOGIC;
		I : in STD_LOGIC_VECTOR (7 downto 0);
		O : out STD_LOGIC);
	end component;
	
	component pulse port (
		CLK : in STD_LOGIC;
		EN : in STD_LOGIC;
		I : in STD_LOGIC_VECTOR (7 downto 0);
		O : out STD_LOGIC_VECTOR (7 downto 0);
		D : out STD_LOGIC);
	end component;

	signal clk_o : std_logic;
	signal cycle : std_logic_vector (BIT_DIV-1 downto 0);
	
	-- PWM block
	signal pwm_en : std_logic;
	signal pwm_i : std_logic_vector (7 downto 0);
	
	-- Pulse block
	signal pulse_en : std_logic;
	signal pulse_o : std_logic_vector (7 downto 0);
	signal pulse_d : std_logic;
	
	-- Output signals
	signal led_out : std_logic;
	signal timed_out : std_logic;
	signal pwm_out : std_logic;
begin
	i1_pwm : pwm port map (
		CLK => clk_o,
		EN => pwm_en,
		I => pwm_i,
		O => pwm_out
	);
	pwm_en <= '1' when pulse_en = '1' or (RST = '0' and MODE = "10") else '0';
	pwm_i <= pulse_o when pulse_en = '1' else VAL;
	
	i3_pulse : pulse port map (
		CLK => clk_o,
		EN => pulse_en,
		I => VAL,
		O => pulse_o,
		D => pulse_d
	);
	pulse_en <= '1' when RST = '0' and MODE = "01" else '0';
	
	process (CLK) begin
		if CLK'event and CLK = '1' then
			cycle <= cycle + 1;
		end if;
	end process;
	clk_o <= cycle(BIT_DIV-1);
	
	-- Output
	led_out <=	VAL(0)		when MODE = "00" else
			    pwm_out		when MODE = "01" else -- Pulse block will control PWM
			    pwm_out		when MODE = "10" else
			    '0';
			    
    LED <= led_out when ACTIVE_HIGH = true else not led_out;
	
end Behavioral;
