#include <Copyright.h>

/*******************************************************************************
* gtPTP.c
*
* DESCRIPTION:
*       API definitions for Precise Time Protocol logic
*
* DEPENDENCIES:
*
* FILE REVISION NUMBER:
*******************************************************************************/

#include <msApi.h>
#include <gtSem.h>
#include <gtHwCntl.h>
#include <gtDrvSwRegs.h>


#ifdef CONFIG_AVB_FPGA

#undef USE_SINGLE_READ

#define AVB_SMI_ADDR        0xC

#define QD_REG_PTP_INT_OFFSET        0
#define QD_REG_PTP_INTEN_OFFSET        1
#define QD_REG_PTP_FREQ_OFFSET        4
#define QD_REG_PTP_PHASE_OFFSET        6
#define QD_REG_PTP_CLK_CTRL_OFFSET    4
#define QD_REG_PTP_CYCLE_INTERVAL_OFFSET        5
#define QD_REG_PTP_CYCLE_ADJ_OFFSET                6
#define QD_REG_PTP_PLL_CTRL_OFFSET    7
#define QD_REG_PTP_CLK_SRC_OFFSET    0x9
#define QD_REG_PTP_P9_MODE_OFFSET    0xA
#define QD_REG_PTP_RESET_OFFSET        0xB

#define GT_PTP_MERGE_32BIT(_high16,_low16)    (((_high16)<<16)|(_low16))
#define GT_PTP_GET_HIGH16(_data)    ((_data) >> 16) & 0xFFFF
#define GT_PTP_GET_LOW16(_data)        (_data) & 0xFFFF

#if 0

#define AVB_FPGA_READ_REG       gprtGetPhyReg
#define AVB_FPGA_WRITE_REG      gprtSetPhyReg
unsigned int (*avbFpgaReadReg)(void* unused, unsigned int port, unsigned int reg, unsigned int* data);
unsigned int (*avbFpgaWriteReg)(void* unused, unsigned int port, unsigned int reg, unsigned int data);
#else

/* for RMGMT access  and can be controlled by <sw_apps -rmgmt 0/1> */
unsigned int (*avbFpgaReadReg)(void* unused, unsigned int port, unsigned int reg, GT_U32* data)=gprtGetPhyReg;
unsigned int (*avbFpgaWriteReg)(void* unused, unsigned int port, unsigned int reg, GT_U32 data)=gprtSetPhyReg;
#define AVB_FPGA_READ_REG       avbFpgaReadReg
#define AVB_FPGA_WRITE_REG      avbFpgaWriteReg

#endif /* 0 */

#endif

#if 0
#define GT_PTP_BUILD_TIME(_time1, _time2)    (((_time1) << 16) | (_time2))
#define GT_PTP_L16_TIME(_time1)    ((_time1) & 0xFFFF)
#define GT_PTP_H16_TIME(_time1)    (((_time1) >> 16) & 0xFFFF)
#endif


/****************************************************************************/
/* PTP operation function declaration.                                    */
/****************************************************************************/
extern GT_STATUS ptpOperationPerform
(
    IN   GT_QD_DEV             *dev,
    IN   GT_PTP_OPERATION    ptpOp,
    INOUT GT_PTP_OP_DATA     *opData
);


/*******************************************************************************
* gptpSetConfig
*
* DESCRIPTION:
*       This routine writes PTP configuration parameters.
*
* INPUTS:
*        ptpData  - PTP configuration parameters.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetConfig
(
    IN  GT_QD_DEV     *dev,
    IN  GT_PTP_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_LPORT            port;
    GT_PTP_PORT_CONFIG    ptpPortData;

    DBG_INFO(("gptpSetConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_WRITE_DATA;

    /* setting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    opData.ptpData = ptpData->ptpEType;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    /* setting MsgIDTSEn, offset 1 */
    opData.ptpAddr = 1;
    opData.ptpData = ptpData->msgIdTSEn;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    /* setting TSArrPtr, offset 2 */
    opData.ptpAddr = 2;
    opData.ptpData = ptpData->tsArrPtr;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TSArrPtr.\n"));
        return GT_FAIL;
    }

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
#endif
    {
        ptpPortData.transSpec = ptpData->transSpec;
        ptpPortData.disTSpec = 1;    /* default value */
        ptpPortData.disTSOverwrite = ptpData->disTSOverwrite;
        ptpPortData.ipJump = 2;        /* default value */
        ptpPortData.etJump = 12;    /* default value */

        /* per port configuration */
        for(port=0; port<dev->numOfPorts; port++)
        {
            ptpPortData.ptpArrIntEn = (ptpData->ptpArrIntEn & (1 << port))? GT_TRUE : GT_FALSE;
            ptpPortData.ptpDepIntEn = (ptpData->ptpDepIntEn & (1 << port))? GT_TRUE : GT_FALSE;
          if((retVal = gptpSetPortConfig(dev, port, &ptpPortData)) != GT_OK)
          {
                DBG_INFO(("Failed gptpSetPortConfig.\n"));
                return GT_FAIL;
          }
          if (IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE))
      {
            if(!((ptpData->ptpPortConfig[port].arrTSMode==GT_PTP_TS_MODE_IN_REG)||(ptpData->ptpPortConfig[port].arrTSMode==GT_PTP_TS_MODE_IN_RESERVED_2)||(ptpData->ptpPortConfig[port].arrTSMode==GT_PTP_TS_MODE_IN_FRAME_END)))
              ptpData->ptpPortConfig[port].arrTSMode=GT_PTP_TS_MODE_IN_REG;
            if((retVal = gptpSetPortTsMode(dev, port, ptpData->ptpPortConfig[port].arrTSMode)) != GT_OK)
            {
                DBG_INFO(("Failed gptpSetPortConfig.\n"));
                return GT_FAIL;
            }
      }
        }

        return GT_OK;
    }

    /* old PTP block */
    /* setting PTPArrIntEn, offset 3 */
    opData.ptpAddr = 3;
    opData.ptpData = GT_LPORTVEC_2_PORTVEC(ptpData->ptpArrIntEn);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPArrIntEn.\n"));
        return GT_FAIL;
    }

    /* setting PTPDepIntEn, offset 4 */
    opData.ptpAddr = 4;
    opData.ptpData = GT_LPORTVEC_2_PORTVEC(ptpData->ptpDepIntEn);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPDepIntEn.\n"));
        return GT_FAIL;
    }

    /* TransSpec, MsgIDStartBit, DisTSOverwrite bit, offset 5 */
    op = PTP_READ_DATA;
    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    op = PTP_WRITE_DATA;

#ifdef CONFIG_AVB_FPGA
    opData.ptpData = ((ptpData->transSpec&0xF) << 12) | ((ptpData->msgIdStartBit&0x7) << 9) |
                    (opData.ptpData & 0x1) | ((ptpData->disTSOverwrite?1:0) << 1);
#else
    opData.ptpData = ((ptpData->transSpec&0xF) << 12) | (opData.ptpData & 0x1) | ((ptpData->disTSOverwrite?1:0) << 1);
#endif
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing MsgIDStartBit & DisTSOverwrite.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpGetConfig
*
* DESCRIPTION:
*       This routine reads PTP configuration parameters.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpData  - PTP configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetConfig
(
    IN  GT_QD_DEV     *dev,
    OUT GT_PTP_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_LPORT            port;
    GT_PTP_PORT_CONFIG    ptpPortData;

    DBG_INFO(("gptpGetConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    ptpData->ptpEType = opData.ptpData;

    /* getting MsgIDTSEn, offset 1 */
    opData.ptpAddr = 1;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    ptpData->msgIdTSEn = opData.ptpData;

    /* getting TSArrPtr, offset 2 */
    opData.ptpAddr = 2;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TSArrPtr.\n"));
        return GT_FAIL;
    }

    ptpData->tsArrPtr = opData.ptpData;

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
#endif
    {
        ptpData->ptpArrIntEn = 0;
        ptpData->ptpDepIntEn = 0;

        /* per port configuration */
        for(port=0; port<dev->numOfPorts; port++)
        {
            if((retVal = gptpGetPortConfig(dev, port, &ptpPortData)) != GT_OK)
            {
                DBG_INFO(("Failed gptpGetPortConfig.\n"));
                return GT_FAIL;
            }

            ptpData->ptpArrIntEn |= (ptpPortData.ptpArrIntEn ? (1 << port) : 0);
            ptpData->ptpDepIntEn |= (ptpPortData.ptpDepIntEn ? (1 << port) : 0);
            ptpData->ptpPortConfig[port].ptpArrIntEn = ptpPortData.ptpArrIntEn;
            ptpData->ptpPortConfig[port].ptpDepIntEn = ptpPortData.ptpDepIntEn;


          if (IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE))
      {
            ptpData->ptpPortConfig[port].transSpec = ptpPortData.transSpec;
        ptpData->ptpPortConfig[port].disTSOverwrite = ptpPortData.disTSOverwrite;
            if((retVal = gptpGetPortTsMode(dev, port, &ptpData->ptpPortConfig[port].arrTSMode)) != GT_OK)
            {
                DBG_INFO(("Failed gptpGetPortConfig.\n"));
                return GT_FAIL;
            }
      }
        }

        ptpData->transSpec = ptpPortData.transSpec;
        ptpData->disTSOverwrite = ptpPortData.disTSOverwrite;

        ptpData->msgIdStartBit = 4;

        return GT_OK;
    }

    /* old PTP block */
    /* getting PTPArrIntEn, offset 3 */
    opData.ptpAddr = 3;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPArrIntEn.\n"));
        return GT_FAIL;
    }
    opData.ptpData &= dev->validPortVec;
    ptpData->ptpArrIntEn = GT_PORTVEC_2_LPORTVEC(opData.ptpData);


    /* getting PTPDepIntEn, offset 4 */
    opData.ptpAddr = 4;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPDepIntEn.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= dev->validPortVec;
    ptpData->ptpDepIntEn = GT_PORTVEC_2_LPORTVEC(opData.ptpData);

    /* MsgIDStartBit, DisTSOverwrite bit, offset 5 */
    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    ptpData->transSpec = (opData.ptpData >> 12) & 0xF;
