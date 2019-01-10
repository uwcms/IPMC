----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 01/08/2019 01:52:22 PM
-- Design Name: 
-- Module Name: pulse - Behavioral
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

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity pulse is
    Port (
        CLK : in STD_LOGIC;
        RST : in STD_LOGIC;
        EN : in STD_LOGIC;
        
        STEPINC : in STD_LOGIC;
        
        PWM : out STD_LOGIC
    );
end pulse;

architecture Behavioral of pulse is
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
    
    signal step_inc : std_logic := '0';
    signal step_out : std_logic_vector (7 downto 0);
    signal step_max : std_logic;
    
    signal pwm_out : std_logic_vector (7 downto 0);
    signal pwm_bt : std_logic;
    
    signal dec : std_logic := '0';
begin

step_inc <= STEPINC when CLK'event and CLK = '1';

i1_step : varcounter
generic map (
    WIDTH => 8,
    NOLIMIT => true
)
port map (
    CLK => CLK,
    RST => RST,
    EN => step_inc,
    LIM => x"ff",
    VAL => step_out,
    MAX => step_max
);

process (CLK) begin
    if (CLK'event and CLK = '1') then
        if (RST = '1') then
            dec <= '0';
        else
            if (step_max = '1') then
                dec <= not dec;
            end if;
        end if;
    end if;
end process;

i2_pwm : varcounter
generic map (
    WIDTH => 8,
    NOLIMIT => true
)
port map (
    CLK => CLK,
    RST => RST,
    EN => EN,
    LIM => x"ff",
    VAL => pwm_out,
    MAX => open
);

pwm_bt <= '1' when unsigned(pwm_out) > unsigned(step_out) else '0';
PWM <= pwm_bt when dec = '0' else not pwm_bt;

end Behavioral;
