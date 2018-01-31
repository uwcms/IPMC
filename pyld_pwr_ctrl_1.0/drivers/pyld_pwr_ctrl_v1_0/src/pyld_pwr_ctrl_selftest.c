
/***************************** Include Files *******************************/
#include "pyld_pwr_ctrl.h"
#include "xparameters.h"
#include "xil_io.h"

/************************** Constant Definitions ***************************/

/************************** Function Definitions ***************************/
/**
 *
 * Run a self-test on the driver/device. Note this may be a destructive test if
 * resets of the device are performed.
 *
 * If the hardware system is not built correctly, this function may never
 * return to the caller.
 *
 * @param   baseaddr_p is the base address of the PYLD_PWR_CTRLinstance to be worked on.
 *
 * @return
 *
 *    - XST_SUCCESS   if all self-test code passed
 *    - XST_FAILURE   if any self-test code failed
 *
 * @note    Self test may fail if data memory and device are not on the same bus.
 *
 */
XStatus PYLD_PWR_CTRL_Reg_SelfTest(void * baseaddr_p)
{
	// TODO: not implemented (yet)

	return XST_SUCCESS;
}
