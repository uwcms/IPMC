----------------------------------------------------------------------------------
-- IPMC revA and revB have TDO and TDI swapped, fix it here
----------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity ipmc_xvc_sel is
    Port ( TCK_MST : in STD_LOGIC;
           TMS_MST : in STD_LOGIC;
           TDI_MST : in STD_LOGIC;
           TDO_MST : out STD_LOGIC;
           
           EN : in STD_LOGIC;
           REVA : in STD_LOGIC;
           
           TCK_OUT : out STD_LOGIC;
           TMS_OUT : out STD_LOGIC;
           TDI_INOUT : inout STD_LOGIC;
           TDO_INOUT : inout STD_LOGIC
         );
end ipmc_xvc_sel;

architecture Behavioral of ipmc_xvc_sel is
    signal s_tdi_o, s_tdi_i, s_tdi_t : std_logic;
    signal s_tdo_o, s_tdo_i, s_tdo_t : std_logic;
begin
    s_tdi_t <= '1' when EN = '0' else (not REVA);

    TDI_iobuf: component IOBUF
    port map (
        I => s_tdi_o,
        IO => TDI_INOUT,
        O => s_tdi_i,
        T => s_tdi_t
    );
    
    s_tdo_t <= '1' when EN = '0' else REVA;
    
    TDO_iobuf: component IOBUF
    port map (
        I => s_tdo_o,
        IO => TDO_INOUT,
        O => s_tdo_i,
        T => s_tdo_t
    );
    
    TCK_OUT <= TCK_MST;
    TMS_OUT <= TMS_MST;
    
    s_tdi_o <= TDI_MST;
    s_tdo_o <= TDI_MST;
    
    TDO_MST <= s_tdo_i when REVA = '1' else s_tdi_i;

end Behavioral;
