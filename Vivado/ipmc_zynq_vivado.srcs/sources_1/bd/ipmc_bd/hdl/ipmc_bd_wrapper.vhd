--Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2018.2 (lin64) Build 2258646 Thu Jun 14 20:02:38 MDT 2018
--Date        : Tue Jul 24 11:54:35 2018
--Host        : beck.hep.wisc.edu running 64-bit CentOS Linux release 7.5.1804 (Core)
--Command     : generate_target ipmc_bd_wrapper.bd
--Design      : ipmc_bd_wrapper
--Purpose     : IP block netlist
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
library UNISIM;
use UNISIM.VCOMPONENTS.ALL;
entity ipmc_bd_wrapper is
  port (
    ADC_A_clk : out STD_LOGIC;
    ADC_A_miso : in STD_LOGIC;
    ADC_A_mosi : out STD_LOGIC;
    ADC_A_ncs0 : out STD_LOGIC;
    ADC_B_clk : out STD_LOGIC;
    ADC_B_miso : in STD_LOGIC;
    ADC_B_mosi : out STD_LOGIC;
    ADC_B_ncs0 : out STD_LOGIC;
    ATCA_LEDS : out STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_cas_n : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    EEPROM_I2C_0_scl_io : inout STD_LOGIC;
    EEPROM_I2C_0_sda_io : inout STD_LOGIC;
    ESM_UART_rxd : in STD_LOGIC;
    ESM_UART_txd : out STD_LOGIC;
    PIM400_I2C_scl_io : inout STD_LOGIC;
    PIM400_I2C_sda_io : inout STD_LOGIC;
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    TCK : out STD_LOGIC;
    TDI : out STD_LOGIC;
    TDO : in STD_LOGIC;
    TMS : out STD_LOGIC;
    pwr_en : out STD_LOGIC_VECTOR ( 6 downto 0 );
    pwr_status : in STD_LOGIC_VECTOR ( 5 downto 0 )
  );
end ipmc_bd_wrapper;

architecture STRUCTURE of ipmc_bd_wrapper is
  component ipmc_bd is
  port (
    ADC_A_ncs0 : out STD_LOGIC;
    ADC_A_clk : out STD_LOGIC;
    ADC_A_mosi : out STD_LOGIC;
    ADC_A_miso : in STD_LOGIC;
    ADC_B_ncs0 : out STD_LOGIC;
    ADC_B_clk : out STD_LOGIC;
    ADC_B_mosi : out STD_LOGIC;
    ADC_B_miso : in STD_LOGIC;
    DDR_cas_n : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    PIM400_I2C_scl_i : in STD_LOGIC;
    PIM400_I2C_scl_o : out STD_LOGIC;
    PIM400_I2C_scl_t : out STD_LOGIC;
    PIM400_I2C_sda_i : in STD_LOGIC;
    PIM400_I2C_sda_o : out STD_LOGIC;
    PIM400_I2C_sda_t : out STD_LOGIC;
    EEPROM_I2C_0_scl_i : in STD_LOGIC;
    EEPROM_I2C_0_scl_o : out STD_LOGIC;
    EEPROM_I2C_0_scl_t : out STD_LOGIC;
    EEPROM_I2C_0_sda_i : in STD_LOGIC;
    EEPROM_I2C_0_sda_o : out STD_LOGIC;
    EEPROM_I2C_0_sda_t : out STD_LOGIC;
    ESM_UART_rxd : in STD_LOGIC;
    ESM_UART_txd : out STD_LOGIC;
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ATCA_LEDS : out STD_LOGIC_VECTOR ( 3 downto 0 );
    pwr_en : out STD_LOGIC_VECTOR ( 6 downto 0 );
    pwr_status : in STD_LOGIC_VECTOR ( 5 downto 0 );
    TCK : out STD_LOGIC;
    TDI : out STD_LOGIC;
    TDO : in STD_LOGIC;
    TMS : out STD_LOGIC
  );
  end component ipmc_bd;
  component IOBUF is
  port (
    I : in STD_LOGIC;
    O : out STD_LOGIC;
    T : in STD_LOGIC;
    IO : inout STD_LOGIC
  );
  end component IOBUF;
  signal EEPROM_I2C_0_scl_i : STD_LOGIC;
  signal EEPROM_I2C_0_scl_o : STD_LOGIC;
  signal EEPROM_I2C_0_scl_t : STD_LOGIC;
  signal EEPROM_I2C_0_sda_i : STD_LOGIC;
  signal EEPROM_I2C_0_sda_o : STD_LOGIC;
  signal EEPROM_I2C_0_sda_t : STD_LOGIC;
  signal PIM400_I2C_scl_i : STD_LOGIC;
  signal PIM400_I2C_scl_o : STD_LOGIC;
  signal PIM400_I2C_scl_t : STD_LOGIC;
  signal PIM400_I2C_sda_i : STD_LOGIC;
  signal PIM400_I2C_sda_o : STD_LOGIC;
  signal PIM400_I2C_sda_t : STD_LOGIC;
