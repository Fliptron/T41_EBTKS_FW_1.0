//
//	06/27/2020	These defines were moved from EBTKS.cpp to here. (about 500 lines)
//
//  07/xx/2020  Continuous changes to support changes elsewhere
//

//
//  Header file contents and dependencies
//
//  EBTKS.h                 Pin definitions, I/O port control, Macros for Bus buffers and DMA control,
//                          Macros for diagnostic pins: RXD, TXD, T33, T39, LED, Macros for ISRs,
//                          enums for bus cycles and the Logic Analyzer (simple code trace)
//                          HP85 I/O registers, and reserved RAM locations
//
//  EBTKS_Config.h          Configuration for how all the code is compiled, and definition of various
//                          regions of memory on the HP85
//                          Depends on #defines in EBTKS.h
//
//  EBTKS_Global_Data.h
//                          Depends on #defines in EBTKS_Config.h
//
//
//
//
//

#define CORE_PIN_BUFEN            ( 0)
#define CORE_PIN_CTRL_DIR         ( 1)
#define CORE_PIN_NEOPIX_TSY       ( 2)
#define CORE_PIN_INTPRI           ( 3)
#define CORE_PIN_T04              ( 4)
#define CORE_PIN_T05              ( 5)
#define CORE_PIN_T06              ( 6)
#define CORE_PIN_TXD              ( 7)
#define CORE_PIN_RXD              ( 8)
#define CORE_PIN_T09              ( 9)
#define CORE_PIN_T10              (10)
#define CORE_PIN_T11              (11)
#define CORE_PIN_T12              (12)
#define CORE_PIN_T13              (13)
#define CORE_PIN_DB2              (14)
#define CORE_PIN_DB3              (15)
#define CORE_PIN_DB7              (16)
#define CORE_PIN_DB6              (17)
#define CORE_PIN_DB1              (18)
#define CORE_PIN_DB0              (19)
#define CORE_PIN_WR               (20)
#define CORE_PIN_PHASE1           (21)
#define CORE_PIN_LMA              (22)
#define CORE_PIN_RD               (23)
#define CORE_PIN_SCL              (24)
#define CORE_PIN_SDA              (25)
#define CORE_PIN_T26              (26)
#define CORE_PIN_DIR_RC           (27)
#define CORE_PIN_HALTX            (28)
#define CORE_PIN_CTRLEN           (29)
#define CORE_PIN_IPRIH_IN         (30)
#define CORE_PIN_PWO_L            (31)
#define CORE_PIN_IFETCH           (32)
#define CORE_PIN_T33              (33)
#define CORE_PIN_PHASE12          (34)
#define CORE_PIN_PHASE21          (35)
#define CORE_PIN_PWO_O            (36)
#define CORE_PIN_INTERRUPT        (37)
#define CORE_PIN_PHASE2           (38)
#define CORE_PIN_T39              (39)
#define CORE_PIN_DB4              (40)
#define CORE_PIN_DB5              (41)

#define GPIO_DR_BUFEN             GPIO6_DR
#define GPIO_DR_CTRL_DIR          GPIO6_DR
#define GPIO_DR_NEOPIX_TSY        GPIO9_DR
#define GPIO_DR_INTPRI            GPIO9_DR
#define GPIO_DR_T04               GPIO9_DR
#define GPIO_DR_T05               GPIO9_DR
#define GPIO_DR_T06               GPIO7_DR
#define GPIO_DR_TXD               GPIO7_DR
#define GPIO_DR_RXD               GPIO7_DR
#define GPIO_DR_T09               GPIO7_DR
#define GPIO_DR_T10               GPIO7_DR
#define GPIO_DR_T11               GPIO7_DR
#define GPIO_DR_T12               GPIO7_DR
#define GPIO_DR_T13               GPIO7_DR
#define GPIO_DR_DB2               GPIO6_DR
#define GPIO_DR_DB3               GPIO6_DR
#define GPIO_DR_DB7               GPIO6_DR
#define GPIO_DR_DB6               GPIO6_DR
#define GPIO_DR_DB1               GPIO6_DR
#define GPIO_DR_DB0               GPIO6_DR
#define GPIO_DR_WR                GPIO6_DR
#define GPIO_DR_PHASE1            GPIO6_DR
#define GPIO_DR_LMA               GPIO6_DR
#define GPIO_DR_RD                GPIO6_DR
#define GPIO_DR_SCL               GPIO6_DR
#define GPIO_DR_SDA               GPIO6_DR
#define GPIO_DR_T26               GPIO6_DR
#define GPIO_DR_DIR_RC            GPIO6_DR
#define GPIO_DR_HALTX             GPIO8_DR
#define GPIO_DR_CTRLEN            GPIO9_DR
#define GPIO_DR_IPRIH_IN          GPIO8_DR
#define GPIO_DR_PWO_L             GPIO8_DR
#define GPIO_DR_IFETCH            GPIO7_DR
#define GPIO_DR_T33               GPIO9_DR
#define GPIO_DR_PHASE12           GPIO7_DR
#define GPIO_DR_PHASE21           GPIO7_DR
#define GPIO_DR_PWO_O             GPIO7_DR
#define GPIO_DR_INTERRUPT         GPIO7_DR
#define GPIO_DR_PHASE2            GPIO6_DR
#define GPIO_DR_T39               GPIO6_DR
#define GPIO_DR_DB4               GPIO6_DR
#define GPIO_DR_DB5               GPIO6_DR

#define BIT_POSITION_BUFEN        ( 3)
#define BIT_POSITION_CTRL_DIR     ( 2)
#define BIT_POSITION_NEOPIX_TSY   ( 4)
#define BIT_POSITION_INTPRI       ( 5)
#define BIT_POSITION_T04          ( 6)
#define BIT_POSITION_T05          ( 8)
#define BIT_POSITION_T06          (10)
#define BIT_POSITION_TXD          (17)
#define BIT_POSITION_RXD          (16)
#define BIT_POSITION_T09          (11)
#define BIT_POSITION_T10          ( 0)
#define BIT_POSITION_T11          ( 2)
#define BIT_POSITION_T12          ( 1)
#define BIT_POSITION_T13          ( 3)
#define BIT_POSITION_DB2          (18)
#define BIT_POSITION_DB3          (19)
#define BIT_POSITION_DB7          (23)
#define BIT_POSITION_DB6          (22)
#define BIT_POSITION_DB1          (17)
#define BIT_POSITION_DB0          (16)
#define BIT_POSITION_WR           (26)    //  If this changes, see EBTKS_Bus_Interface_ISR.cpp
#define BIT_POSITION_PHASE1       (27)
#define BIT_POSITION_LMA          (24)    //  If this changes, see EBTKS_Bus_Interface_ISR.cpp
#define BIT_POSITION_RD           (25)    //  If this changes, see EBTKS_Bus_Interface_ISR.cpp
#define BIT_POSITION_SCL          (12)
#define BIT_POSITION_SDA          (13)
#define BIT_POSITION_T26          (30)
#define BIT_POSITION_DIR_RC       (31)
#define BIT_POSITION_HALTX        (18)
#define BIT_POSITION_CTRLEN       (31)
#define BIT_POSITION_IPRIH_IN     (23)
#define BIT_POSITION_PWO_L        (22)
#define BIT_POSITION_IFETCH       (12)
#define BIT_POSITION_T33          ( 7)
#define BIT_POSITION_PHASE12      (29)
#define BIT_POSITION_PHASE21      (28)
#define BIT_POSITION_PWO_O        (18)
#define BIT_POSITION_INTERRUPT    (19)
#define BIT_POSITION_PHASE2       (28)
#define BIT_POSITION_T39          (29)
#define BIT_POSITION_DB4          (20)
#define BIT_POSITION_DB5          (21)

