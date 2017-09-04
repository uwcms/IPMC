/*
 * atheros.h
 *
 *  Created on: Aug 31, 2017
 *      Author: mpvl
 */

#ifndef SRC_COMPONENTS_ATHEROS_ATHEROS_H_
#define SRC_COMPONENTS_ATHEROS_ATHEROS_H_

#include <stdlib.h>
#include <netif/xemacpsif.h>

/**
 * enable internal delays on the Atheros AR8035 PHY.
 * @param netif pointer to the network interface where the PHY resides.
 * @return Non-zero if successful.
 */
int ar8035_enable_internal_delays(struct netif *netif);

#endif /* SRC_COMPONENTS_ATHEROS_ATHEROS_H_ */
