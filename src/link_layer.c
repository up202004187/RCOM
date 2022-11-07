// Link layer protocol implementation

#include "link_layer.h"
#include "state_machines.h"
#include "macros.h"
#include "auxil.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include<unistd.h>

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0; Ns = 0; Nr = 1;
int ti = 0;



// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 256

int fd;
struct termios oldtio;
struct termios newtio;
LinkLayer connectionParameters2;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

//Resets alarm counter and disables alarm
void resetAlarm(){
    alarmEnabled = FALSE;
    alarmCount = 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    connectionParameters2 = connectionParameters;
    const char *serialPortName = connectionParameters.serialPort;

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    // TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    printf("Establishing connection\n");

    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

    switch (connectionParameters.role){
    //Receiver
    case LlRx:{
        /* Read até SET estar feito */
        state_machine stateMachineState = Start;
        while (stateMachineState != STOP)
        {
            //read 1 byte at a time
            if(read(fd, buf, 1)==0){
                continue;
            }
            // Set state machine
            switch (stateMachineState)
            {
            case Start:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case FLAG_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==AddrTR){
                    stateMachineState = A_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case A_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==SET){
                    stateMachineState = C_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case C_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==(AddrTR ^ SET)){
                    stateMachineState = BCC_OK;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case BCC_OK:
                if(buf[0]==FLAG){
                    stateMachineState = STOP;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            default:
                break;
            }
        }

        /* Montar UA */
        char ua[BUF_SIZE] = {0};
        ua[0] = FLAG;
        ua[1] = AddrTR;
        ua[2] = UA;
        ua[3] = AddrTR ^ UA;
        ua[4] = FLAG;

        /* Enviar UA */
        sleep(ti);
        write(fd, ua, 5);
        break;
    }
    //Transmiter
    case LlTx:{
        
        /* Setup alarm */
        (void)signal(SIGALRM, alarmHandler);

        /* Montar Set */
        char set[BUF_SIZE] = {0};
        set[0] = FLAG;
        set[1] = AddrTR;
        set[2] = SET;
        set[3] = AddrTR ^ SET;
        set[4] = FLAG;
        
        int nr_retrans = 0;

        while(nr_retrans < connectionParameters.nRetransmissions){

            alarm(connectionParameters.timeout); // Set alarm to be triggered in defined time
            
            /* Enviar Set */
            sleep(ti);
            write(fd, set, 5);

            alarmEnabled = TRUE;

            /* Read até UA estar feito e repetir até ter UA ou nr max de retransmissões excedido */
            state_machine stateMachineState = Start;

            while (stateMachineState != STOP && alarmEnabled){   
                //read 1 byte at a time
                if(read(fd, buf, 1)==0){
                    continue;
                }

                switch (stateMachineState)
                {
                case Start:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case FLAG_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==AddrTR){
                        stateMachineState = A_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case A_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==UA){
                        stateMachineState = C_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case C_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==(AddrTR ^ UA)){
                        stateMachineState = BCC_OK;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case BCC_OK:
                    if(buf[0]==FLAG){
                        stateMachineState = STOP;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                default:
                    break;
                }
            }
            if(stateMachineState == STOP){
                resetAlarm();
                break;
            }
            nr_retrans++;
        }
        if(nr_retrans==connectionParameters.nRetransmissions){
            // Restore the old port settings
            if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
            {
                perror("tcsetattr");
                exit(-1);
            }

            close(fd);
            exit(-1);
        }
        break;
    }
    default:
        break;
    }
    printf("Connection established\n");
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{   
    resetAlarm();

    int NR = changeN(Ns);

    /* Montar IFrame */
    unsigned char info[bufSize*2 + 6]; // Assuming all bytes need stuffing
    info[0] = FLAG; //Flag
    info[1] = AddrTR; //Address
    info[2] = CONTROL(Ns); //Control
    info[3] = info[1] ^ info[2]; //BCC1

    int nr_retrans = 0, size = 4; int bcc2 = 0;

    unsigned char buf2[5];
    
    byteStuff(&info, &size, &bcc2, bufSize, buf);
    
    while(nr_retrans < connectionParameters2.nRetransmissions){

        alarm(connectionParameters2.timeout);

        /* Enviar IFrame */
        sleep(ti);
        write(fd, info, size);
        
        alarmEnabled = TRUE;

        state_machine stateMachineState = Start;
        while (stateMachineState != STOP && alarmEnabled){  

            //read 1 byte at a time
            if(read(fd, buf2, 1)==0){
                continue;
            }

            switch (stateMachineState)
            {
            case Start:
                if(buf2[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case FLAG_RCV:
                if(buf2[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf2[0]==AddrTR){
                    stateMachineState = A_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case A_RCV:
                if(buf2[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf2[0]==PosACK(NR)){
                    stateMachineState = C_RCV;
                }
                else if(buf2[0]==NegACK(NR)){
                    stateMachineState = STOP;
                    if(read(fd, buf2, 1)==0){
                        continue;
                    }
                    if(read(fd, buf2, 1)==0){
                        continue;
                    }
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case C_RCV:
                if(buf2[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf2[0]==(AddrTR ^ PosACK(NR))){
                    stateMachineState = BCC_OK;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case BCC_OK:
                if(buf2[0]==FLAG){
                    stateMachineState = STOP;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            default:
                break;
            }
        }
        if(stateMachineState == STOP){
            resetAlarm();
            break;
        }
        nr_retrans++;
    }
    if(nr_retrans==connectionParameters2.nRetransmissions){
        // Restore the old port settings
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        close(fd);
        exit(-1);
    }
    
    Ns= (Ns+1)%2;
    return size;

}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   

    unsigned char buf[2*MAX_PAYLOAD_SIZE+6];
    int size = 0;

    int NS = changeN(Nr);

    state_machine stateMachineState = Start;
    while (stateMachineState != STOP)
    {
        //read 1 byte at a time
        if(read(fd, buf+size, 1) <= 0){
            continue;
        }

        switch (stateMachineState)
        {
        case Start:
            if(buf[size]==FLAG){
                stateMachineState = FLAG_RCV;
                size++;
            }
            else{
                stateMachineState = Start;
            }
            break;
        case FLAG_RCV:
            if(buf[size]==FLAG){
                stateMachineState = FLAG_RCV;
            }
            else if(buf[size]==AddrTR){
                stateMachineState = A_RCV;
                size++;
            }
            else{
                stateMachineState = Start;
            }
            break;
        case A_RCV:
            if(buf[size]==FLAG){
                stateMachineState = FLAG_RCV;
            }
            else if(buf[size]==CONTROL(NS)){
                stateMachineState = C_RCV;
                size++;
            }
            else{
                stateMachineState = Start;
            }
            break;
        case C_RCV:
            if(buf[size]==FLAG){
                stateMachineState = FLAG_RCV;
            }
            else if(buf[size]==(AddrTR ^ CONTROL(NS))){
                stateMachineState = BCC_OK;
                size++;
            }
            else{
                stateMachineState = Start;
            }
            break;
        case BCC_OK:
            if(buf[size]==FLAG){
                stateMachineState = STOP;
            }
            size++;
            break;
        default:
            break;
        }
    }

    byteDestuff(packet, &size, &buf);

    int bcc2 = calculateBCC2(packet, size);

    if(bcc2 != packet[size-1]){

        //Montar REJ
        unsigned char REJ[5];
        REJ[0] = FLAG; //Flag
        REJ[1] = AddrTR; //Address
        REJ[2] = NegACK(Nr); //Control
        REJ[3] = AddrTR ^ NegACK(Nr); //BCC1
        REJ[4] = FLAG;

        fprintf(stdout, "Sending REJ\n");
        sleep(ti);
        write(fd, REJ, 5);

        return -1;
    }

    packet[size-1] = '\0';

    //Montar RR
    unsigned char RR[5];
    RR[0] = FLAG; //Flag
    RR[1] = AddrTR; //Address
    RR[2] = PosACK(Nr); //Control
    RR[3] = AddrTR ^ PosACK(Nr); //BCC1
    RR[4] = FLAG;
    
    fprintf(stdout, "Sending RR\n");
    sleep(ti);
    write(fd, RR, 5);

    Nr=(Nr+1)%2;
    
    return size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{   

    printf("Severing connection\n");
    unsigned char buf[BUF_SIZE + 1] = {0};

    switch (connectionParameters2.role){
    //Receiver
    case LlRx:{
        /* Read até DISC estar feito */
        state_machine stateMachineState = Start;
        while (stateMachineState != STOP)
        {
            //read 1 byte at a time
            if(read(fd, buf, 1)==0){
                continue;
            }

            switch (stateMachineState)
            {
            case Start:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case FLAG_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==AddrTR){
                    stateMachineState = A_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case A_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==DISC){
                    stateMachineState = C_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case C_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==(AddrTR ^ DISC)){
                    stateMachineState = BCC_OK;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case BCC_OK:
                if(buf[0]==FLAG){
                    stateMachineState = STOP;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            default:
                break;
            }
        }

        /* Montar DISC */
        char ua[BUF_SIZE] = {0};
        ua[0] = FLAG;
        ua[1] = AddrRT;
        ua[2] = DISC;
        ua[3] = AddrRT ^ DISC;
        ua[4] = FLAG;

        /* Enviar DISC */
        sleep(ti);
        write(fd, ua, 5);

        /* Read até UA estar feito */
        stateMachineState = Start;
        while (stateMachineState != STOP)
        {
            //read 1 byte at a time
            if(read(fd, buf, 1)==0){
                continue;
            }

            switch (stateMachineState)
            {
            case Start:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case FLAG_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==AddrTR){
                    stateMachineState = A_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case A_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==UA){
                    stateMachineState = C_RCV;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case C_RCV:
                if(buf[0]==FLAG){
                    stateMachineState = FLAG_RCV;
                }
                else if(buf[0]==(AddrTR ^ UA)){
                    stateMachineState = BCC_OK;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            case BCC_OK:
                if(buf[0]==FLAG){
                    stateMachineState = STOP;
                }
                else{
                    stateMachineState = Start;
                }
                break;
            default:
                break;
            }
        }

        break;
    }
    //Transmiter
    case LlTx:{

        resetAlarm();

        /* Setup alarm */
        (void)signal(SIGALRM, alarmHandler);
        
        /* Montar DISC */
        char set[BUF_SIZE] = {0};
        set[0] = FLAG;
        set[1] = AddrTR;
        set[2] = DISC;
        set[3] = AddrTR ^ DISC;
        set[4] = FLAG;

        int nr_retrans = 0;

        unsigned char buf[5]={0};

        while(nr_retrans < connectionParameters2.nRetransmissions){
            
            alarm(connectionParameters2.timeout); // Set alarm to be triggered in defined time

            /* Enviar DISC */
            sleep(ti);
            write(fd, set, 5);

            alarmEnabled = TRUE;

            /* Read até DISC estar feito e repetir até ter DISC ou nr max de retransmissões excedido */
            state_machine stateMachineState = Start;

            while (stateMachineState != STOP && alarmEnabled)
            {   
                //read 1 byte at a time
                if(read(fd, buf, 1)==0){
                    continue;
                }

                switch (stateMachineState)
                {
                case Start:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case FLAG_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==AddrRT){
                        stateMachineState = A_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case A_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==DISC){
                        stateMachineState = C_RCV;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case C_RCV:
                    if(buf[0]==FLAG){
                        stateMachineState = FLAG_RCV;
                    }
                    else if(buf[0]==(AddrRT ^ DISC)){
                        stateMachineState = BCC_OK;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                case BCC_OK:
                    if(buf[0]==FLAG){
                        stateMachineState = STOP;
                    }
                    else{
                        stateMachineState = Start;
                    }
                    break;
                default:
                    break;
                }
            }
            if(stateMachineState == STOP){
                resetAlarm();
                break;
            }
            nr_retrans++;
        }
        if(nr_retrans==connectionParameters2.nRetransmissions){
            // Restore the old port settings
            if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
            {
                perror("tcsetattr");
                exit(-1);
            }

            close(fd);
            exit(-1);
        }

       /* Montar UA */
        char ua[BUF_SIZE] = {0};
        ua[0] = FLAG;
        ua[1] = AddrTR;
        ua[2] = UA;
        ua[3] = AddrTR ^ UA;
        ua[4] = FLAG;

        /* Enviar UA */
        sleep(ti);
        write(fd, ua, 5);

        break;
    }
    default:
        break;
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    printf("Connection severed\n");

    return 1;
}
