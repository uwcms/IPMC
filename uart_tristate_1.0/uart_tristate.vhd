library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity uart_tristate is
    Port ( T : in std_logic;
           SOURCE_rxd : out std_logic;
           SOURCE_txd : in std_logic;
           GATED_rxd : in std_logic;
           GATED_txd : inout std_logic
           );
end uart_tristate;

architecture Behavioral of uart_tristate is

    ATTRIBUTE X_INTERFACE_INFO : STRING;
    ATTRIBUTE X_INTERFACE_MODE : STRING;
    ATTRIBUTE X_INTERFACE_INFO of SOURCE_txd: SIGNAL is "xilinx.com:interface:uart:1.0 SOURCE TxD";
    ATTRIBUTE X_INTERFACE_INFO of SOURCE_rxd: SIGNAL is "xilinx.com:interface:uart:1.0 SOURCE RxD";
    ATTRIBUTE X_INTERFACE_MODE of SOURCE_rxd: SIGNAL is "slave";
    ATTRIBUTE X_INTERFACE_INFO of GATED_rxd: SIGNAL is "xilinx.com:interface:uart:1.0 GATED RxD";
    ATTRIBUTE X_INTERFACE_INFO of GATED_txd: SIGNAL is "xilinx.com:interface:uart:1.0 GATED TxD";
    ATTRIBUTE X_INTERFACE_MODE of GATED_txd: SIGNAL is "master";

    signal s_rxd : std_logic := '0';
begin
    IOBUF_inst : IOBUF
    port map (
        O => open,
        I => SOURCE_txd,
        IO => GATED_txd,
        T => T
    );
    s_rxd <= '1' when T = '1' else GATED_rxd;
    SOURCE_rxd <= s_rxd;
end Behavioral;
