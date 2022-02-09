#ifndef _MENU_H_

typedef struct {
	char *cmd;
	char *usage;
} ShellCommandSet_t;

const ShellCommandSet_t CmdSet[] = {
#define CMDN_QMARK 0
	{
		"?",
		"?                                  * help command",
	},

#define CMDN_HELP 1
	{
		"help",
		"help                               * help command(same as ?)",
	},

#define CMDN_RTC 2
	{
		"rtc",
		"rtc [year mon day hour min sec]    * read or set RTC",
	},

#define CMDN_RFLASH 3
	{
		"rflash",
		"rflash 0~3                         * read flash memory",
	},

#define CMDN_EFLASH 4
	{
		"eflash",
		"eflash 0~3                         * erase flash memory",
	},

#define CMDN_BATT 5
	{
		"batt",
		"batt                               * read battery level",
	},

#define CMDN_PBATT 6
	{
		"pbatt",
		"pbatt                              * read battery level(same as batt)",
	},

#define CMDN_LPM3 7
	{
		"lpm3",
		"lpm3                               * enter LPM3 Mode",
	},

#define CMDN_PLPM3 8
	{
		"plpm3",
		"plpm3                              * enter LPM3 Mode(same as lpm3)",
	},

#define CMDN_PLPM2 9
	{
		"plpm2",
		"plpm2                              * enter LPM2 Mode",
	},

#define CMDN_REED 10
	{
		"reed",
		"reed                               * check reed sensor",
	},

#define CMDN_PREED 11
	{
		"preed",
		"preed                              * check reed sensor(same as reed)",
	},

#define CMDN_RSN 12
	{ "rsn", "rsn                                * read serial number" },

#define CMDN_WSN 13
	{
		"wsn",
		"wsn s/n                            * write serial number(sn: 12 digit s/n)",
	},

#define CMDN_RESET 14
	{
		"reset",
		"reset                              * reset system",
	},

#define CMDN_PRESET 15
	{
		"preset",
		"preset                             * reset system(same as reset)",
	},

#define CMDN_LCD 16
	{
		"lcd",
		"lcd                                * check LCD",
	},

#define CMDN_PLCD 17
	{
		"plcd",
		"plcd                               * check LCD(same as lcd)",
	},

#define CMDN_PMLCD 18
	{
		"pmlcd",
		"pmlcd                              * check meter LCD (Only for HTM-115W)",
	},

#define CMDN_CONFIG 19
	{
		"config",
		"config                             * show configuration",
	},

#define CMDN_MP 20
	{
		"mp",
		"mp ri mi                           * set metering parameter(report/metering interval)",
	},

#define CMDN_SLEEP 21
	{
		"sleep",
		"sleep mode                         * sleep mode control (mode: 0-normal, 1-sleep)",
	},

#define CMDN_MT 22
	{
		"mt",
		"mt [type]                          * select meter type('mt' display detail info)",
	},

#define CMDN_SI 23
	{
		"short",
		"short flag                         * short interval test(flag: 0-disable, 1-enable)",
	},

#define CMDN_DEBUG 24
	{
		"debug",
		"debug flag                         * debug printing control(flag: 0-off, 1-on)",
	},

#define CMDN_READ_METER 25
	{
		"rmeter",
		"rmeter [type]                      * read meter by selected type('rmeter' display detail info)",
	},

#define CMDN_MODEM 26
	{
		"modem",
		"modem                              * access modem(LoRa or NB-IoT)",
	},

#define CMDN_PWSN 27
	{
		"pwsn",
		"pwsn s/n ri mi                     * set parameter(s/n, metering/report interval)",
	},

#define CMDN_PMWSN 28
	{
		"pmwsn",
		"pmwsn s/n ri mi [meter parameters] * set parameter(s/n, metering/report interval, caliber, Q3, Qt, Q2, Q1, Marker = 1)",
	},

#define CMDN_PRSN 29
	{
		"prsn",
		"prsn                               * check device configuration",
	},

#define CMDN_PMRSN 30
	{
		"pmrsn",
		"pmrsn                              * check device configuration (for SWM)",
	},

#define CMDN_PVER 31
	{
		"pver",
		"pver                               * f/w version",
	},

#define CMDN_PWMETER 32
	{
		"pwmeter",
		"pwmeter num t1 p1 t2 p2 t3 p3      * meter configuration(num of meter, meterType, meterPort)",
	},

#define CMDN_PMODEM 33
	{
		"pmodem",
		"pmodem                             * check modem status",
	},

#define CMDN_PMETER 34
	{
		"pmeter",
		"pmeter num t1 p1 t2 p2 t3 p3       * read meter(num of meter, meterType, meterPort)",
	},

#define CMDN_PWRTC 35
	{
		"pwrtc",
		"pwrtc year mon day hour min sec    * read or set RTC",
	},

#define CMDN_PNFC 36
	{
		"nfc",
		"nfc                                * check NFC",
	},

#define CMDN_RNFC 37
	{
		"pfrnfc",
		"pfrnfc                             * factory reset of NFC Tag",
	},

#define CMDN_WMETER 38
	{
		"wmeter",
		"wmeter value                       * write meter value(format: xxxxxx.yyyyy)",
	},

#define CMDN_MRSN 39
	{
		"mrsn",
		"mrsn                               * read meter information(S/N, F/W, caliber, Q3, Qt, Q2, Q1)",
	},

#define CMDN_PCTX 40
	{   
        "pctx",
	    "pctx ch(0~20) level                * [RF424] transmit carrier (0:10dBm, 1:5dBm, 2:-10dBm)" 
    },

#define CMDN_PTX 41
	{   
        "ptx", 
        "ptx ch(0~20) level len(30~80)      * [RF424] test tx" 
    },

#define CMDN_PRX 42
	{ 
        "prx", 
        "prx ch(0~20)                       * [RF424] test rx" 
    },

#define CMDN_RID 43
	{ 
        "rid", 
        "rid                                * [RF424] read pan, nwk & slave id" 
    },

#define CMDN_WID 44
	{ 
        "wid", 
        "wid pan nwk slaveId                * [RF424] write pan, nwk & slave id" 
    },

#define CMDN_RMSG 45
	{ 
        "rmsg", 
        "rmsg ch(0~20)                      * [RF424] receive message" 
    },

#define CMDN_SCAN 46
	{ 
        "scan",
	    "scan CH_GROUP                      * [RF424] scan carrier(1:1~5, 2:6~10, 3:11~15, 4:16~20)" 
    },

#define CMDN_RSSI 47
	{ 
        "rssi", 
        "rssi ch(0~20)                      * [RF424] check rssi" 
    },

#define CMDN_POWER 48
	{ 
        "power", 
        "power ch(0~20)                     * [RF424] check rx power" 
    },

#define CMDN_RI_CTRL 49
	{
		"rictrl",
		"rictrl mode                        * On/Off report interval control (mode: 0-off, 1-on)",
	},

#define CMDN_ES 50
	{ 
        "es", 
        "es sector                          * [114W/124W] erase one or all data sector" 
    },

#define CMDN_MAP 51
	{ 
        "map", 
        "map                                * [114W/124W] display map sector" 
    },

#define CMDN_DDS 52
	{   
        "dds", 
        "dds sector                         * [114W/124W] display data sector" 
    },

#define CMDN_GD 53
	{ 
        "gd", 
        "gd count [skip]                    * [114W/124W] generate data" 
    },

#define CMDN_RD 54
	{ 
        "rd", 
        "rd year mon day nDays              * [114W/124W] read saved data" 
    },

#if LORA_DEVICE
#define CMDN_PWAEUI 55
	{
		"pwaeui",
		"pwaeui appEUI                      * [LORA] write app EUI",
	},

#define CMDN_PRAEUI 56
	{
		"praeui",
		"praeui                             * [LORA] read AppEUI",
	},

#define CMDN_PWDEUI 57
	{
		"pwdeui",
		"pwdeui deviceEUI                   * [LORA] write device EUI",
	},

#define CMDN_PRDEUI 58
	{
		"prdeui",
		"prdeui                             * [LORA] read device EUI",
	},

#define CMDN_PWAKEY 59
	{
		"pwakey",
		"pwakey appKey                      * [LORA] write App Key",
	},

#define CMDN_PRAKEY 60
	{
		"prakey",
		"prakey                             * [LORA] read App Key",
	},

#else // NBIOT_DEVICE
#define CMDN_PRNBID 55
	{
		"prnbid",
		"prnbid                             * [NBIOT] read IMEI IMSI",
	},

#define CMDN_PWSERVER 56
	{
		"pwserver",
		"pwserver ip port serviceCode       * [NBIOT] set server ip & port no and service code",
	},

#define CMDN_PRSERVER 57
	{
		"prserver",
		"prserver                           * [NBIOT] read server ip & port no and service code",
	},

#define CMDN_PWFOTA 58
	{
		"pwfota",
		"pwfota ip port interval(day)       * [NBIOT] set FOTA server ip & port and interval",
	},

#define CMDN_PRFOTA 59
	{
		"prfota",
		"prfota                             * [NBIOT] read FOTA server ip & port and interval",
	},

#define CMDN_PNBCTX 60
	{
		"pnbctx",
		"pnbctx                             * [NBIOT] Test NB-IoT Signal quality",
	},

#endif
};

#endif // _MENU_H_
