/*
 * Copyright (c) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/utsname.h>
#include "sc_app.h"

/*
 * Version History
 *
 * 1.0 - Added version support.
 * 1.1 - Support for reading VOUT from voltage regulators.
 * 1.2 - Support for reading DIMM's SPD EEPROM and temperature sensor.
 * 1.3 - Support for reading gpio lines.
 * 1.4 - Support for getting total power of different power domains.
 * 1.5 - Support for IO expander.
 * 1.6 - Support for SFP connectors.
 * 1.7 - Support for QSFP connectors.
 * 1.8 - Support for reading EBM's EEPROM.
 * 1.9 - Support for getting board temperature.
 * 1.10 - Support for setting VOUT of voltage regulators.
 * 1.11 - Add 'geteeprom' command to get the entire content of on-board's EEPROM.
 * 1.12 - Support for FPGA Mezzanine Cards (FMCs).
 */
#define MAJOR	1
#define MINOR	12

#define LINUX_VERSION	"5.4.0"
#define BSP_VERSION	"2020_2"



extern I2C_Buses_t I2C_Buses;
extern BootModes_t BootModes;
extern Clocks_t Clocks;
extern Ina226s_t Ina226s;
extern Power_Domains_t Power_Domains;
extern Voltages_t Voltages;
extern Workarounds_t Workarounds;
extern BITs_t BITs;
extern struct ddr_dimm1 Dimm1;
extern struct Gpio_line_name Gpio_target[];
extern IO_Exp_t IO_Exp;
extern SFPs_t SFPs;
extern QSFPs_t QSFPs;
extern OnBoard_EEPROM_t OnBoard_EEPROM;
extern Daughter_Card_t Daughter_Card;
extern FMCs_t FMCs;

int Parse_Options(int argc, char **argv);
int Create_Lockfile(void);
int Destroy_Lockfile(void);
int Version_Ops(void);
int BootMode_Ops(void);
int EEPROM_Ops(void);
int Clock_Ops(void);
int Voltage_Ops(void);
int Power_Ops(void);
int Power_Domain_Ops(void);
int Workaround_Ops(void);
int BIT_Ops(void);
int DDR_Ops(void);
int GPIO_Ops(void);
int IO_Exp_Ops(void);
int SFP_Ops(void);
int QSFP_Ops(void);
int EBM_Ops(void);
int FMC_Ops(void);
extern int Plat_Gpio_target_size(void);
extern int Plat_Version_Ops(int *Major, int *Minor);
extern int Plat_Reset_Ops(void);
extern int Plat_IDCODE_Ops(char *, int);
extern int Plat_Temperature_Ops(void);
extern int Plat_QSFP_Init(void);
extern int Access_Regulator(Voltage_t *, float *, int);
extern int Access_IO_Exp(IO_Exp_t *, int, int, unsigned int *, unsigned int *);
extern int GPIO_Get(char *, int *);
extern int EEPROM_Common(char *);
extern int EEPROM_Board(char *, int);
extern int EEPROM_MultiRecord(char *);

static char Usage[] = "\n\
sc_app -c <command> [-t <target> [-v <value>]]\n\n\
<command>:\n\
	version - version and compatibility information\n\
	listbootmode - list the supported boot mode targets\n\
	setbootmode - set boot mode to <target>\n\
	reset - apply power-on-reset\n\
	eeprom - list the selected content of on-board EEPROM\n\
	geteeprom - get the content of on-board EEPROM from either <target>:\n\
		    'all', 'common', 'board', or 'multirecord'\n\
	temperature - get the board temperature\n\
	listclock - list the supported clock targets\n\
	getclock - get the frequency of <target>\n\
	setclock - set <target> to <value> frequency\n\
	setbootclock - set <target> to <value> frequency at boot time\n\
	restoreclock - restore <target> to default value\n\
	listvoltage - list the supported voltage targets\n\
	getvoltage - get the voltage of <target>, with optional <value> of 'all'\n\
	setvoltage - set <target> to <value> volts\n\
	setbootvoltage - set <target> to <value> volts at boot time\n\
	restorevoltage - restore <target> to default value\n\
	listpower - list the supported power targets\n\
	getpower - get the voltage, current, and power of <target>\n\
	listpowerdomain - list the supported power domain targets\n\
	powerdomain - get the power used by <target> power domain\n\
	listworkaround - list the applicable workaround targets\n\
	workaround - apply <target> workaround (may requires <value>)\n\
	listBIT - list the supported Board Interface Test targets\n\
	BIT - run BIT target\n\
	ddr - get DDR DIMM information: <target> is either 'spd' or 'temp'\n\
	listgpio - list the supported gpio lines\n\
	getgpio - get the state of <target> gpio\n\
	getioexp - get IO expander <target> of either 'all', 'input', or 'output'\n\
	setioexp - set IO expander <target> of either 'direction' or 'output' to <value>\n\
	restoreioexp - restore IO expander to default values\n\
	listSFP - list the supported SFP connectors\n\
	getSFP - get the connector information of <target> SFP\n\
	getpwmSFP - get the power mode value of <target> SFP\n\
	setpwmSFP - set the power mode value of <target> SFP to <value>\n\
	listQSFP - list the supported QSFP connectors\n\
	getQSFP - get the connector information of <target> QSFP\n\
	getpwmQSFP - get the power mode value of <target> QSFP\n\
	setpwmQSFP - set the power mode value of <target> QSFP to <value>\n\
	getpwmoQSFP - get the power mode override value of <target> QSFP\n\
	setpwmoQSFP - set the power mode override value of <target> QSFP to <value>\n\
	getEBM - get the content of EEPROM on EBM card from either <target>:\n\
		 'all', 'common', 'board', or 'multirecord'\n\
	listFMC - list the plugged FMCs\n\
	getFMC - get the content of EEPROM on FMC from a plugged <target>.  The <value>\n\
		 should be either: 'all', 'common', 'board', or 'multirecord'\n\
";

typedef enum {
	VERSION,
	LISTBOOTMODE,
	SETBOOTMODE,
	RESET,
	EEPROM,
	GETEEPROM,
	TEMPERATURE,
	LISTCLOCK,
	GETCLOCK,
	SETCLOCK,
	SETBOOTCLOCK,
	RESTORECLOCK,
	LISTVOLTAGE,
	GETVOLTAGE,
	SETVOLTAGE,
	SETBOOTVOLTAGE,
	RESTOREVOLTAGE,
	LISTPOWER,
	GETPOWER,
	LISTPOWERDOMAIN,
	POWERDOMAIN,
	LISTWORKAROUND,
	WORKAROUND,
	LISTBIT,
	BIT,
	DDR,
	LISTGPIO,
	GETGPIO,
	GETIOEXP,
	SETIOEXP,
	RESTOREIOEXP,
	LISTSFP,
	GETSFP,
	GETPWMSFP,
	SETPWMSFP,
	LISTQSFP,
	GETQSFP,
	GETPWMQSFP,
	SETPWMQSFP,
	GETPWMOQSFP,
	SETPWMOQSFP,
	GETEBM,
	LISTFMC,
	GETFMC,
	COMMAND_MAX,
} CmdId_t;

typedef struct {
	CmdId_t	CmdId;	
	char CmdStr[STRLEN_MAX];
	int (*CmdOps)(void);
} Command_t;