#define BIT_MASK_BUFEN             (1 << BIT_POSITION_BUFEN     )
#define BIT_MASK_CTRL_DIR          (1 << BIT_POSITION_CTRL_DIR  )
#define BIT_MASK_NEOPIX_TSY        (1 << BIT_POSITION_NEOPIX_TSY)
#define BIT_MASK_INTPRI            (1 << BIT_POSITION_INTPRI    )
#define BIT_MASK_T04               (1 << BIT_POSITION_T04       )
#define BIT_MASK_T05               (1 << BIT_POSITION_T05       )
#define BIT_MASK_T06               (1 << BIT_POSITION_T06       )
#define BIT_MASK_TXD               (1 << BIT_POSITION_TXD       )
#define BIT_MASK_RXD               (1 << BIT_POSITION_RXD       )
#define BIT_MASK_T09               (1 << BIT_POSITION_T09       )
#define BIT_MASK_T10               (1 << BIT_POSITION_T10       )
#define BIT_MASK_T11               (1 << BIT_POSITION_T11       )
#define BIT_MASK_T12               (1 << BIT_POSITION_T12       )
#define BIT_MASK_T13               (1 << BIT_POSITION_T13       )
#define BIT_MASK_DB2               (1 << BIT_POSITION_DB2       )
#define BIT_MASK_DB3               (1 << BIT_POSITION_DB3       )
#define BIT_MASK_DB7               (1 << BIT_POSITION_DB7       )
#define BIT_MASK_DB6               (1 << BIT_POSITION_DB6       )
#define BIT_MASK_DB1               (1 << BIT_POSITION_DB1       )
#define BIT_MASK_DB0               (1 << BIT_POSITION_DB0       )
#define BIT_MASK_WR                (1 << BIT_POSITION_WR        )
#define BIT_MASK_PHASE1            (1 << BIT_POSITION_PHASE1    )
#define BIT_MASK_LMA               (1 << BIT_POSITION_LMA       )
#define BIT_MASK_RD                (1 << BIT_POSITION_RD        )
#define BIT_MASK_SCL               (1 << BIT_POSITION_SCL       )
#define BIT_MASK_SDA               (1 << BIT_POSITION_SDA       )
#define BIT_MASK_T26               (1 << BIT_POSITION_T26       )
#define BIT_MASK_DIR_RC            (1 << BIT_POSITION_DIR_RC    )
#define BIT_MASK_HALTX             (1 << BIT_POSITION_HALTX     )
#define BIT_MASK_CTRLEN            (1 << BIT_POSITION_CTRLEN    )
#define BIT_MASK_IPRIH_IN          (1 << BIT_POSITION_IPRIH_IN  )
#define BIT_MASK_PWO_L             (1 << BIT_POSITION_PWO_L     )
#define BIT_MASK_IFETCH            (1 << BIT_POSITION_IFETCH    )
#define BIT_MASK_T33               (1 << BIT_POSITION_T33       )
#define BIT_MASK_PHASE12           (1 << BIT_POSITION_PHASE12   )
#define BIT_MASK_PHASE21           (1 << BIT_POSITION_PHASE21   )
#define BIT_MASK_PWO_O             (1 << BIT_POSITION_PWO_O     )
#define BIT_MASK_INTERRUPT         (1 << BIT_POSITION_INTERRUPT )
#define BIT_MASK_PHASE2            (1 << BIT_POSITION_PHASE2    )
#define BIT_MASK_T39               (1 << BIT_POSITION_T39       )
#define BIT_MASK_DB4               (1 << BIT_POSITION_DB4       )
#define BIT_MASK_DB5               (1 << BIT_POSITION_DB5       )

#define GPIO_DR_SET_BUFEN         GPIO6_DR_SET
#define GPIO_DR_SET_CTRL_DIR      GPIO6_DR_SET
#define GPIO_DR_SET_NEOPIX_TSY    GPIO9_DR_SET
#define GPIO_DR_SET_INTPRI        GPIO9_DR_SET
#define GPIO_DR_SET_T04           GPIO9_DR_SET
#define GPIO_DR_SET_T05           GPIO9_DR_SET
#define GPIO_DR_SET_T06           GPIO7_DR_SET
#define GPIO_DR_SET_TXD           GPIO7_DR_SET
#define GPIO_DR_SET_RXD           GPIO7_DR_SET
#define GPIO_DR_SET_T09           GPIO7_DR_SET
#define GPIO_DR_SET_T10           GPIO7_DR_SET
#define GPIO_DR_SET_T11           GPIO7_DR_SET
#define GPIO_DR_SET_T12           GPIO7_DR_SET
#define GPIO_DR_SET_T13           GPIO7_DR_SET
#define GPIO_DR_SET_DB2           GPIO6_DR_SET
#define GPIO_DR_SET_DB3           GPIO6_DR_SET
#define GPIO_DR_SET_DB7           GPIO6_DR_SET
#define GPIO_DR_SET_DB6           GPIO6_DR_SET
#define GPIO_DR_SET_DB1           GPIO6_DR_SET
#define GPIO_DR_SET_DB0           GPIO6_DR_SET
#define GPIO_DR_SET_WR            GPIO6_DR_SET
#define GPIO_DR_SET_PHASE1        GPIO6_DR_SET
#define GPIO_DR_SET_LMA           GPIO6_DR_SET
#define GPIO_DR_SET_RD            GPIO6_DR_SET
#define GPIO_DR_SET_SCL           GPIO6_DR_SET
#define GPIO_DR_SET_SDA           GPIO6_DR_SET
#define GPIO_DR_SET_T26           GPIO6_DR_SET
#define GPIO_DR_SET_DIR_RC        GPIO6_DR_SET
#define GPIO_DR_SET_HALTX         GPIO8_DR_SET
#define GPIO_DR_SET_CTRLEN        GPIO9_DR_SET
#define GPIO_DR_SET_IPRIH_IN      GPIO8_DR_SET
#define GPIO_DR_SET_PWO_L         GPIO8_DR_SET
#define GPIO_DR_SET_IFETCH        GPIO7_DR_SET
#define GPIO_DR_SET_T33           GPIO9_DR_SET
#define GPIO_DR_SET_PHASE12       GPIO7_DR_SET
#define GPIO_DR_SET_PHASE21       GPIO7_DR_SET
#define GPIO_DR_SET_PWO_O         GPIO7_DR_SET
#define GPIO_DR_SET_INTERRUPT     GPIO7_DR_SET
#define GPIO_DR_SET_PHASE2        GPIO6_DR_SET
#define GPIO_DR_SET_T39           GPIO6_DR_SET
#define GPIO_DR_SET_DB4           GPIO6_DR_SET
#define GPIO_DR_SET_DB5           GPIO6_DR_SET

