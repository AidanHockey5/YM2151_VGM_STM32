#include <Arduino.h>
#include "YM2151.h"
#include "LTC6903.h"
#include <SPI.h>
#include <U8g2lib.h>
#include "SdFat.h"
#include "TrackStructs.h"
#include "ringbuffer.h"

//Debug variables
#define DEBUG false //Set this to true for a detailed printout of the header data & any errored command bytes
#define DEBUG_LED PA10
bool commandFailed = false;
uint8_t failedCmd = 0x00;

//Prototypes
void setup();
void loop();
void handleSerialIn();
void tick();
void removeSVI();
void prebufferLoop();
void injectPrebuffer();
void fillBuffer();
bool topUpBuffer(); 
void clearBuffers();
//void handleButtons();
void prepareChips();
void readGD3();
void setISR();
void drawOLEDTrackInfo();
bool startTrack(FileStrategy fileStrategy, String request = "");
bool vgmVerify();
uint8_t readBuffer();
uint16_t readBuffer16();
uint32_t readBuffer32();
uint32_t readSD32();
uint16_t parseVGM();

//Sound Chips
const int prev_btn = PB12;
const int rand_btn = PB13;
const int next_btn = PB14;
const int loop_btn = PB15;
const int shuf_btn = PA8;
int YM_Datapins[8] = {PB8, PB9, PC13, PC14, PC15, PA0, PA1, PA2};
const int YM_CS = PB3;
const int YM_RD = PA15;
const int YM_WR = PA12;
const int YM_A0 = PA11;
const int YM_IC = PA3; 
const int YM_IRQ = NULL;
YM2151 opm(YM_Datapins, YM_CS, YM_RD, YM_WR, YM_A0, YM_IRQ, YM_IC);

//Clock
LTC6903 ltc(PB0);

//SD & File Streaming
SdFat SD;
File file;
#define MAX_FILE_NAME_SIZE 128
char fileName[MAX_FILE_NAME_SIZE];
uint32_t numberOfFiles = 0;
uint32_t currentFileNumber = 0;

//Buffers
#define CMD_BUFFER_SIZE 8192
#define LOOP_PREBUF_SIZE 512
typedef ringbuffer_t<uint8_t, CMD_BUFFER_SIZE, uint8_t> RingBuffer;
static RingBuffer cmdBuffer;
uint8_t loopPreBuffer[LOOP_PREBUF_SIZE];

//Counters
uint32_t bufferPos = 0;
uint32_t cmdPos = 0;
uint16_t waitSamples = 0;

//VGM Variables
uint16_t loopCount = 0;
uint8_t maxLoops = 3;
bool fetching = false;
volatile bool ready = false;
PlayMode playMode = SHUFFLE;

//OLED
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

