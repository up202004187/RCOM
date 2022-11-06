int changeN(int N);

int buildControlPacket(unsigned char *control_packet, int control, unsigned int fileSize, char *filename);

int buildDataPacket(unsigned char *data_packet, int control, int sequenceNR, int buf_size);

void byteStuff(unsigned char *info, int *size, int *bcc2, int bufSize, unsigned char *buf);

int byteDestuff(unsigned char *packet, int *size, unsigned char *buf);

int calculateBCC2(unsigned char *packet, int size);