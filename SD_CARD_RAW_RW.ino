#include <SPI.h>

#define csPin     10
#define mosiPin   11
#define misoPin   12
#define sckPin    13

#define GO_IDLE_STATE 0x01
#define TYPE_SD 0x01
#define TYPE_MMC 0x05
#define INIT_OK 0x00
#define DATA_TOKEN 0xFE
#define WRITE_OK 0xE5
#define CARD_WAIT 0xFF
#define SYNC_BYTE 0xFF
#define CMD0 0
#define CMD0_ARG 0x00000000
#define CMD0_CRC 0x95
#define CMD8 8
#define CMD8_ARG 0x000001AA
#define CMD8_CRC 0x87
#define CMD12 12
#define CMD12_ARG 0x00000000
#define CMD12_CRC 0x00
#define CMD17 17
#define CMD17_CRC 0x00
#define CMD24 24
#define CMD24_CRC 0x00
#define CMD41 41
#define CMD41_ARG 0x40000000
#define CMD41_CRC 0xFF
#define CMD55 55
#define CMD55_ARG 0x00000000
#define CMD55_CRC 0xFF
  
void setup() 
{
  const char *dataToWrite = "The quick brown fox jumps over the lazy dog";
  int dataLen = strlen(dataToWrite);
  byte mByte, *rxdBytes, nArr[dataLen];
  long sectorAddress = 0x00000000;
  
  initHost();                               
  syncToDevice();
  
  if (goIdleState() == GO_IDLE_STATE)
    Serial.println("Card reset: OK");
  else
  {
    Serial.println("Card reset: ERROR");
    while (1);
  }

  if (sendIfCond() == TYPE_SD)
    Serial.println("Card version: 2\nCard type: SDHC");
  else if (sendIfCond() == TYPE_MMC)
    Serial.println("Card version: 1\nCard type: MMC");
  else
  {
    Serial.println("Version error");
    while (1);
  }

  while ((mByte = sdSendOpCond()) != INIT_OK);
  Serial.println("Card init.: OK\n");

  if (writeSingleBlock(sectorAddress, dataToWrite, dataLen) == WRITE_OK)
    Serial.println("Write: OK");
  else
    Serial.println("Write: ERROR");

  rxdBytes = readSingleBlock(sectorAddress, dataLen, nArr);
  if (*rxdBytes == DATA_TOKEN)
  {
    Serial.println("Read: OK\n\nData (char): ");
    for (int k = 1; k < dataLen + 1; k++)
      Serial.print((char)*(rxdBytes + k));

    Serial.println("\n\nData (hex): ");
    for (int k = 1; k < dataLen + 1; k++)
    {
      Serial.print(*(rxdBytes + k));
      Serial.print(" ");
    }
  }
  else
    Serial.println("Read: ERROR");
}

void loop() 
{
  // do something here
}

void initHost()
{
  Serial.begin(115200);
  SPI.begin();
  pinMode(csPin, OUTPUT);                                 
  pinMode(mosiPin, OUTPUT);                                
  pinMode(misoPin, INPUT);                         
  pinMode(sckPin, OUTPUT);
}

byte sendToDevice(byte mByte)
{
  byte deviceEcho;
  digitalWrite(csPin, HIGH);
  digitalWrite(csPin, LOW);
  deviceEcho = SPI.transfer(mByte);
  return deviceEcho;
}

void syncToDevice()
{
  digitalWrite(csPin, LOW);
  for (int k = 0; k < 10; k++)
    sendToDevice(SYNC_BYTE);
}

byte goIdleState()
{
  byte mByte;
  csPinEnable();
  sendProtocol(CMD0, CMD0_ARG, CMD0_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT);
  csPinDisable();
  return mByte;
}

void csPinEnable()
{
  sendToDevice(SYNC_BYTE);
  digitalWrite(csPin, HIGH);
  sendToDevice(SYNC_BYTE);
}

void csPinDisable()
{
  sendToDevice(SYNC_BYTE);
  digitalWrite(csPin, LOW);
  sendToDevice(SYNC_BYTE);
}

void sendProtocol(byte mByte, long mArg, byte mCrc)
{
  sendToDevice(mByte | 0x40);
  sendToDevice((byte)(mArg >> 24));
  sendToDevice((byte)(mArg >> 16));
  sendToDevice((byte)(mArg >> 8));
  sendToDevice((byte)(mArg));
  sendToDevice(mCrc);
}

byte sendIfCond()
{
  byte mByte;
  csPinEnable();
  sendProtocol(CMD8, CMD8_ARG, CMD8_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT);
  csPinDisable();
  return mByte;
}

byte sdSendOpCond()
{
  byte mByte;
  csPinEnable();
  sendProtocol(55, CMD55_ARG, CMD55_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT); 
  csPinDisable();
    
  csPinEnable();
  sendProtocol(41, CMD41_ARG, CMD41_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT);  
  csPinDisable(); 
  return mByte;
}

byte *readSingleBlock(long mAddr, int mSize, byte mArr[])
{
  byte mByte;
  int nAttempts = 1563, mAttempts = 0;
  csPinEnable();
  sendProtocol(CMD17, mAddr, CMD17_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT);
  if (mByte != SYNC_BYTE)
  {
    while (++mAttempts != nAttempts)
      if ((mByte = sendToDevice(SYNC_BYTE)) != CARD_WAIT)
        break;

    if (mByte == DATA_TOKEN)
    {
      mArr[0] = mByte;
      for (int k = 0; k < mSize; k++)
        mArr[k + 1] = sendToDevice(SYNC_BYTE);
  
      sendToDevice(SYNC_BYTE);
      sendToDevice(SYNC_BYTE);
      stopReadingBlocks();
    }
  }
  return mArr;
}

void stopReadingBlocks()
{
  byte mByte;
  csPinEnable();
  sendProtocol(CMD12, CMD12_ARG, CMD12_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) != CARD_WAIT);
  csPinDisable();
}

byte writeSingleBlock(long mAddr, char *mData, int mSize)
{
  byte mByte;
  int nAttempts = 3907, mAttempts = 0;
  csPinEnable();
  sendProtocol(CMD24, mAddr, CMD24_CRC);
  while ((mByte = sendToDevice(SYNC_BYTE)) == CARD_WAIT);
  if (mByte == 0x00)
  {
    sendToDevice(DATA_TOKEN);
    for(int k = 0; k < mSize; k++) 
      sendToDevice(*(mData + k));

    while (++mAttempts != nAttempts)
      if ((mByte = sendToDevice(SYNC_BYTE)) != CARD_WAIT)
        break;

    if ((mByte & 0x1F) == 0x05)
    {
      mAttempts = 0;
      while(sendToDevice(SYNC_BYTE) == 0x00)
        if(++mAttempts == nAttempts)
          break;
    }
  }
  csPinDisable();
  return mByte;
}