#define GPIO_DR_CLEAR_BUFEN         GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_CTRL_DIR      GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_NEOPIX_TSY    GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_INTPRI        GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_T04           GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_T05           GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_T06           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_TXD           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_RXD           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T09           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T10           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T11           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T12           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T13           GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_DB2           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB3           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB7           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB6           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB1           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB0           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_WR            GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_PHASE1        GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_LMA           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_RD            GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_SCL           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_SDA           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_T26           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DIR_RC        GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_HALTX         GPIO8_DR_CLEAR
#define GPIO_DR_CLEAR_CTRLEN        GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_IPRIH_IN      GPIO8_DR_CLEAR
#define GPIO_DR_CLEAR_PWO_L         GPIO8_DR_CLEAR
#define GPIO_DR_CLEAR_IFETCH        GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_T33           GPIO9_DR_CLEAR
#define GPIO_DR_CLEAR_PHASE12       GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_PHASE21       GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_PWO_O         GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_INTERRUPT     GPIO7_DR_CLEAR
#define GPIO_DR_CLEAR_PHASE2        GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_T39           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB4           GPIO6_DR_CLEAR
#define GPIO_DR_CLEAR_DB5           GPIO6_DR_CLEAR

#define GPIO_DR_TOGGLE_BUFEN        GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_CTRL_DIR     GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_NEOPIX_TSY   GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_INTPRI       GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_T04          GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_T05          GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_T06          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_TXD          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_RXD          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T09          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T10          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T11          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T12          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T13          GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB2          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB3          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB7          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB6          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB1          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB0          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_WR           GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_PHASE1       GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_LMA          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_RD           GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_SCL          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_SDA          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_T26          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DIR_RC       GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_HALTX        GPIO8_DR_TOGGLE
#define GPIO_DR_TOGGLE_CTRLEN       GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_IPRIH_IN     GPIO8_DR_TOGGLE
#define GPIO_DR_TOGGLE_PWO_L        GPIO8_DR_TOGGLE
#define GPIO_DR_TOGGLE_IFETCH       GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_T33          GPIO9_DR_TOGGLE
#define GPIO_DR_TOGGLE_PHASE12      GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_PHASE21      GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_PWO_O        GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_INTERRUPT    GPIO7_DR_TOGGLE
#define GPIO_DR_TOGGLE_PHASE2       GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_T39          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB4          GPIO6_DR_TOGGLE
#define GPIO_DR_TOGGLE_DB5          GPIO6_DR_TOGGLE

#define GPIO_DIRECTION_BUFEN        GPIO6_GDIR
#define GPIO_DIRECTION_CTRL_DIR     GPIO6_GDIR
#define GPIO_DIRECTION_NEOPIX_TSY   GPIO9_GDIR
#define GPIO_DIRECTION_INTPRI       GPIO9_GDIR
#define GPIO_DIRECTION_T04          GPIO9_GDIR
#define GPIO_DIRECTION_T05          GPIO9_GDIR
#define GPIO_DIRECTION_T06          GPIO7_GDIR
#define GPIO_DIRECTION_TXD          GPIO7_GDIR
#define GPIO_DIRECTION_RXD          GPIO7_GDIR
#define GPIO_DIRECTION_T09          GPIO7_GDIR
#define GPIO_DIRECTION_T10          GPIO7_GDIR
#define GPIO_DIRECTION_T11          GPIO7_GDIR
#define GPIO_DIRECTION_T12          GPIO7_GDIR
#define GPIO_DIRECTION_T13          GPIO7_GDIR
#define GPIO_DIRECTION_DB2          GPIO6_GDIR
#define GPIO_DIRECTION_DB3          GPIO6_GDIR
#define GPIO_DIRECTION_DB7          GPIO6_GDIR
#define GPIO_DIRECTION_DB6          GPIO6_GDIR
#define GPIO_DIRECTION_DB1          GPIO6_GDIR
#define GPIO_DIRECTION_DB0          GPIO6_GDIR
#define GPIO_DIRECTION_WR           GPIO6_GDIR
#define GPIO_DIRECTION_PHASE1       GPIO6_GDIR
#define GPIO_DIRECTION_LMA          GPIO6_GDIR         //  onPhi_2_Rise() assumes/requires that LMA, RD, and WR are in the same GPIO group. They are, GPIO6
#define GPIO_DIRECTION_RD           GPIO6_GDIR
#define GPIO_DIRECTION_SCL          GPIO6_GDIR
#define GPIO_DIRECTION_SDA          GPIO6_GDIR
#define GPIO_DIRECTION_T26          GPIO6_GDIR
#define GPIO_DIRECTION_DIR_RC       GPIO6_GDIR
#define GPIO_DIRECTION_HALTX        GPIO8_GDIR
#define GPIO_DIRECTION_CTRLEN       GPIO9_GDIR
#define GPIO_DIRECTION_IPRIH_IN     GPIO8_GDIR
#define GPIO_DIRECTION_PWO_L        GPIO8_GDIR
#define GPIO_DIRECTION_IFETCH       GPIO7_GDIR
#define GPIO_DIRECTION_T33          GPIO9_GDIR
#define GPIO_DIRECTION_PHASE12      GPIO7_GDIR
#define GPIO_DIRECTION_PHASE21      GPIO7_GDIR
#define GPIO_DIRECTION_PWO_O        GPIO7_GDIR
#define GPIO_DIRECTION_INTERRUPT    GPIO7_GDIR
#define GPIO_DIRECTION_PHASE2       GPIO6_GDIR
#define GPIO_DIRECTION_T39          GPIO6_GDIR
#define GPIO_DIRECTION_DB4          GPIO6_GDIR
#define GPIO_DIRECTION_DB5          GPIO6_GDIR