#ifdef CONFIG_AVB_FPGA
    ptpData->msgIdStartBit = (opData.ptpData >> 9) & 0x7;
#else
    ptpData->msgIdStartBit = 0;
#endif
    ptpData->disTSOverwrite = ((opData.ptpData >> 1) & 0x1) ? GT_TRUE : GT_FALSE;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetGlobalConfig
*
* DESCRIPTION:
*       This routine writes PTP global configuration parameters.
*
* INPUTS:
*        ptpData  - PTP global configuration parameters.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetGlobalConfig
(
    IN  GT_QD_DEV     *dev,
    IN  GT_PTP_GLOBAL_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpSetGlobalConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_WRITE_DATA;

    /* setting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    opData.ptpData = ptpData->ptpEType;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    /* setting MsgIDTSEn, offset 1 */
    opData.ptpAddr = 1;
    opData.ptpData = ptpData->msgIdTSEn;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    /* setting TSArrPtr, offset 2 */
    opData.ptpAddr = 2;
    opData.ptpData = ptpData->tsArrPtr;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TSArrPtr.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGlobalGetConfig
*
* DESCRIPTION:
*       This routine reads PTP global configuration parameters.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpData  - PTP global configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetGlobalConfig
(
    IN  GT_QD_DEV     *dev,
    OUT GT_PTP_GLOBAL_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpGetGlobalConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    ptpData->ptpEType = opData.ptpData;

    /* getting MsgIDTSEn, offset 1 */
    opData.ptpAddr = 1;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    ptpData->msgIdTSEn = opData.ptpData;

    /* getting TSArrPtr, offset 2 */
    opData.ptpAddr = 2;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TSArrPtr.\n"));
        return GT_FAIL;
    }

    ptpData->tsArrPtr = opData.ptpData;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpSetPortConfig
*
* DESCRIPTION:
*       This routine writes PTP port configuration parameters.
*
* INPUTS:
*        ptpData  - PTP port configuration parameters.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetPortConfig
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    IN  GT_PTP_PORT_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32          hwPort;         /* the physical port number     */

    DBG_INFO(("gptpSetPortConfig Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    if (ptpData->transSpec > 0xF)    /* 4 bits */
        return GT_BAD_PARAM;

    if (ptpData->etJump > 0x1F)    /* 5 bits */
        return GT_BAD_PARAM;

    if (ptpData->ipJump > 0x3F)    /* 6 bits */
        return GT_BAD_PARAM;


    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = hwPort;

    /* TransSpec, DisTSpecCheck, DisTSOverwrite bit, offset 0 */
    op = PTP_READ_DATA;
    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    op = PTP_WRITE_DATA;
    opData.ptpData = (ptpData->transSpec << 12) | (opData.ptpData & 0x1) |
                    ((ptpData->disTSpec?1:0) << 11) |
                    ((ptpData->disTSOverwrite?1:0) << 1);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TransSpec,DisTSpecCheck,DisTSOverwrite.\n"));
        return GT_FAIL;
    }

    /* setting etJump and ipJump, offset 1 */
    opData.ptpAddr = 1;
    opData.ptpData = (ptpData->ipJump << 8) | ptpData->etJump;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    /* setting Int, offset 2 */
    opData.ptpAddr = 2;
    opData.ptpData = (ptpData->ptpArrIntEn?1:0) |
                    ((ptpData->ptpDepIntEn?1:0) << 1);
    if (IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE))
    {
       opData.ptpData |= ((ptpData->arrTSMode&0xff) << 8);  /* from Agate to set ArrTSMode */
    }

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TSArrPtr.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetPortConfig
*
* DESCRIPTION:
*       This routine reads PTP configuration parameters for a port.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpData  - PTP port configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPortConfig
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    OUT GT_PTP_PORT_CONFIG    *ptpData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32          hwPort;         /* the physical port number     */

    DBG_INFO(("gptpGetPortConfig Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = hwPort;
    op = PTP_READ_DATA;

    /* TransSpec, DisTSpecCheck, DisTSOverwrite bit, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    ptpData->transSpec = opData.ptpData >> 12;
    ptpData->disTSpec = ((opData.ptpData >> 11) & 0x1) ? GT_TRUE : GT_FALSE;
    ptpData->disTSOverwrite = ((opData.ptpData >> 1) & 0x1) ? GT_TRUE : GT_FALSE;

    /* getting MsgIDTSEn, offset 1 */
    opData.ptpAddr = 1;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading MsgIDTSEn.\n"));
        return GT_FAIL;
    }

    ptpData->ipJump = (opData.ptpData >> 8) & 0x3F;
    ptpData->etJump = opData.ptpData & 0x1F;

    /* getting TSArrPtr, offset 2 */
    opData.ptpAddr = 2;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TSArrPtr.\n"));
        return GT_FAIL;
    }

    ptpData->ptpDepIntEn = ((opData.ptpData >> 1) & 0x1) ? GT_TRUE : GT_FALSE;
    ptpData->ptpArrIntEn = (opData.ptpData & 0x1) ? GT_TRUE : GT_FALSE;
    if (IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE))
    {
      ptpData->arrTSMode = (opData.ptpData &0xff00) >> 8;  /* from Agate to get ArrTSMode */
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpSetPTPEn
*
* DESCRIPTION:
*       This routine enables or disables PTP.
*
* INPUTS:
*        en - GT_TRUE to enable PTP, GT_FALSE to disable PTP
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetPTPEn
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL        en
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_LPORT            port;

    DBG_INFO(("gptpSetPTPEn Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
#endif
    {
        /* per port configuration */
        for(port=0; port<dev->numOfPorts; port++)
        {
            if((retVal = gptpSetPortPTPEn(dev, port, en)) != GT_OK)
            {
                DBG_INFO(("Failed gptpSetPortPTPEn.\n"));
                return GT_FAIL;
            }
        }

        return GT_OK;
    }

    /* old PTP block */
    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    op = PTP_WRITE_DATA;
    opData.ptpData &= ~0x1;
    opData.ptpData |= (en ? 0 : 1);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing MsgIDStartBit & DisTSOverwrite.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetPTPEn
*
* DESCRIPTION:
*       This routine checks if PTP is enabled.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        en - GT_TRUE if enabled, GT_FALSE otherwise
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPTPEn
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *en
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpGetPTPEn Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
#endif
    {
        if((retVal = gptpGetPortPTPEn(dev, 0, en)) != GT_OK)
        {
               DBG_INFO(("Failed gptpGetPortPTPEn.\n"));
            return GT_FAIL;
        }

        return GT_OK;
    }

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *en = (opData.ptpData & 0x1) ? GT_FALSE : GT_TRUE;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetPortPTPEn
*
* DESCRIPTION:
*       This routine enables or disables PTP on a port.
*
* INPUTS:
*        en - GT_TRUE to enable PTP, GT_FALSE to disable PTP
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetPortPTPEn
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    IN  GT_BOOL        en
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32            hwPort;

    DBG_INFO(("gptpSetPortPTPEn Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpAddr = 0;

    opData.ptpPort = hwPort;

    op = PTP_READ_DATA;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    op = PTP_WRITE_DATA;

    if (en)
        opData.ptpData &= ~0x1;
    else
        opData.ptpData |= 0x1;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TransSpec,DisTSpecCheck,DisTSOverwrite.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpGetPortPTPEn
*
* DESCRIPTION:
*       This routine checks if PTP is enabled on a port.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        en - GT_TRUE if enabled, GT_FALSE otherwise
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPortPTPEn
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    OUT GT_BOOL        *en
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32            hwPort;

    DBG_INFO(("gptpGetPortPTPEn Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpAddr = 0;
    opData.ptpPort = hwPort;

    op = PTP_READ_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *en = (opData.ptpData & 0x1) ? GT_FALSE : GT_TRUE;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetPortTsMode
*
* DESCRIPTION:
*       This routine set PTP arrive 0 TS mode on a port.
*
* INPUTS:
*        tsMod - GT_PTP_TS_MODE_IN_REG
*                GT_PTP_TS_MODE_IN_RESERVED_2
*                GT_PTP_TS_MODE_IN_FRAME_END
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetPortTsMode
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    IN  GT_PTP_TS_MODE  tsMode
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32            hwPort;

    DBG_INFO(("gptpSetPortTsMode Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    if (!(IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE)))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpAddr = 2;

    opData.ptpPort = hwPort;

    op = PTP_READ_DATA;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TsMode.\n"));
        return GT_FAIL;
    }

    op = PTP_WRITE_DATA;

    opData.ptpData &= 0xff;
    switch (tsMode)
    {
      case GT_PTP_TS_MODE_IN_REG:
        break;
      case GT_PTP_TS_MODE_IN_RESERVED_2:
        opData.ptpData |= (PTP_TS_LOC_RESERVED_2<<8); /* set TS in reserved 1 */
        break;
      case GT_PTP_TS_MODE_IN_FRAME_END:
        opData.ptpData |= (PTP_FRAME_SIZE<<8); /* set TS in end of PTP frame */
        break;
      default:
        DBG_INFO(("GT_NOT_SUPPORTED the TS mode\n"));
        return GT_NOT_SUPPORTED;
    }

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing Ts Mode.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpGetPortTsMode
*
* DESCRIPTION:
*       This routine get PTP arrive 0 TS mode on a port.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        tsMod - GT_PTP_TS_MODE_IN_REG
*                GT_PTP_TS_MODE_IN_RESERVED_2
*                GT_PTP_TS_MODE_IN_FRAME_END
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPortTsMode
(
    IN  GT_QD_DEV     *dev,
    IN    GT_LPORT    port,
    OUT GT_PTP_TS_MODE  *tsMode
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32            hwPort;
    GT_U16            tmpData;

    DBG_INFO(("gptpGetPortTsMode Called.\n"));

    /* translate LPORT to hardware port */
    hwPort = (GT_U32)GT_LPORT_2_PORT(port);

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP_2))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    if (!(IS_IN_DEV_GROUP(dev, DEV_ARRV_TS_MODE)))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpAddr = 2;

    opData.ptpPort = hwPort;

    op = PTP_READ_DATA;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TsMode.\n"));
        return GT_FAIL;
    }

    tmpData = qdLong2Short(opData.ptpData >>8);
    if (tmpData>=PTP_FRAME_SIZE)
      *tsMode = GT_PTP_TS_MODE_IN_FRAME_END;
    else if (tmpData == PTP_TS_LOC_RESERVED_2)
      *tsMode = GT_PTP_TS_MODE_IN_RESERVED_2;
    else
      *tsMode = GT_PTP_TS_MODE_IN_REG;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpGetPTPInt
*
* DESCRIPTION:
*       This routine gets PTP interrupt status for each port.
*        The PTP Interrupt bit gets set for a given port when an incoming PTP
*        frame is time stamped and PTPArrIntEn for that port is set to 0x1.
*        Similary PTP Interrupt bit gets set for a given port when an outgoing
*        PTP frame is time stamped and PTPDepIntEn for that port is set to 0x1.
*        This bit gets cleared upon software reading and clearing the corresponding
*        time counter valid bits that are valid for that port.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpInt     - interrupt status for each port (bit 0 for port 0, bit 1 for port 1, etc.)
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPTPInt
(
    IN  GT_QD_DEV     *dev,
    OUT GT_U32        *ptpInt
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpGetPTPInt Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpPort = 0xF;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 8;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= (1 << dev->maxPorts) - 1;

    *ptpInt = GT_PORTVEC_2_LPORTVEC(opData.ptpData);

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetPTPGlobalTime
*
* DESCRIPTION:
*       This routine gets the global timer value that is running off of the free
*        running switch core clock.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpTime    - PTP global time
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetPTPGlobalTime
(
    IN  GT_QD_DEV     *dev,
    OUT GT_U32        *ptpTime
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpGetPTPGlobalTime Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

#ifndef USE_SINGLE_READ
    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpPort = IS_IN_DEV_GROUP(dev,DEV_TAI)?0xE:0xF;    /* Global register */
    op = PTP_READ_MULTIPLE_DATA;
    opData.ptpAddr = IS_IN_DEV_GROUP(dev,DEV_TAI)?0xE:9;
    opData.nData = 2;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *ptpTime = GT_PTP_BUILD_TIME(opData.ptpMultiData[1],opData.ptpMultiData[0]);
#else
    {
    GT_U32 data[2];

    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpPort = IS_IN_DEV_GROUP(dev,DEV_TAI)?0xE:0xF;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = IS_IN_DEV_GROUP(dev,DEV_TAI)?0xE:9;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    data[0] = opData.ptpData;

    op = PTP_READ_DATA;
    opData.ptpAddr++;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    data[1] = opData.ptpData;

    *ptpTime = GT_PTP_BUILD_TIME(data[1],data[0]);

    }
#endif

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetTimeStamped
*
* DESCRIPTION:
*        This routine retrieves the PTP port status that includes time stamp value
*        and sequce Id that are captured by PTP logic for a PTP frame that needs
*        to be time stamped.
*
* INPUTS:
*       port         - logical port number.
*       timeToRead    - Arr0, Arr1, or Dep time (GT_PTP_TIME enum type)
*
* OUTPUTS:
*        ptpStatus    - PTP port status
*
* RETURNS:
*       GT_OK         - on success
*       GT_FAIL     - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetTimeStamped
(
    IN  GT_QD_DEV     *dev,
    IN  GT_LPORT    port,
    IN    GT_PTP_TIME    timeToRead,
    OUT GT_PTP_TS_STATUS    *ptpStatus
)
{
    GT_STATUS           retVal;
    GT_U32                hwPort;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32                baseReg;

    DBG_INFO(("gptpGetTimeStamped Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    hwPort = (GT_U32)GT_LPORT_2_PORT(port);
    if (hwPort == GT_INVALID_PORT)
    {
        DBG_INFO(("Invalid port number\n"));
        return GT_BAD_PARAM;
    }

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
        baseReg = 8;
    else
        baseReg = 0;
#else
    baseReg = 8;
#endif

    switch (timeToRead)
    {
        case PTP_ARR0_TIME:
            opData.ptpAddr = baseReg + 0;
            break;
        case PTP_ARR1_TIME:
            opData.ptpAddr = baseReg + 4;
            break;
        case PTP_DEP_TIME:
            opData.ptpAddr = baseReg + 8;
            break;
        default:
            DBG_INFO(("Invalid time to be read\n"));
            return GT_BAD_PARAM;
    }

    opData.ptpPort = hwPort;
    opData.ptpBlock = 0;

#ifndef USE_SINGLE_READ
    op = PTP_READ_TIMESTAMP_DATA;
    opData.nData = 4;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    ptpStatus->isValid = (opData.ptpMultiData[0] & 0x1) ? GT_TRUE : GT_FALSE;
    ptpStatus->status = (GT_PTP_INT_STATUS)((opData.ptpMultiData[0] >> 1) & 0x3);
    ptpStatus->timeStamped = GT_PTP_BUILD_TIME(opData.ptpMultiData[2],opData.ptpMultiData[1]);
    ptpStatus->ptpSeqId = opData.ptpMultiData[3];
#else
    {
    GT_U32 data[4], i;

    op = PTP_READ_DATA;

    for (i=0; i<4; i++)
    {
        if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
        {
            DBG_INFO(("Failed reading DisPTP.\n"));
            return GT_FAIL;
        }

        data[i] = opData.ptpData;
        opData.ptpAddr++;
    }

    ptpStatus->isValid = (data[0] & 0x1) ? GT_TRUE : GT_FALSE;
    ptpStatus->status = (GT_PTP_INT_STATUS)((data[0] >> 1) & 0x3);
    ptpStatus->timeStamped = GT_PTP_BUILD_TIME(data[2],data[1]);
    ptpStatus->ptpSeqId = data[3];

    }
#endif

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpResetTimeStamp
*
* DESCRIPTION:
*        This routine resets PTP Time valid bit so that PTP logic can time stamp
*        a next PTP frame that needs to be time stamped.
*
* INPUTS:
*       port         - logical port number.
*       timeToReset    - Arr0, Arr1, or Dep time (GT_PTP_TIME enum type)
*
* OUTPUTS:
*        None.
*
* RETURNS:
*       GT_OK         - on success
*       GT_FAIL     - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpResetTimeStamp
(
    IN  GT_QD_DEV     *dev,
    IN  GT_LPORT    port,
    IN    GT_PTP_TIME    timeToReset
)
{
    GT_STATUS           retVal;
    GT_U32                hwPort;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32                baseReg;

    DBG_INFO(("gptpResetTimeStamp Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    hwPort = (GT_U32)GT_LPORT_2_PORT(port);
    if (hwPort == GT_INVALID_PORT)
    {
        DBG_INFO(("Invalid port number\n"));
        return GT_BAD_PARAM;
    }

#ifndef CONFIG_AVB_FPGA
    if (IS_IN_DEV_GROUP(dev,DEV_PTP_2))
        baseReg = 8;
    else
        baseReg = 0;
#else
    baseReg = 8;
#endif

    switch (timeToReset)
    {
        case PTP_ARR0_TIME:
            opData.ptpAddr = baseReg + 0;
            break;
        case PTP_ARR1_TIME:
            opData.ptpAddr = baseReg + 4;
            break;
        case PTP_DEP_TIME:
            opData.ptpAddr = baseReg + 8;
            break;
        default:
            DBG_INFO(("Invalid time to reset\n"));
            return GT_BAD_PARAM;
    }

    opData.ptpPort = hwPort;
    opData.ptpData = 0;
    opData.ptpBlock = 0;
    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing Port Status.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetReg
*
* DESCRIPTION:
*       This routine reads PTP register.
*
* INPUTS:
*       port         - logical port number.
*       regOffset    - register to read
*
* OUTPUTS:
*        data        - register data
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetReg
(
    IN  GT_QD_DEV     *dev,
    IN  GT_LPORT    port,
    IN  GT_U32        regOffset,
    OUT GT_U32        *data
)
{
    GT_STATUS           retVal;
    GT_U32                hwPort;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpGetReg Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    hwPort = (GT_U32)port;

    if (regOffset > 0x1F)
    {
        DBG_INFO(("Invalid reg offset\n"));
        return GT_BAD_PARAM;
    }

    op = PTP_READ_DATA;
    opData.ptpPort = hwPort;
    opData.ptpAddr = regOffset;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *data = opData.ptpData;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetReg
*
* DESCRIPTION:
*       This routine writes data to PTP register.
*
* INPUTS:
*       port         - logical port number
*       regOffset    - register to be written
*        data        - data to be written
*
* OUTPUTS:
*        None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetReg
(
    IN  GT_QD_DEV     *dev,
    IN  GT_LPORT    port,
    IN  GT_U32        regOffset,
    IN  GT_U32        data
)
{
    GT_STATUS           retVal;
    GT_U32                hwPort;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gptpSetReg Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_PTP))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    hwPort = (GT_U32)port;

    if ((regOffset > 0x1F) || (data > 0xFFFF))
    {
        DBG_INFO(("Invalid reg offset/data\n"));
        return GT_BAD_PARAM;
    }

    op = PTP_WRITE_DATA;
    opData.ptpPort = hwPort;
    opData.ptpAddr = regOffset;
    opData.ptpData = data;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }


    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/* TAI functions */
/*******************************************************************************
* gtaiSetEventConfig
*
* DESCRIPTION:
*       This routine sets TAI Event Capture configuration parameters.
*
* INPUTS:
*        eventData  - TAI Event Capture configuration parameters.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiSetEventConfig
(
    IN  GT_QD_DEV     *dev,
    IN  GT_TAI_EVENT_CONFIG    *eventData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetEventConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= ~((3<<14)|(1<<8));
    if (eventData->intEn)
        opData.ptpData |= (1 << 8);
    if (eventData->eventOverwrite)
        opData.ptpData |= (1 << 15);
    if (eventData->eventCtrStart)
        opData.ptpData |= (1 << 14);
    if (IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
      opData.ptpData &= ~(1<<13);
      if (eventData->eventPhase)
        opData.ptpData |= (1 << 13);
    }
    if (IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN)) /* after 6320 */
    {
	}

    op = PTP_WRITE_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

	if(IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN))
	{
     /* getting Capture trigger, offset 9 */
      op = PTP_READ_DATA;
      opData.ptpAddr = 9;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
	  }
      opData.ptpData &= ~(1<<14);
      opData.ptpData |= (eventData->captTrigEvent==GT_TRUE)?0x4000:0x0;

      op = PTP_WRITE_DATA;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
	  }
	}

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gtaiGetEventConfig
*
* DESCRIPTION:
*       This routine gets TAI Event Capture configuration parameters.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        eventData  - TAI Event Capture configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetEventConfig
(
    IN  GT_QD_DEV     *dev,
    OUT GT_TAI_EVENT_CONFIG    *eventData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetEventConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    eventData->intEn = (opData.ptpData & (1<<8))?GT_TRUE:GT_FALSE;
    eventData->eventOverwrite = (opData.ptpData & (1<<15))?GT_TRUE:GT_FALSE;
    eventData->eventCtrStart = (opData.ptpData & (1<<14))?GT_TRUE:GT_FALSE;
    if (IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
      eventData->eventPhase = (opData.ptpData & (1<<13))?GT_TRUE:GT_FALSE;
    }

	if(IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN))
	{
     /* getting Capture trigger, offset 9 */
      op = PTP_READ_DATA;
      opData.ptpAddr = 9;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
	  }
      eventData->captTrigEvent = (opData.ptpData & (1<<14))?GT_TRUE:GT_FALSE;
	}
    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gtaiGetEventStatus
*
* DESCRIPTION:
*       This routine gets TAI Event Capture status.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        eventData  - TAI Event Capture configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetEventStatus
(
    IN  GT_QD_DEV     *dev,
    OUT GT_TAI_EVENT_STATUS    *status
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32                 data[2];

    DBG_INFO(("gtaiGetEventStatus Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 9;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    status->isValid = (opData.ptpData & (1<<8))?GT_TRUE:GT_FALSE;
    status->eventCtr = opData.ptpData & 0xFF;
    status->eventErr = (opData.ptpData & (1<<9))?GT_TRUE:GT_FALSE;

    if (status->isValid == GT_FALSE)
    {
        return GT_OK;
    }

    opData.ptpAddr = 10;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }
    data[0] = opData.ptpData;

    opData.ptpAddr = 11;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }
    data[1] = opData.ptpData;

    status->eventTime = GT_PTP_BUILD_TIME(data[1],data[0]);

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gtaiGetEventInt
*
* DESCRIPTION:
*       This routine gets TAI Event Capture Interrupt status.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        intStatus     - interrupt status for TAI Event capture
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetEventInt
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *intStatus
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetEventInt Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpPort = 0xE;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 9;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *intStatus = (opData.ptpData & 0x8000)?GT_TRUE:GT_FALSE;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gtaiClearEventInt
*
* DESCRIPTION:
*       This routine clear TAI Event Capture Interrupt status.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiClearEventInt
(
    IN  GT_QD_DEV     *dev
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiClearEventInt Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpPort = 0xE;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 9;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading eventInt.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= 0x7EFF;
    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing eventInt.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gtaiSetClockSelect
*
* DESCRIPTION:
*       This routine sets several clock select in TAI.
*
* INPUTS:
*        clkSelect  - TAI clock select configuration parameters.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiSetClockSelect
(
    IN  GT_QD_DEV     *dev,
    IN  GT_TAI_CLOCK_SELECT    *clkSelect
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetClockSelect Called.\n"));

    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting Tai clock select, offset 8 */
    opData.ptpAddr = 8;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= ~(0x4077);
    opData.ptpData |= (((clkSelect->priRecClkSel)&7) << 0);
    opData.ptpData |= (((clkSelect->syncRecClkSel)&7) << 4);
    opData.ptpData |= (((clkSelect->ptpExtClk)&1) << 14);

    op = PTP_WRITE_DATA;

    opData.ptpAddr = 8;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gtaiGetClockSelect
*
* DESCRIPTION:
*       This routine gets several clock select in TAI.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       clkSelect  - TAI clock select configuration parameters.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetClockSelect
(
    IN  GT_QD_DEV     *dev,
    OUT  GT_TAI_CLOCK_SELECT    *clkSelect
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA      opData;

    DBG_INFO(("gtaiGetClockSelect Called.\n"));

    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting Tai clock select, offset 8 */
    opData.ptpAddr = 8;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    clkSelect->priRecClkSel = qdLong2Char(opData.ptpData&7);
    clkSelect->syncRecClkSel = qdLong2Char((opData.ptpData >> 4) & 7);
    clkSelect->ptpExtClk = (opData.ptpData>> 14) & 1;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gtaiGetTrigInt
*
* DESCRIPTION:
*       This routine gets TAI Trigger Interrupt status.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        intStatus     - interrupt status for TAI Trigger
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetTrigInt
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *intStatus
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetTrigInt Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpPort = 0xE;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 8;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *intStatus = (opData.ptpData & 0x8000)?GT_TRUE:GT_FALSE;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiClearTrigInt
*
* DESCRIPTION:
*       This routine clear TAI Trigger Interrupt status.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiClearTrigInt
(
    IN  GT_QD_DEV     *dev
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiClearTrigInt Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpPort = 0xE;    /* Global register */
    op = PTP_READ_DATA;
    opData.ptpAddr = 8;
    opData.ptpBlock = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= 0x7fff;
    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiSetTrigConfig
*
* DESCRIPTION:
*       This routine sets TAI Trigger configuration parameters.
*
* INPUTS:
*        trigEn    - enable/disable TAI Trigger.
*        trigData  - TAI Trigger configuration parameters (valid only if trigEn is GT_TRUE).
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiSetTrigConfig
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL     trigEn,
    IN  GT_TAI_TRIGGER_CONFIG    *trigData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetTrigConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    if (IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
      if (trigData->trigPhase)
        opData.ptpData |= (1 << 12);
    }

    opData.ptpData &= ~(3|(1<<9));

    if (trigEn == GT_FALSE)
    {
        op = PTP_WRITE_DATA;

        if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
        {
            DBG_INFO(("Failed writing PTPEType.\n"));
            return GT_FAIL;
        }

        return GT_OK;
    }

    opData.ptpData |= 1;

    if (trigData->intEn)
        opData.ptpData |= (1 << 9);

    if (trigData->mode == GT_TAI_TRIG_ON_GIVEN_TIME)
        opData.ptpData |= (1 << 1);


    op = PTP_WRITE_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpAddr = 2;
    opData.ptpData = GT_PTP_L16_TIME(trigData->trigGenAmt);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpAddr = 3;
    opData.ptpData = GT_PTP_H16_TIME(trigData->trigGenAmt);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    if (trigData->mode == GT_TAI_TRIG_ON_GIVEN_TIME)
    {
        if ((trigData->pulseWidth >= 0) && (trigData->pulseWidth <= 0xF))
        {
            op = PTP_READ_DATA;
            opData.ptpAddr = 5;        /* PulseWidth */

            if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
            {
                DBG_INFO(("Failed writing PTPEType.\n"));
                return GT_FAIL;
            }

            op = PTP_WRITE_DATA;
            opData.ptpAddr = 5;        /* PulseWidth */
            opData.ptpData &= (~0xF000);
            opData.ptpData |= (GT_U16)(trigData->pulseWidth << 12);

            if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
            {
                DBG_INFO(("Failed writing PTPEType.\n"));
                return GT_FAIL;
            }
        }
    }
    else
    {
        op = PTP_WRITE_DATA;
        opData.ptpAddr = 4;        /* TrigClkComp */
        opData.ptpData = (GT_U16)trigData->trigClkComp;

        if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
        {
            DBG_INFO(("Failed writing PTPEType.\n"));
            return GT_FAIL;
        }
    }

    if (IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN)) /* after 6320 */
    {
      op = PTP_WRITE_DATA;
      opData.ptpAddr = 0x10;
      opData.ptpData = GT_PTP_L16_TIME(trigData->trigGenTime);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x11;
      opData.ptpData = GT_PTP_H16_TIME(trigData->trigGenTime);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x13;
      opData.ptpData = GT_PTP_L16_TIME(trigData->trigGenDelay);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x12;
      opData.ptpData = 0xF & GT_PTP_L16_TIME(trigData->lockCorrect);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x14;
      opData.ptpData = GT_PTP_L16_TIME(trigData->trigGen2Time);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x15;
      opData.ptpData = GT_PTP_H16_TIME(trigData->trigGen2Time);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x17;
      opData.ptpData = GT_PTP_L16_TIME(trigData->trigGen2Delay);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

      opData.ptpAddr = 0x16;
      opData.ptpData = 0xF & GT_PTP_L16_TIME(trigData->lockCorrect2);

      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }

    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gtaiGetTrigConfig
*
* DESCRIPTION:
*       This routine gets TAI Trigger configuration parameters.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        trigEn    - enable/disable TAI Trigger.
*        trigData  - TAI Trigger configuration parameters (valid only if trigEn is GT_TRUE).
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetTrigConfig
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL     *trigEn,
    OUT GT_TAI_TRIGGER_CONFIG    *trigData
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32                 data[2];

    DBG_INFO(("gtaiGetTrigConfig Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    if (!(opData.ptpData & 1))
    {
        *trigEn = GT_FALSE;
        return GT_OK;
    }

    if (trigData == NULL)
    {
        return GT_BAD_PARAM;
    }

    *trigEn = GT_TRUE;
    trigData->mode = (opData.ptpData >> 1) & 1;
    trigData->intEn = (opData.ptpData >> 9) & 1;
    if (IS_IN_DEV_GROUP(dev,DEV_TAI_EXT_CLK))
    {
      trigData->trigPhase = (opData.ptpData >>12) & 1;
    }


    opData.ptpAddr = 2;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }
    data[0] = opData.ptpData;

    opData.ptpAddr = 3;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }
    data[1] = opData.ptpData;

    trigData->trigGenAmt = GT_PTP_BUILD_TIME(data[1],data[0]);

    opData.ptpAddr = 5;        /* PulseWidth */
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    trigData->pulseWidth = (GT_U32)((opData.ptpData >> 12) & 0xF);

    /* getting TrigClkComp, offset 4 */
    opData.ptpAddr = 4;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    trigData->trigClkComp = (GT_U32)opData.ptpData;

    if (IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN)) /* after 6320 */
    {
      op = PTP_READ_DATA;
      opData.ptpAddr = 0x10;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      data[0] = opData.ptpData;

      opData.ptpAddr = 0x11;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      data[1] = opData.ptpData;
      trigData->trigGenTime = GT_PTP_BUILD_TIME(data[1],data[0]);


	  opData.ptpAddr = 0x13;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      trigData->trigGenDelay = opData.ptpData;

      opData.ptpAddr = 0x12;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      trigData->lockCorrect = 0xF & opData.ptpData;

      opData.ptpAddr = 0x14;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      data[0] = opData.ptpData;

      opData.ptpAddr = 0x15;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      data[1] = opData.ptpData;
      trigData->trigGen2Time = GT_PTP_BUILD_TIME(data[1],data[0]);


	  opData.ptpAddr = 0x17;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      trigData->trigGen2Delay = opData.ptpData;

      opData.ptpAddr = 0x16;
      if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
	  {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
	  }
      trigData->lockCorrect2 = 0xF & opData.ptpData;


    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gtaiSetTrigLock
*
* DESCRIPTION:
*       This routine sets TAI Trigger lock.
*
* INPUTS:
*        trigLock       - trigger lock set.
*        trigLockRange  - trigger lock range.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiSetTrigLock
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL    trigLock,
    IN  GT_U8      trigLockRange
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetTrigLock Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }

    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpPort = 0xE;    /* TAI register */

    op = PTP_READ_DATA;
    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= ~(0xf<<4);

    opData.ptpData |= (trigLock==GT_TRUE) ?0x80:0;
    opData.ptpData |= ((trigLockRange&0x7)<<4);

    op = PTP_WRITE_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing PTPEType.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gtaiGetTrigLock
*
* DESCRIPTION:
*       This routine gets TAI Trigger lock and trigger lock range.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        trigLock       - trigger lock set.
*        trigLockRange  - trigger lock range.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gtaiGetTrigLock
(
    IN  GT_QD_DEV     *dev,
    OUT  GT_BOOL    *trigLock,
    OUT  GT_U8      *trigLockRange
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetTrigLock Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_TRIG_GEN))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }

    opData.ptpBlock = 0x0;    /* PTP register space */
    opData.ptpPort = 0xE;    /* TAI register */

    op = PTP_READ_DATA;
    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading PTPEType.\n"));
        return GT_FAIL;
    }

    trigLock = (opData.ptpData&0x80)?GT_TRUE:GT_FALSE;
    trigLockRange = (opData.ptpData&0x70)>>4;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiGetTSClkPer
*
* DESCRIPTION:
*         Time Stamping Clock Period in pico seconds.
*        This routine specifies the clock period for the time stamping clock supplied
*        to the PTP hardware logic.
*        This is the clock that is used by the hardware logic to update the PTP
*        Global Time Counter.
*
* INPUTS:
*         None.
*
* OUTPUTS:
*        clk        - time stamping clock period
*
* RETURNS:
*         GT_OK      - on success
*         GT_FAIL    - on error
*         GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         None
*
*******************************************************************************/
GT_STATUS gtaiGetTSClkPer
(
    IN  GT_QD_DEV     *dev,
    OUT GT_U32        *clk
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetTSClkPer Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 1;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *clk = (GT_U32)opData.ptpData;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiSetTSClkPer
*
* DESCRIPTION:
*         Time Stamping Clock Period in pico seconds.
*        This routine specifies the clock period for the time stamping clock supplied
*        to the PTP hardware logic.
*        This is the clock that is used by the hardware logic to update the PTP
*        Global Time Counter.
*
* INPUTS:
*        clk        - time stamping clock period
*
* OUTPUTS:
*         None.
*
* RETURNS:
*         GT_OK      - on success
*         GT_FAIL    - on error
*         GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         None
*
*******************************************************************************/
GT_STATUS gtaiSetTSClkPer
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32        clk
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetTSClkPer Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_WRITE_DATA;

    opData.ptpAddr = 1;

    opData.ptpData = (GT_U16)clk;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiSetMultiPTPSync
*
* DESCRIPTION:
*         This routine sets Multiple PTP device sync mode and sync time (TrigGenAmt).
*        When enabled, the hardware logic detects a low to high transition on the
*        EventRequest(GPIO) and transfers the sync time into the PTP Global Time
*        register. The EventCapTime is also updated at that instant.
*
* INPUTS:
*        multiEn        - enable/disable Multiple PTP device sync mode
*        syncTime    - sync time (valid only if multiEn is GT_TRUE)
*
* OUTPUTS:
*        None.
*
* RETURNS:
*        GT_OK      - on success
*        GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*        GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         When enabled, gtaiSetTrigConfig, gtaiSetEventConfig, gtaiSetTimeInc,
*        and gtaiSetTimeDec APIs are not operational.
*
*******************************************************************************/
GT_STATUS gtaiSetMultiPTPSync
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL     multiEn,
    IN  GT_32        syncTime
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetMultiPTPSync Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_MULTI_PTP_SYNC))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= ~(1 << 2);

    if (multiEn == GT_FALSE)
    {
        op = PTP_WRITE_DATA;

        if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
        {
            DBG_INFO(("Failed writing TAI register.\n"));
            return GT_FAIL;
        }

        return GT_OK;
    }

    opData.ptpData |= (1 << 2);

    op = PTP_WRITE_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TAI register.\n"));
        return GT_FAIL;
    }

    opData.ptpAddr = 2;
    opData.ptpData = GT_PTP_L16_TIME(syncTime);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TAI register.\n"));
        return GT_FAIL;
    }

    opData.ptpAddr = 3;
    opData.ptpData = GT_PTP_H16_TIME(syncTime);

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TAI register.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gtaiGetMultiPTPSync
*
* DESCRIPTION:
*         This routine sets Multiple PTP device sync mode and sync time (TrigGenAmt).
*        When enabled, the hardware logic detects a low to high transition on the
*        EventRequest(GPIO) and transfers the sync time into the PTP Global Time
*        register. The EventCapTime is also updated at that instant.
*
* INPUTS:
*        None.
*
* OUTPUTS:
*        multiEn        - enable/disable Multiple PTP device sync mode
*        syncTime    - sync time (valid only if multiEn is GT_TRUE)
*
* RETURNS:
*        GT_OK      - on success
*        GT_FAIL    - on error
*        GT_BAD_PARAM - if invalid parameter is given
*        GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         When enabled, gtaiSetTrigConfig, gtaiSetEventConfig, gtaiSetTimeInc,
*        and gtaiSetTimeDec APIs are not operational.
*
*******************************************************************************/
GT_STATUS gtaiGetMultiPTPSync
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL     *multiEn,
    OUT GT_32        *syncTime
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;
    GT_U32                 data[2];

    DBG_INFO(("gtaiGetMultiPTPSync Called.\n"));

    /* check if device supports this feature */
#ifndef CONFIG_AVB_FPGA
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI_MULTI_PTP_SYNC))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    /* getting PTPEType, offset 0 */
    opData.ptpAddr = 0;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }

    if(!(opData.ptpData & (1 << 2)))
    {
        *multiEn = GT_FALSE;
        *syncTime = 0;
        return GT_OK;
    }

    opData.ptpAddr = 2;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }
    data[0] = opData.ptpData;

    opData.ptpAddr = 3;
    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }
    data[1] = opData.ptpData;

    *syncTime = GT_PTP_BUILD_TIME(data[1],data[0]);

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gtaiGetTimeIncDec
*
* DESCRIPTION:
*         This routine retrieves Time increment/decrement setup.
*        This amount specifies the number of units of PTP Global Time that need to be
*        incremented or decremented. This is used for adjusting the PTP Global Time
*        counter value by a certain amount.
*
* INPUTS:
*         None.
*
* OUTPUTS:
*        expired    - GT_TRUE if inc/dec occurred, GT_FALSE otherwise
*        inc        - GT_TRUE if increment, GT_FALSE if decrement
*        amount    - increment/decrement amount
*
* RETURNS:
*         GT_OK      - on success
*         GT_FAIL    - on error
*         GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         Time increment or decrement will be excuted once.
*
*******************************************************************************/
GT_STATUS gtaiGetTimeIncDec
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *expired,
    OUT GT_BOOL        *inc,
    OUT GT_U32        *amount
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiGetTimeIncDec Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *inc = (opData.ptpData & 0x800)?GT_FALSE:GT_TRUE;
    *amount = (GT_U32)(opData.ptpData & 0x7FF);

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    *expired = (opData.ptpData & 0x8)?GT_FALSE:GT_TRUE;

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiSetTimeInc
*
* DESCRIPTION:
*         This routine enables time increment by the specifed time increment amount.
*        The amount specifies the number of units of PTP Global Time that need to be
*        incremented. This is used for adjusting the PTP Global Time counter value by
*        a certain amount.
*        Increment occurs just once.
*
* INPUTS:
*        amount    - time increment amount (0 ~ 0x7FF)
*
* OUTPUTS:
*         None.
*
* RETURNS:
*         GT_OK      - on success
*         GT_FAIL    - on error
*         GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         None
*
*******************************************************************************/
GT_STATUS gtaiSetTimeInc
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32        amount
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetTimeInc Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    /* set TimeIncAmt */
    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= 0xF000;
    opData.ptpData |= amount;

    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    /* set TimeIncEn */
    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    opData.ptpData |= 0x8;

    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading DisPTP.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/*******************************************************************************
* gtaiSetTimeDec
*
* DESCRIPTION:
*         This routine enables time decrement by the specifed time decrement amount.
*        The amount specifies the number of units of PTP Global Time that need to be
*        decremented. This is used for adjusting the PTP Global Time counter value by
*        a certain amount.
*        Decrement occurs just once.
*
* INPUTS:
*        amount    - time decrement amount (0 ~ 0x7FF)
*
* OUTPUTS:
*         None.
*
* RETURNS:
*         GT_OK      - on success
*         GT_FAIL    - on error
*         GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*         None
*
*******************************************************************************/
GT_STATUS gtaiSetTimeDec
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32        amount
)
{
    GT_STATUS           retVal;
    GT_PTP_OPERATION    op;
    GT_PTP_OP_DATA        opData;

    DBG_INFO(("gtaiSetTimeInc Called.\n"));

#ifndef CONFIG_AVB_FPGA
    /* check if device supports this feature */
    if (!IS_IN_DEV_GROUP(dev,DEV_TAI))
    {
        DBG_INFO(("GT_NOT_SUPPORTED\n"));
        return GT_NOT_SUPPORTED;
    }
#endif

    /* set TimeIncAmt */
    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 5;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }

    opData.ptpData &= 0xF000;
    opData.ptpData |= amount;
    opData.ptpData |= 0x800;    /* decrement */

    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TAI register.\n"));
        return GT_FAIL;
    }

    /* set TimeIncEn */
    opData.ptpBlock = 0x0;    /* PTP register space */

    opData.ptpPort = 0xE;    /* TAI register */
    op = PTP_READ_DATA;

    opData.ptpAddr = 0;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed reading TAI register.\n"));
        return GT_FAIL;
    }

    opData.ptpData |= 0x8;

    op = PTP_WRITE_DATA;

    if((retVal = ptpOperationPerform(dev, op, &opData)) != GT_OK)
    {
        DBG_INFO(("Failed writing TAI register.\n"));
        return GT_FAIL;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}


