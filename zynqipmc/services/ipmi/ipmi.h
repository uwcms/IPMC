/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_H_

#include <xil_types.h>
#include <map>
#include <string>

namespace IPMI {
	namespace NetFn {
		extern const std::map<u8, std::string> id_to_netfn; ///< Mapping for runtime lookups. (Contains only request NetFns.)
		extern const std::map<std::string, u8> netfn_to_id; ///< Mapping for runtime lookups. (Contains only request NetFns.)

		/// NetFn Code Constants
		///@{
		const u8 Chassis      = 0x00;
		const u8 Bridge       = 0x02;
		const u8 Sensor_Event = 0x04;
		const u8 App          = 0x06;
		const u8 Firmware     = 0x08;
		const u8 Storage      = 0x0A;
		const u8 Transport    = 0x0C;
		const u8 PICMG        = 0x2C;
		const u8 CMS          = 0x3C;
		///@}

		/**
		 * Convert NetFn code to equivalent Request NetFn.
		 * @param netfn A NetFn code.
		 * @return The equivalent Request NetFn code.
		 */
		static constexpr inline u8 requestNetFn(const u8 netfn) { return netfn & 0xfe; };

		/**
		 * Convert NetFn code to equivalent Response NetFn.
		 * @param netfn A NetFn cod.
		 * @return The equivalent Response NetFn code.
		 */
		static constexpr inline u8 responseNetFn(const u8 netfn) { return netfn | 0x01; };
	}

	namespace Completion {
		extern const std::map<u8, std::string> id_to_cmplcode; ///< Mapping for runtime lookups.
		extern const std::map<std::string, u8> cmplcode_to_id; ///< Mapping for runtime lookups.

		/// Completion Code Constants
		///@{
		const u8 Success                                                                 = 0x00;
		const u8 FRU_Device_Busy                                                         = 0x81;
		const u8 Node_Busy                                                               = 0xC0;
		const u8 Invalid_Command                                                         = 0xC1;
		const u8 Command_Invalid_For_Lun                                                 = 0xC2;
		const u8 Processing_Timeout                                                      = 0xC3;
		const u8 Out_Of_Space                                                            = 0xC4;
		const u8 Reservation_Cancelled                                                   = 0xC5;
		const u8 Request_Data_Truncated                                                  = 0xC6;
		const u8 Request_Data_Length_Invalid                                             = 0xC7;
		const u8 Request_Data_Field_Length_Limit_Exceeded                                = 0xC8;
		const u8 Parameter_Out_Of_Range                                                  = 0xC9;
		const u8 Cannot_Return_Requested_Number_Of_Data_Bytes                            = 0xCA;
		const u8 Requested_Sensor_Data_Or_Record_Not_Present                             = 0xCB;
		const u8 Invalid_Data_Field_In_Request                                           = 0xCC;
		const u8 Command_Illegal_For_Specific_Sensor_Or_Record_Type                      = 0xCD;
		const u8 Command_Response_Could_Not_Be_Provided                                  = 0xCE;
		const u8 Cannot_Execute_Duplicated_Request                                       = 0xCF;
		const u8 Command_Response_Could_Not_Be_Provided_SDR_Repository_Is_In_Update_Mode = 0xD0;
		const u8 Command_Response_Could_Not_Be_Provided_Device_In_Firmware_Update_Mode   = 0xD1;
		const u8 Command_Response_Could_Not_Be_Provided_BMC_Initialization_In_Progress   = 0xD2;
		const u8 Destination_Unavailable                                                 = 0xD3;
		const u8 Insufficient_Privilege                                                  = 0xD4;
		const u8 Command_Or_Parameters_Not_Supported_In_Current_State                    = 0xD5;
		const u8 Command_Subfunction_Disabled_Or_Unavailable                             = 0xD6;
		///@}
	}

	extern const std::map< u16, std::pair<std::string, std::string> > id_to_cmd; ///< Mapping for runtime lookups. (Contains only request commands.)
	extern const std::map<std::string, u16> cmd_to_id; ///< Mapping for runtime lookups.  (Contains only request commands.)

	/**
	 * Convert an IPMI command to an equivalent Request command.
	 * @param cmd An IPMI command code.
	 * @return The equivalent Request command code.
	 */
	static constexpr inline u16 requestCmd(const u16 cmd) { return cmd & 0xfeff; };

