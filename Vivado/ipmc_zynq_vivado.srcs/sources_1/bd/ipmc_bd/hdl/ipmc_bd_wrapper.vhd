--Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2018.2 (lin64) Build 2258646 Thu Jun 14 20:02:38 MDT 2018
--Date        : Fri Dec  7 16:21:27 2018
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
    ELM_LINK_tri_io : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    ELM_UART_rxd : in STD_LOGIC;
    ELM_UART_txd : out STD_LOGIC;
    ESM_FLASH_SPI_io0_io : inout STD_LOGIC;
    ESM_FLASH_SPI_io1_io : inout STD_LOGIC;
    ESM_FLASH_SPI_sck_io : inout STD_LOGIC;
    ESM_FLASH_SPI_ss_io : inout STD_LOGIC_VECTOR ( 0 to 0 );
    ESM_RESET_tri_io : inout STD_LOGIC_VECTOR ( 1 downto 0 );
    ESM_UART_rxd : in STD_LOGIC;
    ESM_UART_txd : out STD_LOGIC;
    HNDL_SW : in STD_LOGIC;
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
    pwr_status : in STD_LOGIC_VECTOR ( 5 downto 0 );
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ATCA_LEDS : out STD_LOGIC_VECTOR ( 3 downto 0 );
    pwr_en : out STD_LOGIC_VECTOR ( 6 downto 0 );
    TCK : out STD_LOGIC;
    TDI : out STD_LOGIC;
    TDO : in STD_LOGIC;
    TMS : out STD_LOGIC;
    HNDL_SW : in STD_LOGIC;
    ELM_LINK_tri_i : in STD_LOGIC_VECTOR ( 3 downto 0 );
    ELM_LINK_tri_o : out STD_LOGIC_VECTOR ( 3 downto 0 );
    ELM_LINK_tri_t : out STD_LOGIC_VECTOR ( 3 downto 0 );
    ELM_UART_rxd : in STD_LOGIC;
    ELM_UART_txd : out STD_LOGIC;
    ESM_UART_rxd : in STD_LOGIC;
    ESM_UART_txd : out STD_LOGIC;
    ESM_RESET_tri_i : in STD_LOGIC_VECTOR ( 1 downto 0 );
    ESM_RESET_tri_o : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ESM_RESET_tri_t : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ESM_FLASH_SPI_io0_i : in STD_LOGIC;
    ESM_FLASH_SPI_io0_o : out STD_LOGIC;
    ESM_FLASH_SPI_io0_t : out STD_LOGIC;
    ESM_FLASH_SPI_io1_i : in STD_LOGIC;
    ESM_FLASH_SPI_io1_o : out STD_LOGIC;
    ESM_FLASH_SPI_io1_t : out STD_LOGIC;
    ESM_FLASH_SPI_sck_i : in STD_LOGIC;
    ESM_FLASH_SPI_sck_o : out STD_LOGIC;
    ESM_FLASH_SPI_sck_t : out STD_LOGIC;
    ESM_FLASH_SPI_ss_i : in STD_LOGIC_VECTOR ( 0 to 0 );
    ESM_FLASH_SPI_ss_o : out STD_LOGIC_VECTOR ( 0 to 0 );
    ESM_FLASH_SPI_ss_t : out STD_LOGIC
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
  signal ELM_LINK_tri_i_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ELM_LINK_tri_i_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ELM_LINK_tri_i_2 : STD_LOGIC_VECTOR ( 2 to 2 );
  signal ELM_LINK_tri_i_3 : STD_LOGIC_VECTOR ( 3 to 3 );
  signal ELM_LINK_tri_io_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ELM_LINK_tri_io_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ELM_LINK_tri_io_2 : STD_LOGIC_VECTOR ( 2 to 2 );
  signal ELM_LINK_tri_io_3 : STD_LOGIC_VECTOR ( 3 to 3 );
  signal ELM_LINK_tri_o_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ELM_LINK_tri_o_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ELM_LINK_tri_o_2 : STD_LOGIC_VECTOR ( 2 to 2 );
  signal ELM_LINK_tri_o_3 : STD_LOGIC_VECTOR ( 3 to 3 );
  signal ELM_LINK_tri_t_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ELM_LINK_tri_t_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ELM_LINK_tri_t_2 : STD_LOGIC_VECTOR ( 2 to 2 );
  signal ELM_LINK_tri_t_3 : STD_LOGIC_VECTOR ( 3 to 3 );
  signal ESM_FLASH_SPI_io0_i : STD_LOGIC;
  signal ESM_FLASH_SPI_io0_o : STD_LOGIC;
  signal ESM_FLASH_SPI_io0_t : STD_LOGIC;
  signal ESM_FLASH_SPI_io1_i : STD_LOGIC;
  signal ESM_FLASH_SPI_io1_o : STD_LOGIC;
  signal ESM_FLASH_SPI_io1_t : STD_LOGIC;
  signal ESM_FLASH_SPI_sck_i : STD_LOGIC;
  signal ESM_FLASH_SPI_sck_o : STD_LOGIC;
  signal ESM_FLASH_SPI_sck_t : STD_LOGIC;
  signal ESM_FLASH_SPI_ss_i_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_FLASH_SPI_ss_io_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_FLASH_SPI_ss_o_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_FLASH_SPI_ss_t : STD_LOGIC;
  signal ESM_RESET_tri_i_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_RESET_tri_i_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ESM_RESET_tri_io_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_RESET_tri_io_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ESM_RESET_tri_o_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_RESET_tri_o_1 : STD_LOGIC_VECTOR ( 1 to 1 );
  signal ESM_RESET_tri_t_0 : STD_LOGIC_VECTOR ( 0 to 0 );
  signal ESM_RESET_tri_t_1 : STD_LOGIC_VECTOR ( 1 to 1 );
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
ELM_LINK_tri_iobuf_0: component IOBUF
     port map (
      I => ELM_LINK_tri_o_0(0),
      IO => ELM_LINK_tri_io(0),
      O => ELM_LINK_tri_i_0(0),
      T => ELM_LINK_tri_t_0(0)
    );