/****************************************************************************/
/* Internal functions.                                                  */
/****************************************************************************/

/*******************************************************************************
* ptpOperationPerform
*
* DESCRIPTION:
*       This function accesses PTP Command Register and Data Register.
*
* INPUTS:
*       ptpOp      - The stats operation bits to be written into the stats
*                    operation register.
*
* OUTPUTS:
*       ptpData    - points to the data storage that the operation requires.
*
* RETURNS:
*       GT_OK on success,
*       GT_FAIL otherwise.
*
* COMMENTS:
*
*******************************************************************************/
GT_STATUS ptpOperationPerform
(
    IN    GT_QD_DEV             *dev,
    IN    GT_PTP_OPERATION        ptpOp,
    INOUT GT_PTP_OP_DATA        *opData
)
{
    GT_STATUS       retVal;    /* Functions return value */
    GT_U32             i;

#ifdef CONFIG_AVB_FPGA
    GT_U32             tmpData;
#endif


    gtSemTake(dev,dev->ptpRegsSem,OS_WAIT_FOREVER);

    /* Wait until the ptp in ready. */
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
    {
      HW_DEV_REG_ACCESS regAccess;

      regAccess.entries = 1;

      regAccess.rw_reg_list[0].cmd = HW_REG_WAIT_TILL_0;
      regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
      regAccess.rw_reg_list[0].reg = QD_REG_PTP_COMMAND;
      regAccess.rw_reg_list[0].data = 15;
      retVal = hwAccessMultiRegs(dev, &regAccess);
      if(retVal != GT_OK)
      {
        gtSemGive(dev,dev->tblRegsSem);
        return retVal;
      }
    }
#else
    {
    GT_U16 data = 1;
    while(data == 1)
    {
        retVal = hwGetGlobal2RegField(dev,QD_REG_PTP_COMMAND,15,1,&data);
        if(retVal != GT_OK)
        {
            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;
        }
    }
    }
#endif
#else /* CONFIG_AVB_FPGA */
    {
    GT_U16 data = 1;
    while(data == 1)
    {
        retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,&tmpData);
        data = (GT_U16)tmpData;
        data = (data >> 15) & 0x1;
        if(retVal != GT_OK)
        {
            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;
        }
    }
    }
