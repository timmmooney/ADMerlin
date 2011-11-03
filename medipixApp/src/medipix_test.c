/**
 * Test program for interface library to medipix Labview system.
 * 
 * Matthew Pearson
 * Nov 2011
 */

#include "medipix_low.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) 
{
  char value[MPX_MAXLINE] = {'\0'};
  char errMsg[MPX_MAXLINE] = {'\0'};
  int conn_flag = 0;
  int status = 0;

  unsigned int data[1000000];

  printf("Test program for medipix_low interface library.\n");

  if ((status = mpxConnect("172.23.244.34", 14000, 14001)) != MPX_OK) {
    printf("ERROR Connecting. status: %d\n", status);
  }
  
  if ((status = mpxIsConnected(&conn_flag)) != MPX_OK) {
    printf("ERROR. status: %d\n", status);
  } else {
    if (conn_flag == 1) {
      printf("We are connected.\n");
    } else {
      printf("We are NOT connected.\n");
    }
  }

  sleep(5);

  if ((status = mpxGet("NUMFRAMESTOACQUIRE", value)) != MPX_OK) {
    printf("ERROR. status: %d\n", status);
  }
  printf("NUMFRAMESTOACQUIRE: %s\n", value);

  

  if ((status = mpxSet("NUMFRAMESTOACQUIRE", "2")) != MPX_OK) {
    printf("ERROR. status: %d\n", status);
  }

  
  if ((status = mpxGet("NUMFRAMESTOACQUIRE", value)) != MPX_OK) {
    printf("ERROR. status: %d\n", status);
  }
  printf("NUMFRAMESTOACQUIRE: %s\n", value);

  sleep(5);

  if ((status = mpxData(data)) != MPX_OK) {
    printf("ERROR reading data. status: %d\n", status);
  }

  if ((status = mpxDisconnect()) != MPX_OK) {
    printf("ERROR Disconnecting . status: %d\n", status);
  }

  if ((status = mpxGet("NUMFRAMESTOACQUIRE", value)) != MPX_OK) {
    printf("ERROR. status: %d\n", status);
    mpxError(status, errMsg);
    printf("ERROR Message: %s\n", errMsg);
  }
  
  sleep(2);

  //if ((status = mpxConnect("172.23.244.34", 14000, 14001)) != MPX_OK) {
  //  printf("ERROR Connecting. status: %d\n", status);
  //}

  //if ((status = mpxIsConnected(&conn_flag)) != MPX_OK) {
  //  printf("ERROR. status: %d\n", status);
  //} else {
  //  if (conn_flag == 1) {
  //    printf("We are connected.\n");
  //  } else {
  //    printf("We are NOT connected.\n");
  //  }
  //}

  //sleep(5);

  //if ((status = mpxDisconnect()) != MPX_OK) {
  //  printf("ERROR Disconnecting . status: %d\n", status);
  //}

  return EXIT_SUCCESS;
}