ELM_LINK_tri_iobuf_1: component IOBUF
     port map (
      I => ELM_LINK_tri_o_1(1),
      IO => ELM_LINK_tri_io(1),
      O => ELM_LINK_tri_i_1(1),
      T => ELM_LINK_tri_t_1(1)
    );
ELM_LINK_tri_iobuf_2: component IOBUF
     port map (
      I => ELM_LINK_tri_o_2(2),
      IO => ELM_LINK_tri_io(2),
      O => ELM_LINK_tri_i_2(2),
      T => ELM_LINK_tri_t_2(2)
    );
ELM_LINK_tri_iobuf_3: component IOBUF
     port map (
      I => ELM_LINK_tri_o_3(3),
      IO => ELM_LINK_tri_io(3),
      O => ELM_LINK_tri_i_3(3),
      T => ELM_LINK_tri_t_3(3)
    );
ESM_FLASH_SPI_io0_iobuf: component IOBUF
     port map (
      I => ESM_FLASH_SPI_io0_o,
      IO => ESM_FLASH_SPI_io0_io,
      O => ESM_FLASH_SPI_io0_i,
      T => ESM_FLASH_SPI_io0_t
    );
ESM_FLASH_SPI_io1_iobuf: component IOBUF
     port map (
      I => ESM_FLASH_SPI_io1_o,
      IO => ESM_FLASH_SPI_io1_io,
      O => ESM_FLASH_SPI_io1_i,
      T => ESM_FLASH_SPI_io1_t
    );
ESM_FLASH_SPI_sck_iobuf: component IOBUF
     port map (
      I => ESM_FLASH_SPI_sck_o,
      IO => ESM_FLASH_SPI_sck_io,
      O => ESM_FLASH_SPI_sck_i,
      T => ESM_FLASH_SPI_sck_t
    );
