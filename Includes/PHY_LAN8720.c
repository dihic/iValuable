/* ----------------------------------------------------------------------
 * Copyright (C) 2013-2014 ARM Limited. All rights reserved.
 *  
 * $Date:        24. July 2014
 * $Revision:    V6.00
 *  
 * Driver:       Driver_ETH_PHYn (default: Driver_ETH_PHY0)
 * Project:      Ethernet Physical Layer Transceiver (PHY)
 *               Driver for LAN8720
 * ---------------------------------------------------------------------- 
 * Use the following configuration settings in the middleware component
 * to connect to this driver.
 * 
 *   Configuration Setting                     Value
 *   ---------------------                     -----
 *   Connect to hardware via Driver_ETH_PHY# = n (default: 0)
 * -------------------------------------------------------------------- */

/* History:
 *  Version 6.00
 *    Based on API V2.00
 *  Version 5.01
 *    Based on API V1.10 (namespace prefix ARM_ added)
 *  Version 5.00
 *    Initial release
 */ 

#include "PHY_LAN8720.h"
#include "Driver_ETH_PHY.h"
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX


#define ARM_ETH_PHY_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(6,00) /* driver version */


#ifndef ETH_PHY_NUM
#define ETH_PHY_NUM     0        /* Default driver number */
#endif

#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR    0x01     /* Default device address */
#endif


/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion = {
  ARM_ETH_PHY_API_VERSION,
  ARM_ETH_PHY_DRV_VERSION
};


/* Ethernet PHY local variables */
static ARM_ETH_PHY_Read_t  reg_rd;
static ARM_ETH_PHY_Write_t reg_wr;
static uint16_t            reg_bmcr;