#define GPIO_PAD_STATUS_REG_BUFEN        GPIO6_PSR
#define GPIO_PAD_STATUS_REG_CTRL_DIR     GPIO6_PSR
#define GPIO_PAD_STATUS_REG_NEOPIX_TSY   GPIO9_PSR
#define GPIO_PAD_STATUS_REG_INTPRI       GPIO9_PSR
#define GPIO_PAD_STATUS_REG_T04          GPIO9_PSR
#define GPIO_PAD_STATUS_REG_T05          GPIO9_PSR
#define GPIO_PAD_STATUS_REG_T06          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_TXD          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_RXD          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T09          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T10          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T11          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T12          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T13          GPIO7_PSR
#define GPIO_PAD_STATUS_REG_DB2          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB3          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB7          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB6          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB1          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB0          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_WR           GPIO6_PSR
#define GPIO_PAD_STATUS_REG_PHASE1       GPIO6_PSR
#define GPIO_PAD_STATUS_REG_LMA          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_RD           GPIO6_PSR
#define GPIO_PAD_STATUS_REG_SCL          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_SDA          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_T26          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DIR_RC       GPIO6_PSR
#define GPIO_PAD_STATUS_REG_HALTX        GPIO8_PSR
#define GPIO_PAD_STATUS_REG_CTRLEN       GPIO9_PSR
#define GPIO_PAD_STATUS_REG_IPRIH_IN     GPIO8_PSR
#define GPIO_PAD_STATUS_REG_PWO_L        GPIO8_PSR
#define GPIO_PAD_STATUS_REG_IFETCH       GPIO7_PSR
#define GPIO_PAD_STATUS_REG_T33          GPIO9_PSR
#define GPIO_PAD_STATUS_REG_PHASE12      GPIO7_PSR
#define GPIO_PAD_STATUS_REG_PHASE21      GPIO7_PSR
#define GPIO_PAD_STATUS_REG_PWO_O        GPIO7_PSR
#define GPIO_PAD_STATUS_REG_INTERRUPT    GPIO7_PSR
#define GPIO_PAD_STATUS_REG_PHASE2       GPIO6_PSR
#define GPIO_PAD_STATUS_REG_T39          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB4          GPIO6_PSR
#define GPIO_PAD_STATUS_REG_DB5          GPIO6_PSR

#define IOMUX_CTRL_BUFEN                IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_03
#define IOMUX_CTRL_CTRL_DIR             IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_02
#define IOMUX_CTRL_NEOPIX_TSY           IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_04
#define IOMUX_CTRL_INTPRI               IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_05
#define IOMUX_CTRL_T04                  IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06
#define IOMUX_CTRL_T05                  IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08
#define IOMUX_CTRL_T06                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10
#define IOMUX_CTRL_TXD                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01
#define IOMUX_CTRL_RXD                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00
#define IOMUX_CTRL_T09                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_11
#define IOMUX_CTRL_T10                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00
#define IOMUX_CTRL_T11                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02
#define IOMUX_CTRL_T12                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01
#define IOMUX_CTRL_T13                  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03
#define IOMUX_CTRL_DB2                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_02
#define IOMUX_CTRL_DB3                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_03
#define IOMUX_CTRL_DB7                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07
#define IOMUX_CTRL_DB6                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06
#define IOMUX_CTRL_DB1                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_01
#define IOMUX_CTRL_DB0                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_00
#define IOMUX_CTRL_WR                   IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10
#define IOMUX_CTRL_PHASE1               IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11
#define IOMUX_CTRL_LMA                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08
#define IOMUX_CTRL_RD                   IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09
#define IOMUX_CTRL_SCL                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12
#define IOMUX_CTRL_SDA                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13
#define IOMUX_CTRL_T26                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_14
#define IOMUX_CTRL_DIR_RC               IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_15
#define IOMUX_CTRL_HALTX                IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_32
#define IOMUX_CTRL_CTRLEN               IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31
#define IOMUX_CTRL_IPRIH_IN             IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_37
#define IOMUX_CTRL_PWO_L                IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_36
#define IOMUX_CTRL_IFETCH               IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_12
#define IOMUX_CTRL_T33                  IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_07
#define IOMUX_CTRL_PHASE12              IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_13
#define IOMUX_CTRL_PHASE21              IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_12
#define IOMUX_CTRL_PWO_O                IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02
#define IOMUX_CTRL_INTERRUPT            IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_03
#define IOMUX_CTRL_PHASE2               IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_12
#define IOMUX_CTRL_T39                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_13
#define IOMUX_CTRL_DB4                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_04
#define IOMUX_CTRL_DB5                  IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05

//
// Some helper macros
//
//    All the GPIO_DR_.... should only appear in these macro definitions.
//      Everywhere should use these macro definitions, for consistency, clarity, and reliability
//        Validated 2020_07_19 by doing a global search of all project files
//


#define ENABLE_BUS_BUFFER_U2    (GPIO_DR_CLEAR_BUFEN = BIT_MASK_BUFEN)
#define DISABLE_BUS_BUFFER_U2   (GPIO_DR_SET_BUFEN   = BIT_MASK_BUFEN)

#define BUS_DIR_TO_HP           (GPIO_DR_SET_DIR_RC   = BIT_MASK_DIR_RC)
#define BUS_DIR_FROM_HP         (GPIO_DR_CLEAR_DIR_RC = BIT_MASK_DIR_RC)

#define CTRL_DIR_TO_HP          (GPIO_DR_SET_CTRL_DIR   = BIT_MASK_CTRL_DIR)
#define CTRL_DIR_FROM_HP        (GPIO_DR_CLEAR_CTRL_DIR = BIT_MASK_CTRL_DIR)

#define ASSERT_INT              (GPIO_DR_SET_INTERRUPT   = BIT_MASK_INTERRUPT)
#define RELEASE_INT             (GPIO_DR_CLEAR_INTERRUPT = BIT_MASK_INTERRUPT)

#define ASSERT_HALT             (GPIO_DR_SET_HALTX   = BIT_MASK_HALTX)                   // Teensy pin goes high, transistor is on,  /HALTX is driven low
#define RELEASE_HALT            (GPIO_DR_CLEAR_HALTX = BIT_MASK_HALTX)                   // Teensy pin goes low,  transistor is off, external pull-up pulls /HALTX high

#define ASSERT_PWO_OUT          (GPIO_DR_SET_PWO_O   = BIT_MASK_PWO_O)
#define RELEASE_PWO_OUT         (GPIO_DR_CLEAR_PWO_O = BIT_MASK_PWO_O)

#define ASSERT_INTPRI           (GPIO_DR_SET_INTPRI   = BIT_MASK_INTPRI)
#define RELEASE_INTPRI          (GPIO_DR_CLEAR_INTPRI = BIT_MASK_INTPRI)

//
//    On EBTKS V2.0 , with Teensy 4.1, the 8 bit data bus is GPIO 6
//    The bits are contiguous, with LSB being bit 16
//
//    All the register references in these macros should only appear in these macro definitions.
//      Everywhere should use these macro definitions, for consistency, clarity, and reliability
//        Validated 2020_07_19 by doing a global search of all project files

#define DATA_BUS_MASK                      (0x00FF0000U)
#define DATA_BUS_GDIR                      (GPIO_DIRECTION_DB0)
#define SET_T4_BUS_TO_OUTPUT               (DATA_BUS_GDIR |= DATA_BUS_MASK)
#define SET_T4_BUS_TO_INPUT                (DATA_BUS_GDIR &= (~DATA_BUS_MASK))

//
//    Pin Tests and Waits
//
//    All the register references in these macros should only appear in these macro definitions.
//      Everywhere should use these macro definitions, for consistency, clarity, and reliability
//        Validated 2020_07_19 by doing a global search of all project files

#define IS_PWO_HIGH                  ((GPIO_PAD_STATUS_REG_PWO_L & BIT_MASK_PWO_L) != 0)
#define IS_PWO_LOW                   ((GPIO_PAD_STATUS_REG_PWO_L & BIT_MASK_PWO_L) == 0)
#define WAIT_WHILE_PWO_LOW           while(IS_PWO_LOW) {}

