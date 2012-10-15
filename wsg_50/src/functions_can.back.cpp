//======================================================================
/**
 *  @file
 *  functions_can.c
 *
 *  @section functions_can.c_general General file information
 *
 *  @brief
 *  
 *
 *  @author Marc Benetó
 *  @date   14.09.2012
 *  
 *  
 *  @section functions_can.c_copyright Copyright
 *  
 *  Copyright 2012 Robotnik Automation, SLL
 *  
 *  The distribution of this code and excerpts thereof, neither in 
 *  source nor in any binary form, is prohibited, except you have our 
 *  explicit and written permission to do so.
 *
 */
//======================================================================



//------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> 

#include "wsg_50/functions_can.h"
#include "wsg_50/checksum.h"
#include "wsg_50/common.h"
#include "wsg_50/msg.h"
#include "wsg_50/aux.h"

//------------------------------------------------------------------------
// Support functions
//------------------------------------------------------------------------

float convert(unsigned char *b){
	float tmp;
  	unsigned int src = 0;

	/*
	dbgPrint("b[3]=%x\n", b[3]);
	dbgPrint("b[2]=%x\n", b[2]);
	dbgPrint("b[1]=%x\n", b[1]);
	dbgPrint("b[0]=%x\n", b[0]);
	*/

  	src = b[3] * 16777216 + b[2] * 65536 + b[1] * 256 + b[0];

  	memcpy(&tmp, &src, sizeof tmp);
  	//printf("Converted value: %f \n", tmp);

	return tmp;
}

float readCANbus( char id ){
	
	int i = 0;
	TPCANRdMsg tpcmsg_read;
	bool answer = false;
	unsigned char vResult[4];
    
    
	while(i < 10){
		
		int iRet = LINUX_CAN_Read(h, &tpcmsg_read);
		
		if (iRet == CAN_ERR_OK)  // Read OK
		{
			if (tpcmsg_read.Msg.MSGTYPE == MSGTYPE_STANDARD) {
				//return OK;
				//printf("Read OK\n");
				
				if (answer == true){
					vResult[0] = tpcmsg_read.Msg.DATA[0];
					vResult[1] = tpcmsg_read.Msg.DATA[1];
					vResult[2] = tpcmsg_read.Msg.DATA[2];
					vResult[3] = tpcmsg_read.Msg.DATA[3];
					
					return convert(vResult);
				}
				
				if (tpcmsg_read.Msg.DATA[3] == id && answer == false){
					//for (int i = 4; i < tpcmsg_read.Msg.LEN; i++){
					//	printf("tpcmsg_read.Msg.DATA[%d]: 0x%x\n", i, tpcmsg_read.Msg.DATA[i]);
					//}
					// With this we point that the answer will be available in the next read.
					answer = true; 
				}
				
			}else {
				//return NOT_ERROR;
				//printf("Read OK, no standard message\n");
			}
		}
		else if(tpcmsg_read.Msg.MSGTYPE == MSGTYPE_STATUS)
		{  
			if (iRet < 0) {
				int last_err = nGetLastError();
				printf("ERROR READING => Last Error: %d\n", last_err);  
			}
		}
         
		i++;

	}	
	
	return 0;
	
}


//------------------------------------------------------------------------
// Function implementation
//------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Note: Argument values that are outside the gripper’s physical limits are clamped to the highest/lowest available value. //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////
// BUS MANAGEMENT FUNCTIONS //
//////////////////////////////

bool CAN_connect( void ){
	
   const char *szDevNode = DEFAULT_NODE;
   char txt[64];
   int errno;

   printf("Openning PEAK CAN device...\n");

   h = LINUX_CAN_Open(szDevNode, O_RDWR | O_NONBLOCK);
   
   if(h){
   
		CAN_VersionInfo(h, txt);   

		printf("CAN connection established. PEAK USB Card Info: %s\n", txt);
	
		errno = CAN_Init(h, CAN_BAUD_500K, CAN_INIT_TYPE_ST); // _ST:standard frames, _EX:extended frames 
		
		return true;
		
	}else{
		
		return false;
		
	}
	
}

void CAN_disconnect( void ){
	
	CAN_Close(h);
	
}



/////////////////////////
// ACTUATION FUNCTIONS //
/////////////////////////


