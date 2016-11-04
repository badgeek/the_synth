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

#ifndef synth_h
#define synth_h

#include <arduino.h>

class synth{
  public:
    synth();
    void begin(void);
    void setVoice(unsigned char i, unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length);
    void play(unsigned char i);
    unsigned char getNextPlay(void);
    unsigned char getNextChannel(void);
    boolean pause(void);
 /*   boolean pulseBpm(void);
    unsigned char getBpm(void);
    unsigned char setBpm(unsigned char bpmToSet);
*/

private:
    void setWave(unsigned char i, unsigned char wave);
    void setPitch(unsigned char i, unsigned char pitch);
    void setEnv(unsigned char i, unsigned char env);
    void setLength(unsigned char i, unsigned char length);
};

#endif