	/**
	 * Convert an IPMI command to an equivalent Response command.
	 * @param cmdAn IPMI command code.
	 * @return The equivalent Response command code.
	 */
	static constexpr inline u16 responseCmd(const u8 cmd) { return cmd | 0x0100; };

	namespace Chassis {
		/// Chassis Device Commands
		///@{
		const u16 Get_Chassis_Capabilities                     = 0x0000; // O
		const u16 Get_Chassis_Status                           = 0x0001; // O
		const u16 Chassis_Control                              = 0x0002; // O
		const u16 Chassis_Reset                                = 0x0003; // O
		const u16 Chassis_Identify                             = 0x0004; // O
		const u16 Set_Chassis_Capabilities                     = 0x0005; // O
		const u16 Set_Power_Restore_Policy                     = 0x0006; // O
		const u16 Get_System_Restart_Cause                     = 0x0007; // O*
		const u16 Set_System_Boot_Options                      = 0x0008; // O*
		const u16 Get_System_Boot_Options                      = 0x0009; // O*
		const u16 Get_POH_Counter                              = 0x000F; // O
		///@}
	}

	namespace Bridge {
		/// Bridge Management Commands (ICMB)
		///@{
		const u16 Get_Bridge_State                             = 0x0200; // O
		const u16 Set_Bridge_State                             = 0x0201; // O
		const u16 Get_ICMB_Address                             = 0x0202; // O
		const u16 Set_ICMB_Address                             = 0x0203; // O
		const u16 Set_Bridge_Proxy_Address                     = 0x0204; // O
		const u16 Get_Bridge_Statistics                        = 0x0205; // O
		const u16 Get_ICMB_Capabilities                        = 0x0206; // O
		const u16 Clear_Bridge_Statistics                      = 0x0208; // O
		const u16 Get_Bridge_Proxy_Address                     = 0x0209; // O
		const u16 Get_ICMB_Connector_Info                      = 0x020A; // O
		const u16 Get_ICMB_Connection_ID                       = 0x020B; // O
		const u16 Send_ICMB_Connection_ID                      = 0x020C; // O
		///@}

		/// Discovery Commands (ICMB)
		///@{
		const u16 Prepare_For_Discovery                        = 0x0210; // O
		const u16 Get_Addresses_Bridge                         = 0x0211; // O
		const u16 Set_Discovered                               = 0x0212; // O
		const u16 Get_Chassis_Device_ID                        = 0x0213; // O
		const u16 Set_Chassis_Device_ID                        = 0x0214; // O
		///@}

		/// Bridging Commands (ICMB)
		///@{
		const u16 Bridge_Request                               = 0x0220; // O
		const u16 Bridge_Message                               = 0x0221; // O
		///@}

		/// Event Commands (ICMB)
		///@{
		const u16 Get_Event_Count                              = 0x0230; // O
		const u16 Set_Event_Destination                        = 0x0231; // O
		const u16 Set_Event_Reception_State                    = 0x0232; // O
		const u16 Send_ICMB_Event_Message                      = 0x0233; // O
		const u16 Get_Event_Destination                        = 0x0234; // O
		const u16 Get_Event_Reception_State                    = 0x0235; // O
		///@}

		/// Other Bridge Commands
		///@{
		const u16 Error_Report                                 = 0x02FF; // O
		///@}
	}

	namespace Sensor_Event {
		/// Event Commands
		///@{
		const u16 Set_Event_Receiver                           = 0x0400; // M
		const u16 Get_Event_Receiver                           = 0x0401; // M
		const u16 Platform_Event                               = 0x0402; // M
		///@}

		/// PEF and Alerting Commands
		///@{
		const u16 Get_PEF_Capabilities                         = 0x0410; // M*
		const u16 Arm_PEF_Postpone_Timer                       = 0x0411; // M*
		const u16 Set_PEF_Configuration_Parameters             = 0x0412; // M*
		const u16 Get_PEF_Configuration_Parameters             = 0x0413; // M*
		const u16 Set_Last_Processed_Event_ID                  = 0x0414; // M*
		const u16 Get_Last_Processed_Event_ID                  = 0x0415; // M*
		const u16 Alert_Immediate                              = 0x0416; // O*
		const u16 PET_Acknowledge                              = 0x0417; // O*
		///@}

