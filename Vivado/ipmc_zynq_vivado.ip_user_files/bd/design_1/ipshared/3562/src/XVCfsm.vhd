----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 11/11/2014 02:14:53 PM
-- Design Name: 
-- Module Name: XVCfsm - Behavioral
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
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity XVCfsm is
    Port ( --bram controller interface signal definitions
      din : in STD_LOGIC_VECTOR (31 downto 0);
      dout : out STD_LOGIC_VECTOR (31 downto 0);
      avalidin : in STD_LOGIC;                  --command register
      psclkin : in STD_LOGIC;
      psrstin : in STD_LOGIC;
      psenain : in STD_LOGIC;
      pswenin : in STD_LOGIC_VECTOR (3 downto 0);
      --config register with params
      bit_count : in STD_LOGIC_VECTOR (7 downto 0);
      ckdown_width : in STD_LOGIC_VECTOR (7 downto 0);
      ckup_width : in STD_LOGIC_VECTOR (7 downto 0);
      --interface signals to I/O and shift FFs
      rststb : out STD_LOGIC;
      tck : out STD_LOGIC;
      Doutsample_ev : out STD_LOGIC;
      Doutshift_ev : out STD_LOGIC;
      Dincapture_ev : out STD_LOGIC;
      Dinshift_ev : out STD_LOGIC;
      DEBUG_PORT : out STD_LOGIC_VECTOR (31 DOWNTO 0)
   );
end XVCfsm;

architecture Behavioral of XVCfsm is

  constant STARTCMDBITPOS : NATURAL := 31;
  constant RESETCMDBITPOS : NATURAL := 30;
  
  
  type outer_FSM_codes is (main_idle, inc_count, wait_current, wait_last);
  type bit_FSM_codes is (bit_idle, wait_low, high1, high2, wait_high);
  
  signal outer_FSM_current, outer_FSM_next : outer_FSM_codes;
  signal bit_FSM_current, bit_FSM_next : bit_FSM_codes;
  
  signal bit_up_counter, wait_down_counter : STD_LOGIC_VECTOR(7 downto 0);          -- bit and clock edge wait counters
  signal start_cmd, reset_cmd, bit_start_ev, bit_done_ev : STD_LOGIC;
  signal done_status_bit, idle_status_bit : STD_LOGIC;
  signal dout_samp_shift_ev, din_capt_ev, din_shift_ev : STD_LOGIC;
  signal tckff : STD_LOGIC;