#define IS_PHI_1_HIGH                ((GPIO_PAD_STATUS_REG_PHASE1 & BIT_MASK_PHASE1) != 0)
#define IS_PHI_1_LOW                 ((GPIO_PAD_STATUS_REG_PHASE1 & BIT_MASK_PHASE1) == 0)
#define WAIT_WHILE_PHI_1_HIGH        while(IS_PHI_1_HIGH) {}
#define WAIT_WHILE_PHI_1_LOW         while(IS_PHI_1_LOW) {}

#define IS_PHI_2_HIGH                ((GPIO_PAD_STATUS_REG_PHASE2 & BIT_MASK_PHASE2) != 0)
#define IS_PHI_2_LOW                 ((GPIO_PAD_STATUS_REG_PHASE2 & BIT_MASK_PHASE2) == 0)
#define WAIT_WHILE_PHI_2_HIGH        while(IS_PHI_2_HIGH) {}
#define WAIT_WHILE_PHI_2_LOW         while(IS_PHI_2_LOW) {}
#define IS_IPRIH_IN_LO               ((GPIO_PAD_STATUS_REG_IPRIH_IN & BIT_MASK_IPRIH_IN) == 0)

#define PHI_1_and_2_IMR              (GPIO6_IMR)
#define PHI_1_and_2_ISR              (GPIO6_ISR)

//
//  Use the TX5/RX5 on Teensy pins 20 1nd 21 as diag pins. Bit 26 is RXD on Pin 20
//                                                         Bit 27 is TXD on Pin 21
//
//  Note that the RXD and TXD implied direction is from the ESP-32 point of view. So if we were doing serial,
//  Bit 26 on pin 20 is actually teensy's TXD5

#define SET_RXD                            (GPIO_DR_SET_RXD    = BIT_MASK_RXD)
#define CLEAR_RXD                          (GPIO_DR_CLEAR_RXD  = BIT_MASK_RXD)
#define TOGGLE_RXD                         (GPIO_DR_TOGGLE_RXD = BIT_MASK_RXD)

#define SET_TXD                            (GPIO_DR_SET_TXD    = BIT_MASK_TXD)
#define CLEAR_TXD                          (GPIO_DR_CLEAR_TXD  = BIT_MASK_TXD)
#define TOGGLE_TXD                         (GPIO_DR_TOGGLE_TXD = BIT_MASK_TXD)

#define SET_T33                            (GPIO_DR_SET_T33    = BIT_MASK_T33)
#define CLEAR_T33                          (GPIO_DR_CLEAR_T33  = BIT_MASK_T33)
#define TOGGLE_T33                         (GPIO_DR_TOGGLE_T33 = BIT_MASK_T33)

#define SET_T39                            (GPIO_DR_SET_T39    = BIT_MASK_T39)
#define CLEAR_T39                          (GPIO_DR_CLEAR_T39  = BIT_MASK_T39)
#define TOGGLE_T39                         (GPIO_DR_TOGGLE_T39 = BIT_MASK_T39)

#define SET_LED                            (GPIO_DR_SET_T13    = BIT_MASK_T13)
#define CLEAR_LED                          (GPIO_DR_CLEAR_T13  = BIT_MASK_T13)
#define TOGGLE_LED                         (GPIO_DR_TOGGLE_T13 = BIT_MASK_T13)

//
//  Log file support
//

#if LOGLEVEL_GEN == LOG_NONE
#define LOGPRINTF(...) do {} while(0)
#elif LOGLEVEL_GEN == LOG_SERIAL
#define LOGPRINTF(...) do {Serial.printf(__VA_ARGS__); Serial.flush(); } while(0)
#elif LOGLEVEL_GEN == LOG_FILE
#define  LOGPRINTF(...)  do {sprintf(logfile_temp_text, __VA_ARGS__); append_to_logfile(logfile_temp_text); } while(0)
#else
#error LOGLEVEL_GEN is invalid
#endif



#if LOGLEVEL_AUX == LOG_NONE
#define LOGPRINTF_AUX(...) do {} while(0)
#endif

#if LOGLEVEL_AUX == LOG_SERIAL
#define LOGPRINTF_AUX(...) do {Serial.printf(__VA_ARGS__); Serial.flush(); } while(0)
#endif

#if LOGLEVEL_AUX == LOG_FILE
#define  LOGPRINT_AUXF(...)  do {sprintf(logfile_temp_text, __VA_ARGS__); append_to_logfile(logfile_temp_text); } while(0)
#endif



#if LOGLEVEL_1MB5 == LOG_NONE
#define LOGPRINTF_1MB5(...) do {} while(0)
#endif

#if LOGLEVEL_1MB5 == LOG_SERIAL
#define LOGPRINTF_1MB5(...) do {Serial.printf(__VA_ARGS__); Serial.flush(); } while(0)
#endif

#if LOGLEVEL_1MB5 == LOG_FILE
#define  LOGPRINTF_1MB5(...) do {sprintf(logfile_temp_text, __VA_ARGS__); append_to_logfile(logfile_temp_text); } while(0)
#endif



#if LOGLEVEL_TAPE == LOG_NONE
#define LOGPRINTF_TAPE(...) do {} while(0)
#endif

#if LOGLEVEL_TAPE == LOG_SERIAL
#define LOGPRINTF_TAPE(...) do {Serial.printf(__VA_ARGS__); Serial.flush(); } while(0)
#endif

#if LOGLEVEL_TAPE == LOG_FILE
#define  LOGPRINTF_TAPE(...) do {sprintf(logfile_temp_text, __VA_ARGS__); append_to_logfile(logfile_temp_text); } while(0)
#endif

//
//  DMA Support. EBTKS gets to generate /LMA, /RD, /WR
//

//      Assert LMA means drive LOW
#define ASSERT_LMA                         (GPIO_DR_CLEAR_LMA = BIT_MASK_LMA)
#define RELEASE_LMA                        (GPIO_DR_SET_LMA   = BIT_MASK_LMA)

//      Assert RD means drive LOW
#define ASSERT_RD                          (GPIO_DR_CLEAR_RD = BIT_MASK_RD)
#define RELEASE_RD                         (GPIO_DR_SET_RD   = BIT_MASK_RD)

//      Assert WR means drive LOW
#define ASSERT_WR                          (GPIO_DR_CLEAR_WR = BIT_MASK_WR)
#define RELEASE_WR                         (GPIO_DR_SET_WR   = BIT_MASK_WR)


//    Simple Logic Analyzer
//
//  This implements a simple Logic Analyzer that traces bus transactions and some program state
//

enum analyzer_state{
        ANALYZER_IDLE = 0,
        ANALYZER_ACQUIRING = 1,
        ANALYZER_ACQUISITION_DONE = 2
        };

//
//  Configuration related
//

//
//  HP-85 Standard I/O registers
//

