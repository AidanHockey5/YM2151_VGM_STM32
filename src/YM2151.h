#ifndef YM2151_H_
#define YM2151_H_
#include <Arduino.h>
class YM2151
{
private:
    int * _dataPins; //Digital I/O pins
    int _CS;
    int _RD;
    int _WR;
    int _A0;
    int _IRQ;
    int _IC;
    void WriteDataPins(unsigned char data);
public:
    YM2151(int * dataPins, int CS, int RD, int WR, int A0, int IRQ, int IC);
    void Reset();
    void SendDataPins(unsigned char addr, unsigned char data);
};
#endif