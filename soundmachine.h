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

#define CPU                 16000000
#define SAMPLING            20000       //22050, 20000, 16000 or 11025
#define F_A                 440

//We need the SAMPLING value to be defined in order to set the right tables
//So tables must be included now.
#include "tables.h"

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
    unsigned char play(unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length);
    void stop(unsigned char i);
    unsigned char getNextPlay(void);
    unsigned char getNextChannel(void);
    boolean pause(void);
    void setBpm(unsigned char);
    unsigned char getBpm(void);
    bool getTick(void);
    void setSignature(unsigned char);
    bool getTime(void);

protected:
    void _isrInit(void);
    void _bufferInit(void);
    void _setWave(unsigned char i, unsigned char wave);
    void _setPitch(unsigned char i, unsigned char pitch);
    void _setEnv(unsigned char i, unsigned char env);
    void _setLength(unsigned char i, unsigned char length);

    unsigned char bpm;
};

#endif

