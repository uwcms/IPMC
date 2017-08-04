--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   11:42:03 04/10/2017
-- Design Name:   
-- Module Name:   /data/mpv/temp/ise_sims/ad7689_spi_intf_tb.vhd
-- Project Name:  ise_sims
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: ad7689_spi_intf
-- 
-- Dependencies:
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
--
-- Notes: 
-- This testbench has been automatically generated using types std_logic and
-- std_logic_vector for the ports of the unit under test.  Xilinx recommends
-- that these types always be used for the top-level I/O of a design in order
-- to guarantee that the testbench will bind correctly to the post-implementation 
-- simulation model.
--------------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;
 
ENTITY ad7689_spi_intf_tb IS
END ad7689_spi_intf_tb;
 
ARCHITECTURE behavior OF ad7689_spi_intf_tb IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT ad7689_spi_intf
    PORT(
         mclk_i : IN  std_logic;
         mreset_i : IN  std_logic;
         spi_clk_o : OUT  std_logic;
         spi_ncs0_o : OUT  std_logic;
         spi_mosi_o : OUT  std_logic;
         spi_miso_i : IN  std_logic;
         cnv_done_o : OUT  std_logic;
         cnv_channel_o : OUT  std_logic_vector(4 downto 0);
         cnv_value_o : OUT  std_logic_vector(15 downto 0);
         cnv_error_o : OUT  std_logic
        );
    END COMPONENT;
    

   --Inputs
   signal mclk_i : std_logic := '0';
   signal mreset_i : std_logic := '1';
   signal spi_miso_i : std_logic := '1';

 	--Outputs
   signal spi_clk_o : std_logic;
   signal spi_ncs0_o : std_logic;
   signal spi_mosi_o : std_logic;
   signal cnv_done_o : std_logic;
   signal cnv_channel_o : std_logic_vector(4 downto 0);
   signal cnv_value_o : std_logic_vector(15 downto 0);
   signal cnv_error_o : std_logic;
   -- No clocks detected in port list. Replace mclk_i below with 
   -- appropriate port name 
 
   constant mclk_i_period : time := 10 ns;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: ad7689_spi_intf PORT MAP (
          mclk_i => mclk_i,
          mreset_i => mreset_i,
          spi_clk_o => spi_clk_o,
          spi_ncs0_o => spi_ncs0_o,
          spi_mosi_o => spi_mosi_o,
          spi_miso_i => spi_miso_i,
          cnv_done_o => cnv_done_o,
          cnv_channel_o => cnv_channel_o,
          cnv_value_o => cnv_value_o,
          cnv_error_o => cnv_error_o
        );

   -- Clock process definitions
   mclk_i_process :process
   begin
		mclk_i <= '0';
		wait for mclk_i_period/2;
		mclk_i <= '1';
		wait for mclk_i_period/2;
   end process;
 

   -- Stimulus process
   stim_proc: process
   begin		
      -- hold reset state for 100 ns.
      mreset_i <= '1';
      wait for 100 ns;
      mreset_i <= '0';

      wait for mclk_i_period*40;
      
      spi_miso_i <= '0';
      wait for mclk_i_period*5;
      spi_miso_i <= '1';
      
      wait for mclk_i_period*40;
            
      spi_miso_i <= '0';
      wait for mclk_i_period*5;
      spi_miso_i <= '1';
      
      wait for mclk_i_period*40;
                  
      spi_miso_i <= '0';
      wait for mclk_i_period*5;
      spi_miso_i <= '1';

      -- insert stimulus here 

      wait;
   end process;

END;