void homing( void )
{
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x20;
	header[4] = 0x01;
	header[5] = 0x00;


	data[0] = 0x00;								// DATA  0x00 => Normal homing. 0x02 => Inverted homing.

	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( data, 1, crc );
	

	// Homing message (1st part)
    msg.ID = can_id;
    msg.LEN = 0x08;         // 8 data byte
    msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
	msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= data[0];		// DATA[0]
    msg.DATA[7]= (unsigned char) lo(crc); // CRC[0]

	CAN_Write(h, &msg);

	// Homing message (2nd part)
		
    msg.ID = can_id;
    msg.LEN = 0x01;         // 1 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= (unsigned char) hi(crc); // CRC[1]
    msg.DATA[1]= 0;
    msg.DATA[2]= 0;
    msg.DATA[3]= 0;
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;

	CAN_Write(h, &msg);
	
}


void move( float width, float speed )
{

	// Perform a simple movement
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x21;
	header[4] = 0x09;
	header[5] = 0x00;

	unsigned char dataMove[9];
	dataMove[0] = 0x00;
						
	memcpy( &dataMove[1], &width, sizeof( float ) );
	memcpy( &dataMove[5], &speed, sizeof( float ) );
	
	//for (int i = 0; i<9; i++){	
	//	printf("dataMove[%d]: 0x%x\n", i, dataMove[i]); 
	//}
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 9, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]

	CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];	// DATA[2]
    msg.DATA[1]= dataMove[3];	// DATA[3]
    msg.DATA[2]= dataMove[4];	// DATA[4]
    msg.DATA[3]= dataMove[5];	// DATA[5]
    msg.DATA[4]= dataMove[6];	// DATA[6]
    msg.DATA[5]= dataMove[7];	// DATA[7]
    msg.DATA[6]= dataMove[8];	// DATA[8]
    msg.DATA[7]= (unsigned char) lo(crc); // CRC[0]
    
    CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x01;         // 1 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= (unsigned char) hi(crc);	// CRC[1]
    msg.DATA[1]= 0;
    msg.DATA[2]= 0;
    msg.DATA[3]= 0;
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;
 
    CAN_Write(h, &msg);

}


/*
int stop( void )
{
	status_t status;
	int res;
	unsigned char payload[1];
	unsigned char *resp;
	unsigned int resp_len;

	//payload[0] = 0x00;

	// Submit command and wait for response. Push result to stack.
	res = cmd_submit( 0x22, payload, 0, true, &resp, &resp_len );
	if ( res != 2 )
	{
		dbgPrint( "Response payload length doesn't match (is %d, expected 2)\n", res );
		if ( res > 0 ) free( resp );
		return 0;
	}

	// Check response status
	status = cmd_get_response_status( resp );
	free( resp );
	if ( status != E_SUCCESS )
	{
		dbgPrint( "Command STOP not successful: %s\n", status_to_str( status ) );
		return -1;
	}

	return 0;
}


int ack_fault( void )
{
	status_t status;
	int res;
	unsigned char payload[3];
	unsigned char *resp;
	unsigned int resp_len;

	payload[0] = 0x61;  //MBJ: Està ben enviat, si es posa alrevés no torna error en terminal però si que es posa roig el LED
	payload[1] = 0x63;
	payload[2] = 0x6B;

	// Submit command and wait for response. Push result to stack.
	res = cmd_submit( 0x24, payload, 3, true, &resp, &resp_len );
	if ( res != 2 )
	{
		dbgPrint( "Response payload length doesn't match (is %d, expected 2)\n", res );
		if ( res > 0 ) free( resp );
		return 0;
	}


	// Check response status
	status = cmd_get_response_status( resp );
	free( resp );
	if ( status != E_SUCCESS )
	{
		dbgPrint( "Command ACK not successful: %s\n", status_to_str( status ) );
		return -1;
	}

	return 0;
}
*/

void grasp( float objWidth, float speed )
{
	// Perform a simple grasping
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x25;
	header[4] = 0x08;
	header[5] = 0x00;

	unsigned char dataMove[8];
	dataMove[0] = 0x00;
						
	memcpy( &dataMove[0], &objWidth, sizeof( float ) );
	memcpy( &dataMove[4], &speed, sizeof( float ) );
	
	//for (int i = 0; i<9; i++){	
	//	printf("dataMove[%d]: 0x%x\n", i, dataMove[i]); 
	//}
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 8, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]

	CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];	// DATA[2]
    msg.DATA[1]= dataMove[3];	// DATA[3]
    msg.DATA[2]= dataMove[4];	// DATA[4]
    msg.DATA[3]= dataMove[5];	// DATA[5]
    msg.DATA[4]= dataMove[6];	// DATA[6]
    msg.DATA[5]= dataMove[7];	// DATA[7]
    msg.DATA[6]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[7]= (unsigned char) hi(crc);	// CRC[1]
    
    CAN_Write(h, &msg);
    
    return 0;
}