#endif

    /* Set the PTP Operation register */
    switch (ptpOp)
    {
        case PTP_WRITE_DATA:
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
            {
              HW_DEV_REG_ACCESS regAccess;

              regAccess.entries = 2;

              regAccess.rw_reg_list[0].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[0].reg = QD_REG_PTP_DATA;
              regAccess.rw_reg_list[0].data = (GT_U16)opData->ptpData;
              regAccess.rw_reg_list[1].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[1].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[1].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[1].data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                                (opData->ptpPort << 8)  |
                                (opData->ptpBlock << 5) |
                                (opData->ptpAddr & 0x1F));
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
            }
#else
    {
    GT_U16 data;
            data = (GT_U16)opData->ptpData;
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_DATA,data);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_COMMAND,data);
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }
    }
#endif
#else /* CONFIG_AVB_FPGA */
    {
    GT_U16 data;
            data = (GT_U16)opData->ptpData;
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }
    }
#endif
            break;

        case PTP_READ_DATA:
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
            {
              HW_DEV_REG_ACCESS regAccess;

              regAccess.entries = 3;

              regAccess.rw_reg_list[0].cmd = HW_REG_WRITE;
#ifndef CONFIG_AVB_FPGA
              regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
#else
              regAccess.rw_reg_list[0].addr = AVB_SMI_ADDR;
#endif
              regAccess.rw_reg_list[0].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[0].data = (GT_U16)((1 << 15) | (PTP_READ_DATA << 12) |
                                (opData->ptpPort << 8)  |
                                (opData->ptpBlock << 5) |
                                (opData->ptpAddr & 0x1F));
              regAccess.rw_reg_list[1].cmd = HW_REG_WAIT_TILL_0;
#ifndef CONFIG_AVB_FPGA
              regAccess.rw_reg_list[1].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
#else
              regAccess.rw_reg_list[1].addr = AVB_SMI_ADDR;
#endif
              regAccess.rw_reg_list[1].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[1].data = 15;
              regAccess.rw_reg_list[2].cmd = HW_REG_READ;
#ifndef CONFIG_AVB_FPGA
              regAccess.rw_reg_list[2].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
#else
              regAccess.rw_reg_list[2].addr = AVB_SMI_ADDR;
#endif
              regAccess.rw_reg_list[2].reg = QD_REG_PTP_DATA;
              regAccess.rw_reg_list[2].data = 0;
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
              opData->ptpData = (GT_U32)    regAccess.rw_reg_list[2].data;
            }