void setup()
{
  ltc.SetFrequency(3579545); 
  u8g2.begin();
  u8g2.setFont(u8g2_font_fub11_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(0,16,"Aidan Lawrence");
  u8g2.drawStr(0,32,"YM2151, 2018");
  u8g2.sendBuffer();
  delay(500);
  pinMode(prev_btn, INPUT_PULLUP);
  pinMode(rand_btn, INPUT_PULLUP);
  pinMode(next_btn, INPUT_PULLUP);
  pinMode(loop_btn, INPUT_PULLUP);
  pinMode(shuf_btn, INPUT_PULLUP);

  //COM
  Serial.begin(9600);

  //SD
  if(!SD.begin(PA4, SD_SCK_HZ(F_CPU/2)))
  {
    u8g2.clearBuffer();
    u8g2.drawStr(0,16,"SD Mount");
    u8g2.drawStr(0,32,"failed!");
    u8g2.sendBuffer();
    while(true){Serial.println("SD MOUNT FAILED"); delay(1000);}
  }

  //Prepare files
  removeSVI();

  File countFile;
  while ( countFile.openNext( SD.vwd(), O_READ ))
  {
    countFile.close();
    numberOfFiles++;
  }
  countFile.close();
  SD.vwd()->rewind();

  //44.1KHz tick
  setISR();

  //Begin
  startTrack(FIRST_START);
  vgmVerify();
  prepareChips();
}

void setISR()
{
  Timer4.pause();
  Timer4.setPrescaleFactor(1);
  Timer4.setOverflow(1633);
  Timer4.setChannel1Mode(TIMER_OUTPUT_COMPARE);
  Timer4.attachCompare1Interrupt(tick);
  Timer4.refresh();
  Timer4.resume();  
}

void prepareChips()
{
  opm.Reset();
}

void drawOLEDTrackInfo()
{
  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.clearBuffer();
  char *cstr = &gd3.enTrackName[0u];
  u8g2.drawStr(0,9, cstr);
  cstr = &gd3.enGameName[0u];
  u8g2.drawStr(0,22, cstr);

  u8g2.setFont(u8g2_font_micro_tr);
  if(playMode == LOOP)
    u8g2.drawStr(0,32, "LOOP");
  else if(playMode == SHUFFLE)
    u8g2.drawStr(0,32, "SHUFFLE");
  else
    u8g2.drawStr(0,32, "IN ORDER");
  u8g2.sendBuffer();
}

//Mount file and prepare for playback. Returns true if file is found.
bool startTrack(FileStrategy fileStrategy, String request)
{
  ready = false;
  File nextFile;
  memset(fileName, 0x00, MAX_FILE_NAME_SIZE);

  switch(fileStrategy)
  {
    case FIRST_START:
    {
      nextFile.openNext(SD.vwd(), O_READ);
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
      currentFileNumber = 0;
    }
    break;
    case NEXT:
    {
      if(currentFileNumber+1 >= numberOfFiles)
      {
          SD.vwd()->rewind();
          currentFileNumber = 0;
      }
      else
          currentFileNumber++;
      nextFile.openNext(SD.vwd(), O_READ);
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
    }
    break;
    case PREV:
    {
      if(currentFileNumber != 0)
      {
        currentFileNumber--;
        SD.vwd()->rewind();
        for(uint32_t i = 0; i<=currentFileNumber; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        nextFile.close();
      }
      else
      {
        currentFileNumber = numberOfFiles-1;
        SD.vwd()->rewind();
        for(uint32_t i = 0; i<=currentFileNumber; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        nextFile.close();
      }
    }
    break;
    case RND:
    {
      randomSeed(micros());
      uint32_t randomFile = currentFileNumber;
      if(numberOfFiles > 1)
      {
        while(randomFile == currentFileNumber)
          randomFile = random(numberOfFiles-1);
      }
      currentFileNumber = randomFile;
      SD.vwd()->rewind();
      nextFile.openNext(SD.vwd(), O_READ);
      {
        for(uint32_t i = 0; i<randomFile; i++)
        {
          nextFile.close();
          nextFile.openNext(SD.vwd(), O_READ);
        }
      }
      nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
      nextFile.close();
    }
    break;
    case REQUEST:
    {
      SD.vwd()->rewind();
      bool fileFound = false;
      Serial.print("REQUEST: ");Serial.println(request);
      for(uint32_t i = 0; i<numberOfFiles; i++)
      {
        nextFile.close();
        nextFile.openNext(SD.vwd(), O_READ);
        nextFile.getName(fileName, MAX_FILE_NAME_SIZE);
        String tmpFN = String(fileName);
        tmpFN.trim();
        request.trim();
        if(tmpFN == request)
        {
          currentFileNumber = i;
          fileFound = true;
          break;
        }
      }
      nextFile.close();
      if(fileFound)
      {
        Serial.println("File found!");
      }
      else
      {
        Serial.println("ERROR: File not found! Continuing with current song.");
        ready = true;
        return false;
      }
    }
    break;
  }

  cmdPos = 0;
  bufferPos = 0;
  waitSamples = 0;
  loopCount = 0;

  if(file.isOpen())
    file.close();
  file = SD.open(fileName, FILE_READ);
  if(!file)
    Serial.println("Failed to read file");

  clearBuffers();
  memset(&loopPreBuffer, 0, LOOP_PREBUF_SIZE);
  header.Reset();
  fillBuffer();

  //VGM Header
  header.indent = readBuffer32();
  header.EoF = readBuffer32(); 
  header.version = readBuffer32(); 
  header.sn76489Clock = readBuffer32(); 
  header.ym2413Clock = readBuffer32();
  header.gd3Offset = readBuffer32();
  header.totalSamples = readBuffer32(); 
  header.loopOffset = readBuffer32(); 
  header.loopNumSamples = readBuffer32(); 
  header.rate = readBuffer32(); 
  header.snX = readBuffer32(); 
  header.ym2612Clock = readBuffer32(); 
  header.ym2151Clock = readBuffer32(); 
  header.vgmDataOffset = readBuffer32(); 
  header.segaPCMClock = readBuffer32(); 
  header.spcmInterface = readBuffer32(); 
  header.rf5C68clock = readBuffer32();
  header.ym2203clock = readBuffer32();
  header.ym2608clock = readBuffer32();
  header.ym2610clock = readBuffer32();
  header.ym3812clock = readBuffer32();
  header.ym3526clock = readBuffer32();
  header.y8950clock = readBuffer32();
  header.ymf262clock = readBuffer32();
  header.ymf271clock = readBuffer32();
  header.ymz280Bclock = readBuffer32();
  header.rf5C164clock = readBuffer32();
  header.pwmclock = readBuffer32();
  header.ay8910clock = readBuffer32();
  header.ayclockflags = readBuffer32();
  header.vmlblm = readBuffer32();
  if(header.version > 0x151)
  {
    header.gbdgmclock = readBuffer32();
    header.nesapuclock = readBuffer32();
    header.multipcmclock = readBuffer32();
    header.upd7759clock = readBuffer32();
    header.okim6258clock = readBuffer32();
    header.ofkfcf = readBuffer32();
    header.okim6295clock = readBuffer32();
    header.k051649clock = readBuffer32();
    header.k054539clock = readBuffer32();
    header.huc6280clock = readBuffer32();
    header.c140clock = readBuffer32();
    header.k053260clock = readBuffer32();
    header.pokeyclock = readBuffer32();
    header.qsoundclock = readBuffer32();
    header.scspclock = readBuffer32();
    header.extrahdrofs = readBuffer32();
    header.wonderswanclock = readBuffer32();
    header.vsuClock = readBuffer32();
    header.saa1099clock = readBuffer32();
  }

  #if DEBUG
  Serial.print("Indent: 0x"); Serial.println(header.indent, HEX);
  Serial.print("EoF: 0x"); Serial.println(header.EoF, HEX);
  Serial.print("Version: 0x"); Serial.println(header.version, HEX);
  Serial.print("SN Clock: "); Serial.println(header.sn76489Clock);
  Serial.print("YM2413 Clock: "); Serial.println(header.ym2413Clock);
  Serial.print("GD3 Offset: 0x"); Serial.println(header.gd3Offset, HEX);
  Serial.print("Total Samples: "); Serial.println(header.totalSamples);
  Serial.print("Loop Offset: 0x"); Serial.println(header.loopOffset, HEX);
  Serial.print("Loop # Samples: "); Serial.println(header.loopNumSamples);
  Serial.print("Rate: "); Serial.println(header.rate);
  Serial.print("SN etc.: 0x"); Serial.println(header.snX, HEX);
  Serial.print("YM2612 Clock: "); Serial.println(header.ym2612Clock);
  Serial.print("YM2151 Clock: "); Serial.println(header.ym2151Clock);
  Serial.print("VGM data Offset: 0x"); Serial.println(header.vgmDataOffset, HEX);
  Serial.print("SPCM Interface: 0x"); Serial.println(header.spcmInterface, HEX);
  Serial.println("...");
  Serial.print("YM3812 Clock: 0x"); Serial.println(header.ym3812clock, HEX);
  Serial.print("YMF262clock Clock: 0x"); Serial.println(header.ymf262clock, HEX);
  Serial.print("SAA1099 Clock: 0x"); Serial.println(header.saa1099clock, HEX);
  #endif

  //Jump to VGM data start and compute loop location
  if(header.vgmDataOffset == 0x0C)
    header.vgmDataOffset = 0x40;
  else
    header.vgmDataOffset += 0x34;
  
  if(header.vgmDataOffset != 0x40)
  {
    for(uint32_t i = 0x40; i<header.vgmDataOffset; i++)
      readBuffer();
  }
  if(header.loopOffset == 0x00)
  {
    header.loopOffset = header.vgmDataOffset;
  }
  else
    header.loopOffset += 0x1C;

  prebufferLoop();
  #if DEBUG
  //Dump the contents of the prebuffer
  for(int i = 0; i<LOOP_PREBUF_SIZE; i++)
  {
    if(i % 32 == 0)
      Serial.println();
    Serial.print("0x"); Serial.print(loopPreBuffer[i], HEX); Serial.print(", ");
  }
  #endif
  return true;
}

bool vgmVerify()
{
  if(header.indent != 0x206D6756) //VGM. Indent check
  {
    startTrack(NEXT);
    return false;
  }
  ltc.SetFrequency(header.ym2151Clock);
  Serial.println("VGM OK!");
  readGD3();
  Serial.println(gd3.enGameName);
  Serial.println(gd3.enTrackName);
  Serial.println(gd3.enSystemName);
  Serial.println(gd3.releaseDate);
  Serial.print("Version: "); Serial.println(header.version, HEX);
  drawOLEDTrackInfo();
  ready = true;
  return true;
}

void readGD3()
{
  uint32_t prevLocation = file.curPosition();
  uint32_t tag = 0;
  gd3.Reset();
  file.seek(0);
  file.seek(header.gd3Offset+0x14);
  for(int i = 0; i<4; i++) {tag += uint32_t(file.read());} //Get GD3 tag bytes and add them up for an easy comparison.
  if(tag != 0xFE) //GD3 tag bytes do not sum up to the constant. No valid GD3 data detected. 
  {Serial.print("INVALID GD3 SUM:"); Serial.println(tag); file.seekSet(prevLocation); return;}
  for(int i = 0; i<4; i++) {file.read();} //Skip version info
  uint8_t v[4];
  file.readBytes(v,4);
  gd3.size = uint32_t(v[0] + (v[1] << 8) + (v[2] << 16) + (v[3] << 24));
  char a, b;
  uint8_t itemIndex = 0;
  for(uint32_t i = 0; i<gd3.size; i++)
  {
    a = file.read();
    b = file.read();
    if(a+b == 0) //Double 0 detected
    {
      itemIndex++;
      continue;
    }
    switch(itemIndex)
    {
      case 0:
      gd3.enTrackName += a;
      break;
      case 1:
      //JP TRACK NAME
      break;
      case 2:
      gd3.enGameName += a;
      break;
      case 3:
      //JP GAME NAME
      break;
      case 4:
      gd3.enSystemName += a;
      break;
      case 5:
      //JP SYSTEM NAME
      break;
      case 6:
      gd3.enAuthor += a;
      break;
      case 7:
      //JP AUTHOR
      break;
      case 8:
      gd3.releaseDate += a;
      break;
      default:
      //IGNORE CONVERTER NAME + NOTES
      break;
    }
  }
  file.seekSet(prevLocation);
}

void removeSVI() //Sometimes, Windows likes to place invisible files in our SD card without asking... GTFO!
{
  File nextFile;
  nextFile.openNext(SD.vwd(), O_READ);
  char name[MAX_FILE_NAME_SIZE];
  nextFile.getName(name, MAX_FILE_NAME_SIZE);
  String n = String(name);
  if(n == "System Volume Information")
  {
      if(!nextFile.rmRfStar())
        Serial.println("Failed to remove SVI file");
  }
  SD.vwd()->rewind();
  nextFile.close();
}

//Keep a small cache of commands right at the loop point to prevent excessive SD seeking lag
void prebufferLoop() 
{
  uint32_t prevPos = file.curPosition();
  file.seekSet(header.loopOffset);
  file.readBytes(loopPreBuffer, LOOP_PREBUF_SIZE);
  file.seekSet(prevPos);
  #if DEBUG
  Serial.print("FIRST LOOP BYTE: "); Serial.println(loopPreBuffer[0], HEX);
  #endif
}

//On loop, inject the small prebuffer back into the main ring buffer
void injectPrebuffer()
{
  for(int i = 0; i<LOOP_PREBUF_SIZE; i++)
    cmdBuffer.push_back(loopPreBuffer[i]);
  file.seekSet(header.loopOffset+LOOP_PREBUF_SIZE);
  cmdPos = LOOP_PREBUF_SIZE-1;
  #if DEBUG
  Serial.println(file.curPosition());
  #endif
}

//Completely fill command buffer
void fillBuffer()
{
  while(!topUpBuffer()){};
}

//Add to buffer from SD card. Returns true when buffer is full
bool topUpBuffer() 
{
  if(cmdBuffer.full())
    return true;
  if(cmdBuffer.available() >= file.size()) 
     return true;
  fetching = true;
  cmdBuffer.push_back_nc(file.read());
  bufferPos = 0;
  fetching = false;
  return false;
}

void clearBuffers()
{
  bufferPos = 0;
  cmdBuffer.clear();
}

uint8_t readBuffer()
{
  if(cmdBuffer.empty()) //Buffer exauhsted prematurely. Force replenish
  {
    topUpBuffer();
  }
  bufferPos++;
  cmdPos++;
  return cmdBuffer.pop_front_nc();
}

uint16_t readBuffer16()
{
  uint16_t d;
  byte v0 = readBuffer();
  byte v1 = readBuffer();
  d = uint16_t(v0 + (v1 << 8));
  bufferPos+=2;
  cmdPos+=2;
  return d;
}

uint32_t readBuffer32()
{
  uint32_t d;
  byte v0 = readBuffer();
  byte v1 = readBuffer();
  byte v2 = readBuffer();
  byte v3 = readBuffer();
  d = uint32_t(v0 + (v1 << 8) + (v2 << 16) + (v3 << 24));
  bufferPos+=4;
  cmdPos+=4;
  return d;
}

//Read 32 bits right off of the SD card.
uint32_t readSD32()
{
  uint32_t d;
  byte v[4];
  file.readBytes(v, 4);
  d = uint32_t(v[0] + (v[1] << 8) + (v[2] << 16) + (v[3] << 24));
  return d;
}

//Count at 44.1KHz
void tick()
{
  if(!ready || cmdBuffer.empty())
    return;
  if(waitSamples > 0)
    waitSamples--;
}

//Execute next VGM command set. Return back wait time in samples
uint16_t parseVGM() 
{
  uint8_t cmd = readBuffer();
  switch(cmd)
  {
    case 0x54:
    {
      uint8_t a = readBuffer();
      uint8_t d = readBuffer();
      opm.SendDataPins(a, d);
      break;
    }
    case 0x61:
    return readBuffer16();
    case 0x62:
    return 735;
    case 0x63:
    return 882;
    case 0x67: //Ignore PCM data blocks
    {
        readBuffer(); //0x66
        readBuffer(); //Datatype
        uint32_t pcmSize = readBuffer32(); //Payload size;
        for(int i=0; i<pcmSize; i++)
          readBuffer();
        break;
    }
    case 0xB5: //Ignore common secondary PCM chips
    case 0xB6:
    case 0xB7:
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    readBuffer16();
    break;
    case 0xC0: //Ignore SegaPCM:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    readBuffer();readBuffer();readBuffer();
    break;
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    {
      return (cmd & 0x0F)+1;
    }
    case 0x66:
    {
    ready = false;
    clearBuffers();
    cmdPos = 0;
    injectPrebuffer();
    loopCount++;
    ready = true;
    }
    return 0;
    default:
    commandFailed = true;
    failedCmd = cmd;
    return 0;
  }
  return 0;
}

//Poll the serial port
void handleSerialIn()
{
  bool newTrack = false;
  while(Serial.available())
  {
    char serialCmd = Serial.read();
    switch(serialCmd)
    {
      case '+':
        newTrack = startTrack(NEXT);
      break;
      case '-':
        newTrack = startTrack(PREV);
      break;
      case '*':
        newTrack = startTrack(RND);
      break;
      case '/':
        playMode = SHUFFLE;
        drawOLEDTrackInfo();
      break;
      case '.':
        playMode = LOOP;
        drawOLEDTrackInfo();
      break;
      case '?':
        Serial.println(gd3.enGameName);
        Serial.println(gd3.enTrackName);
        Serial.println(gd3.enSystemName);
        Serial.println(gd3.releaseDate);
        Serial.print("Version: "); Serial.println(header.version, HEX);
      break;
      case '!':

      break;
      case 'r':
      {
        String req = Serial.readString();
        req.remove(0, 1); //Remove colon character
        newTrack = startTrack(REQUEST, req);
      }
      break;
      default:
        continue;
    }
  }
  if(newTrack)
  {
    vgmVerify();
    prepareChips();
  }
}

//Check for button input
bool buttonLock = false;
void handleButtons()
{
  bool newTrack = false;
  uint32_t count = 0;
  
  if(!digitalRead(next_btn))
    newTrack = startTrack(NEXT);
  if(!digitalRead(prev_btn))
    newTrack = startTrack(PREV);
  if(!digitalRead(rand_btn))
    newTrack = startTrack(RND);
  if(!digitalRead(shuf_btn) && !buttonLock)
  {
    playMode == SHUFFLE ? playMode = IN_ORDER : playMode = SHUFFLE;
    drawOLEDTrackInfo();
    buttonLock = true;
    delay(50);
  }
  if(!digitalRead(loop_btn) && !buttonLock)
  {
    playMode == LOOP ? playMode = IN_ORDER : playMode = LOOP;
    drawOLEDTrackInfo();
    buttonLock = true;
    delay(50);
  }

  if(buttonLock)
  {  
    if(digitalRead(loop_btn) && digitalRead(shuf_btn))
      buttonLock = false;
  }
  if(newTrack)
  {
    vgmVerify();
    prepareChips();
    delay(100);
  }
}

void loop()
{    
  topUpBuffer();
  if(waitSamples == 0)
  {
    waitSamples += parseVGM();
    return;
  }
  if(loopCount >= maxLoops && playMode != LOOP)
  {
    bool newTrack = false;
    if(playMode == SHUFFLE)
      newTrack = startTrack(RND);
    if(playMode == IN_ORDER)
      newTrack = startTrack(NEXT);
    if(newTrack)
    {
      vgmVerify();
      prepareChips();
    }
  }
  if(Serial.available() > 0)
    handleSerialIn();
  handleButtons();
  #if DEBUG
  if(commandFailed)
  {
    commandFailed = false;
    Serial.print("CMD ERROR: "); Serial.println(failedCmd, HEX);
  }
  #endif
}