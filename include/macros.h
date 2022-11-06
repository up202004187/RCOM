/*
    Global
*/
#define FLAG 0x7E //delimiter flag
#define AddrTR 0x03 //commands sent by the Transmitter and Replies sent by the Receiver
#define AddrRT 0x01 //commands sent by the Receiver and Replies sent by the Transmiter

/*
    Supervision (S) and Unnumbered (U) Frames
*/
#define SET 0x03 //control field = set up
#define DISC 0x0B //control field = disconnect
#define UA 0x07 //control field = unnumbered acknowledgment
#define PosACK(Nr) (Nr<<7)|0x05 //control field = positive acknowledgement
#define NegACK(Nr) (Nr<<7)|0x01 //control field = negative acknowledgement

/*
    Information (I) Frames
*/
#define CONTROL(Ns) (Ns<<6) //information frame 0 or 1, is either 0x00 or 0x40

/*
    Packets
*/
#define DATA 0x01 //data control field
#define START 0x02 //start control field
#define END 0x03 //end control field
#define FILE_SIZE 0x00 //T for file size
#define FILE_NAME 0x01 //T for file name

//slide 13
//if Di = 0x7E -> 0x7D 0x5E
//if Di = 0x7D -> 0x7D 0x5D