void release( float width, float speed )
{
	// Perform a simple releasing
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x26;
	header[4] = 0x08;
	header[5] = 0x00;

	unsigned char dataMove[8];
	dataMove[0] = 0x00;
						
	memcpy( &dataMove[0], &width, sizeof( float ) );
	memcpy( &dataMove[4], &speed, sizeof( float ) );
	
	//for (int i = 0; i<9; i++){	
	//	printf("dataMove[%d]: 0x%x\n", i, dataMove[i]); 
	//}
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 8, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]

	CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];	// DATA[2]
    msg.DATA[1]= dataMove[3];	// DATA[3]
    msg.DATA[2]= dataMove[4];	// DATA[4]
    msg.DATA[3]= dataMove[5];	// DATA[5]
    msg.DATA[4]= dataMove[6];	// DATA[6]
    msg.DATA[5]= dataMove[7];	// DATA[7]
    msg.DATA[6]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[7]= (unsigned char) hi(crc);	// CRC[1]
    
    CAN_Write(h, &msg);
    
    return 0;
}


///////////////////
// SET FUNCTIONS //
///////////////////

void setAcceleration( float acc )
{
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x30;
	header[4] = 0x04;
	header[5] = 0x00;

	unsigned char dataMove[4];

	// Copy desired acceleration
	memcpy( &dataMove[0], &acc, sizeof( float ) );
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 4, crc );

    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]
    
    CAN_Write(h, &msg);
    
    msg.ID = can_id;
	msg.LEN = 0x04;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];		// PREAMBLE[0]
    msg.DATA[1]= dataMove[3];		// PREAMBLE[1]
    msg.DATA[2]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[3]= (unsigned char) hi(crc); // CRC[0]
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;
    
    CAN_Write(h, &msg);
    
    return 0;
}

void setGraspingForceLimit( float force )
{
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x32;
	header[4] = 0x04;
	header[5] = 0x00;

	unsigned char dataMove[4];

	// Copy desired grasping force limit
	memcpy( &dataMove[0], &force, sizeof( float ) );
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 4, crc );

    msg.ID = can_id;
	msg.LEN = 0x08;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]
    
    CAN_Write(h, &msg);
    
    msg.ID = can_id;
	msg.LEN = 0x04;         // 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];		// PREAMBLE[0]
    msg.DATA[1]= dataMove[3];		// PREAMBLE[1]
    msg.DATA[2]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[3]= (unsigned char) hi(crc); // CRC[0]
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;
    
    CAN_Write(h, &msg);
    
    return 0;
}


///////////////////
// GET FUNCTIONS //
///////////////////

/*
const char * systemState( void ) 
{
	// Get the system state
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x40;
	header[4] = 0x03;
	header[5] = 0x00;

	unsigned char dataMove[3];
	dataMove[0] = 0x00;
	dataMove[1] = 0x00;	
	dataMove[2] = 0x00;
	
	//for (int i = 0; i<9; i++){	
	//	printf("dataMove[%d]: 0x%x\n", i, dataMove[i]); 
	//}
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 3, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         	// 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]

	CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x03;         // 3 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];	// DATA[2]
    msg.DATA[1]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[2]= (unsigned char) hi(crc);	// CRC[1]
    msg.DATA[3]= 0;
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;
                    
    CAN_Write(h, &msg);
    
    int i = 0;
	TPCANRdMsg tpcmsg_read;
	bool answer = false;
	unsigned char vResult[4];
    
    
	while(i < 10){
		
		int iRet = LINUX_CAN_Read(h, &tpcmsg_read);
		
		if (iRet == CAN_ERR_OK)  // Read OK
		{
			if (tpcmsg_read.Msg.MSGTYPE == MSGTYPE_STANDARD) {
				//return OK;
				//printf("Read OK\n");
				
				if (answer == true){
					vResult[0] = tpcmsg_read.Msg.DATA[0];
					vResult[1] = tpcmsg_read.Msg.DATA[1];
					vResult[2] = tpcmsg_read.Msg.DATA[2];
					vResult[3] = tpcmsg_read.Msg.DATA[3];
					
					//return convert(vResult);
					return getStateValues(vResult);
				}
				
				if (tpcmsg_read.Msg.DATA[3] == 0x43 && answer == false){
					//for (int i = 4; i < tpcmsg_read.Msg.LEN; i++){
					//	printf("tpcmsg_read.Msg.DATA[%d]: 0x%x\n", i, tpcmsg_read.Msg.DATA[i]);
					//}
					answer = true; // Marca que la resposta vindrà en la seguent lectura
				}
				
			}else {
				//return NOT_ERROR;
				//printf("Read OK, no standard message\n");
			}
		}
		else if(tpcmsg_read.Msg.MSGTYPE == MSGTYPE_STATUS)
		{  
			if (iRet < 0) {
				int last_err = nGetLastError();
				printf("Error last_err: %d\n", last_err);  
			}
		}
         
		i++;

	}
	
	return 0;
	
}
*/
/*
int graspingState( void )
{
	status_t status;
	int res;
	unsigned char payload[3];
	unsigned char *resp;
	unsigned int resp_len;

	// Don't use automatic update, so the payload bytes are 0.
	memset( payload, 0, 3 );

	// Submit command and wait for response. Expecting exactly 4 bytes response payload.
	res = cmd_submit( 0x41, payload, 3, false, &resp, &resp_len );
	if ( res != 3 )
	{
		dbgPrint( "Response payload length doesn't match (is %d, expected 3)\n", res );
		if ( res > 0 ) free( resp );
		return 0;
	}

	// Check response status
	status = cmd_get_response_status( resp );
	if ( status != E_SUCCESS )
	{
		dbgPrint( "Command GET GRASPING STATE not successful: %s\n", status_to_str( status ) );
		free( resp );
		return 0;
	}

	free( resp );

	dbgPrint("GRASPING STATUS: %s\n", status_to_str (status) );

	return (int) resp[2];
}
*/

