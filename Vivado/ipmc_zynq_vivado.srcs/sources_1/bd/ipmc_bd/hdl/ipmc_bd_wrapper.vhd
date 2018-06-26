--Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2017.2 (lin64) Build 1909853 Thu Jun 15 18:39:10 MDT 2017
--Date        : Tue Jun 26 16:16:14 2018
--Host        : beck.hep.wisc.edu running 64-bit CentOS Linux release 7.4.1708 (Core)
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
    ALARM_A : in STD_LOGIC;
    ALARM_B : in STD_LOGIC;
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
    GPIO : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    HNDL_SW : in STD_LOGIC;
    PG_A : in STD_LOGIC;
    PG_B : in STD_LOGIC;
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
    ADC_A_clk : out STD_LOGIC;
    ADC_A_mosi : out STD_LOGIC;
    ADC_A_miso : in STD_LOGIC;
    ADC_A_ncs0 : out STD_LOGIC;
    ADC_B_clk : out STD_LOGIC;
    ADC_B_mosi : out STD_LOGIC;
    ADC_B_miso : in STD_LOGIC;
    ADC_B_ncs0 : out STD_LOGIC;
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
    ALARM_A : in STD_LOGIC;
    ALARM_B : in STD_LOGIC;
    HNDL_SW : in STD_LOGIC;
    PG_A : in STD_LOGIC;
    PG_B : in STD_LOGIC;
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ATCA_LEDS : out STD_LOGIC_VECTOR ( 3 downto 0 );
    pwr_en : out STD_LOGIC_VECTOR ( 6 downto 0 );
    GPIO : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    pwr_status : in STD_LOGIC_VECTOR ( 5 downto 0 );
    TCK : out STD_LOGIC;
    TDI : out STD_LOGIC;
    TDO : in STD_LOGIC;
    TMS : out STD_LOGIC
  );
  end component ipmc_bd;
begin
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
      ALARM_A => ALARM_A,
      ALARM_B => ALARM_B,
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
      GPIO(31 downto 0) => GPIO(31 downto 0),
      HNDL_SW => HNDL_SW,
      PG_A => PG_A,
      PG_B => PG_B,
      PL_LEDS(1 downto 0) => PL_LEDS(1 downto 0),
      TCK => TCK,
      TDI => TDI,
      TDO => TDO,
      TMS => TMS,
      pwr_en(6 downto 0) => pwr_en(6 downto 0),
      pwr_status(5 downto 0) => pwr_status(5 downto 0)
    );
end STRUCTURE;