#define GINTEN  (0177400)       //  GL0BAL INTERRUPT ENABLE
#define GINTDS  (0177401)       //  GLOBAL INTERRUPT DISABLE
#define KEYSTS  (0177402)       //  KEYBOARD STATUS
#define KEYCOD  (0177403)       //  KEYBOARD CODE AND EOJOB
#define CRTSAD  (0177404)       //  CRT START ADDRESS
#define CRTBAD  (0177405)       //  CRT BYTE ADDRESS
#define CRTSTS  (0177406)       //  CRT STATUS
#define CRTDAT  (0177407)       //  CRT DATA
#define TAPSTS  (0177410)       //  TAPE STATUS
#define TAPDAT  (0177411)       //  TAPE DATA
#define CLKSTS  (0177412)       //  CL0CK STATUS
#define CLKDAT  (0177413)       //  CL0CK DATA
#define PRMLEN  (0177414)       //  PRINTER MESSAGE LENGTH
#define PRCHAR  (0177415)       //  PRINTER CHARACTER ROM
#define PRSTS   (0177416)       //  PRINTER STATUS
#define PRDATA  (0177417)       //  PRINTER DATA
#define IOSTAT  (0177420)       //  I0 STATUS
#define IOINTC  (0177421)       //  I0 C0NTR-INTRUPT
#define IODATA  (0177422)       //  I0 DATA
#define PPOLL   (0177423)       //  PARALLEL P0LL REG
#define MLAD    (0177424)       //  SERIAL P0LL REGISTER
#define SERPOL  (0177425)       //  MY LISTEN ADDRESS
#define EXSTAT  (0177426)       //  EXTENDED IO STATUS
#define RSELEC  (0177430)       //  ROM SELECT
#define HEYEBTKS (0177740)      //  Mailbox alert for AUXROM Functions. ()

//
//  HP-85 RAM Mirrors for some Standard I/O registers
//

#define CRTBYT  (0100176)       //  CRT BYTE ADDRESS
#define CRTRAM  (0100200)       //  CRT START ADDRESS
#define CRTWRS  (0101016)       //  CRT STATUS IN RAM


