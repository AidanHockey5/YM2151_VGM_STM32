#include "YM2151.h"
#include <Arduino.h>
YM2151::YM2151(int * dataPins, int CS, int RD, int WR, int A0, int IRQ, int IC)
{
    disableDebugPorts();
    _dataPins = dataPins;
    _CS = CS;
    _RD = RD;
    _WR = WR;
    _A0 = A0;
    _IRQ = IRQ;
    _IC = IC;
    for(int i = 0; i<8; i++)
    {
        pinMode(*(_dataPins+i), OUTPUT);
        digitalWrite(*(_dataPins+i), LOW);
    }

    pinMode(_CS, OUTPUT);
    pinMode(_RD, OUTPUT);
    pinMode(_WR, OUTPUT);
    pinMode(_A0, OUTPUT);
    pinMode(_IRQ, OUTPUT);
    pinMode(_IC, OUTPUT);

    digitalWrite(_CS, HIGH);
    digitalWrite(_RD, HIGH);
    digitalWrite(_WR, HIGH);
    digitalWrite(_A0, HIGH);
    if(_IRQ != NULL)
        digitalWrite(_IRQ, LOW);
    digitalWrite(_IC, HIGH);
}

void YM2151::Reset()
{
    digitalWrite(_IC, LOW);
    delayMicroseconds(25);
    digitalWrite(_IC, HIGH);
}

void YM2151::WriteDataPins(unsigned char data) //Digital I/O
{
    for(int i=0; i<8; i++)
    {
      digitalWrite(*(_dataPins+i), ((data >> i)&1));
    }
}

void YM2151::SendDataPins(unsigned char addr, unsigned char data)
{
        digitalWrite(_WR, LOW);
        digitalWrite(_A0, LOW);
        WriteDataPins(addr);
        digitalWrite(_CS, LOW);
        delayMicroseconds(1); //Replace with 10 nS delay?
        digitalWrite(_CS, HIGH);
        digitalWrite(_WR, LOW);
        digitalWrite(_A0, HIGH);
        WriteDataPins(data);
        digitalWrite(_CS, LOW);
        delayMicroseconds(1); //Replace with 10 nS delay?
        digitalWrite(_CS, HIGH);
        digitalWrite(_WR, HIGH);
}