#else
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_COMMAND,data);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = hwGetGlobal2RegField(dev,QD_REG_PTP_COMMAND,15,1,&data);
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = hwReadGlobal2Reg(dev,QD_REG_PTP_DATA,&data);
            opData->ptpData = (GT_U32)data;
    }
#endif
#else /*CONFIG_AVB_FPGA */
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,&tmpData);
                data = (GT_U32)tmpData;
                data = (data >> 15) & 0x1;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,&tmpData);
            data = (GT_U32)tmpData;
            opData->ptpData = (GT_U32)data;
    }
#endif
            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;

        case PTP_READ_MULTIPLE_DATA:
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
            {
              HW_DEV_REG_ACCESS regAccess;

              regAccess.rw_reg_list[0].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[0].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[0].data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                                (opData->ptpPort << 8)  |
                                (opData->ptpBlock << 5) |
                                (opData->ptpAddr & 0x1F));
              regAccess.rw_reg_list[1].cmd = HW_REG_WAIT_TILL_0;
              regAccess.rw_reg_list[1].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[1].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[1].data = 15;
              for(i=0; i<opData->nData; i++)
              {
                regAccess.rw_reg_list[2+i].cmd = HW_REG_READ;
                regAccess.rw_reg_list[2+i].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
                regAccess.rw_reg_list[2+i].reg = QD_REG_PTP_DATA;
                regAccess.rw_reg_list[2+i].data = 0;
              }
              regAccess.entries = 2+i;
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
              for(i=0; i<opData->nData; i++)
              {
                opData->ptpMultiData[i] = (GT_U32)    regAccess.rw_reg_list[2+i].data;
              }
            }