#define FWUSER  (0100000)       //  FWA USER AREA
#define FWPRGM  (0100002)       //  FWA PR0GRAM AREA
#define FWCURR  (0100004)       //  PTR TO CURRENT PGM
#define NXTMEM  (0100006)       //  NEXT IN AVAIL USER MEM
#define LAVAIL  (0100010)       //  LAST AVAIL WD IN PGM AREA
#define CALVRB  (0100012)       //  START OF CALC VARIABLES
#define RTNSTK  (0100014)       //  TOP OF GOSUB RETURN STAK
#define NXTRTN  (0100016)       //  NEXT AVAIL GOSUB/RTN
#define FWBIN   (0100020)       //  FWA USER BIN PROG
#define LWAMEM  (0100022)       //  LWA USER MEM
#define CONTOK  (0100024)       //  CONTINUE LEGAL FLAG
#define LLDCOM  (0100025)       //  LAST LINE DECOMPILE
#define FLDCOM  (0100027)       //  FIRST LINE DECOMPILE
#define TRCFL1  (0100031)       //  TRACE MODE JUMP SEEN
#define TRCFL2  (0100032)       //  TRACE ALL
#define DISPTR  (0100033)       //  DISP BUFFER PTR
#define PRTPTR  (0100035)       //  PRINT BUFFER PTR
#define ONFLAG  (0100040)       //  ON GOSUB FLAG
#define RESULT  (0100042)       //  LAST CALC RESULT
#define THENFL  (0100052)       //  LEGAL-THEN STMT
#define AUTOI   (0100056)       //  AUTO LINE # INC
#define ERGOTO  (0100060)       //  ON ERROR JMP ADDR
#define ERRROM  (0100065)       //  ROM# OF ERROR
#define EDMOD2  (0100067)       //  EDITOR MODE
#define ERRORS  (0100070)       //  RUN TIME ERRORS
#define ERRTYP  (0100071)       //  ERROR TYPE
#define ASNTBL  (0100072)       //  24 BYTES ASSIGN FILES
#define PLOTSY  (0100116)       //  PL0TTER ON/OFF FLAG
#define DEFAUL  (0100117)       //  DEFAULT ERROR FLAG
#define KEYCNT  (0100120)       //  KEYBOARD COUNTER RPT
#define KRPET1  (0100121)       //  MAJOR KYBD REPEAT
#define KRPET2  (0100122)       //  MINOR KYBD REPEAT
#define LDFLTR  (0100123)       //  LIST BREAK LINE COUNT
#define DRG     (0100125)       //  DEG/RAD GRAD
#define LASTX   (0100126)       //  LAST X PL0TTED
#define LASTY   (0100136)       //  LAST Y PLOTTED
#define LDIRF   (0100146)       //  LDIR FLAG
#define PRALLM  (0100147)       //  PRINT ALL MODE
#define PENUPF  (0100150)       //  PEN UP FLAG
#define SVCWRD  (0100151)       //  SERVICE WORD
#define IOSW    (0100152)       //  IO SVC WORD
#define PRMODE  (0100153)       //  PRINTER POWER ON FLAG
#define CURFUN  (0100154)       //  CURRENT FUNCTION ADDRESS
#define INPCOM  (0100156)       //  INPUT COMLETION ADDR
#define RSTAR   (0100160)       //  2 BYTE TEMP ALLOC
#define RESEND  (0100160)       //  2 BYTE TAPE RESMEM END
#define GAPLEN  (0100160)       //  2 BYTE TEMP TAPE
#define SAVE26  (0100160)       //  2 BYTE TEMP TAPE
#define AVAILP  (0100160)       //  2 BYTE TEMP TAPE DIR. PTR.
#define CSTAR   (0100162)       //  1 BYTE TEMP ALLOC
#define OLDSEG  (0100162)       //  TAPE DIR. SEGMENT USED
#define DATYPE  (0100162)       //  STRING TYPE FOR PRINT# STRING
#define AXIS3   (0100163)       //  1 BYTE TEMP PLOTTER
#define DATUM   (0100163)       //  1 BYTE TAPE R/W DATUM
#define OPTBAS  (0100164)       //  2 BYTE PERMANENT!
#define TRACK   (0100166)       //  TAPE TRACK 0,1
//#define RANDOM  (0100167)       //  1 BYTE TAPE SERIAL RANDOM FLA
#define MTFLAG  (0100170)       //  1 BYTE TAPE EMPTY SCAN
#define SECURN  (0100170)       //  2 BYTE TAPE SECURITY NAME
#define YMAPT   (0100170)       //  2 BYTE TEMP FOR PLOT
#define DTAWPR  (0100171)       //  1 BYTE SOFT PROTECT DATA FILE
#define LISTCT  (0100170)       //  LINE COUNT FOR LIST TEMP
#define DFPAR1  (0100170)       //  ALLOC
#define AXIS2   (0100172)       //  2 BYTE TEMP PL0TTER
#define SAV12   (0100172)       //  (WAS SAVR12, BUT COLLIDES WITH 'REAL' SAVR12) 2 BYTE TEMP HPIL
#define DFPAR2  (0100172)       //  2 TYPE TEMP ALLOC
#define DCOUNT  (0100172)       //  2 BYTE TAPE COUNT DECOM LENGT
#define CRTONT  (0100172)       //
#define INPTOS  (0100174)       //  INPUT TOP OF STAK
#define CRXMAX  (0100202)       //
#define CRYMAX  (0100212)       //
#define CRXMIN  (0100222)       //  SCALE STATEMENT PARAMETERS
#define CRYMIN  (0100232)       //  THESE 6 MUST BE
#define XFACT   (0100242)       //    IN THIS 0RDER!!!
#define YFACT   (0100252)       //
#define XMAP    (0100262)       //  LAST X PLOTTED 0-255
#define YMAP    (0100263)       //  LAST Y PLOTTED 0-255
#define BINFLG  (0100304)       //  BIN PROG FLAG
#define INPBUF  (0100310)       //  PARSER INPUT BUF
#define LAST10  (0100435)       //  10 FROM END OF INPUT BUFFER
#define LASTIN  (0100447)       //  END OF INPUT BUFFER
#define ERRBUF  (0100450)       //  ERROR BUFFER 44 BYTES
#define ERBEND  (0100524)       //  END BUFFER + 1
#define PRTBUF  (0100524)       //  PRINT BUFFER
#define DISBUF  (0100564)       //  DISPLAY BUFFER
#define FP5     (054071 )       //
#define ONFLG   (0100626)       //  ON FLAG FOR DECOMPILE
#define CRTGBA  (0100627)       //  2 BYTES CRT
#define PRECNT  (0100631)       //  COUNT OF PRECEDENCE GRPS
#define ERSV50  (0100632)       //  USED BY SAVREG, RSTREG
#define PCR     (0100642)       //  BASIC PGM LINE PTR
#define PRFLAG  (0100644)       //
#define DSFLAG  (0100645)       //  DISPLAY FLAG
#define WENABL  (0100646)       //  DC100 WRITE ENABLED
#define D2HOOK  (0100647)       //  HOOK FOR SECOND DIRECTORY
#define TIME    (0100650)       //  TIME OF DAY
#define DATE    (0100660)       //  JULIAN DAY YEAR
#define VALIDD  (0100663)       //  VALID TAPE DIRECTORY
#define TOTALR  (0100664)       //  TOTAL DC100 READ ERROR CT.
#define DISPLN  (0100665)       //  1 BYTES DISPLAY LINE LENGTH
#define PRNTLN  (0100666)       //  1 BYTES PRINTER LINE LENGTH
#define IOBITS  (0100667)       //  1 BYTE IO
#define ERRSC   (0100670)       //  ERROR SELECT CODE
#define KEYHIT  (0100671)       //  KEYBOARD ASCII
#define INPTR   (0100672)       //  INPUT LINE POINTER
#define ERTEMP  (0100674)       //  12 BYTES TEMP
#define SYSTAB  (0100707)       //  LAST ENTRY 0F ABOVE
#define LEGEND  (0100710)       //
#define LEGEN2  (0100750)       //
#define NXTDAT  (0101010)       //  NXT DATA ADDRESS FOR THE TAPE BUF
#define LSTDAT  (0101012)       //  LAST DATA ADDR. FOR TAPE BUF
#define NXTBUF  (0101014)       //  NXT BUFFER ADDRESS
#define ERBASE  (0101017)       //  BASE # READ ERRORS EACH REC.
#define OLDPCR  (0101020)       //  2 BYTE OLD PCR FOR TRACE
#define DCLCOM  (0101022)       //  COMMON DECLARATION FLAG
#define NXTCOM  (0101023)       //  NEXT COMMON ITEM
#define COMFLG  (0101025)       //  1 BYTE COMMON FLAG
#define COMOPT  (0101026)       //  2 BYTE COMMON OPT BASE
#define COMMON  (0101030)       //  2 BYTE COMMON LEN
#define ERRTMP  (0101032)       //  2 BYTE ERROR REGISTER SAVE
#define FILTYP  (0101034)       //  1 BYTE TAPE, TEMP
#define CRFLAG  (0101035)       //  1 BYTE TAPE TEMP, NO USE BY AL
#define NEWAVA  (0101036)       //  2 BYTES TEMP TAPE DIR .ADDR.
#define IPBIN   (0101042)       //  I0 ROM COM LINK
#define START   (0101043)       //  10 BYTES TEMP AXIS
#define TSTBUF  (0101043)       //  2 BYTES TEMP TESET,PWO TEST
#define SAVNM2  (0101043)       //  6 BYTES TEMP TAPE FOR RENAME
#define FINISH  (0101053)       //  10 BYTES TEMP AXIS
#define TSTBF0  (0101053)       //
#define TSTBF1  (0101055)       //
#define TSTBF2  (0101057)       //
#define TSTBF3  (0101061)       //
#define TIC     (0101063)       //  10 BYTES TEMP AXIS
#define HEDADR  (0101063)       //  6 BYTES TAPE HEADERS
#define HEDAD2  (0101064)       //  6 BYTES TAPE HEADERS
#define HEDAD3  (0101065)       //  6 BYTES TAPE HEADERS
#define HEDAD4  (0101066)       //  6 BYTES TAPE HEADERS
#define HEDAD5  (0101067)       //  6 BYTES TAPE HEADERS
#define HEDAD6  (0101070)       //  6 BYTES TAPE HEADERS
#define TINTEN  (0101071)       //  2 BYTES TAPE INTERRUPT EN.
#define ERRSTP  (0101073)       //  2 BYTES ERR ADDR ALLOC
#define LINELN  (0101103)       //  DEVICE LINE LENGTH
#define ML      (0101105)       //  LAST PRINTER MESSAGE LENGTH
#define INPR10  (0101106)       //  R10 SAVE DURING INPUT
#define SCTEMP  (0101110)       //  S.C. TEMP STORE
#define SCTMP4  (0101114)       //
#define TEMP22  (0101120)       //  MATH TEMP STORE
#define TYPA    (0101120)       //  MAT ROM TEMP STORE
#define INCRA   (0101121)       //  MAT ROM TEMP STORE
#define TYPB    (0101123)       //  MAT ROM TEMP STORE
#define INCRB   (0101124)       //  MAT ROM TEMP STORE
#define MTEMP   (0101126)       //  MAT ROM TEMP STORE
#define SAVNAM  (0101120)       //  TAPE FILE NAME TEMP
#define SCRTYP  (0101120)       //  1 BYTE TAPE ERROR TYPE
#define STSIZE  (0101130)       //  STkIT SIZE PLAfE y0L[,N;:
#define TOS     (0101132)       //  T0P R12 5Tvki
#define TIMTAB  (0101134)       //  6 BYTES TIMER TABLE
#define TIMTB2  (0101136)       //
#define TIMTB3  (0101140)       //
#define IKSLEN  (0101142)       //  IMAGE START LEN
#define IMSADR  (0101144)       //  IMAGE START ADDR
#define IMCLEN  (0101146)       //  IMAGE CURRENT LEN
#define IMCADR  (0101150)       //  IMAGE CURRENT ADDR
#define IMWADR  (0101152)       //  IMAGE WRAP ADDR
#define TYPC    (0101154)       // 
#define INCRC   (0101155)       //
#define TRCFLG  (0101157)       //
#define LOADAD  (0101170)       //  2 BYTES TAPES
#define ZERO    (0111170)       //  1 BYTE ZERO TRACE VAR
#define DIMFLG  (0101173)       //  DIMENSION FLAG FOR ALLOC
#define SAVER6  (0101174)       //  TAPE RETURN - DONT MESS WITH
#define KEYTAB  (0101200)       //  BASE ADDR KEY TABL
#define CURLOC  (0101221)       //  2 BYTES TAPE RESMEM PTR
#define DATLEN  (0101223)       //  2 BYTES TAPE PRINT#
#define RESDAT  (0101225)       //  2 BYTES TAPE RESMEM PTR2
#define SAVR10  (0101227)       //  2 BYTES R10 SAVED PERMANENT
#define ROMFL   (0101231)       //  ROM FLAG FOR INIT,SCR.,ETC.
#define NBSROM  (0101232)       //  NBS TAPE GET FLAG
#define BINTAB  (0101233)       //  16 BYTES ROM BASE ADDRS.
#define ROMTAB  (0101235)       //  INITIAL ROM BASE ADDRS.
#define ROMLST  (0101272)       //  LAST ENTRY IN R0M TABLE
#define ROMEND  (0101274)       //  END OF ROMADDRS
#define CURFIL  (0101274)       //  CURRENT FILE#
#define CURREC  (0101276)       //  CURRENT RECORD #
#define INTPR6  (0101304)       //  STAK PTR FROM INTERP
#define INTRAD  (0656   )       //  ADDRESS 0F INTERP RTN
#define PRS_R6  (0101317)       //  STAK PTR FROM PARSER
#define R6LIM1  (0101740)       //  LEAVES 40 BYTES
#define R6LIM2  (0101720)       //  LEAVES 60 BYTES
#define XAPRIM  (0101740)       //
#define YAPRIM  (0101750)       //
#define XBPRIM  (0101760)       //
#define YBPRIM  (0101770)       //  --PRIM ARE 40 BYTES OF ADJACE
#define RECBUF  (0102000)       //  TAPE BUFFER
#define LSTBUF  (0102400)       //  LWA + 1 TAPE BUFFER
#define IOTRFC	(0102400)       //   7 BYTES TRAFIC INTERCEPT
#define IOSP    (0102407)       //   I-0 SERVICE POINTER
#define CHIDLE  (0102416)       //   CHAR. EDITOR INTERCEPT
#define KYIDLE  (0102425)       //  KEYB0ARD INTERCEPT
#define RMIDLE  (0102434)       //  EXEC INTERCEPT
#define STRANG  (0102443)       //  STRANGE DATA TYPES INTERCEPT
#define IMERR   (0102452)       //  IMAGE ERROR INTERCEPT
#define PRSIDL  (0102461)       //  PARSER, ALSO
#define IRQ20   (0102470)       //  IO INTERRUPT
#define IRQPAD  (0102505)       //  PAD ADDR IN IRQ
#define IRQRTN  (0102506)       //  RTN ADDR IN IRQ20
#define SPAR0   (0102512)       //  SPARE INT. 0
#define SPAR1   (0102523)       //  SPARE INTERRUPT #1
#define IOBASE  (0102536)       //  IO ROM TEMP AREA PTR
#define MSBASE  (0102540)       //  MASS STORAGE PTR
#define AGLBAS  (0105542)       //  GRAPHICS R0M
#define APRBAS  (0102544)       //  AP       R0M
#define BSRBAS  (0102546)       //  BLUE SPRUCE
#define MBASE   (0102550)       //  MATRIX ROM BASE ADDRESS
#define ASMBAS  (0102552)       //  ASSEMBLER R0M BASE
#define CPRBAS  (0102556)       //  CAPR BASE ADDRESS
#define XDIFF   (0102561)       //  10 BYTES PLOTTER
#define CURASN  (0102571)       //  2 BYTES TAPE
#define CURCAT  (0102573)       //  2 BYTES TAPE
#define DIRECT  (0102600)       //  TAPE DIRECT0RY AREA
#define DIREND  (0103174)       //
#define DIRSEG  (0103174)       //  THESE TWO MUST BE ADJACENT
#define FL1TK1  (0103175)       //  THREE BYTES FL1TK1
#define FWROM   (0103300)       //  FWA USER PROGRAM ROMRAM
#define ZRO     (00     )       //  FOR JSB XN,ZRO
#define STACK   (0101300)       //  REAL CAPRICORN STACK
#define STAK1   (0101307)       //  "CAPR" STACK VALUE