void getOpening( void )  
{
	// Get the gripper openning
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x43;
	header[4] = 0x03;
	header[5] = 0x00;

	unsigned char dataMove[3];
	dataMove[0] = 0x00;
	dataMove[1] = 0x00;	
	dataMove[2] = 0x00;
	
	//for (int i = 0; i<9; i++){	
	//	printf("dataMove[%d]: 0x%x\n", i, dataMove[i]); 
	//}
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 3, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         	// 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= dataMove[0];	// DATA[0]
    msg.DATA[7]= dataMove[1];	// DATA[1]

	CAN_Write(h, &msg);

    msg.ID = can_id;
	msg.LEN = 0x03;         // 3 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= dataMove[2];	// DATA[2]
    msg.DATA[1]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[2]= (unsigned char) hi(crc);	// CRC[1]
    msg.DATA[3]= 0;
    msg.DATA[4]= 0;
    msg.DATA[5]= 0;
    msg.DATA[6]= 0;
    msg.DATA[7]= 0;
                    
    CAN_Write(h, &msg);
    
    return readCANbus(0x43);
}


void getGraspingForceLimit( void )  
{
	// Get the gripper force
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x33;
	header[4] = 0x00;
	header[5] = 0x00;

	unsigned char dataMove[1];
	//dataMove[0] = 0x00;
	
	// Don't use automatic update, so the payload bytes are 0.
	memset( dataMove, 0, 1 );
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 0, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         	// 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[7]= (unsigned char) hi(crc); // CRC[1]

	CAN_Write(h, &msg);
    
    return readCANbus(0x33);
}


void getAcceleration( void )  
{
	// Get the gripper acceleration
	
	header[0] = 0xAA;
	header[1] = 0xAA;
	header[2] = 0xAA;
	header[3] = 0x31;
	header[4] = 0x00;
	header[5] = 0x00;

	unsigned char dataMove[1];
	dataMove[0] = 0x00;
	
	crc = checksum_crc16( header, 6 );
	crc = checksum_update_crc16( dataMove, 0, crc );

	
    msg.ID = can_id;
	msg.LEN = 0x08;         	// 8 data byte
	msg.MSGTYPE = MSGTYPE_STANDARD;
    msg.DATA[0]= header[0];		// PREAMBLE[0]
    msg.DATA[1]= header[1];		// PREAMBLE[1]
    msg.DATA[2]= header[2];		// PREAMBLE[2]
    msg.DATA[3]= header[3];		// ID MESSAGE
    msg.DATA[4]= header[4];		// SIZE PAYLOAD [0]
    msg.DATA[5]= header[5];		// SIZE PAYLOAD [1]
    msg.DATA[6]= (unsigned char) lo(crc); // CRC[0]
    msg.DATA[7]= (unsigned char) hi(crc);	// CRC[1]

	CAN_Write(h, &msg);
    
    return readCANbus(0x31);
}

//------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------