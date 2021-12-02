library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity gpio_master_tristate is
    Generic ( PINS : integer := 1 );
    Port (
            T : in std_logic;
            SOURCE_tri_o : in STD_LOGIC_VECTOR(PINS-1 downto 0);
            SOURCE_tri_t : in STD_LOGIC_VECTOR(PINS-1 downto 0);
            SOURCE_tri_i : out STD_LOGIC_VECTOR(PINS-1 downto 0);
            GATED_tri_o : out STD_LOGIC_VECTOR(PINS-1 downto 0);
            GATED_tri_t : out STD_LOGIC_VECTOR(PINS-1 downto 0);
            GATED_tri_i : in STD_LOGIC_VECTOR(PINS-1 downto 0)
        );
end gpio_master_tristate;

architecture Behavioral of gpio_master_tristate is

    ATTRIBUTE X_INTERFACE_INFO : STRING;
    ATTRIBUTE X_INTERFACE_MODE : STRING;
    ATTRIBUTE X_INTERFACE_INFO of SOURCE_tri_o: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 SOURCE TRI_O";
    ATTRIBUTE X_INTERFACE_INFO of SOURCE_tri_t: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 SOURCE TRI_T";
    ATTRIBUTE X_INTERFACE_INFO of SOURCE_tri_i: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 SOURCE TRI_I";
    ATTRIBUTE X_INTERFACE_MODE of SOURCE_tri_i: SIGNAL is "slave";
    ATTRIBUTE X_INTERFACE_INFO of GATED_tri_o: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 GATED TRI_O";
    ATTRIBUTE X_INTERFACE_INFO of GATED_tri_t: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 GATED TRI_T";
    ATTRIBUTE X_INTERFACE_INFO of GATED_tri_i: SIGNAL is "xilinx.com:interface:gpio_rtl:1.0 GATED TRI_I";
    ATTRIBUTE X_INTERFACE_MODE of GATED_tri_i: SIGNAL is "master";

    signal s_O : std_logic_vector(PINS-1 downto 0);
    signal s_T : std_logic_vector(PINS-1 downto 0);
    signal s_I : std_logic_vector(PINS-1 downto 0);

begin
    s_O <= SOURCE_tri_o;
    s_T <= (others => '1') when T = '1' else SOURCE_tri_t;
    s_I <= GATED_tri_i;
    GATED_tri_o <= s_O;
    GATED_tri_t <= s_T;
    SOURCE_tri_i <= s_I;
end Behavioral;