//
//  These names are not C language compliant
//

//    #define USING?  (0100037)       //  USING FLAG
//    #define AUTO#   (0100054)       //  AUTO LINE # LAST VAL
//    #define ERLIN#  (0100062)       //  LINE# OF BAD LINE
//    #define ERNUM#  (0100064)       //  ERROR NUMBER
//    #define ERROM#  (0100066)       //  ROM# OF LAST ERROR
//    #define STRNG?  (0100162)       //  COMPILE FLAG FOR PRINT
//    #define USED?   (0100162)       //  TAPE DIR. SEGMENT USED
//    #define CRTON?  (0100162)       //  CRT ON FLAG FOR PRINT DRIVER
//    #define SEC#    (0100163)       //  1 BYTE TAPE SECURITY
//    #define YMAPT+  (0100171)       //  TEMP TOP-HALF OF YMAPT
//    #define CS.C.   (0100264)       //  CRT SELECT CODE-8 BYTES
//    #define CS.C+7  (0100273)       //  CRT SELECT CODE MSB
//    #define PS.C.   (0100274)       //  PRINTER SELECT CODE
//    #define PS.C+7  (0100303)       //  PTR SELECT CODE MSB
//    #define INPB-3  (0100305)       //  3 PERMANENT IN FRONT OF INPBU
//    #define LAST-1  (0100446)       //  END-1
//    #define LAST+1  (0100450)       //  END OF BUFFER + 1
//    #define FPAR#   (0100624)       //  NUMBER OF FORMAL PARAMS
//    #define B/REC   (0101010)       //  BYTES PER REC FOR CREATE
//    #define NXTDA+  (0101012)       //  NXT DATA SAVE TEMP
//    #define R/FILE  (0101040)       //  2 BYTES TAPE TEMP
//    #define CR.CNT  (0101043)       //  2 BYTES TEMP TAPE FOR CREATE
//    #define P.BUFF  (0101075)       //  INDIRECT BUFFER POINTERS
//    #define P.PTR   (0101077)       //
//    #define P.FLAG  (0101101)       //
//    #define SCT+6   (0101116)       //  S.C. TEMP FOR I/O ROM
//    #define SCT+7   (0101117)       //  S.C. TEMP STORE MSB
//    #define X(K-1)  (0101160)       //  PREVIOUS RND NUMBER
//    #define KYTB+8  (0101210)       //
//    #define INTPR-  (0101302)       //  R6 ADDRESS OF INTERP RTN
//    #define IOTRF+  (0102403)       //   MSROM SAVE/GET
//    #define IRQ20+  (0102477)       //  IO INTERRUPT