/**
  \fn          ARM_DRIVER_VERSION GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION GetVersion (void) {
  return DriverVersion;
}


/**
  \fn          int32_t Initialize (ARM_ETH_PHY_Read_t  fn_read,
                                   ARM_ETH_PHY_Write_t fn_write)
  \brief       Initialize Ethernet PHY Device.
  \param[in]   fn_read   Pointer to \ref ARM_ETH_MAC_PHY_Read
  \param[in]   fn_write  Pointer to \ref ARM_ETH_MAC_PHY_Write
  \return      \ref execution_status
*/
static int32_t Initialize (ARM_ETH_PHY_Read_t fn_read, ARM_ETH_PHY_Write_t fn_write) {
  uint16_t val;

  /* Register PHY read/write functions. */
  if ((fn_read == NULL) || (fn_write == NULL)) return ARM_DRIVER_ERROR_PARAMETER;
  reg_rd = fn_read;
  reg_wr = fn_write;
	
  /* Check Device Identification. */
  reg_rd(ETH_PHY_ADDR, REG_PHYIDR1, &val);
  if (val != PHY_ID1) {
    /* Wrong PHY device. */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
  reg_rd(ETH_PHY_ADDR, REG_PHYIDR2, &val);
  if ((val & 0xFFF0) != PHY_ID2) {
    /* Wrong PHY device. */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
	
	//Soft Reset
	reg_wr(ETH_PHY_ADDR, REG_BMCR, BMCR_RESET);
	osDelay(10);

  reg_bmcr = BMCR_POWER_DOWN;
  return (reg_wr(ETH_PHY_ADDR, REG_BMCR, reg_bmcr));
}

/**
  \fn          int32_t Uninitialize (void)
  \brief       De-initialize Ethernet PHY Device.
  \return      \ref execution_status
*/
static int32_t Uninitialize (void) {
  return (reg_wr(ETH_PHY_ADDR, REG_BMCR, BMCR_POWER_DOWN));
}

/**
  \fn          int32_t PowerControl (ARM_POWER_STATE state)
  \brief       Control Ethernet PHY Device Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t PowerControl (ARM_POWER_STATE state) {

  switch (state) {
    case ARM_POWER_OFF:
      reg_bmcr |=  BMCR_POWER_DOWN;
      break;
    case ARM_POWER_FULL:
      reg_bmcr &= ~BMCR_POWER_DOWN;
      break;
    default:
      return ARM_DRIVER_ERROR_UNSUPPORTED;
  }

  return (reg_wr(ETH_PHY_ADDR, REG_BMCR, reg_bmcr));
}

/**
  \fn          int32_t SetInterface (uint32_t interface)
  \brief       Set Ethernet Media Interface.
  \param[in]   interface  Media Interface type
  \return      \ref execution_status
*/
static int32_t SetInterface (uint32_t interface) {

  switch (interface) {
    case ARM_ETH_INTERFACE_RMII:
      break;
    case ARM_ETH_INTERFACE_MII:
    default:
      return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
  return(0);
}

/**
  \fn          int32_t SetMode (uint32_t mode)
  \brief       Set Ethernet PHY Device Operation mode.
  \param[in]   mode  Operation Mode
  \return      \ref execution_status
*/
static int32_t SetMode (uint32_t mode) {
  uint16_t val;

  val = reg_bmcr & BMCR_POWER_DOWN;

  switch (mode & ARM_ETH_PHY_SPEED_Msk) {
    case ARM_ETH_PHY_SPEED_10M:
      break;
    case ARM_ETH_PHY_SPEED_100M:
      val |=  BMCR_SPEED_SEL;
      break;
    default:
      return ARM_DRIVER_ERROR_UNSUPPORTED;
  }

  switch (mode & ARM_ETH_PHY_DUPLEX_Msk) {
    case ARM_ETH_PHY_DUPLEX_HALF:
      break;
    case ARM_ETH_PHY_DUPLEX_FULL:
      val |=  BMCR_DUPLEX;
      break;
  }

  if (mode & ARM_ETH_PHY_AUTO_NEGOTIATE) {
    val |= BMCR_ANEG_EN;
  }

  if (mode & ARM_ETH_PHY_LOOPBACK) {
    val |= BMCR_LOOPBACK;
  }

  if (mode & ARM_ETH_PHY_ISOLATE) {
    val |= BMCR_ISOLATE;
  }

  reg_bmcr = val;

  return (reg_wr(ETH_PHY_ADDR, REG_BMCR, reg_bmcr));
}

/**
  \fn          ARM_ETH_LINK_STATE GetLinkState (void)
  \brief       Get Ethernet PHY Device Link state.
  \return      current link status \ref ARM_ETH_LINK_STATE
*/
static ARM_ETH_LINK_STATE GetLinkState (void) {
  ARM_ETH_LINK_STATE state;
  uint16_t           val;

  reg_rd(ETH_PHY_ADDR, REG_BMSR, &val);
  state = (val & BMSR_LINK_STAT) ? ARM_ETH_LINK_UP : ARM_ETH_LINK_DOWN;

  return (state);
}

/**
  \fn          ARM_ETH_LINK_INFO GetLinkInfo (void)
  \brief       Get Ethernet PHY Device Link information.
  \return      current link parameters \ref ARM_ETH_LINK_INFO
*/
static ARM_ETH_LINK_INFO GetLinkInfo (void) {
  ARM_ETH_LINK_INFO info;
  uint16_t          val;

  reg_rd(ETH_PHY_ADDR, REG_PSCSR, &val);
  info.speed  = (val & PSCSR_SPEED)  ? ARM_ETH_SPEED_10M   : ARM_ETH_SPEED_100M;
  info.duplex = (val & PSCSR_DUPLEX) ? ARM_ETH_DUPLEX_FULL : ARM_ETH_DUPLEX_HALF;

  return (info);
}


/* PHY Driver Control Block */
ARM_DRIVER_ETH_PHY ARM_Driver_ETH_PHY_(ETH_PHY_NUM) = {
  GetVersion,
  Initialize,
  Uninitialize,
  PowerControl,
  SetInterface,
  SetMode,
  GetLinkState,
  GetLinkInfo
};