begin

  --command strobe generation
  start_cmd <= '1' when ((psenain = '1') and (pswenin(3) = '1') and (avalidin = '1') and (din(STARTCMDBITPOS) = '1')) else '0';
  reset_cmd <= '1' when ((psenain = '1') and (pswenin(3) = '1') and (avalidin = '1') and (din(RESETCMDBITPOS) = '1')) else '0';

  --combinatorial assignments for top-level interface connections
  dout <= X"00000" & "00" & idle_status_bit & done_status_bit & bit_up_counter;
  Doutsample_ev <= dout_samp_shift_ev;
  Doutshift_ev <= dout_samp_shift_ev;
  Dincapture_ev <= din_capt_ev;
  Dinshift_ev <= din_shift_ev;
  rststb <= reset_cmd;
  tck <= tckff;
  
  --next state combinatorial process for outer FSM
  process (outer_FSM_current, start_cmd, reset_cmd, bit_up_counter, bit_count, bit_done_ev) begin
    if (reset_cmd = '1') then
      outer_FSM_next <= main_idle;
    else
      case outer_FSM_current is
        when main_idle =>
          if (start_cmd = '1') then
            outer_FSM_next <= inc_count;
          else
            outer_FSM_next <= main_idle;
          end if;
            
        when inc_count =>
          if (bit_up_counter < bit_count) then
            outer_FSM_next <= wait_current;         --wait for current bit to finish, there will be another
          else
            outer_FSM_next <= wait_last;            --this is last bit in sequence
          end if;
        
        when wait_current =>
          if (bit_done_ev = '1') then
            outer_FSM_next <= inc_count;
          else
            outer_FSM_next <= wait_current;
          end if;
          
        when wait_last =>
          if (bit_done_ev = '1') then
            outer_FSM_next <= main_idle;
          else
            outer_FSM_next <= wait_last;
          end if;
        
        when others =>
          outer_FSM_next <= main_idle;
          
      end case;
    end if;
  end process;

  --combinatorial signals derived from next state
  bit_start_ev <= '1' when (outer_FSM_current = inc_count) else '0';   --also used to increment bit count
  
  --sequential process for outer FSM
  process (psclkin) begin
    if (rising_edge(psclkin)) then
      if ((psrstin = '1') or (reset_cmd = '1')) then
        outer_FSM_current <= main_idle;
        bit_up_counter <= (others => '0');
        done_status_bit <= '0';
        idle_status_bit <= '0';
      else
        --update next state from FSM
        outer_FSM_current <= outer_FSM_next;

        case outer_FSM_next is
          when main_idle    => DEBUG_PORT(11 downto 8) <= "0000";
          when inc_count    => DEBUG_PORT(11 downto 8) <= "0001";
          when wait_current => DEBUG_PORT(11 downto 8) <= "0010";
          when wait_last    => DEBUG_PORT(11 downto 8) <= "0011";
          when others       => DEBUG_PORT(11 downto 8) <= "1111";
        end case;

        --update the bit counter
        if (outer_FSM_next = main_idle) then
          bit_up_counter <= (others => '0');
        elsif (outer_FSM_next = inc_count) then
          bit_up_counter <= bit_up_counter + X"01";
        end if;
      
        --update the status bits
        if (outer_FSM_current = main_idle) then
          idle_status_bit <= '1';
        else
          idle_status_bit <= '0';
        end if;
          
        if ((outer_FSM_current = wait_last) and (bit_done_ev = '1')) then
          done_status_bit <= '1';
        elsif (start_cmd = '1') then
          done_status_bit <= '0';
        end if;
          
      end if;
    end if;
  end process;

  --next state process for intra-bit FSM
  process (bit_FSM_current, start_cmd, reset_cmd, wait_down_counter, bit_start_ev) begin
    if (reset_cmd = '1') then
      bit_FSM_next <= bit_idle;
    else
      case bit_FSM_current is
        when bit_idle =>
          if (bit_start_ev = '1') then
            bit_FSM_next <= wait_low;
          else
            bit_FSM_next <= bit_idle;
          end if;
          
        when wait_low =>
          if (wait_down_counter = X"00") then
            bit_FSM_next <= high1;
          else
            bit_FSM_next <= wait_low;
          end if;
           
        when high1 =>
          bit_FSM_next <= high2;
          
        when high2 =>
          bit_FSM_next <= wait_high;
         
        when wait_high =>
          if (wait_down_counter = X"00") then
            bit_FSM_next <= bit_idle;
          else
            bit_FSM_next <= wait_high;
          end if;
        
        when others =>
          bit_FSM_next <= bit_idle;
          
      end case;
    end if;
  end process;
  
  --combinatorial logic for intra-bit FSM
  bit_done_ev <= '1' when ((bit_FSM_current = wait_high) and (bit_FSM_next = bit_idle)) else '0';
  dout_samp_shift_ev <= '1' when ((bit_FSM_current = bit_idle) and (bit_FSM_next = wait_low)) else '0';
  din_capt_ev <= '1' when (bit_FSM_current = high2) else '0';
  din_shift_ev <= bit_done_ev;              --shift the bits when entering the done state
 
  --sequential process for intra-bit FSM
  process (psclkin) begin
    if (rising_edge(psclkin)) then
      if ((psrstin = '1') or (reset_cmd = '1')) then
        wait_down_counter <= (others => '0');
        tckff <= '0';
        bit_FSM_current <= bit_idle;
      else
        --update FSM state variable
        bit_FSM_current <= bit_FSM_next;

        case bit_FSM_next is
          when bit_idle   => DEBUG_PORT(15 downto 12) <= "0000";
          when wait_low   => DEBUG_PORT(15 downto 12) <= "0001";
          when high1      => DEBUG_PORT(15 downto 12) <= "0010";
          when high2      => DEBUG_PORT(15 downto 12) <= "0011";
          when wait_high  => DEBUG_PORT(15 downto 12) <= "0100";
          when others     => DEBUG_PORT(15 downto 12) <= "1111";
        end case;

        --update down counter
        if ((bit_FSM_current = bit_idle) and (bit_FSM_next = wait_low)) then
          --loading for clock low phase
          wait_down_counter <= ckdown_width;
        elsif (bit_FSM_current = high2) then
          --loading for clock high phase
          wait_down_counter <= ckup_width;
        elsif ((bit_FSM_current = wait_low) or (bit_FSM_current = wait_high)) then
          --decrementing
          wait_down_counter <= wait_down_counter - X"01";
        end if;
        
        --update tck ff
        if ((bit_FSM_next = bit_idle) or (bit_FSM_next = wait_low)) then
          tckff <= '0';
        elsif (bit_FSM_next = high1) then
          tckff <= '1';
        end if;
        
      end if;
    end if;
  end process;
  
  -- combinatorial assignment for debug port

  DEBUG_PORT(0) <= '1' when ((bit_FSM_current = wait_high) and (bit_FSM_next = bit_idle)) else '0';
  DEBUG_PORT(1) <= '1' when ((bit_FSM_current = bit_idle) and (bit_FSM_next = wait_low)) else '0';
  DEBUG_PORT(2) <= '1' when (bit_FSM_current = high2) else '0';
  DEBUG_PORT(3) <= '1' when ((bit_FSM_current = wait_high) and (bit_FSM_next = bit_idle)) else '0';
  DEBUG_PORT(4) <= '1' when (outer_FSM_next = inc_count) else '0';
  -- DEBUG_PORT(0) <= bit_done_ev;
  -- DEBUG_PORT(1) <= dout_samp_shift_ev;
  -- DEBUG_PORT(2) <= din_capt_ev;
  -- DEBUG_PORT(3) <= din_shift_ev;
  -- DEBUG_PORT(4) <= bit_start_ev;

  DEBUG_PORT(7 downto 5) <= (others => '0');

  DEBUG_PORT(23 downto 16) <= bit_up_counter;

  DEBUG_PORT(31 downto 24) <= (others => '0');
end Behavioral;