static Command_t Commands[] = {
	{ .CmdId = VERSION, .CmdStr = "version", .CmdOps = Version_Ops, },
	{ .CmdId = LISTBOOTMODE, .CmdStr = "listbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = SETBOOTMODE, .CmdStr = "setbootmode", .CmdOps = BootMode_Ops, },
	{ .CmdId = RESET, .CmdStr = "reset", .CmdOps = Plat_Reset_Ops, },
	{ .CmdId = EEPROM, .CmdStr = "eeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = GETEEPROM, .CmdStr = "geteeprom", .CmdOps = EEPROM_Ops, },
	{ .CmdId = TEMPERATURE, .CmdStr = "temperature", .CmdOps = Plat_Temperature_Ops, },
	{ .CmdId = LISTCLOCK, .CmdStr = "listclock", .CmdOps = Clock_Ops, },
	{ .CmdId = GETCLOCK, .CmdStr = "getclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETCLOCK, .CmdStr = "setclock", .CmdOps = Clock_Ops, },
	{ .CmdId = SETBOOTCLOCK, .CmdStr = "setbootclock", .CmdOps = Clock_Ops, },
	{ .CmdId = RESTORECLOCK, .CmdStr = "restoreclock", .CmdOps = Clock_Ops, },
	{ .CmdId = LISTVOLTAGE, .CmdStr = "listvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = GETVOLTAGE, .CmdStr = "getvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = SETVOLTAGE, .CmdStr = "setvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = SETBOOTVOLTAGE, .CmdStr = "setbootvoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = RESTOREVOLTAGE, .CmdStr = "restorevoltage", .CmdOps = Voltage_Ops, },
	{ .CmdId = LISTPOWER, .CmdStr = "listpower", .CmdOps = Power_Ops, },
	{ .CmdId = GETPOWER, .CmdStr = "getpower", .CmdOps = Power_Ops, },
	{ .CmdId = LISTPOWERDOMAIN, .CmdStr = "listpowerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = POWERDOMAIN, .CmdStr = "powerdomain", .CmdOps = Power_Domain_Ops, },
	{ .CmdId = LISTWORKAROUND, .CmdStr = "listworkaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = WORKAROUND, .CmdStr = "workaround", .CmdOps = Workaround_Ops, },
	{ .CmdId = LISTBIT, .CmdStr = "listBIT", .CmdOps = BIT_Ops, },
	{ .CmdId = BIT, .CmdStr = "BIT", .CmdOps = BIT_Ops, },
	{ .CmdId = DDR, .CmdStr = "ddr", .CmdOps = DDR_Ops, },
	{ .CmdId = LISTGPIO, .CmdStr = "listgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = GETGPIO, .CmdStr = "getgpio", .CmdOps = GPIO_Ops, },
	{ .CmdId = GETIOEXP, .CmdStr = "getioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = SETIOEXP, .CmdStr = "setioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = RESTOREIOEXP, .CmdStr = "restoreioexp", .CmdOps = IO_Exp_Ops, },
	{ .CmdId = LISTSFP, .CmdStr = "listSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETSFP, .CmdStr = "getSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = GETPWMSFP, .CmdStr = "getpwmSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = SETPWMSFP, .CmdStr = "setpwmSFP", .CmdOps = SFP_Ops, },
	{ .CmdId = LISTQSFP, .CmdStr = "listQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = GETQSFP, .CmdStr = "getQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = GETPWMQSFP, .CmdStr = "getpwmQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = SETPWMQSFP, .CmdStr = "setpwmQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = GETPWMOQSFP, .CmdStr = "getpwmoQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = SETPWMOQSFP, .CmdStr = "setpwmoQSFP", .CmdOps = QSFP_Ops, },
	{ .CmdId = GETEBM, .CmdStr = "getEBM", .CmdOps = EBM_Ops, },
	{ .CmdId = LISTFMC, .CmdStr = "listFMC", .CmdOps = FMC_Ops, },
	{ .CmdId = GETFMC, .CmdStr = "getFMC", .CmdOps = FMC_Ops, },
};

char Command_Arg[STRLEN_MAX];
char Target_Arg[STRLEN_MAX];
char Value_Arg[STRLEN_MAX];
int C_Flag, T_Flag, V_Flag;
static Command_t Command;

/*
 * Main routine
 */
int
main(int argc, char **argv)
{
	int Ret;

	SC_OPENLOG("sc_app");
	SC_INFO(__FILE__ ":%d:%s() start", __LINE__, __func__);
	if (Create_Lockfile() != 0) {
		return -1;
	}

	Ret = Parse_Options(argc, argv);
	if (Ret != 0) {
		goto Unlock;
	}

	Ret = (*Command.CmdOps)();

Unlock:
	if (Destroy_Lockfile() != 0) {
		return -1;
	}

	SC_INFO(__FILE__ ":%d:%s() done", __LINE__, __func__);
	return Ret;
}

/*
 * Parse the command line.
 */
int
Parse_Options(int argc, char **argv)
{
	int Options = 0;
	int c;

	opterr = 0;
	while ((c = getopt(argc, argv, "hc:t:v:")) != -1) {
		Options++;
		switch (c) {
		case 'h':
			printf("%s\n", Usage);
			return 1;
			break;
		case 'c':
			C_Flag = 1;
			strcpy(Command_Arg, optarg);
			break;
		case 't':
			T_Flag = 1;
			strcpy(Target_Arg, optarg);
			break;
		case 'v':
			V_Flag = 1;
			strcpy(Value_Arg, optarg);
			break;
		case '?':
			printf("ERROR: invalid argument\n");
			printf("%s\n", Usage);
			return -1;
		default:
			printf("%s\n", Usage);
			return -1;
		}
	}

	if (Options == 0) {
		printf("%s\n", Usage);
		return -1;
	}

	for (int i = 0; i < COMMAND_MAX; i++) {
		if (strcmp(Command_Arg, (char *)Commands[i].CmdStr) == 0) {
			Command = Commands[i];
			return 0;
		}
	}

	printf("ERROR: invalid command\n");
	return -1;
}

/*
 * Create the lockfile
 */
int
Create_Lockfile(void)
{
	FILE *FP = NULL;
	pid_t PID;
	char Output[STRLEN_MAX];

	/* If there is no lockfile, create one */
	if (access(LOCKFILE, F_OK) != 0) {
		goto LockFile;
	}

	/* Verify the validity of pid recorded in lockfile */
	FP = fopen(LOCKFILE, "r");
	if (FP == NULL) {
		SC_ERR("open %s failed: %m", LOCKFILE);
		return -1;
	}

	(void) fgets(Output, sizeof(Output), FP);
	(void) fclose(FP);
	PID = atoi(Output);
	if ((PID < 301) || (kill(PID, 0) == -1 && errno == ESRCH)) {
		/*
		 * If pid is below the minimum or pid in the lockfile is stale,
		 * replace it with current pid.  Minimum pid used for new
		 * processes is 301.
		 */
		goto LockFile;
	} else {
		/* Another instance of sc_app is running */
		SC_ERR("lockfile %s exists", LOCKFILE);
		return -1;
	}

LockFile:
	FP = fopen(LOCKFILE, "w");
	if (FP == NULL) {
		SC_ERR("open/create %s failed: %m", LOCKFILE);
		return -1;
	}

	fprintf(FP, "%d\n", getpid());
	(void) fclose(FP);
	return 0;
}

/*
 * Destroy the lockfile
 */
int
Destroy_Lockfile(void)
{
	(void) remove(LOCKFILE);
	return 0;
}

/*
 * Version Operations
 */
int
Version_Ops(void)
{
	int Major, Minor;
	struct utsname UTS;
	char BSP_Version[STRLEN_MAX];
	int Linux_Compatible = 1;
	int BSP_Compatible = 1;
	char *CP;

	(void) (*Plat_Version_Ops)(&Major, &Minor);
	if (Major == -1 && Minor == -1) {
		Major = MAJOR;
		Minor = MINOR;
	}

	printf("Version:\t%d.%d\n", Major, Minor);

	if (uname(&UTS) != 0) {
		SC_ERR("get OS information: uname failed: %m");
		return -1;
	}

	CP = strrchr(UTS.nodename, '-');
	if (CP == NULL) {
		SC_ERR("failed to obtain BSP release");
		return -1;
	}

	(void) strcpy(BSP_Version, ++CP);

	if (Major == 1) {
		if (strcmp(UTS.release, LINUX_VERSION) != 0) {
			Linux_Compatible = 0;
		}

		if (strcmp(BSP_Version, BSP_VERSION) != 0) {
			BSP_Compatible = 0;
		}
	}

	printf("Linux:\t\t%s (%sCompatible)\n", UTS.release,
	    (Linux_Compatible) ? "" : "Not ");
	printf("BSP:\t\t%s (%sCompatible)\n", BSP_Version,
	    (BSP_Compatible) ? "" : "Not ");
	return 0;
}

/*
 * Boot Mode Operations
 */
int
BootMode_Ops(void)
{
	int Target_Index = -1;
	FILE *FP;
	BootMode_t *BootMode;
	char Buffer[STRLEN_MAX];

	if (Command.CmdId == LISTBOOTMODE) {
		for (int i = 0; i < BootModes.Numbers; i++) {
			printf("%s\t0x%x\n", BootModes.BootMode[i].Name,
			    BootModes.BootMode[i].Value);
		}
		return 0;
	}

	/* Validate the bootmode target */
	if (T_Flag == 0) {
		SC_ERR("no bootmode target");
		return -1;
	}

	for (int i = 0; i < BootModes.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BootModes.BootMode[i].Name) == 0) {
			Target_Index = i;
			BootMode = &BootModes.BootMode[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		SC_ERR("invalid bootmode target");
		return -1;
	}

	switch (Command.CmdId) {
	case SETBOOTMODE:
		/* Record the boot mode */
		FP = fopen(BOOTMODEFILE, "w");
		if (FP == NULL) {
			SC_ERR("failed to write boot_mode file %s: open: %m", BOOTMODEFILE);
			return -1;
		}

		(void) sprintf(Buffer, "%s\n", BootMode->Name);
		(void) fputs(Buffer, FP);
		(void) fclose(FP);
		break;
	default:
		printf("ERROR: invalid bootmode command\n");
		break;
	}

	return 0;
}

void
EEPROM_Print_All(char *Buffer, int Size, int Partition)
{
	printf("    ");
	for (int i = 0; i < Partition; i++) {
		printf("%2x ", i);
	}

	printf("\n00: ");
	for (int i = 0, j = Partition; i < Size; i++) {
		printf("%.2x ", Buffer[i]);
		if (((i + 1) % Partition) == 0) {
			if ((i + 1) < Size) {
				printf("\n%.2x: ", j);
				j += Partition;
			}
		}
	}

	printf("\n");
	return;
}

/*
 * EEPROM Operations
 */
int
EEPROM_Ops(void)
{
	EEPROM_Targets Target;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[STRLEN_MAX];
	char Buffer[STRLEN_MAX];
	static struct tm BuildDate;
	time_t Time;
	int Offset, Length;

	if (Command.CmdId == EEPROM) {
		Target = EEPROM_SUMMARY;
	}

	if (Command.CmdId == GETEEPROM) {
		if (T_Flag == 0) {
			printf("ERROR: no geteeprom target\n");
			return -1;
		}

		if (strcmp(Target_Arg, "all") == 0) {
			Target = EEPROM_ALL;
		} else if (strcmp(Target_Arg, "common") == 0) {
			Target = EEPROM_COMMON;
		} else if (strcmp(Target_Arg, "board") == 0) {
			Target = EEPROM_BOARD;
		} else if (strcmp(Target_Arg, "multirecord") == 0) {
			Target = EEPROM_MULTIRECORD;
		} else {
			printf("ERROR: invalid geteeprom target\n");
			return -1;
		}
	}

	FD = open(OnBoard_EEPROM.I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open onboard EEPROM\n");
		return -1;
	}

	if (ioctl(FD, I2C_SLAVE_FORCE, OnBoard_EEPROM.I2C_Address) < 0) {
		printf("ERROR: unable to access onboard EEPROM\n");
		(void) close(FD);
		return -1;
	}

	Out_Buffer[0] = 0x0;
	Out_Buffer[1] = 0x0;
	if (write(FD, Out_Buffer, 2) != 2) {
		printf("ERROR: unable to set register address of onboard EEPROM\n");
		(void) close(FD);
		return -1;
	}

	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	if (read(FD, In_Buffer, 256) != 256) {
		printf("ERROR: unable to read onboard EEPROM\n");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	switch (Target) {
	case EEPROM_SUMMARY:
		printf("Language: %d\n", In_Buffer[0xA]);
		if (Plat_IDCODE_Ops(Buffer, STRLEN_MAX) != 0) {
			printf("ERROR: failed to get silicon revision\n");
			return -1;
		}

		(void) strtok(Buffer, " ");
		(void) strcpy(Buffer, strtok(NULL, "\n"));
		printf("Silicon Revision: %s\n", Buffer);

		/* Base build date for manufacturing is 1/1/1996 */
		BuildDate.tm_year = 96;
		BuildDate.tm_mday = 1;
		BuildDate.tm_min = (In_Buffer[0xD] << 16 | In_Buffer[0xC] << 8 |
				    In_Buffer[0xB]);
		Time = mktime(&BuildDate);
		if (Time == -1) {
			printf("ERROR: invalid manufacturing date\n");
			return -1;
		}

		printf("Manufacturing Date: %s", ctime(&Time));
		Offset = 0xE;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("Manufacturer: %s\n", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("Product Name: %s\n", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("Board Serial Number: %s\n", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("Board Part Number: %s\n", Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		/* Skip FRU File ID */
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("Board Revision: %s\n", Buffer);
		printf("MAC Address 0: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
		       In_Buffer[0x80], In_Buffer[0x81], In_Buffer[0x82],
		       In_Buffer[0x83], In_Buffer[0x84], In_Buffer[0x85]);
		printf("MAC Address 1: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
		       In_Buffer[0x86], In_Buffer[0x87], In_Buffer[0x88],
		       In_Buffer[0x89], In_Buffer[0x8A], In_Buffer[0x8B]);
		break;
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		if (EEPROM_Common(In_Buffer) != 0) {
			return -1;
		}

		break;
	case EEPROM_BOARD:
		if (EEPROM_Board(In_Buffer, 1) != 0) {
			return -1;
		}

		break;
	case EEPROM_MULTIRECORD:
		if (EEPROM_MultiRecord(In_Buffer) != 0) {
			return -1;
		}

		break;
	default:
		printf("ERROR: invalid geteeprom target\n");
		return -1;
	}

	return 0;
}

/*
 * Clock Operations
 */
int
Clock_Ops(void)
{
	int Target_Index = -1;
	Clock_t *Clock;
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	char Output[STRLEN_MAX];
	double Frequency;
	double Upper, Lower;

	if (Command.CmdId == LISTCLOCK) {
		for (int i = 0; i < Clocks.Numbers; i++) {
			printf("%s\n", Clocks.Clock[i].Name);
		}
		return 0;
	}

	/* Validate the clock target */
	if (T_Flag == 0) {
		printf("ERROR: no clock target\n");
		return -1;
	}

	for (int i = 0; i < Clocks.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Clocks.Clock[i].Name) == 0) {
			Target_Index = i;
			Clock = &Clocks.Clock[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid clock target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETCLOCK:
		(void) sprintf(System_Cmd, "cat %s", Clock->Sysfs_Path);
		FP = popen(System_Cmd, "r");
		if (FP == NULL) {
			printf("ERROR: failed to access sysfs path\n");
			return -1;
		}

		(void) fgets(Output, sizeof(Output), FP);
		(void) pclose(FP);
		Frequency = strtod(Output, NULL) / 1000000.0;	// In MHz
		/* Print out 3-digit after decimal point without rounding */
		printf("Frequency(MHz):\t%.3f\n",
		   ((signed long)(Frequency * 1000) * 0.001f));
		break;
	case SETCLOCK:
	case SETBOOTCLOCK:
		/* Validate the frequency */
		if (V_Flag == 0) {
			printf("ERROR: no clock frequency\n");
			return -1;
		}

		Frequency = strtod(Value_Arg, NULL);
		Upper = Clock->Upper_Freq;
		Lower = Clock->Lower_Freq;
		if (Frequency > Upper || Frequency < Lower) {
			printf("ERROR: valid frequency range is %.3f MHz - %.3f MHz\n",
			    Lower, Upper);
			return -1;
		}

		(void) sprintf(System_Cmd, "echo %u > %s",
		    (unsigned int)(Frequency * 1000000), Clock->Sysfs_Path);
		system(System_Cmd);

		if (Command.CmdId == SETBOOTCLOCK) {
			/* Remove the old value, if any */
			(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
			    Clock->Name, CLOCKFILE);
			system(System_Cmd);

			(void) sprintf(System_Cmd, "%s:\t%.3f\n", Clock->Name,
			    Frequency);
			FP = fopen(CLOCKFILE, "a");
			if (FP == NULL) {
				printf("ERROR: failed to append clock file\n");
				return -1;
			}

			(void) fprintf(FP, "%s", System_Cmd);
			(void) fflush(FP);
			(void) fsync(fileno(FP));
			(void) fclose(FP);
		}

		break;
	case RESTORECLOCK:
		Frequency = Clock->Default_Freq;
		(void) sprintf(System_Cmd, "echo %u > %s",
		    (unsigned int)(Frequency * 1000000), Clock->Sysfs_Path);
		system(System_Cmd);

		/* Remove any custom boot frequency */
		(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
		    Clock->Name, CLOCKFILE);
		system(System_Cmd);
		break;
	default:
		printf("ERROR: invalid clock command\n");
		return -1;
	}

	return 0;
}

/*
 * Voltage Operations
 */
int Voltage_Ops(void)
{
	int Target_Index = -1;
	Voltage_t *Regulator;
	FILE *FP;
	char System_Cmd[SYSCMD_MAX];
	float Voltage;

	if (Command.CmdId == LISTVOLTAGE) {
		for (int i = 0; i < Voltages.Numbers; i++) {
			printf("%s\n", Voltages.Voltage[i].Name);
		}
		return 0;
	}

	/* Validate the voltage target */
	if (T_Flag == 0) {
		printf("ERROR: no voltage target\n");
		return -1;
	}

	for (int i = 0; i < Voltages.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Voltages.Voltage[i].Name) == 0) {
			Target_Index = i;
			Regulator = &Voltages.Voltage[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid voltage target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETVOLTAGE:
		if (Access_Regulator(Regulator, &Voltage, 0) != 0) {
			printf("ERROR: failed to get voltage from regulator\n");
			return -1;
		}

		if (V_Flag != 0) {
			if (strcmp(Value_Arg, "all") != 0) {
				SC_ERR("invalid value argument %s", Value_Arg);
				return -1;
			}

			return Access_Regulator(Regulator, &Voltage, 2);
		}

		break;
	case SETVOLTAGE:
	case SETBOOTVOLTAGE:
		if (V_Flag == 0) {
			printf("ERROR: no voltage value\n");
			return -1;
		}

		Voltage = strtof(Value_Arg, NULL);
		if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
			printf("ERROR: failed to set voltage on regulator\n");
			return -1;
		}

		if (Command.CmdId == SETBOOTVOLTAGE) {
			/* Remove the old value, if any */
			(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
			    Regulator->Name, VOLTAGEFILE);
			system(System_Cmd);

			(void) sprintf(System_Cmd, "%s:\t%.3f\n", Regulator->Name,
			    Voltage);
			FP = fopen(VOLTAGEFILE, "a");
			if (FP == NULL) {
				printf("ERROR: failed to append voltage file\n");
				return -1;
			}

			(void) fprintf(FP, "%s", System_Cmd);
			(void) fflush(FP);
			(void) fsync(fileno(FP));
			(void) fclose(FP);
		}

		break;
	case RESTOREVOLTAGE:
		Voltage = Regulator->Typical_Volt;
		if (Access_Regulator(Regulator, &Voltage, 1) != 0) {
			printf("ERROR: failed to set voltage on regulator\n");
			return -1;
		}

		/* Remove any custom boot voltage */
		(void) sprintf(System_Cmd, "sed -i -e \'/^%s:/d\' %s 2> /dev/NULL",
		    Regulator->Name, VOLTAGEFILE);
		system(System_Cmd);
		break;
	default:
		printf("ERROR: invalid voltage command\n");
		break;
	}

	return 0;
}

int Read_Sensor(Ina226_t *Ina226, float *Voltage, float *Current, float *Power)
{
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	float Shunt_Voltage;
	int Ret = 0;

	FD = open(Ina226->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open INA226 sensor\n");
		return -1;
	}

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x1;	// Shunt Voltage Register(01h)
	I2C_READ(FD, Ina226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	Shunt_Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);
	/* Ignore negative reading */
	if (Shunt_Voltage >= 0x8000) {
		Shunt_Voltage = 0;
	}

	Shunt_Voltage *= 2.5;	// 2.5 μV per bit
	*Current = (Shunt_Voltage / (float)Ina226->Shunt_Resistor);
	*Current *= Ina226->Phase_Multiplier;

	(void *) memset(Out_Buffer, 0, STRLEN_MAX);
	(void *) memset(In_Buffer, 0, STRLEN_MAX);
	Out_Buffer[0] = 0x2;	// Bus Voltage Register(02h)
	I2C_READ(FD, Ina226->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	*Voltage = ((In_Buffer[0] << 8) | In_Buffer[1]);
	*Voltage *= 1.25;	// 1.25 mV per bit
	*Voltage /= 1000;

	*Power = *Current * (*Voltage);
	(void) close(FD);
	return 0;
}

/*
 * Power Operations
 */
int Power_Ops(void)
{
	int Target_Index = -1;
	Ina226_t *Ina226;
	float Voltage;
	float Current;
	float Power;

	if (Command.CmdId == LISTPOWER) {
		for (int i = 0; i < Ina226s.Numbers; i++) {
			printf("%s\n", Ina226s.Ina226[i].Name);
		}
		return 0;
	}

	/* Validate the power target */
	if (T_Flag == 0) {
		printf("ERROR: no power target\n");
		return -1;
	}

	for (int i = 0; i < Ina226s.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Ina226s.Ina226[i].Name) == 0) {
			Target_Index = i;
			Ina226 = &Ina226s.Ina226[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid power target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETPOWER:
		if (Read_Sensor(Ina226, &Voltage, &Current, &Power) == -1) {
			printf("ERROR: failed to get power\n");
			return -1;
		}

		printf("Voltage(V):\t%.4f\n", Voltage);
		printf("Current(A):\t%.4f\n", Current);
		printf("Power(W):\t%.4f\n", Power);
		break;

	default:
		printf("ERROR: invalid power command\n");
		break;
	}

	return 0;
}

/*
 * Power Domain Operations
 */
int Power_Domain_Ops(void)
{
	int Target_Index = -1;
	Power_Domain_t *Power_Domain;
	Ina226_t *Ina226;
	float Voltage;
	float Current;
	float Power;
	float Total_Power = 0;

	if (Command.CmdId == LISTPOWERDOMAIN) {
		for (int i = 0; i < Power_Domains.Numbers; i++) {
			printf("%s\n", Power_Domains.Power_Domain[i].Name);
		}
		return 0;
	}

	/* Validate the power domain target */
	if (T_Flag == 0) {
		printf("ERROR: no power domain target\n");
		return -1;
	}

	for (int i = 0; i < Power_Domains.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Power_Domains.Power_Domain[i].Name) == 0) {
			Target_Index = i;
			Power_Domain = &Power_Domains.Power_Domain[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid power domain target\n");
		return -1;
	}

	switch (Command.CmdId) {
	case POWERDOMAIN:
		for (int i = 0; i < Power_Domain->Numbers; i++) {
			Ina226 = &Ina226s.Ina226[Power_Domain->Rails[i]];
			if (Read_Sensor(Ina226, &Voltage, &Current, &Power) == -1) {
				printf("ERROR: failed to get total power\n");
				return -1;
			}

			Total_Power += Power;
		}

		printf("Power(W):\t%.4f\n", Total_Power);
		break;

	default:
		printf("ERROR: invalid power domain command\n");
		break;
	}

	return 0;
}

/*
 * Workaround Operations
 */
int Workaround_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	int Return = -1;

	if (Command.CmdId == LISTWORKAROUND) {
		for (int i = 0; i < Workarounds.Numbers; i++) {
			printf("%s\n", Workarounds.Workaround[i].Name);
		}
		return 0;
	}

	/* Validate the workaround target */
	if (T_Flag == 0) {
		printf("ERROR: no workaround target\n");
		return -1;
	}

	for (int i = 0; i < Workarounds.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)Workarounds.Workaround[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid workaround target\n");
		return -1;
	}

	/* Does the workaround need argument? */
	if (Workarounds.Workaround[Target_Index].Arg_Needed == 1 && V_Flag == 0) {
		printf("ERROR: no workaround value\n");
		return -1;
	}

	if (V_Flag == 0) {
		Return = (*Workarounds.Workaround[Target_Index].Plat_Workaround_Op)(NULL);
	} else {
		Value = atol(Value_Arg);
		if (Value != 0 && Value != 1) {
			printf("ERROR: invalid value\n");
			return -1;
		}

		Return = (*Workarounds.Workaround[Target_Index].Plat_Workaround_Op)(&Value);
	}

	if (Return == -1) {
		printf("ERROR: failed to apply workaround\n");
		return -1;
	}

	return 0;
}

/*
 * BIT Operations
 */
int BIT_Ops(void)
{
	int Target_Index = -1;
	int Return = -1;

	if (Command.CmdId == LISTBIT) {
		for (int i = 0; i < BITs.Numbers; i++) {
			printf("%s\n", BITs.BIT[i].Name);
		}
		return 0;
	}

	/* Validate the BIT target */
	if (T_Flag == 0) {
		printf("ERROR: no BIT target\n");
		return -1;
	}

	for (int i = 0; i < BITs.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)BITs.BIT[i].Name) == 0) {
			Target_Index = i;
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid BIT target\n");
		return -1;
	}

	Return = (*BITs.BIT[Target_Index].Plat_BIT_Op)(&BITs.BIT[Target_Index]);

	return Return;
}

/*
 * DDR serial presence detect (SPD) EEPROM Operations.
 *
 * SPD is a standardized way to access information about a memory module.
 * It's an EEPROM on a DIMM where the lower 128 bytes contain certain
 * parameters required by the JEDEC standards, including type, size, etc.
 * The SPD EEPROM is accessed using SMBus; address range: 0x50–0x57 or
 * 0x30–0x37. The TSE2004av extension uses addresses 0x18–0x1F to access
 * an optional on-chip temperature sensor.
 */

struct spd_ddr4 {
	__u8 spd_bytes;		// 0
	__u8 spd_rev;		// 1
	__u8 spd_mem_type;  // 2
	__u8 spd_mod_type;  // 3 needs mask 0xf
	__u8 spd_mem_size;  // 4 needs mask 0xf
	__u8 spd_to14[9];   // byte 5-9
	__u8 spd_tsensor;   // msbit: Thermal sensor
};

union spd_ddr {
	__u64 a64[2];
	__u8  b[16];
	struct spd_ddr4 ddr4;
	// struct spd_ddr3 ddr3;
};

int DDRi2c_read(int fd, __u8 *Buf, struct i2c_info *Iic)
{
	int Ret = 0;

	if (ioctl(fd, I2C_SLAVE_FORCE, Iic->Bus_addr) < 0) {
		perror(Dimm1.I2C_Bus);
		return -1;
	}
	I2C_READ(fd, Iic->Bus_addr, Iic->Read_len, &Iic->Reg_addr, Buf, Ret);
	return Ret;
}

#ifdef DEBUG
void showbuf(const char *b, unsigned int sz)
{
	for (int j = 0; j < sz; j++)
		printf("%c%02x", (0xf & j) ? ' ' : '\n', b[j]);
	putchar('\n');
}
#endif

/*
 * DDRSPD is the EEPROM on a DIMM card. First 16 bytes indicate type.
 * First 16 bytes indicate type.
 * Nibbles 4 and 5 (numbered from 0) indicates ddr4ram: "0C" in ASCII,
 * Nibble 9 indicates size: 0, .5G, 1G, 2G, 4G, 8G, or 16G
 * Nibble 28 temp sensor: 8 = present, 0 = not present
 */
int DIMM_spd(int fd)
{
	union spd_ddr Spd_buf = {.a64 = {0, 0}};
	struct spd_ddr4 *p = &Spd_buf.ddr4;
	__u8 Sz256;
	int Ret;

	Ret = DDRi2c_read(fd, Spd_buf.b, &Dimm1.Spd);
	if (Ret < 0) {
		perror(Dimm1.I2C_Bus);
		return Ret;
	}

	Sz256 = 0xf & p->spd_mem_size;
	printf("DDR4 SDRAM?\t%s\n",
		((p->spd_mem_type == 0xc) ? "Yes" : "No"));
	if (Sz256 > 1) {
		printf("Size(Gb):\t%u\n", (1 << (Sz256 - 2)));
	} else {
		printf("Size(Mb):\t%s\n", (Sz256 ? "512" : "0"));
	}
	printf("Temp. Sensor?\t%s\n",
		((0x80 & p->spd_tsensor) ? "Yes" : "No"));
#ifdef DEBUG
	showbuf(Spd_buf.b, sizeof(Spd_buf.b));
	printf("spd_bytes, revision = %u, %u\n", p->spd_bytes, p->spd_rev);
	printf("spd_mem_type = %u\n", p->spd_mem_type);
	printf("spd_mod_type = %u\n", 0xf & p->spd_mod_type);
	printf("spd_mem_size = %u or %u MB\n", Sz256, Sz256 ? (256 << Sz256) : 0);
	printf("spd_tsensor  = %c\n", (0x80 & p->spd_tsensor) ? 'Y' : 'N');
#endif
	return Ret;
}

/*
 * See Temperature format description in SE98A data sheet
 *   tttt_tttt_XXXS_TTTT  ->  STTT_Tttt_tttt_t000
 * swap bytes, set the signed bit with shifts
 * adjust for the .125 C resolution in printf
 */
int DIMM_temperature(int fd)
{
	__u16 Tbuf = 0;
	int Ret = 0;

	Ret = DDRi2c_read(fd, (void *)&Tbuf, &Dimm1.Therm);
	if (Ret == 0) {
		__s16 t = (Tbuf << 8 | Tbuf >> 8) << 3;
		printf("Temperature(C):\t%.2f\n", (.125 * t / 16));
	}
	return Ret;
}

int target_match(const char *str)
{
	return strcmp(Target_Arg, str) == 0;
}

int valid_ddr_target(void)
{
	int Ret = !T_Flag || target_match("spd") || target_match("temp");

	if (!Ret)
		fprintf(stderr, "%sERROR: no %s target for ddr command\n",
			Usage, Target_Arg);
	return Ret;
}

/*
 * DDRSPD is the EEPROM on a DIMM card. There might be a temperature sensor.
 * Assume there is only one dimm on our boards
 */
int DDR_Ops(void)
{
	int fd, Ret = 0;

	if (!valid_ddr_target())
		return -1;

	fd = open(Dimm1.I2C_Bus, O_RDWR);
	if (fd < 0) {
		perror(Dimm1.I2C_Bus);
		return -1;
	}

	// Skip if SPD only, else show Temperature
	if (Target_Arg[0] != 's')
		Ret = DIMM_temperature(fd);

	// Skip if Temperature only, else show SPD
	if (Target_Arg[0] != 't')
		Ret = DIMM_spd(fd);

	(void) close(fd);
	return Ret;
}

static int Gpio_get1(const struct Gpio_line_name *p)
{
	int State;

	if (GPIO_Get((char *)p->Internal_Name, &State) != 0) {
		printf("ERROR: failed to get GPIO line %s\n", p->Display_Name);
		return -1;
	}

	printf("%s (line %2d):\t%d\n", p->Display_Name, p->Line, State);
	return 0;
}

static int Gpio_get_all(void)
{
	FILE *FP;
	int Line, State;
	char Buffer[SYSCMD_MAX];
	char Label[STRLEN_MAX];
	char Usage[STRLEN_MAX];

	(void) strcpy(Buffer, "/usr/bin/gpioinfo");
	FP = popen(Buffer, "r");
	if (FP == NULL) {
		printf("ERROR: failed to get GPIO info\n");
		return -1;
	}

	while (fgets(Buffer, sizeof(Buffer), FP) != NULL) {
		if ((strstr(Buffer, "unnamed") != NULL) ||
		    (strstr(Buffer, "lines") != NULL)) {
			continue;
		}

		(void) strtok(Buffer, " :\"");
		Line = atoi(strtok(NULL, " :\""));
		(void) strcpy(Label, strtok(NULL, " :\""));
		(void) strcpy(Usage, strtok(NULL, " :\""));
		if (strcmp(Usage, "unused") != 0) {
			printf("%s (line %d):\tbusy, used by %s\n", Label,
			       Line, Usage);
			continue;
		}

		if (GPIO_Get(Label, &State) != 0) {
			printf("ERROR: failed to get GPIO line %s\n", Label);
			(void) pclose(FP);
			return -1;
		}

		printf("%s (line %d):\t%d\n", Label, Line, State);
	}

	(void) pclose(FP);
	return 0;
}

static int Gpio_get(void)
{
	int i, Total = Plat_Gpio_target_size();
	long Tval = strtol(Target_Arg, NULL, 0);

	if (!strcmp("all", Target_Arg)) {
		if (Gpio_get_all() != 0) {
			printf("ERROR: failed to get all GPIO lines\n");
			return -1;
		};

		return 0;
	}

	for (i = 0; i < Total; i++) {
		if (Tval == Gpio_target[i].Line ||
		    !strncmp(Gpio_target[i].Display_Name, Target_Arg, STRLEN_MAX) ||
		    !strncmp(Gpio_target[i].Internal_Name, Target_Arg, STRLEN_MAX)) {
			if (Gpio_get1(&Gpio_target[i]) != 0) {
				return -1;
			}

			return 0;
		}
	}

	fprintf(stderr, "ERROR: no %s target for %s command\n", Target_Arg,
		Command.CmdStr);
	return -1;
}

static void Gpio_list(void)
{
	int i, Total = Plat_Gpio_target_size();

	for (i = 0; i < Total; i++) {
		puts(Gpio_target[i].Display_Name);
	}
}

/*
 * GPIO_Ops lists the supported gpio lines
 * "-c getgpio -t n" - get the state of gpio line "n"
 */
int GPIO_Ops(void)
{
	if (Command.CmdId == LISTGPIO) {
		Gpio_list();
	} else {
		if (Gpio_get() != 0) {
			return -1;
		}
	}

	return 0;
}

/*
 * IO Expander Operations
 */
int IO_Exp_Ops(void)
{
	unsigned long int Value;

	if (strcmp(IO_Exp.Name, "TCA6416A") != 0) {
		printf("ERROR: unsupported IO expander chip\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETIOEXP:
		/* A target argument is required */
		if (T_Flag == 0) {
			printf("ERROR: no IO expander target\n");
			return -1;
		}

		if (strcmp(Target_Arg, "all") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x0, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read input\n");
				return -1;
			}

			printf("Input GPIO:\t0x%x\n", Value);

			if (Access_IO_Exp(&IO_Exp, 0, 0x2, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read output\n");
				return -1;
			}

			printf("Output GPIO:\t0x%x\n", Value);

			if (Access_IO_Exp(&IO_Exp, 0, 0x6, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read direction\n");
				return -1;
			}

			printf("Direction:\t0x%x\n", Value);

		} else if (strcmp(Target_Arg, "input") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x0, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read input\n");
				return -1;
			}

			for (int i = 0; i < IO_Exp.Numbers; i++) {
				if (IO_Exp.Directions[i] == 1) {
					printf("%s:\t%d\n", IO_Exp.Labels[i],
					    ((Value >> (IO_Exp.Numbers - i - 1)) & 1));
				}
			}

		} else if (strcmp(Target_Arg, "output") == 0) {
			if (Access_IO_Exp(&IO_Exp, 0, 0x2, NULL,
			    (unsigned int *)&Value) != 0) {
				printf("ERROR: failed to read output\n");
				return -1;
			}

			for (int i = 0; i < IO_Exp.Numbers; i++) {
				if (IO_Exp.Directions[i] == 0) {
					printf("%s:\t%d\n", IO_Exp.Labels[i],
					    ((Value >> (IO_Exp.Numbers - i - 1)) & 1));
				}
			}

		} else {
			printf("ERROR: invalid getioexp target\n");
			return -1;
		}

		break;

	case SETIOEXP:
		/* A target argument is required */
		if (T_Flag == 0) {
			printf("ERROR: no IO expander target\n");
			return -1;
		}

		/* Validate the value argument */
		if (V_Flag == 0) {
			printf("ERROR: no IO expander value\n");
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (strcmp(Target_Arg, "direction") == 0) {
			if (Access_IO_Exp(&IO_Exp, 1, 0x6, (unsigned int *)&Value,
			    NULL) != 0) {
				printf("ERROR: failed to set direction\n");
				return -1;
			}

		} else if (strcmp(Target_Arg, "output") == 0) {
			if (Access_IO_Exp(&IO_Exp, 1, 0x2, (unsigned int *)&Value,
			    NULL) != 0) {
				printf("ERROR: failed to set output\n");
				return -1;
			}

		} else {
			printf("ERROR: invalid setioexp target\n");
			return -1;
		}

		break;

	case RESTOREIOEXP:
		Value = 0;
		for (int i = 0; i < IO_Exp.Numbers; i++) {
			Value <<= 1;
			if ((IO_Exp.Directions[i] == 1) ||
			   (IO_Exp.Directions[i] == -1)) {
				Value |= 1;
			}
		}

		if (Access_IO_Exp(&IO_Exp, 1, 0x6, (unsigned int *)&Value,
		    NULL) != 0) {
			printf("ERROR: failed to set direction\n");
			return -1;
		}

		Value = ~Value;
		if (Access_IO_Exp(&IO_Exp, 1, 0x2, (unsigned int *)&Value,
		    NULL) != 0) {
			printf("ERROR: failed to set output\n");
			return -1;
		}

		break;

	default:
		printf("ERROR: invalid IO expander command\n");
		return -1;
	}

	return 0;
}

/*
 * SFP Operations
 */
int SFP_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	SFP_t *SFP;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	if (Command.CmdId == LISTSFP) {
		for (int i = 0; i < SFPs.Numbers; i++) {
			printf("%s\n", SFPs.SFP[i].Name);
		}
		return 0;
	}

	/* Validate the SFP target */
	if (T_Flag == 0) {
		printf("ERROR: no SFP target\n");
		return -1;
	}

	for (int i = 0; i < SFPs.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)SFPs.SFP[i].Name) == 0) {
			Target_Index = i;
			SFP = &SFPs.SFP[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid SFP target\n");
		return -1;
	}

	FD = open(SFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open SFP connector\n");
		return -1;
	}

	switch (Command.CmdId) {
	case GETSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x14;	// 0x14-0x23: SFP Vendor Name
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		printf("Manufacturer:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x44;	// 0x44-0x53: Serial Number
		I2C_READ(FD, SFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		printf("Serial Number:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x60;	// 0x60-0x61: Temperature Monitor
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		printf("Internal Temperature(C):\t%.3f\n", ((float)Value / 256));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x62;	// 0x62-0x63: Voltage Sense
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		/* Each bit is 100 uV */
		printf("Supply Voltage(V):\t%.2f\n", ((float)Value * 0.0001));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x70;	// 0x70-0x71: Alarm
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		printf("Alarm:\t%x\n", (In_Buffer[0] << 8) | In_Buffer[1]);
		break;

	case GETPWMSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x80;	// 0x80-0x81: PWM1 & PWM2 Controller
		I2C_READ(FD, SFP->I2C_Address + 1, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		printf("Power Mode(0-2W):\t%x\n", (In_Buffer[0] << 8) | In_Buffer[1]);
		break;

	case SETPWMSFP:
		/* Validate the value */
		if (V_Flag == 0) {
			printf("ERROR: no PWM value\n");
			(void) close(FD);
			return -1;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Value > 0xFF) {
			printf("ERROR: invalid PWM value\n");
			(void) close(FD);
			return -1;
		}

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x80;	// 0x80: PWM1 Controller
		Out_Buffer[1] = (Value & 0xFF);
		I2C_WRITE(FD, SFP->I2C_Address + 1, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		/* Add a delay, since back-to-back write fails for this device. */
		sleep(1);
		Out_Buffer[0] = 0x81;	// 0x81: PWM2 Controller
		Out_Buffer[1] = (Value & 0xFF);
		I2C_WRITE(FD, SFP->I2C_Address + 1, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return Ret;
		}

		break;

	default:
		printf("ERROR: invalid SFP command\n");
		(void) close(FD);
		return -1;
	}

	(void) close(FD);
	return 0;
}

/*
 * QSFP Operations
 */
int QSFP_Ops(void)
{
	int Target_Index = -1;
	unsigned long int Value;
	QSFP_t *QSFP;
	int FD;
	char In_Buffer[STRLEN_MAX];
	char Out_Buffer[STRLEN_MAX];
	int Ret = 0;

	if (Command.CmdId == LISTQSFP) {
		for (int i = 0; i < QSFPs.Numbers; i++) {
			printf("%s\n", QSFPs.QSFP[i].Name);
		}
		return 0;
	}

	/* Validate the QSFP target */
	if (T_Flag == 0) {
		printf("ERROR: no QSFP target\n");
		return -1;
	}

	for (int i = 0; i < QSFPs.Numbers; i++) {
		if (strcmp(Target_Arg, (char *)QSFPs.QSFP[i].Name) == 0) {
			Target_Index = i;
			QSFP = &QSFPs.QSFP[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid QSFP target\n");
		return -1;
	}

	if (Plat_QSFP_Init() != 0) {
		return -1;
	}

	FD = open(QSFP->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open QSFP connector\n");
		Ret = -1;
		goto Out;
	}

	switch (Command.CmdId) {
	case GETQSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x94;	// 0x94-0xA3: QSFP Vendor Name
		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Manufacturer:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0xA8;	// 0xA8-0xB7: Part Number
		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Part Number:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0xC4;	// 0xC4-0xD3: Serial Number
		I2C_READ(FD, QSFP->I2C_Address, 16, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Serial Number:\t%s\n", In_Buffer);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x16;	// 0x16-0x17: Temperature Monitor
		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		Value = (Value & 0x7FFF) - (Value & 0x8000);
		/* Each bit of low byte is equivalent to 1/256 celsius */
		printf("Internal Temperature(C):\t%.3f\n", ((float)Value / 256));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x1A;	// 0x1A-0x1B: Supply Voltage
		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		Value = (In_Buffer[0] << 8) | In_Buffer[1];
		/* Each bit is 100 uV */
		printf("Supply Voltage(V):\t%.2f\n", ((float)Value * 0.0001));

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x3;	// 0x3-0x4: Alarms
		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Alarms (Bytes 3-4):\t%x\n", (In_Buffer[0] << 8) |
		    In_Buffer[1]);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x6;	// 0x6-0x7: Alarms
		I2C_READ(FD, QSFP->I2C_Address, 2, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Alarms (Bytes 6-7):\t%x\n", (In_Buffer[0] << 8) |
		    In_Buffer[1]);

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x9;	// 0x9-0xC: Alarms
		I2C_READ(FD, QSFP->I2C_Address, 4, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Alarms (Bytes 9-12):\t%x\n", ((In_Buffer[0] << 24) |
		    (In_Buffer[1] << 16) | (In_Buffer[2] << 8) | In_Buffer[3]));
		break;

	case GETPWMQSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x62;	// 0x62: PWM Controller
		I2C_READ(FD, QSFP->I2C_Address, 1, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Register 98, bit7 +2.5w, bit6 +1.5w, bits5-0 up to 1.0w:\t%x\n",
		    In_Buffer[0]);
		break;

	case SETPWMQSFP:
		/* Validate the value */
		if (V_Flag == 0) {
			printf("ERROR: no PWM value\n");
			Ret = -1;
			goto Out;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Value > 0xFF) {
			printf("ERROR: invalid PWM value\n");
			Ret = -1;
			goto Out;
		}

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x62;	// 0x62: PWM Controller
		Out_Buffer[1] = (Value & 0xFF);
		I2C_WRITE(FD, QSFP->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		break;

	case GETPWMOQSFP:
		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		(void *) memset(In_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x5D;	// 0x5D: Low Power Mode Override
		I2C_READ(FD, QSFP->I2C_Address, 1, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		printf("Register 93, 0 = use LPMode pin, 1 = hi pwr, 3 = low pwr:\t%x\n",
		    In_Buffer[0]);
		break;

	case SETPWMOQSFP:
		/* Validate the value */
		if (V_Flag == 0) {
			printf("ERROR: no PWM Override value\n");
			Ret = -1;
			goto Out;
		}

		Value = strtol(Value_Arg, NULL, 16);
		if (Value != 0x0 && Value != 0x1 && Value != 0x3) {
			printf("ERROR: valid PWM Override value: 0x0, 0x1, or 0x3\n");
			Ret = -1;
			goto Out;
		}

		(void *) memset(Out_Buffer, 0, STRLEN_MAX);
		Out_Buffer[0] = 0x5D;	// 0x5D: Low Power Mode Override
		Out_Buffer[1] = (Value & 0x3);
		I2C_WRITE(FD, QSFP->I2C_Address, 2, Out_Buffer, Ret);
		if (Ret != 0) {
			goto Out;
		}

		break;

	default:
		printf("ERROR: invalid QSFP command\n");
		Ret = -1;
	}

Out:
	/*
	 * The Plat_QSFP_Init() call may change the current boot mode to JTAG
	 * to download a PDI.  Calling Plat_Reset_Ops() restores the current
	 * boot mode.
	 */
	(void) Plat_Reset_Ops();
	(void) close(FD);
	return Ret;
}

/*
 * EBM Operations
 */
int EBM_Ops(void)
{
	EEPROM_Targets Target;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	char Buffer[STRLEN_MAX];
	int Ret = 0;

	/* Validate the EBM target */
	if (T_Flag == 0) {
		printf("ERROR: no EBM target\n");
		return -1;
	}

	if (strcmp(Target_Arg, "all") == 0) {
		Target = EEPROM_ALL;
	} else if (strcmp(Target_Arg, "common") == 0) {
		Target = EEPROM_COMMON;
	} else if (strcmp(Target_Arg, "board") == 0) {
		Target = EEPROM_BOARD;
	} else if (strcmp(Target_Arg, "multirecord") == 0) {
		Target = EEPROM_MULTIRECORD;
	} else {
		printf("ERROR: invalid EBM target\n");
		return -1;
	}

	FD = open(Daughter_Card.I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open EBM card\n");
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;
	I2C_READ(FD, Daughter_Card.I2C_Address, 256, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	(void) close(FD);
	switch (Target) {
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		if (EEPROM_Common(In_Buffer) != 0) {
			return -1;
		}

		break;
	case EEPROM_BOARD:
		if (EEPROM_Board(In_Buffer, 0) != 0) {
			return -1;
		}

		break;
	case EEPROM_MULTIRECORD:
		if (EEPROM_MultiRecord(In_Buffer) != 0) {
			return -1;
		}

		break;
	default:
		printf("ERROR: invalid EBM target\n");
		return -1;
	}

	return 0;
}

int FMC_List(void)
{
	FMC_t *FMC;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[STRLEN_MAX];
	char Buffer[STRLEN_MAX];
	int Ret = 0;
	int Offset, Length;

	for (int i = 0; i < FMCs.Numbers; i++) {
		FMC = &FMCs.FMC[i];
		FD = open(FMC->I2C_Bus, O_RDWR);
		if (FD < 0) {
			printf("ERROR: unable to open I2C bus %s\n",
			       FMC->I2C_Bus);
			return -1;
		}

		if (ioctl(FD, I2C_SLAVE_FORCE, FMC->I2C_Address) < 0) {
			printf("ERROR: unable to configure I2C for address 0x%x\n",
			       FMC->I2C_Address);
			(void) close(FD);
			return -1;
		}

		/*
		 * If write fails, it indicates that there is no FMC plugged into
		 * the connector referenced by the I2C device address.
		 */
		Out_Buffer[0] = 0x0;
		if (write(FD, Out_Buffer, 1) != 1) {
			(void) close(FD);
			continue;
		}

		/*
		 * Since there is a FMC on this connector, read its Manufacturer
		 * and its Product Name.
		 */
		(void) memset(Out_Buffer, 0, SYSCMD_MAX);
		(void) memset(In_Buffer, 0, SYSCMD_MAX);
		Out_Buffer[0] = 0x0;    // EEPROM offset 0
		I2C_READ(FD, FMC->I2C_Address, 0xFF, Out_Buffer, In_Buffer, Ret);
		if (Ret != 0) {
			(void) close(FD);
			return -1;
		}

		(void) close(FD);
		Offset = 0xE;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("%s - %s ", FMC->Name, Buffer);
		Offset = Offset + Length + 1;
		Length = (In_Buffer[Offset] & 0x3F);
		snprintf(Buffer, Length + 1, "%s", &In_Buffer[Offset + 1]);
		printf("%s\n", Buffer);
        }

	return 0;
}

/*
 * FMC Operations
 */
int FMC_Ops(void)
{
	int Target_Index = -1;
	FMC_t *FMC;
	EEPROM_Targets Area;
	int FD;
	char In_Buffer[SYSCMD_MAX];
	char Out_Buffer[SYSCMD_MAX];
	int Ret = 0;

	if (Command.CmdId == LISTFMC) {
		return FMC_List();
	}

	if (T_Flag == 0) {
		printf("ERROR: no FMC target\n");
		return -1;
	}

	(void) strcpy(Out_Buffer, strtok(Target_Arg, " - "));
	for (int i = 0; i < FMCs.Numbers; i++) {
		if (strcmp(Out_Buffer, FMCs.FMC[i].Name) == 0) {
			Target_Index = i;
			FMC = &FMCs.FMC[Target_Index];
			break;
		}
	}

	if (Target_Index == -1) {
		printf("ERROR: invalid FMC target\n");
		return -1;
	}

	if (V_Flag == 0) {
		printf("ERROR: no FMC value\n");
		return -1;
	}

	if (strcmp(Value_Arg, "all") == 0) {
		Area = EEPROM_ALL;
	} else if (strcmp(Value_Arg, "common") == 0) {
		Area = EEPROM_COMMON;
	} else if (strcmp(Value_Arg, "board") == 0) {
		Area = EEPROM_BOARD;
	} else if (strcmp(Value_Arg, "multirecord") == 0) {
		Area = EEPROM_MULTIRECORD;
	} else {
		printf("ERROR: invalid FMC value\n");
		return -1;
	}

	FD = open(FMC->I2C_Bus, O_RDWR);
	if (FD < 0) {
		printf("ERROR: unable to open FMC\n");
		return -1;
	}

	(void) memset(Out_Buffer, 0, SYSCMD_MAX);
	(void) memset(In_Buffer, 0, SYSCMD_MAX);
	Out_Buffer[0] = 0x0;
	I2C_READ(FD, FMC->I2C_Address, 256, Out_Buffer, In_Buffer, Ret);
	if (Ret != 0) {
		(void) close(FD);
		return Ret;
	}

	(void) close(FD);
	switch (Area) {
	case EEPROM_ALL:
		EEPROM_Print_All(In_Buffer, 256, 16);
		break;
	case EEPROM_COMMON:
		if (EEPROM_Common(In_Buffer) != 0) {
			return -1;
		}

		break;
	case EEPROM_BOARD:
		if (EEPROM_Board(In_Buffer, 0) != 0) {
			return -1;
		}

		break;
	case EEPROM_MULTIRECORD:
		if (EEPROM_MultiRecord(In_Buffer) != 0) {
			return -1;
		}

		break;
	default:
		printf("ERROR: invalid FMC value\n");
		return -1;
	}

	return 0;
}