		/// Sensor Device Commands
		///@{
		const u16 Get_Device_SDR_Info                          = 0x0420; // M
		const u16 Get_Device_SDR                               = 0x0421; // M*
		const u16 Reserve_Device_SDR_Repository                = 0x0422; // M*
		const u16 Get_Sensor_Reading_Factors                   = 0x0423; // O*
		const u16 Set_Sensor_Hysteresis                        = 0x0424; // O
		const u16 Get_Sensor_Hysteresis                        = 0x0425; // O
		const u16 Set_Sensor_Threshold                         = 0x0426; // O
		const u16 Get_Sensor_Threshold                         = 0x0427; // O*
		const u16 Set_Sensor_Event_Enable                      = 0x0428; // O
		const u16 Get_Sensor_Event_Enable                      = 0x0429; // O*
		const u16 Rearm_Sensor_Events                          = 0x042A; // O*
		const u16 Get_Sensor_Event_Status                      = 0x042B; // O
		const u16 Get_Sensor_Reading                           = 0x042D; // M
		const u16 Set_Sensor_Type                              = 0x042E; // O
		const u16 Get_Sensor_Type                              = 0x042F; // O*
		///@}
	}

	namespace App {
		/// IPM Device “Global” Commands
		///@{
		const u16 Get_Device_ID                                = 0x0601; // M
		const u16 Cold_Reset                                   = 0x0602; // O*
		const u16 Warm_Reset                                   = 0x0603; // O
		const u16 Get_Self_Test_Results                        = 0x0604; // M
		const u16 Manufacturing_Test_On                        = 0x0605; // O
		const u16 Set_ACPI_Power_State                         = 0x0606; // O
		const u16 Get_ACPI_Power_State                         = 0x0607; // O*
		const u16 Get_Device_GUID                              = 0x0608; // O
		///@}

		/// BMC Watchdog Timer Commands
		///@{
		const u16 Reset_Watchdog_Timer                         = 0x0622; // M
		const u16 Set_Watchdog_Timer                           = 0x0624; // M
		const u16 Get_Watchdog_Timer                           = 0x0625; // M
		///@}

		/// BMC Device and Messaging Commands
		///@{
		const u16 Set_BMC_Global_Enables                       = 0x062E; // O/M
		const u16 Get_BMC_Global_Enables                       = 0x062F; // O/M
		const u16 Clear_Message_Flags                          = 0x0630; // O/M
		const u16 Get_Message_Flags                            = 0x0631; // O/M
		const u16 Enable_Message_Channel_Receive               = 0x0632; // O
		const u16 Get_Message                                  = 0x0633; // O/M
		const u16 Send_Message                                 = 0x0634; // O/M
		const u16 Read_Event_Message_Buffer                    = 0x0635; // O
		const u16 Get_BT_Interface_Capabilities                = 0x0636; // O/M
		const u16 Get_System_GUID                              = 0x0637; // O*
		const u16 Get_Channel_Authentication_Capabilities      = 0x0638; // O*
		const u16 Get_Session_Challenge                        = 0x0639; // O*
		const u16 Activate_Session                             = 0x063A; // O*
		const u16 Set_Session_Privilege_Level                  = 0x063B; // O*
		const u16 Close_Session                                = 0x063C; // O*
		const u16 Get_Session_Info                             = 0x063D; // O*
		const u16 Get_AuthCode                                 = 0x063F; // O
		const u16 Set_Channel_Access                           = 0x0640; // O*
		const u16 Get_Channel_Access                           = 0x0641; // O*
		const u16 Get_Channel_Info                             = 0x0642; // O*
		const u16 Set_User_Access                              = 0x0643; // O*
		const u16 Get_User_Access                              = 0x0644; // O*
		const u16 Set_User_Name                                = 0x0645; // O*
		const u16 Get_User_Name                                = 0x0646; // O*
		const u16 Set_User_Password                            = 0x0647; // O*
		///@}

		/// AdvancedTCA
		///@{
		const u16 Activate_Payload_for_the_IPMI_Trace_Payload  = 0x0648; // O*
		///@}

		/// BMC Device and Messaging Commands
		///@{
		const u16 Master_WriteRead                             = 0x0652; // O/M
		///@}
	}

	namespace Firmware {
	}

	namespace Storage {
		/// FRU Device Commands
		///@{
		const u16 Get_FRU_Inventory_Area_Info                  = 0x0A10; // M
		const u16 Read_FRU_Data                                = 0x0A11; // M
		const u16 Write_FRU_Data                               = 0x0A12; // M
		///@}