begin
EEPROM_I2C_0_scl_iobuf: component IOBUF
     port map (
      I => EEPROM_I2C_0_scl_o,
      IO => EEPROM_I2C_0_scl_io,
      O => EEPROM_I2C_0_scl_i,
      T => EEPROM_I2C_0_scl_t
    );
EEPROM_I2C_0_sda_iobuf: component IOBUF
     port map (
      I => EEPROM_I2C_0_sda_o,
      IO => EEPROM_I2C_0_sda_io,
      O => EEPROM_I2C_0_sda_i,
      T => EEPROM_I2C_0_sda_t
    );
PIM400_I2C_scl_iobuf: component IOBUF
     port map (
      I => PIM400_I2C_scl_o,
      IO => PIM400_I2C_scl_io,
      O => PIM400_I2C_scl_i,
      T => PIM400_I2C_scl_t
    );
PIM400_I2C_sda_iobuf: component IOBUF
     port map (
      I => PIM400_I2C_sda_o,
      IO => PIM400_I2C_sda_io,
      O => PIM400_I2C_sda_i,
      T => PIM400_I2C_sda_t
    );
ipmc_bd_i: component ipmc_bd
     port map (
      ADC_A_clk => ADC_A_clk,
      ADC_A_miso => ADC_A_miso,
      ADC_A_mosi => ADC_A_mosi,
      ADC_A_ncs0 => ADC_A_ncs0,
      ADC_B_clk => ADC_B_clk,
      ADC_B_miso => ADC_B_miso,
      ADC_B_mosi => ADC_B_mosi,
      ADC_B_ncs0 => ADC_B_ncs0,
      ATCA_LEDS(3 downto 0) => ATCA_LEDS(3 downto 0),
      DDR_addr(14 downto 0) => DDR_addr(14 downto 0),
      DDR_ba(2 downto 0) => DDR_ba(2 downto 0),
      DDR_cas_n => DDR_cas_n,
      DDR_ck_n => DDR_ck_n,
      DDR_ck_p => DDR_ck_p,
      DDR_cke => DDR_cke,
      DDR_cs_n => DDR_cs_n,
      DDR_dm(3 downto 0) => DDR_dm(3 downto 0),
      DDR_dq(31 downto 0) => DDR_dq(31 downto 0),
      DDR_dqs_n(3 downto 0) => DDR_dqs_n(3 downto 0),
      DDR_dqs_p(3 downto 0) => DDR_dqs_p(3 downto 0),
      DDR_odt => DDR_odt,
      DDR_ras_n => DDR_ras_n,
      DDR_reset_n => DDR_reset_n,
      DDR_we_n => DDR_we_n,
      EEPROM_I2C_0_scl_i => EEPROM_I2C_0_scl_i,
      EEPROM_I2C_0_scl_o => EEPROM_I2C_0_scl_o,
      EEPROM_I2C_0_scl_t => EEPROM_I2C_0_scl_t,
      EEPROM_I2C_0_sda_i => EEPROM_I2C_0_sda_i,
      EEPROM_I2C_0_sda_o => EEPROM_I2C_0_sda_o,
      EEPROM_I2C_0_sda_t => EEPROM_I2C_0_sda_t,
      ESM_UART_rxd => ESM_UART_rxd,
      ESM_UART_txd => ESM_UART_txd,
      PIM400_I2C_scl_i => PIM400_I2C_scl_i,
      PIM400_I2C_scl_o => PIM400_I2C_scl_o,
      PIM400_I2C_scl_t => PIM400_I2C_scl_t,
      PIM400_I2C_sda_i => PIM400_I2C_sda_i,
      PIM400_I2C_sda_o => PIM400_I2C_sda_o,
      PIM400_I2C_sda_t => PIM400_I2C_sda_t,
      PL_LEDS(1 downto 0) => PL_LEDS(1 downto 0),
      TCK => TCK,
      TDI => TDI,
      TDO => TDO,
      TMS => TMS,
      pwr_en(6 downto 0) => pwr_en(6 downto 0),
      pwr_status(5 downto 0) => pwr_status(5 downto 0)
    );
end STRUCTURE;
