#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbra.h"
#include "acsi.h"
#include "translated.h"
#include "gemdos.h"

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */
 
/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */
typedef void  (*TrapHandlerPointer)( void );

extern void gemdos_handler( void );
extern TrapHandlerPointer old_gemdos_handler;
int32_t (*gemdos_table[256])( void* sp ) = { 0 };
int16_t useOldHandler = 0;									/* 0: use new handlers, 1: use old handlers */

/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */
BYTE ce_findId(void);
BYTE ce_identify(BYTE ACSI_id);
void ce_initialize(void);

#define DMA_BUFFER_SIZE		512
BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

BYTE deviceID;
BYTE command[6] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};

_DTA *pDta;
BYTE tempDta[45];

BYTE switchToSuper;

#define Clear_home()    Cconws("\33E")

/* ------------------------------------------------------------------ */
/* the custom GEMDOS handlers now follow */

static int32_t custom_dgetdrv( void *sp )
{
	DWORD res;

	command[4] = GEMDOS_Dgetdrv;										/* store GEMDOS function number */
	command[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetdrv();
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;															/* return the result */
}

static int32_t custom_dsetdrv( void *sp )
{
	DWORD res;

	/* get the drive # from stack */
	WORD drive = (WORD) *((WORD *) sp);
	
	if(drive > 15) {													/* probably invalid param */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetdrv(drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	command[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
	command[5] = (BYTE) drive;											/* store drive number */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetdrv(drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	// TODO: replace BIOS Drvmap too!
	
	WORD drivesMapOrig	= Drvmap();										/* BIOS call - get drives bitmap */
	WORD myDrivesMap	= (WORD) *pDmaBuffer;							/* read result, which is drives bitmap*/	
	
	return (drivesMapOrig | myDrivesMap);								/* result = original + my drives bitmap */
}

static int32_t custom_dfree( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *pDiskInfo	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	command[4] = GEMDOS_Dfree;											/* store GEMDOS function number */
	command[5] = drive;									

	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dfree(pDiskInfo, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	memcpy(pDiskInfo, pDmaBuffer, 16);									/* copy in the results */
	return res;
}

static int32_t custom_dcreate( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	command[4] = GEMDOS_Dcreate;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dcreate(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

static int32_t custom_ddelete( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	command[4] = GEMDOS_Ddelete;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Ddelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

static int32_t custom_fdelete( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	command[4] = GEMDOS_Fdelete;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fdelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

static int32_t custom_dsetpath( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	command[4] = GEMDOS_Dsetpath;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetpath(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

static int32_t custom_dgetpath( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *buffer	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	command[4] = GEMDOS_Dgetpath;										/* store GEMDOS function number */
	command[5] = drive;									

	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetpath(buffer, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	strncpy((char *)buffer, (char *)pDmaBuffer, DMA_BUFFER_SIZE);		/* copy in the results */
	return res;
}

static int32_t custom_frename( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	params += 2;														/* skip reserved WORD */
	char *oldName	= (char *)	*((DWORD *) params);
	params += 4;
	char *newName	= (char *)	*((DWORD *) params);
	
	command[4] = GEMDOS_Frename;										/* store GEMDOS function number */
	command[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	strncpy((char *) pDmaBuffer, oldName, (DMA_BUFFER_SIZE / 2) - 2);	/* copy in the old name	*/
	
	int oldLen = strlen((char *) pDmaBuffer);							/* get the length of old name */
	
	char *pDmaNewName = ((char *) pDmaBuffer) + oldLen + 1;
	strncpy(pDmaNewName, newName, (DMA_BUFFER_SIZE / 2) - 2);			/* copy in the new name	*/

	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Frename(0, oldName, newName);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

static int32_t custom_fattrib( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD flag		= (WORD)	*((WORD *)  params);
	params += 2;
	WORD attr		= (WORD)	*((WORD *)  params);
	
	command[4] = GEMDOS_Fattrib;										/* store GEMDOS function number */
	command[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	
	pDmaBuffer[0] = (BYTE) flag;										/* store set / get flag */
	pDmaBuffer[1] = (BYTE) attr;										/* store attributes */
	
	strncpy(((char *) pDmaBuffer) + 2, fileName, DMA_BUFFER_SIZE -1 );	/* copy in the file name */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fattrib(fileName, flag, attr);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

/* **************************************************************** */
/* those next functions are used for file / dir search */

static int32_t custom_fsetdta( void *sp )
{
	pDta = (_DTA *)	*((DWORD *) sp);									/* store the new DTA pointer */

	useOldHandler = 1;													/* call the old handler */
	Fsetdta(pDta);
	useOldHandler = 0;

	// TODO: on application start set the pointer to the default position (somewhere before the app args)
	
	return 0;
}

static int32_t custom_fsfirst( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsnext( void *sp )
{
	/* not handled */
	return 0;
}

/* **************************************************************** */
/* the following functions work with files and file handles */

static int32_t custom_fcreate( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fopen( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fclose( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fread( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fwrite( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fseek( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fdatime( void *sp )
{
	/* not handled */
	return 0;
}

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;

	/* write some header out */
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx disk driver ]\r\n[    by Jookie 2013    ]\33q\r\n\r\n");

	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	switchToSuper = TRUE;
	
	/* search for CosmosEx on ACSI bus */ 
	found = ce_findId();

	if(!found) {								/* not found? quit */
		sleep(1);
//		return 0;
	}
	
	/* tell the device to initialize */
	ce_initialize();							
	
	pDta = (_DTA *) &tempDta[0];				/* use this buffer as temporary one for DTA - just in case */
	
	/* ----------------------------------------- */
	/* fill the table with pointers to functions */
	gemdos_table[0x0e] = custom_dsetdrv;
	gemdos_table[0x19] = custom_dgetdrv;
	gemdos_table[0x1a] = custom_fsetdta;
	gemdos_table[0x36] = custom_dfree;
	gemdos_table[0x39] = custom_dcreate;
	gemdos_table[0x3a] = custom_ddelete;
	gemdos_table[0x3b] = custom_dsetpath;
	gemdos_table[0x3c] = custom_fcreate;
	gemdos_table[0x3d] = custom_fopen;
	gemdos_table[0x3e] = custom_fclose;
	gemdos_table[0x3f] = custom_fread;
	gemdos_table[0x40] = custom_fwrite;
	gemdos_table[0x41] = custom_fdelete;
	gemdos_table[0x42] = custom_fseek;
	gemdos_table[0x43] = custom_fattrib;
	gemdos_table[0x47] = custom_dgetpath;
	gemdos_table[0x4e] = custom_fsfirst;
	gemdos_table[0x4f] = custom_fsnext;
	gemdos_table[0x56] = custom_frename;
	gemdos_table[0x57] = custom_fdatime;

	/* either remove the old one or do nothing, old memory isn't released */
	if( unhook_xbra( VEC_GEMDOS, 'CEDD' ) == 0L ) {
		(void)Cconws( "\r\nDriver installed.\r\n" );
	} else {
		(void)Cconws( "\r\nDriver reinstalled, some memory was lost.\r\n" );
	}

	/* and now place the new gemdos handler */
	old_gemdos_handler = Setexc( VEC_GEMDOS, gemdos_handler );

	switchToSuper = FALSE;

	/* wait for a while so the user could read the message and quit */
	sleep(1);

	/* now terminate and stay resident */
	Ptermres( 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 );

	return 0;		/* make compiler happy, we wont return */
}

/* this function scans the ACSI bus for any active CosmosEx translated drive */
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	deviceID = 0;
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		res = ce_identify(i);      					/* try to read the IDENTITY string */
		
		if(res == 1) {                           	/* if found the CosmosEx */
			deviceID = i;                     		/* store the ACSI ID of device */

			(void) Cconws("\r\nCosmosEx found on ACSI ID: ");
			bfr[0] = i + '0';
			(void) Cconws(bfr);

			return 1;
		}
	}

	/* if not found */
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}

/* send an IDENTIFY command to specified ACSI ID and check if the result is as expected */
BYTE ce_identify(BYTE ACSI_id)
{
	WORD res;
  
	command[0] = (ACSI_id << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	command[4] = TRAN_CMD_IDENTIFY;
  
	memset(pDmaBuffer, 0, 512);              		/* clear the buffer */

	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);	/* issue the command and check the result */

	if(res != OK) {                        			/* if failed, return FALSE */
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
		return 0;
	}
	
	return 1;                             			/* success */
}

/* send INITIALIZE command to the CosmosEx device telling it to do all the stuff it needs at start */
void ce_initialize(void)
{
	command[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	command[4] = GD_CUSTOM_initialize;
  
	acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);			/* issue the command and check the result */
}