ESM_FLASH_SPI_ss_iobuf_0: component IOBUF
     port map (
      I => ESM_FLASH_SPI_ss_o_0(0),
      IO => ESM_FLASH_SPI_ss_io(0),
      O => ESM_FLASH_SPI_ss_i_0(0),
      T => ESM_FLASH_SPI_ss_t
    );
ESM_RESET_tri_iobuf_0: component IOBUF
     port map (
      I => ESM_RESET_tri_o_0(0),
      IO => ESM_RESET_tri_io(0),
      O => ESM_RESET_tri_i_0(0),
      T => ESM_RESET_tri_t_0(0)
    );
ESM_RESET_tri_iobuf_1: component IOBUF
     port map (
      I => ESM_RESET_tri_o_1(1),
      IO => ESM_RESET_tri_io(1),
      O => ESM_RESET_tri_i_1(1),
      T => ESM_RESET_tri_t_1(1)
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
      ELM_LINK_tri_i(3) => ELM_LINK_tri_i_3(3),
      ELM_LINK_tri_i(2) => ELM_LINK_tri_i_2(2),
      ELM_LINK_tri_i(1) => ELM_LINK_tri_i_1(1),
      ELM_LINK_tri_i(0) => ELM_LINK_tri_i_0(0),
      ELM_LINK_tri_o(3) => ELM_LINK_tri_o_3(3),
      ELM_LINK_tri_o(2) => ELM_LINK_tri_o_2(2),
      ELM_LINK_tri_o(1) => ELM_LINK_tri_o_1(1),
      ELM_LINK_tri_o(0) => ELM_LINK_tri_o_0(0),
      ELM_LINK_tri_t(3) => ELM_LINK_tri_t_3(3),
      ELM_LINK_tri_t(2) => ELM_LINK_tri_t_2(2),
      ELM_LINK_tri_t(1) => ELM_LINK_tri_t_1(1),
      ELM_LINK_tri_t(0) => ELM_LINK_tri_t_0(0),
      ELM_UART_rxd => ELM_UART_rxd,
      ELM_UART_txd => ELM_UART_txd,
      ESM_FLASH_SPI_io0_i => ESM_FLASH_SPI_io0_i,
      ESM_FLASH_SPI_io0_o => ESM_FLASH_SPI_io0_o,
      ESM_FLASH_SPI_io0_t => ESM_FLASH_SPI_io0_t,
      ESM_FLASH_SPI_io1_i => ESM_FLASH_SPI_io1_i,
      ESM_FLASH_SPI_io1_o => ESM_FLASH_SPI_io1_o,
      ESM_FLASH_SPI_io1_t => ESM_FLASH_SPI_io1_t,
      ESM_FLASH_SPI_sck_i => ESM_FLASH_SPI_sck_i,
      ESM_FLASH_SPI_sck_o => ESM_FLASH_SPI_sck_o,
      ESM_FLASH_SPI_sck_t => ESM_FLASH_SPI_sck_t,
      ESM_FLASH_SPI_ss_i(0) => ESM_FLASH_SPI_ss_i_0(0),
      ESM_FLASH_SPI_ss_o(0) => ESM_FLASH_SPI_ss_o_0(0),
      ESM_FLASH_SPI_ss_t => ESM_FLASH_SPI_ss_t,
      ESM_RESET_tri_i(1) => ESM_RESET_tri_i_1(1),
      ESM_RESET_tri_i(0) => ESM_RESET_tri_i_0(0),
      ESM_RESET_tri_o(1) => ESM_RESET_tri_o_1(1),
      ESM_RESET_tri_o(0) => ESM_RESET_tri_o_0(0),
      ESM_RESET_tri_t(1) => ESM_RESET_tri_t_1(1),
      ESM_RESET_tri_t(0) => ESM_RESET_tri_t_0(0),
      ESM_UART_rxd => ESM_UART_rxd,
      ESM_UART_txd => ESM_UART_txd,
      HNDL_SW => HNDL_SW,
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