		/// SDR Device Commands
		///@{
		const u16 Get_SDR_Repository_Info                      = 0x0A20; // O/M
		const u16 Get_SDR_Repository_Allocation_Info           = 0x0A21; // O
		const u16 Reserve_SDR_Repository_Storage               = 0x0A22; // O/M
		const u16 Get_SDR                                      = 0x0A23; // O/M*
		const u16 Add_SDR                                      = 0x0A24; // O/M*
		const u16 Partial_Add_SDR                              = 0x0A25; // O/M*
		const u16 Delete_SDR                                   = 0x0A26; // O*
		const u16 Clear_SDR_Repository                         = 0x0A27; // O/M*
		const u16 Get_SDR_Repository_Time                      = 0x0A28; // O/M*
		const u16 Set_SDR_Repository_Time                      = 0x0A29; // O/M*
		const u16 Enter_SDR_Repository_Update_Mode             = 0x0A2A; // O*
		const u16 Exit_SDR_Repository_Update_Mode              = 0x0A2B; // O/M*
		const u16 Run_Initialization_Agent                     = 0x0A2C; // O*
		///@}

		/// SEL Device Commands
		///@{
		const u16 Get_SEL_Info                                 = 0x0A40; // M
		const u16 Get_SEL_Allocation_Info                      = 0x0A41; // O
		const u16 Reserve_SEL                                  = 0x0A42; // O*
		const u16 Get_SEL_Entry                                = 0x0A43; // M
		const u16 Add_SEL_Entry                                = 0x0A44; // M*
		const u16 Partial_Add_SEL_Entry                        = 0x0A45; // M*
		const u16 Delete_SEL_Entry                             = 0x0A46; // O
		const u16 Clear_SEL                                    = 0x0A47; // M
		const u16 Get_SEL_Time                                 = 0x0A48; // M
		const u16 Set_SEL_Time                                 = 0x0A49; // M
		const u16 Get_Auxiliary_Log_Status                     = 0x0A5A; // O
		const u16 Set_Auxiliary_Log_Status                     = 0x0A5B; // O*
		///@}
	}

	namespace Transport {
		/// LAN Device Commands
		///@{
		const u16 Set_LAN_Configuration_Parameters             = 0x0C01; // O/M*
		const u16 Get_LAN_Configuration_Parameters             = 0x0C02; // O/M*
		const u16 Suspend_BMC_ARPs                             = 0x0C03; // O/M*
		const u16 Get_IP_UDP_RMCP_Statistics                   = 0x0C04; // O
		///@}

		/// Serial/Modem Device Commands
		///@{
		const u16 Set_Serial_Modem_Configuration               = 0x0C10; // O/M*
		const u16 Get_Serial_Modem_Configuration               = 0x0C11; // O/M*
		const u16 Set_Serial_Modem_Mux                         = 0x0C12; // O*
		const u16 Get_TAP_Response_Codes                       = 0x0C13; // O*
		const u16 Set_PPP_UDP_Proxy_Transmit_Data              = 0x0C14; // O*
		const u16 Get_PPP_UDP_Proxy_Transmit_Data              = 0x0C15; // O*
		const u16 Send_PPP_UDP_Proxy_Packet_Transport          = 0x0C16; // O*
		const u16 Get_PPP_UDP_Proxy_Receive_Data               = 0x0C17; // O*
		const u16 Serial_Modem_Connection_Active               = 0x0C18; // O/M*
		const u16 Callback                                     = 0x0C19; // O
		const u16 Set_User_Callback_Options                    = 0x0C1A; // O*
		const u16 Get_User_Callback_Options                    = 0x0C1B; // O*
		///@}
	}

