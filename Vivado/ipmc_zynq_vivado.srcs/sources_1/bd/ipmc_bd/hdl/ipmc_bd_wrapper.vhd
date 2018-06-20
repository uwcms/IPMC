--Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2017.2 (lin64) Build 1909853 Thu Jun 15 18:39:10 MDT 2017
--Date        : Wed Feb 28 15:47:50 2018
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
    HNDL_SW : in STD_LOGIC;
    I2C_0_scl : out STD_LOGIC;
    I2C_0_sda : inout STD_LOGIC;
    I2C_1_scl : out STD_LOGIC;
    I2C_1_sda : inout STD_LOGIC;
    I2C_2_scl : out STD_LOGIC;
    I2C_2_sda : inout STD_LOGIC;
    I2C_3_scl : out STD_LOGIC;
    I2C_3_sda : inout STD_LOGIC;
    I2C_4_scl : out STD_LOGIC;
    I2C_4_sda : inout STD_LOGIC;
    JTAG_tck : out STD_LOGIC;
    JTAG_tdi : out STD_LOGIC;
    JTAG_tdo : in STD_LOGIC;
    JTAG_tms : out STD_LOGIC;
    JTAG_trst : out STD_LOGIC;
    MC_0_enable_n : out STD_LOGIC;
    MC_0_ps1_n : in STD_LOGIC;
    MC_1_enable_n : out STD_LOGIC;
    MC_1_ps1_n : in STD_LOGIC;
    MC_2_enable_n : out STD_LOGIC;
    MC_2_ps1_n : in STD_LOGIC;
    MC_3_enable_n : out STD_LOGIC;
    MC_3_ps1_n : in STD_LOGIC;
    MC_4_enable_n : out STD_LOGIC;
    MC_4_ps1_n : in STD_LOGIC;
    PG_A : in STD_LOGIC;
    PG_B : in STD_LOGIC;
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    PYLD12V_EN : out STD_LOGIC_VECTOR ( 0 to 0 );
    TPS2358_0_mp_ena_n : out STD_LOGIC;
    TPS2358_0_mp_flt_n : in STD_LOGIC;
    TPS2358_0_mp_pg_n : in STD_LOGIC;
    TPS2358_0_pwr_ena_n : out STD_LOGIC;
    TPS2358_0_pwr_flt_n : in STD_LOGIC;
    TPS2358_0_pwr_pg_n : in STD_LOGIC;
    TPS2358_1_mp_ena_n : out STD_LOGIC;
    TPS2358_1_mp_flt_n : in STD_LOGIC;
    TPS2358_1_mp_pg_n : in STD_LOGIC;
    TPS2358_1_pwr_ena_n : out STD_LOGIC;
    TPS2358_1_pwr_flt_n : in STD_LOGIC;
    TPS2358_1_pwr_pg_n : in STD_LOGIC;
    TPS2358_2_mp_ena_n : out STD_LOGIC;
    TPS2358_2_mp_flt_n : in STD_LOGIC;
    TPS2358_2_mp_pg_n : in STD_LOGIC;
    TPS2358_2_pwr_ena_n : out STD_LOGIC;
    TPS2358_2_pwr_flt_n : in STD_LOGIC;
    TPS2358_2_pwr_pg_n : in STD_LOGIC;
    TPS2358_3_mp_ena_n : out STD_LOGIC;
    TPS2358_3_mp_flt_n : in STD_LOGIC;
    TPS2358_3_mp_pg_n : in STD_LOGIC;
    TPS2358_3_pwr_ena_n : out STD_LOGIC;
    TPS2358_3_pwr_flt_n : in STD_LOGIC;
    TPS2358_3_pwr_pg_n : in STD_LOGIC;
    TPS2358_4_mp_ena_n : out STD_LOGIC;
    TPS2358_4_mp_flt_n : in STD_LOGIC;
    TPS2358_4_mp_pg_n : in STD_LOGIC;
    TPS2358_4_pwr_ena_n : out STD_LOGIC;
    TPS2358_4_pwr_flt_n : in STD_LOGIC;
    TPS2358_4_pwr_pg_n : in STD_LOGIC;
    pim400_i2c_scl_io : inout STD_LOGIC;
    pim400_i2c_sda_io : inout STD_LOGIC
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
    JTAG_trst : out STD_LOGIC;
    JTAG_tdi : out STD_LOGIC;
    JTAG_tms : out STD_LOGIC;
    JTAG_tck : out STD_LOGIC;
    JTAG_tdo : in STD_LOGIC;
    ALARM_A : in STD_LOGIC;
    ALARM_B : in STD_LOGIC;
    HNDL_SW : in STD_LOGIC;
    PYLD12V_EN : out STD_LOGIC_VECTOR ( 0 to 0 );
    PG_A : in STD_LOGIC;
    PG_B : in STD_LOGIC;
    PL_LEDS : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ATCA_LEDS : out STD_LOGIC_VECTOR ( 3 downto 0 );
    I2C_3_scl : out STD_LOGIC;
    I2C_3_sda : inout STD_LOGIC;
    TPS2358_3_mp_ena_n : out STD_LOGIC;
    TPS2358_3_mp_pg_n : in STD_LOGIC;
    TPS2358_3_pwr_pg_n : in STD_LOGIC;
    TPS2358_3_pwr_flt_n : in STD_LOGIC;
    TPS2358_3_mp_flt_n : in STD_LOGIC;
    TPS2358_3_pwr_ena_n : out STD_LOGIC;
    MC_3_ps1_n : in STD_LOGIC;
    MC_3_enable_n : out STD_LOGIC;
    I2C_4_scl : out STD_LOGIC;
    I2C_4_sda : inout STD_LOGIC;
    TPS2358_4_mp_ena_n : out STD_LOGIC;
    TPS2358_4_mp_pg_n : in STD_LOGIC;
    TPS2358_4_pwr_pg_n : in STD_LOGIC;
    TPS2358_4_pwr_flt_n : in STD_LOGIC;
    TPS2358_4_mp_flt_n : in STD_LOGIC;
    TPS2358_4_pwr_ena_n : out STD_LOGIC;
    MC_4_ps1_n : in STD_LOGIC;
    MC_4_enable_n : out STD_LOGIC;
    I2C_0_scl : out STD_LOGIC;
    I2C_0_sda : inout STD_LOGIC;
    TPS2358_0_mp_ena_n : out STD_LOGIC;
    TPS2358_0_mp_pg_n : in STD_LOGIC;
    TPS2358_0_pwr_pg_n : in STD_LOGIC;
    TPS2358_0_pwr_flt_n : in STD_LOGIC;
    TPS2358_0_mp_flt_n : in STD_LOGIC;
    TPS2358_0_pwr_ena_n : out STD_LOGIC;
    MC_0_ps1_n : in STD_LOGIC;
    MC_0_enable_n : out STD_LOGIC;
    I2C_2_scl : out STD_LOGIC;
    I2C_2_sda : inout STD_LOGIC;
    TPS2358_2_mp_ena_n : out STD_LOGIC;
    TPS2358_2_mp_pg_n : in STD_LOGIC;
    TPS2358_2_pwr_pg_n : in STD_LOGIC;
    TPS2358_2_pwr_flt_n : in STD_LOGIC;
    TPS2358_2_mp_flt_n : in STD_LOGIC;
    TPS2358_2_pwr_ena_n : out STD_LOGIC;
    MC_2_ps1_n : in STD_LOGIC;
    MC_2_enable_n : out STD_LOGIC;
    I2C_1_scl : out STD_LOGIC;
    I2C_1_sda : inout STD_LOGIC;
    TPS2358_1_mp_ena_n : out STD_LOGIC;
    TPS2358_1_mp_pg_n : in STD_LOGIC;
    TPS2358_1_pwr_pg_n : in STD_LOGIC;
    TPS2358_1_pwr_flt_n : in STD_LOGIC;
    TPS2358_1_mp_flt_n : in STD_LOGIC;
    TPS2358_1_pwr_ena_n : out STD_LOGIC;
    MC_1_ps1_n : in STD_LOGIC;
    MC_1_enable_n : out STD_LOGIC;
    PIM400_I2C_scl_i : in STD_LOGIC;
    PIM400_I2C_scl_o : out STD_LOGIC;
    PIM400_I2C_scl_t : out STD_LOGIC;
    PIM400_I2C_sda_i : in STD_LOGIC;
    PIM400_I2C_sda_o : out STD_LOGIC;
    PIM400_I2C_sda_t : out STD_LOGIC
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
  signal pim400_i2c_scl_i : STD_LOGIC;
  signal pim400_i2c_scl_o : STD_LOGIC;
  signal pim400_i2c_scl_t : STD_LOGIC;
  signal pim400_i2c_sda_i : STD_LOGIC;
  signal pim400_i2c_sda_o : STD_LOGIC;
  signal pim400_i2c_sda_t : STD_LOGIC;
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
      HNDL_SW => HNDL_SW,
      I2C_0_scl => I2C_0_scl,
      I2C_0_sda => I2C_0_sda,
      I2C_1_scl => I2C_1_scl,
      I2C_1_sda => I2C_1_sda,
      I2C_2_scl => I2C_2_scl,
      I2C_2_sda => I2C_2_sda,
      I2C_3_scl => I2C_3_scl,
      I2C_3_sda => I2C_3_sda,
      I2C_4_scl => I2C_4_scl,
      I2C_4_sda => I2C_4_sda,
      JTAG_tck => JTAG_tck,
      JTAG_tdi => JTAG_tdi,
      JTAG_tdo => JTAG_tdo,
      JTAG_tms => JTAG_tms,
      JTAG_trst => JTAG_trst,
      MC_0_enable_n => MC_0_enable_n,
      MC_0_ps1_n => MC_0_ps1_n,
      MC_1_enable_n => MC_1_enable_n,
      MC_1_ps1_n => MC_1_ps1_n,
      MC_2_enable_n => MC_2_enable_n,
      MC_2_ps1_n => MC_2_ps1_n,
      MC_3_enable_n => MC_3_enable_n,
      MC_3_ps1_n => MC_3_ps1_n,
      MC_4_enable_n => MC_4_enable_n,
      MC_4_ps1_n => MC_4_ps1_n,
      PG_A => PG_A,
      PG_B => PG_B,
      PIM400_I2C_scl_i => pim400_i2c_scl_i,
      PIM400_I2C_scl_o => pim400_i2c_scl_o,
      PIM400_I2C_scl_t => pim400_i2c_scl_t,
      PIM400_I2C_sda_i => pim400_i2c_sda_i,
      PIM400_I2C_sda_o => pim400_i2c_sda_o,
      PIM400_I2C_sda_t => pim400_i2c_sda_t,
      PL_LEDS(1 downto 0) => PL_LEDS(1 downto 0),
      PYLD12V_EN(0) => PYLD12V_EN(0),
      TPS2358_0_mp_ena_n => TPS2358_0_mp_ena_n,
      TPS2358_0_mp_flt_n => TPS2358_0_mp_flt_n,
      TPS2358_0_mp_pg_n => TPS2358_0_mp_pg_n,
      TPS2358_0_pwr_ena_n => TPS2358_0_pwr_ena_n,
      TPS2358_0_pwr_flt_n => TPS2358_0_pwr_flt_n,
      TPS2358_0_pwr_pg_n => TPS2358_0_pwr_pg_n,
      TPS2358_1_mp_ena_n => TPS2358_1_mp_ena_n,
      TPS2358_1_mp_flt_n => TPS2358_1_mp_flt_n,
      TPS2358_1_mp_pg_n => TPS2358_1_mp_pg_n,
      TPS2358_1_pwr_ena_n => TPS2358_1_pwr_ena_n,
      TPS2358_1_pwr_flt_n => TPS2358_1_pwr_flt_n,
      TPS2358_1_pwr_pg_n => TPS2358_1_pwr_pg_n,
      TPS2358_2_mp_ena_n => TPS2358_2_mp_ena_n,
      TPS2358_2_mp_flt_n => TPS2358_2_mp_flt_n,
      TPS2358_2_mp_pg_n => TPS2358_2_mp_pg_n,
      TPS2358_2_pwr_ena_n => TPS2358_2_pwr_ena_n,
      TPS2358_2_pwr_flt_n => TPS2358_2_pwr_flt_n,
      TPS2358_2_pwr_pg_n => TPS2358_2_pwr_pg_n,
      TPS2358_3_mp_ena_n => TPS2358_3_mp_ena_n,
      TPS2358_3_mp_flt_n => TPS2358_3_mp_flt_n,
      TPS2358_3_mp_pg_n => TPS2358_3_mp_pg_n,
      TPS2358_3_pwr_ena_n => TPS2358_3_pwr_ena_n,
      TPS2358_3_pwr_flt_n => TPS2358_3_pwr_flt_n,
      TPS2358_3_pwr_pg_n => TPS2358_3_pwr_pg_n,
      TPS2358_4_mp_ena_n => TPS2358_4_mp_ena_n,
      TPS2358_4_mp_flt_n => TPS2358_4_mp_flt_n,
      TPS2358_4_mp_pg_n => TPS2358_4_mp_pg_n,
      TPS2358_4_pwr_ena_n => TPS2358_4_pwr_ena_n,
      TPS2358_4_pwr_flt_n => TPS2358_4_pwr_flt_n,
      TPS2358_4_pwr_pg_n => TPS2358_4_pwr_pg_n
    );
pim400_i2c_scl_iobuf: component IOBUF
     port map (
      I => pim400_i2c_scl_o,
      IO => pim400_i2c_scl_io,
      O => pim400_i2c_scl_i,
      T => pim400_i2c_scl_t
    );
pim400_i2c_sda_iobuf: component IOBUF
     port map (
      I => pim400_i2c_sda_o,
      IO => pim400_i2c_sda_io,
      O => pim400_i2c_sda_i,
      T => pim400_i2c_sda_t
    );
end STRUCTURE;
