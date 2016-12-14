//*************************************************************************************
//  Arduino synth V4.1
//  Optimized audio driver, modulation engine, envelope engine.
//
//  Dzl/Illutron 2014
//
//*************************************************************************************

/*
 * Height channel sound generator for arduino.
 *
 * enhancements by Pierre-Loup Martin, 2015.
 *
 * It can generate sounds out of waveform, enveloppes and midi pitchs.
 * tables32u4.h contains table to set waveforms, enveloppes, midi pitchs, length and bpm.
 */

/*
 * methods declaration
 */

#ifndef SoundMachine_H
#define SoundMachine_H

#include <Arduino.h>
#include <avr/pgmspace.h>
#include "tables.h"

#define F_CPU               16000000
#define SAMPLING            16000       //22050
#define F_A                 440

#define CHANNELS            8

#define SIN                 0                                   
#define TRI                 1
#define SQUARE              2
#define SAW                 3
#define NOISE               4

class SoundMachine{
  public:
    SoundMachine();

    void begin(void);

    int update(void);

    void setVoice(unsigned char i, unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length);
    void play(unsigned char i);
    unsigned char getNextPlay(void);
    unsigned char getNextChannel(void);
    boolean pause(void);

protected:
    void _isrInit(void);
    void _bufferInit(void);
    void _setWave(unsigned char i, unsigned char wave);
    void _setPitch(unsigned char i, unsigned char pitch);
    void _setEnv(unsigned char i, unsigned char env);
    void _setLength(unsigned char i, unsigned char length);
};

#endif

