#include "auxil.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>

int changeN(int N){
    if(N == 1) return 0;
    else return 1;
}

int buildControlPacket(unsigned char *control_packet, int control, unsigned int fileSize, char *filename){
    
    control_packet[0] = control; // Control field

    control_packet[1] = FILE_SIZE; // T1 Field for fileSize

    unsigned int aux = fileSize, byteCount = 0;
    while(aux != 0){
        aux >>= 8;
        byteCount++;
    }

    unsigned int L1 = byteCount;
    control_packet[2] = L1; // L1 Field for fileSize nr of Bytes

    memcpy(&control_packet[3], &fileSize, L1); // V1 for fileSize, will be stored in little endian order

    control_packet[3+L1] = FILE_NAME; // T2 field for filename

    unsigned int L2 = strlen(filename);
    control_packet[4+L1] = L2; // L2 Field for fileSize nr of Bytes

    memcpy(&control_packet[5+L1], filename, L2); // V2 for fileSize

    return 3 + L1 + 2 + L2; // Size of control Packet
}

int buildDataPacket(unsigned char *data_packet, int control, int sequenceNR, int buf_size){
    
    data_packet[0] = control; // Control field

    data_packet[1] = sequenceNR % 255; // Sequence NR mod 255

    data_packet[2] = buf_size/256; // L2

    data_packet[3] = buf_size % 256; // L1

    return 4;
}

void byteStuff(unsigned char *info, int *size, int *bcc2, int bufSize, unsigned char *buf){

    int i = 0;

    //Byte stuffing and BCC2 calculating
    for(int i = 0; i < bufSize; i++){
        *bcc2 ^= buf[i];
        if(buf[i] == 0x7e){
            info[*size] = 0x7d;
            info[*size+1] = 0x5e;
            *size += 2;
        }
        else if(buf[i] == 0x7d){
            info[*size] = 0x7d;
            info[*size+1] = 0x5d;
            *size += 2;
        }
        else{
            info[*size] = buf[i];
            *size += 1;
        }
    }

    //BCC2
    if(*bcc2 == 0x7e){
        info[*size] = 0x7d;
        info[*size + 1] = 0x5e;
        info[*size + 2] = FLAG; //Flag
        *size += 3;
    }
    else if(*bcc2 == 0x7d){
        info[*size] = 0x7d;       
        info[*size + 1] = 0x5d; 
        info[*size + 2] = FLAG; //Flag
        *size += 3;
    }
    else{
        info[*size] = *bcc2;
        info[*size + 1] = FLAG; //Flag
        *size += 2;
    }
}

int byteDestuff(unsigned char *packet, int *size, unsigned char *buf){

    int escape = 0, j = 0;
    *size-=1;
    int counter = 0;
    
    // Start of packet to BCC2 field  
    for(int i = 4; i < *size; i++){

        //byte de-stuffing
        //0x7d 0x5e -> 0x7e 
        //0x7d 0x5d -> 0x7d
        if(buf[i] == 0x7d){
            escape = 1;
            counter++;
            continue;
        }

        if(escape){
            if(buf[i] == 0x5e){
                escape = 0;
                packet[j] = 0x7e;
            }
            else if(buf[i] == 0x5d){
                escape = 0;
                packet[j] = 0x7d;
            }
        }
        else{
            packet[j] = buf[i];
        }

        j++;
    }
    *size-=4;
    *size -= counter;
}

int calculateBCC2(unsigned char *packet, int size){

    int bcc2 = 0;

    for(int i = 0; i < size-1; i++){
        bcc2 ^= packet[i];
    }

    return bcc2;
}