#else
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_COMMAND,data);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = hwGetGlobal2RegField(dev,QD_REG_PTP_COMMAND,15,1,&data);
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            for(i=0; i<opData->nData; i++)
            {
                retVal = hwReadGlobal2Reg(dev,QD_REG_PTP_DATA,&data);
                opData->ptpMultiData[i] = (GT_U32)data;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }
    }
#endif
#else /* CONFIG_AVB_FPGA */
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,&tmpData);
                data = (GT_U32)tmpData;
                data = (data >> 15) & 0x1;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            for(i=0; i<opData->nData; i++)
            {
                retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,&tmpData);
                data = (GT_U32)tmpData;
                opData->ptpMultiData[i] = (GT_U32)data;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }
    }
#endif

            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;

        case PTP_READ_TIMESTAMP_DATA:
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
            {
              HW_DEV_REG_ACCESS regAccess;

              regAccess.entries = 3;

              regAccess.rw_reg_list[0].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[0].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[0].data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                                (opData->ptpPort << 8)  |
                                (opData->ptpBlock << 5) |
                                (opData->ptpAddr & 0x1F));
              regAccess.rw_reg_list[1].cmd = HW_REG_WAIT_TILL_0;
              regAccess.rw_reg_list[1].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[1].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[1].data = 15;
              regAccess.rw_reg_list[2].cmd = HW_REG_READ;
              regAccess.rw_reg_list[2].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[2].reg = QD_REG_PTP_DATA;
              regAccess.rw_reg_list[2].data = 0;
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
              opData->ptpMultiData[0] = (GT_U32)    regAccess.rw_reg_list[2].data;

              if (!(opData->ptpMultiData[0] & 0x1))
              {
                /* valid bit is not set */
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
              }

              for(i=0; i<(opData->nData-1); i++)
              {
                regAccess.rw_reg_list[i].cmd = HW_REG_READ;
                regAccess.rw_reg_list[i].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
                regAccess.rw_reg_list[i].reg = QD_REG_PTP_DATA;
                regAccess.rw_reg_list[i].data = 0;
              }
              regAccess.entries = i;
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
              for(i=0; i<(opData->nData-1); i++)
              {
                opData->ptpMultiData[i+1] = (GT_U32)    regAccess.rw_reg_list[i].data;
              }


              regAccess.entries = 2;

              regAccess.rw_reg_list[0].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[0].reg = QD_REG_PTP_DATA;
              regAccess.rw_reg_list[0].data = (GT_U16)0;
              regAccess.rw_reg_list[1].cmd = HW_REG_WRITE;
              regAccess.rw_reg_list[1].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
              regAccess.rw_reg_list[1].reg = QD_REG_PTP_COMMAND;
              regAccess.rw_reg_list[1].data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                                (opData->ptpPort << 8)  |
                                (opData->ptpBlock << 5) |
                                (opData->ptpAddr & 0x1F));
              retVal = hwAccessMultiRegs(dev, &regAccess);
              if(retVal != GT_OK)
              {
                gtSemGive(dev,dev->tblRegsSem);
                return retVal;
              }
            }