	namespace PICMG {
		/// AdvancedTCA
		///@{
		const u16 Get_PICMG_Properties                         = 0x2C00; // M
		const u16 Get_Address_Info                             = 0x2C01; // M*
		const u16 Get_Shelf_Address_Info                       = 0x2C02; // O
		const u16 Set_Shelf_Address_Info                       = 0x2C03; // O
		const u16 FRU_Control                                  = 0x2C04; // M
		const u16 Get_FRU_LED_Properties                       = 0x2C05; // M
		const u16 Get_LED_Color_Capabilities                   = 0x2C06; // M
		const u16 Set_FRU_LED_State                            = 0x2C07; // M
		const u16 Get_FRU_LED_State                            = 0x2C08; // M
		const u16 Set_IPMB_State                               = 0x2C09; // M
		const u16 Set_FRU_Activation_Policy                    = 0x2C0A; // M
		const u16 Get_FRU_Activation_Policy                    = 0x2C0B; // M
		const u16 Set_FRU_Activation                           = 0x2C0C; // M
		const u16 Get_Device_Locator_Record_ID                 = 0x2C0D; // M
		const u16 Set_Port_State                               = 0x2C0E; // O/M*
		const u16 Get_Port_State                               = 0x2C0F; // O/M*
		const u16 Compute_Power_Properties                     = 0x2C10; // M
		const u16 Set_Power_Level                              = 0x2C11; // M
		const u16 Get_Power_Level                              = 0x2C12; // M
		const u16 Renegotiate_Power                            = 0x2C13; // O
		const u16 Get_Fan_Speed_Properties                     = 0x2C14; // M*
		const u16 Set_Fan_Level                                = 0x2C15; // O/M*
		const u16 Get_Fan_Level                                = 0x2C16; // O/M*
		const u16 Bused_Resource                               = 0x2C17; // O/M*
		const u16 Get_IPMB_Link_Info                           = 0x2C18; // O/M*
		const u16 Get_Shelf_Manager_IPMB_Address               = 0x2C1B; // NA*
		const u16 Set_Fan_Policy                               = 0x2C1C; // NA*
		const u16 Get_Fan_Policy                               = 0x2C1D; // NA*
		const u16 FRU_Control_Capabilities                     = 0x2C1E; // M
		const u16 FRU_Inventory_Device_Lock_Control            = 0x2C1F; // O
		const u16 FRU_Inventory_Device_Write                   = 0x2C20; // O
		const u16 Get_Shelf_Manager_IP_Addresses               = 0x2C21; // O
		const u16 Get_Shelf_Power_Allocation                   = 0x2C22; // NA*
		const u16 Get_Telco_Alarm_Capability                   = 0x2C29; // O/M*
		const u16 Set_Telco_Alarm_State                        = 0x2C2A; // O/M*
		const u16 Get_Telco_Alarm_State                        = 0x2C2B; // O/M*
		const u16 Get_Target_Upgrade_Capabilities              = 0x2C2E; // M
		const u16 Get_Component_Properties                     = 0x2C2F; // M
		const u16 Abort_Firmware_Upgrade                       = 0x2C30; // O
		const u16 Initiate_Upgrade_Action                      = 0x2C31; // M
		const u16 Upload_Firmware_Block                        = 0x2C32; // M
		const u16 Finish_Firmware_Upload                       = 0x2C33; // M
		const u16 Get_Upgrade_Status                           = 0x2C34; // O/M*
		const u16 Activate_Firmware                            = 0x2C35; // M
		const u16 Query_Self_Test_Results                      = 0x2C36; // O/M*
		const u16 Query_Rollback_Status                        = 0x2C37; // O/M*
		const u16 Initiate_Manual_Rollback                     = 0x2C38; // O/M*
		const u16 Get_Telco_Alarm_Location                     = 0x2C39; // O
		const u16 Set_FRU_Extracted                            = 0x2C3A; // O
		const u16 Get_HPM_x_Capabilities                       = 0x2C3E; // M
		const u16 Get_Dynamic_Credentials                      = 0x2C3F; // O*
		const u16 Get_Session_Handle_for_Explicit_LAN_Bridging = 0x2C40; // O*
		const u16 Get_ATCA_Extended_Management_Resources       = 0x2C41; // O*
		const u16 Get_AMC_Extended_Management_Resources        = 0x2C42; // O*
		const u16 Set_ATCA_Extended_Management_State           = 0x2C43; // O*
		const u16 Get_ATCA_Extended_Management_State           = 0x2C44; // O*
		const u16 Set_AMC_Power_State                          = 0x2C45; // O*
		const u16 Get_AMC_Power_State                          = 0x2C46; // O*
		const u16 Assign_SOL_Payload_Instance                  = 0x2C47; // O*
		const u16 Get_IP_Address_Source                        = 0x2C48; // N/A
		///@}
	}

	namespace CMS {
	}

	using namespace Chassis;
	using namespace Bridge;
	using namespace Sensor_Event;
	using namespace App;
	using namespace Firmware;
	using namespace Storage;
	using namespace Transport;
	using namespace PICMG;
	using namespace CMS;
}

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_H_ */
