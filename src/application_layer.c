// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "macros.h"
#include "auxil.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


#define INFO_SIZE MAX_PAYLOAD_SIZE
#define CONTROL_PACKET_SIZE 100

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    struct timeval tval_before, tval_after, tval_result;

    //Create and setup Link Layer
    LinkLayer Link_Layer;
    strcpy(Link_Layer.serialPort, serialPort);

    if(strcmp(role, "tx")==0) Link_Layer.role = LlTx;
    else if(strcmp(role, "rx")==0) Link_Layer.role = LlRx;

    Link_Layer.baudRate = baudRate;
    Link_Layer.nRetransmissions = nTries;
    Link_Layer.timeout = timeout;

    llopen(Link_Layer);
    gettimeofday(&tval_before, NULL);

    switch (Link_Layer.role)
    {
    //Transmiter
    case LlTx:{

        //open file
        FILE *sentFileFd = fopen(filename, "r");

        //check if file exists
        if (sentFileFd==NULL)
        {
            fprintf(stderr,"Invalid file");
            exit(-1);
        }

        // Get file size in bytes and convert to hex
        fseek(sentFileFd, 0L, SEEK_END);
        unsigned int fileSize = ftell(sentFileFd);
        rewind(sentFileFd);

        //build special start packet
        unsigned char start_control_packet[CONTROL_PACKET_SIZE] = {0};

        int ctrlPacketSize = buildControlPacket(start_control_packet, START, fileSize, filename);
        
        //llwrite special start packet
        llwrite(&start_control_packet, ctrlPacketSize);

        //split file into pieces
        unsigned char data_packet[2*INFO_SIZE+4] = {0};
        unsigned char data[2*INFO_SIZE];
        
        int bytes, dataSize, sequenceNR = 0;
            
        //while not end of file read from file
        while(bytes = fread(data, 1, INFO_SIZE, sentFileFd)){

            // build data packet
            dataSize = buildDataPacket(data_packet, DATA, sequenceNR, bytes);
            memcpy(&data_packet[4], data, bytes);

            // llwrite
            if(llwrite(&data_packet, bytes+dataSize)==-1){fprintf(stderr, "LLwrite error\n"); break;};
            sequenceNR++;
        }


        //build special end packet
        unsigned char end_control_packet[CONTROL_PACKET_SIZE] = {0};

        ctrlPacketSize = buildControlPacket(end_control_packet, END, fileSize, filename);
        
        //llwrite special end packet
        llwrite(&end_control_packet, ctrlPacketSize);

        break;
    }
    //Receiver
    case LlRx:{
    
        //create and open received file
        FILE *fp;
        fp = fopen (filename, "w");

        unsigned char packet[2*INFO_SIZE], data[INFO_SIZE];

        while(TRUE){

            if(llread(&packet)==-1){
                printf("Error\n");
                continue;
            }

            if(packet[0] == DATA){
                unsigned int sequenceNR = packet[1];
                unsigned int L2 = packet[2];
                unsigned int L1 = packet[3];
                unsigned int L2L1 = (256*L2) + L1; 

                //store in file
                fwrite(packet+4, 1, L2L1, fp);
            }
            else if(packet[0] == START){
                fprintf(stderr, "Starting file transmission\n");
                continue;
            }
            else if(packet[0] == END){
                fprintf(stderr, "Ending file transmission\n");
                break;
            }
        
        }

        break;
    }
    default:
        break;
    }
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    printf("Time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
    llclose(0);
}