#else
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_COMMAND,data);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = hwGetGlobal2RegField(dev,QD_REG_PTP_COMMAND,15,1,&data);
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = hwReadGlobal2Reg(dev,QD_REG_PTP_DATA,&data);
            opData->ptpMultiData[0] = (GT_U32)data;
            if(retVal != GT_OK)
               {
                   gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            if (!(data & 0x1))
            {
                /* valid bit is not set */
                   gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            for(i=1; i<opData->nData; i++)
            {
                retVal = hwReadGlobal2Reg(dev,QD_REG_PTP_DATA,&data);
                opData->ptpMultiData[i] = (GT_U32)data;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_DATA,0);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            retVal = hwWriteGlobal2Reg(dev,QD_REG_PTP_COMMAND,data);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }
    }
#endif
#else /* CONFIG_AVB_FPGA */
    {
    GT_U16 data;
            data = (GT_U16)((1 << 15) | (PTP_READ_MULTIPLE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = 1;
            while(data == 1)
            {
                retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,&tmpData);
                data = (GT_U32)tmpData;
                data = (data >> 15) & 0x1;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,&tmpData);
            data = (GT_U32)tmpData;
            opData->ptpMultiData[0] = (GT_U32)data;
            if(retVal != GT_OK)
               {
                   gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            if (!(data & 0x1))
            {
                /* valid bit is not set */
                   gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            for(i=1; i<opData->nData; i++)
            {
                retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,&tmpData);
                data = (GT_U32)tmpData;
                opData->ptpMultiData[i] = (GT_U32)data;
                if(retVal != GT_OK)
                {
                    gtSemGive(dev,dev->ptpRegsSem);
                    return retVal;
                }
            }

            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_DATA,0);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }

            data = (GT_U16)((1 << 15) | (PTP_WRITE_DATA << 12) |
                    (opData->ptpPort << 8)    |
                    (opData->ptpBlock << 5)    |
                    (opData->ptpAddr & 0x1F));
            tmpData = (GT_U32)data;
            retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,tmpData);
            if(retVal != GT_OK)
            {
                gtSemGive(dev,dev->ptpRegsSem);
                return retVal;
            }
    }
#endif
            gtSemGive(dev,dev->ptpRegsSem);
            break;

        default:

            gtSemGive(dev,dev->ptpRegsSem);
            return GT_FAIL;
    }

    /* Wait until the ptp is ready. */
#ifndef CONFIG_AVB_FPGA
#ifdef GT_RMGMT_ACCESS
    {
      HW_DEV_REG_ACCESS regAccess;

      regAccess.entries = 1;

      regAccess.rw_reg_list[0].cmd = HW_REG_WAIT_TILL_0;
      regAccess.rw_reg_list[0].addr = CALC_SMI_DEV_ADDR(dev, 0, GLOBAL2_REG_ACCESS);
      regAccess.rw_reg_list[0].reg = QD_REG_PTP_COMMAND;
      regAccess.rw_reg_list[0].data = 15;
      retVal = hwAccessMultiRegs(dev, &regAccess);
      if(retVal != GT_OK)
      {
        gtSemGive(dev,dev->tblRegsSem);
        return retVal;
      }
    }
#else
    {
    GT_U16 data;
    data = 1;
    while(data == 1)
    {
        retVal = hwGetGlobal2RegField(dev,QD_REG_PTP_COMMAND,15,1,&data);
        if(retVal != GT_OK)
        {
            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;
        }
    }
    }
#endif
#else /* CONFIG_AVB_FPGA */
    {
    GT_U16 data;
    data = 1;
    while(data == 1)
    {
        retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_COMMAND,&tmpData);
        data = (GT_U16)tmpData;
        data = (data >> 15) & 0x1;
        if(retVal != GT_OK)
        {
            gtSemGive(dev,dev->ptpRegsSem);
            return retVal;
        }
    }
    }
#endif

    gtSemGive(dev,dev->ptpRegsSem);
    return retVal;
}


#ifdef CONFIG_AVB_FPGA

/*******************************************************************************
* gptpGetFPGAIntStatus
*
* DESCRIPTION:
*       This routine gets interrupt status of PTP logic.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        ptpInt    - PTP Int Status
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetFPGAIntStatus
(
    IN  GT_QD_DEV     *dev,
    OUT GT_U32        *ptpInt
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetPTPIntStatus Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_INT_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    *ptpInt = (GT_U32)data & 0x1;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetFPGAIntStatus
*
* DESCRIPTION:
*       This routine sets interrupt status of PTP logic.
*
* INPUTS:
*    ptpInt    - PTP Int Status
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetFPGAIntStatus
(
    IN  GT_QD_DEV     *dev,
    OUT GT_U32    ptpInt
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpSetPTPIntStatus Called.\n"));

    data = ptpInt?1:0;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_INT_OFFSET,ptpInt);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpSetFPGAIntEn
*
* DESCRIPTION:
*       This routine enables PTP interrupt.
*
* INPUTS:
*        ptpInt    - PTP Int Status (1 to enable, 0 to disable)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetFPGAIntEn
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32        ptpInt
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetPTPIntEn Called.\n"));

    data = (ptpInt == 0)?0:1;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_INTEN_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }


    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpGetClockSource
*
* DESCRIPTION:
*       This routine gets PTP Clock source setup.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        clkSrc    - PTP clock source (A/D Device or FPGA)
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetClockSource
(
    IN  GT_QD_DEV     *dev,
    OUT GT_PTP_CLOCK_SRC     *clkSrc
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetClockSource Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_SRC_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    *clkSrc = (GT_PTP_CLOCK_SRC)(data & 0x1);

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetClockSource
*
* DESCRIPTION:
*       This routine sets PTP Clock source setup.
*
* INPUTS:
*        clkSrc    - PTP clock source (A/D Device or FPGA)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetClockSource
(
    IN  GT_QD_DEV     *dev,
    IN  GT_PTP_CLOCK_SRC     clkSrc
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpSetClockSource Called.\n"));

    data = (GT_U32)clkSrc;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_SRC_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpGetP9Mode
*
* DESCRIPTION:
*       This routine gets Port 9 Mode.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        mode - Port 9 mode (GT_PTP_P9_MODE enum type)
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetP9Mode
(
    IN  GT_QD_DEV     *dev,
    OUT GT_PTP_P9_MODE     *mode
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetP9Mode Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_P9_MODE_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    if (data & 0x1)
    {
        switch (data & 0x6)
        {
            case 0:
                *mode = PTP_P9_MODE_GMII;
                break;
            case 2:
                *mode = PTP_P9_MODE_MII;
                break;
            case 4:
                *mode = PTP_P9_MODE_MII_CONNECTOR;
                break;
            default:
                return GT_BAD_PARAM;
        }
    }
    else
    {
        *mode = PTP_P9_MODE_JUMPER;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpSetP9Mode
*
* DESCRIPTION:
*       This routine sets Port 9 Mode.
*
* INPUTS:
*        mode - Port 9 mode (GT_PTP_P9_MODE enum type)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetP9Mode
(
    IN  GT_QD_DEV     *dev,
    IN  GT_PTP_P9_MODE     mode
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpSetP9Mode Called.\n"));

    switch (mode)
    {
        case PTP_P9_MODE_GMII:
            data = 1;
            break;
        case PTP_P9_MODE_MII:
            data = 3;
            break;
        case PTP_P9_MODE_MII_CONNECTOR:
            data = 5;
            break;
        case PTP_P9_MODE_JUMPER:
            data = 0;
            break;
        default:
            return GT_BAD_PARAM;
    }

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_P9_MODE_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpReset
*
* DESCRIPTION:
*       This routine performs software reset for PTP logic.
*
* INPUTS:
*        None.
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpReset
(
    IN  GT_QD_DEV     *dev
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpReset Called.\n"));

    data = 1;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_RESET_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }


    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetCycleAdjustEn
*
* DESCRIPTION:
*       This routine checks if PTP Duty Cycle Adjustment is enabled.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        adjEn    - GT_TRUE if enabled, GT_FALSE otherwise
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetCycleAdjustEn
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *adjEn
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetCycleAdjustEn Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    *adjEn = (data & 0x2)?GT_TRUE:GT_FALSE;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpSetCycleAdjustEn
*
* DESCRIPTION:
*       This routine enables/disables PTP Duty Cycle Adjustment.
*
* INPUTS:
*        adjEn    - GT_TRUE to enable, GT_FALSE to disable
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetCycleAdjustEn
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL        adjEn
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetCycleAdjustEn Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    if (adjEn == GT_FALSE)
        data &= ~0x3;    /* clear both Enable bit and Valid bit */
    else
        data |= 0x2;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetCycleAdjust
