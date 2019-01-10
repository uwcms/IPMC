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
    Generic ( ACTIVE_HIGH : boolean := false );
    Port ( RST : in STD_LOGIC;
           CLK : in STD_LOGIC;
           
           LIMIT : in STD_LOGIC_VECTOR (27 downto 0);
           COMP : in STD_LOGIC_VECTOR (27 downto 0);
           PULSING : in STD_LOGIC;
           
           LED : out STD_LOGIC);
end led_ctrlr;

architecture Behavioral of led_ctrlr is
    component varcounter generic (
        WIDTH : integer := 28;
        NOLIMIT : boolean := false);
    port (
        CLK : in STD_LOGIC;
        RST : in STD_LOGIC;
        EN : in STD_LOGIC;
        LIM : in STD_LOGIC_VECTOR (WIDTH-1 downto 0);
        VAL : out STD_LOGIC_VECTOR (WIDTH-1 downto 0);
        MAX : out STD_LOGIC);
    end component;
    
    component pulse
    port (
        CLK : in STD_LOGIC;
        RST : in STD_LOGIC;
        EN : in STD_LOGIC;
        
        STEPINC : in STD_LOGIC;
        
        PWM : out STD_LOGIC);
    end component;
	
	signal counter_val : std_logic_vector (27 downto 0);
	signal counter_max : std_logic;
	
	signal comp_bt : std_logic;
	
	signal pwm_out : std_logic;
	
	signal led_out : std_logic;
begin
    i1_varcounter : varcounter
    port map (
        CLK => CLK,
        RST => RST,
        EN => '1',
        LIM => LIMIT,
        VAL => counter_val,
        MAX => counter_max
    );
    
    i2_pulse : pulse
    port map (
        CLK => CLK,
        RST => RST,
        EN => PULSING,
        
        STEPINC => counter_max,
        
        PWM => pwm_out
    );
    
    process (CLK) begin
        if (CLK'event and CLK = '1') then
            if (unsigned(counter_val) < unsigned(COMP)) then
                comp_bt <= '1';
            else 
                comp_bt <= '0';
            end if;
        end if;
    end process;
    
    led_out <= comp_bt when PULSING = '0' else pwm_out;
    LED <= led_out when ACTIVE_HIGH = true else not led_out;
	
end Behavioral;