*
* DESCRIPTION:
*       This routine gets clock duty cycle adjustment value.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        adj    - adjustment value (GT_PTP_CLOCK_ADJUSTMENT structure)
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetCycleAdjust
(
    IN  GT_QD_DEV     *dev,
    OUT GT_PTP_CLOCK_ADJUSTMENT    *adj
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetCycleAdjust Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    adj->adjSign = (data & 0x4)?GT_PTP_SIGN_PLUS:GT_PTP_SIGN_NEGATIVE;
    adj->cycleStep = (data >> 3) & 0x7;

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CYCLE_INTERVAL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    adj->cycleInterval = data;

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CYCLE_ADJ_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    adj->cycleAdjust = data;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}

/*******************************************************************************
* gptpSetCycleAdjust
*
* DESCRIPTION:
*       This routine sets clock duty cycle adjustment value.
*
* INPUTS:
*        adj    - adjustment value (GT_PTP_CLOCK_ADJUSTMENT structure)
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetCycleAdjust
(
    IN  GT_QD_DEV     *dev,
    IN  GT_PTP_CLOCK_ADJUSTMENT    *adj
)
{
    GT_STATUS           retVal;
    GT_U32                data;
    GT_U32                data1;

    DBG_INFO(("gptpSetCycleAdjust Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    data &= ~0x1;    /* clear Valid bit */

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    /* Setup the Cycle Interval */
    data1 = adj->cycleInterval & 0xFFFF;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CYCLE_INTERVAL_OFFSET,data1);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    /* Setup the Cycle Adjustment */
    data1 = adj->cycleAdjust & 0xFFFF;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CYCLE_ADJ_OFFSET,data1);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    /* clear Sign bit and Cycle Step bits on QD_REG_PTP_CLK_CTRL_OFFSET value */
    data &= ~0x3C;

    switch (adj->adjSign)
    {
        case GT_PTP_SIGN_PLUS:
            data |= 0x4;
            break;

        case GT_PTP_SIGN_NEGATIVE:
            break;

        default:
            return GT_BAD_PARAM;
    }

    data |= ((adj->cycleStep & 0x7) << 3);    /* setup Step bits */
    data |= 0x1;                            /* set Valid bit */

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_CLK_CTRL_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetPLLEn
*
* DESCRIPTION:
*       This routine checks if PLL is enabled.
*
* INPUTS:
*       None.
*
* OUTPUTS:
*        en        - GT_TRUE if enabled, GT_FALSE otherwise
*        freqSel    - PLL Frequency Selection (default 0x3 - 22.368MHz)
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       PLL Frequence selection is based on the Clock Recovery PLL device.
*        IDT MK1575-01 is the default PLL device.
*
*******************************************************************************/
GT_STATUS gptpGetPLLEn
(
    IN  GT_QD_DEV     *dev,
    OUT GT_BOOL        *en,
    OUT GT_U32        *freqSel
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpGetPLLEn Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_PLL_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    *en = (data & 0x1)?GT_TRUE:GT_FALSE;

    *freqSel = (data >> 1) & 0x7;

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpSetPLLEn
*
* DESCRIPTION:
*       This routine enables/disables PLL device.
*
* INPUTS:
*        en        - GT_TRUE to enable, GT_FALSE to disable
*        freqSel    - PLL Frequency Selection (default 0x3 - 22.368MHz)
*                  Meaningful only when enabling PLL device
*
* OUTPUTS:
*       None.
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       PLL Frequence selection is based on the Clock Recovery PLL device.
*        IDT MK1575-01 is the default PLL device.
*
*******************************************************************************/
GT_STATUS gptpSetPLLEn
(
    IN  GT_QD_DEV     *dev,
    IN  GT_BOOL        en,
    IN  GT_U32        freqSel
)
{
    GT_STATUS           retVal;
    GT_U32                data;

    DBG_INFO(("gptpSetPPLEn Called.\n"));

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_PLL_CTRL_OFFSET,&data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    if(en == GT_FALSE)
    {
        data |= 0x1;
    }
    else
    {
        data &= ~0x1;
        data |= (freqSel & 0x7) << 1;
    }

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,QD_REG_PTP_PLL_CTRL_OFFSET,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;

}


/*******************************************************************************
* gptpGetDDSReg
*
* DESCRIPTION:
*       This routine gets DDS register data.
*
* INPUTS:
*    ddsReg    - DDS Register
*
* OUTPUTS:
*    ddsData    - register data
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpGetDDSReg
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32    ddsReg,
    OUT GT_U32    *ddsData
)
{
    GT_STATUS           retVal;
        GT_U32                  data;
        GT_U32                  timeout = 0x100000;

    DBG_INFO(("gptpGetDDSReg Called.\n"));

    if (ddsReg > 0x3f)
        return GT_BAD_PARAM;
    do
    {
        retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,12,&data);
        if (retVal != GT_OK)
            return retVal;
        timeout--;
        if (timeout == 0)
            return GT_FAIL;
    } while (data & 0x8000);

    data = 0x8000 | 0x4000 | (ddsReg << 8);
    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,12,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,12,&data);
    if (retVal != GT_OK)
        return retVal;

    *ddsData = data & 0xFF;

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,12,0);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpSetDDSReg
*
* DESCRIPTION:
*       This routine sets DDS register data.
*    DDS register data written by this API are not affected until gptpUpdateDDSReg API is called.
*
* INPUTS:
*    ddsReg    - DDS Register
*    ddsData    - register data
*
* OUTPUTS:
*    none
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetDDSReg
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32    ddsReg,
    IN  GT_U32    ddsData
)
{
    GT_STATUS           retVal;
        GT_U32                  data;
        GT_U32                  timeout = 0x100000;

    DBG_INFO(("gptpSetDDSReg Called.\n"));

    if ((ddsReg > 0x3f) || (ddsData > 0xff))
        return GT_BAD_PARAM;

    do
    {
        retVal = AVB_FPGA_READ_REG(dev,AVB_SMI_ADDR,12,&data);
        if (retVal != GT_OK)
            return retVal;
        timeout--;
        if (timeout == 0)
            return GT_FAIL;
    } while (data & 0x8000);

    data = 0x8000 | (ddsReg << 8) | (ddsData);
    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,12,data);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,12,0);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpUpdateDDSReg
*
* DESCRIPTION:
*       This routine updates DDS register data.
*    DDS register data written by gptpSetDDSReg are not affected until this API is called.
*
* INPUTS:
*    none
*
* OUTPUTS:
*    none
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpUpdateDDSReg
(
    IN  GT_QD_DEV     *dev
)
{
    GT_STATUS           retVal;

    DBG_INFO(("gptpUpdateDDSReg Called.\n"));

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,13,0x0);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,13,0x1);
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

/*******************************************************************************
* gptpSetADFReg
*
* DESCRIPTION:
*       This routine sets ADF4156 register data.
*
* INPUTS:
*    adfData    - register data
*
* OUTPUTS:
*    none
*
* RETURNS:
*       GT_OK      - on success
*       GT_FAIL    - on error
*       GT_NOT_SUPPORTED - if current device does not support this feature.
*
* COMMENTS:
*       None
*
*******************************************************************************/
GT_STATUS gptpSetADFReg
(
    IN  GT_QD_DEV     *dev,
    IN  GT_U32    adfData
)
{
    GT_STATUS           retVal;

    DBG_INFO(("gptpSetADFReg Called.\n"));

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,14,(adfData & 0xFFFF));
    if(retVal != GT_OK)
    {
        return retVal;
    }

    retVal = AVB_FPGA_WRITE_REG(dev,AVB_SMI_ADDR,15,((adfData>>16) & 0xFFFF));
    if(retVal != GT_OK)
    {
        return retVal;
    }

    DBG_INFO(("OK.\n"));
    return GT_OK;
}

#endif  /*  CONFIG_AVB_FPGA */
