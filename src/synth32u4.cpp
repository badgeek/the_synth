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

// NOTA: unless clearly specified, "channel" int he comments stands for the 8 channels used by the synth, not the right and left sound output.


#include <arduino.h>
#include "synth32u4.h"
#include "tables32u4.h"

#define CPUFrq 16000000							// CPU frequency
#define samplingFrq 22050						// target sampling frequency. Must match pitchTable[] and tickBPM[] in tables32u4.h

#define SIN 0									
#define TRI 1
#define SQUARE 2
#define SAW 3
#define NOISE 4

/*
 * This is how the synth works:
 * There is an Interrupt Service Routine (ISR) that is called on a regularly basis.
 * This ISR process the data given by the parameters (waveform, enveloppe, length, pitch) and outputs it to the Right and Left channels (pin 5 and 9)
 *
 * waveAcc[] is an accumulator that is incremented on each ISR, by the
 * waveTune[] value. this value is given by the pitchTable[] table, when the note is set. (see setVoice)
 * they are use to cycle trought the wave tables, thanks to the (byte) conversion that convert it %256. (see ISR routine)
 * that way, on each new increment the synth knows which height must be the wave to stick to the pitch.
 *
 * envAcc[] and envTune[] works the same, except they are not made to be used on a circular manner, but linearly.
 * on each ISR (in fact, 1 out of 8, see ISR routine), it goes further in the enveloppe table, starting at the beginning, endind at the end.
 *
 * waveAmp[] stores the amplitude of the waveform to be computed: it multiply the wave value by the enveloppe value, diminushing the overall height of the wave.
 *
 * waves[] simply stores for each channel the wave table that must be used.
 *
 * envs[] stores for each channel the enveloppe table to be used, just as waves[] do for waveforms
 *
 * pitchs[] stores the note the channel has been set to play
 *
 * lengths[] stores the length the note must lasts
 */

volatile unsigned int waveAcc[8]={0};			// waveform accumulator for each of the 8 channels
volatile unsigned int waveTune[8]={0};			// waveform increment
volatile unsigned char waveAmp[8]={0};			// amplitude of the waveform

volatile unsigned int envAcc[8]={0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000};	// enveloppe accumulator
volatile unsigned int envTune[8]={0};			// enveloppe increment

volatile unsigned int waves[8]={};				// pointer to the waveform table
volatile unsigned int envs[8]={};				// pointer to the enveloppe table
volatile char pitchs[8]={69};					// MIDI pitchs for notes (0-127, MIDI)
volatile unsigned char lengths[8]={0};			// length of each note

volatile unsigned char lastPlay=0;				// this remains what was the last channel played.

volatile unsigned char currentChannel=0;		// keeps the track of the channel beeing processed (see the ISR for more information)

// volatile unsigned int countBpm=0;				// used to generate a bpm tick
// volatile boolean tick=0;							// high when on bpm tick, low otherwise
// unsigned char bpm=100;							// default bpm value



/*
 * This ISR is used to update each channel on a regularly basis, depending on the CPU and the sampling frequency. See synth::begin()
 *
 * On each interrupt, the program compute the wave amplitude for one of the eight channel. This way process time can be saved
 *
 * It starts by setting the waveAmp for the channel. It adds the enveloppe tune value to the enveloppe accumulator.
 * It then divide ths value by 256 (that is fix point math: it enables to only use int in calculation, because floats slow up the processe to much)
 * Then it uses this value to go trought the enveloppe table (stored for this in channel in the envs[] table)
 * Finally, it stores the value read in the enveloppe tables in waveAmps[].
 *
 * Then it compute each of the eight channels:
 * waveTune[] is added to the waveAcc[].
 * this value is used to go trought the right wave table (stored in waves[]) and pick the right wave height
 * this wave height is then multpilied by the waveAmp[] value, and divided by 256 (>>8), to be decreased following the enveloppe.
 * last the values of the 8 channels are added one to others and output to dedicated pins
 */
ISR(TIMER1_COMPA_vect){
/*	compteurBpm++;
	if(countBpm>pgm_read_word(tickBPM + bpm)){
		tick=1;
		countBpm=0;
	}
*/
	currentChannel%=8;							// verifies that the currentChannel doesn't overlap the number of channels


	if(envAcc[currentChannel]<0x8000){			// 0x8000 is 128 (the enveloppe tables length) multiplied by 256 (fix point math)
		waveAmp[currentChannel] = pgm_read_byte(envs[currentChannel] + (envAcc[currentChannel] += envTune[currentChannel])/256);
	} else {
		waveAmp[currentChannel]=0;				// if the envAcc overlaps the enveloppe table lenght, then volume is set to 0.
	}

	OCR4A=OCR4B=127 + 
	(
	(((signed char)pgm_read_byte(waves[0] + ((unsigned char *)&(waveAcc[0] += waveTune[0]))[1]) * waveAmp[0]) >> 8) +
	(((signed char)pgm_read_byte(waves[1] + ((unsigned char *)&(waveAcc[1] += waveTune[1]))[1]) * waveAmp[1]) >> 8) +
	(((signed char)pgm_read_byte(waves[2] + ((unsigned char *)&(waveAcc[2] += waveTune[2]))[1]) * waveAmp[2]) >> 8) +
	(((signed char)pgm_read_byte(waves[3] + ((unsigned char *)&(waveAcc[3] += waveTune[3]))[1]) * waveAmp[3]) >> 8) +
	(((signed char)pgm_read_byte(waves[4] + ((unsigned char *)&(waveAcc[4] += waveTune[4]))[1]) * waveAmp[4]) >> 8) +
	(((signed char)pgm_read_byte(waves[5] + ((unsigned char *)&(waveAcc[5] += waveTune[5]))[1]) * waveAmp[5]) >> 8) +
	(((signed char)pgm_read_byte(waves[6] + ((unsigned char *)&(waveAcc[6] += waveTune[6]))[1]) * waveAmp[6]) >> 8) +
	(((signed char)pgm_read_byte(waves[7] + ((unsigned char *)&(waveAcc[7] += waveTune[7]))[1]) * waveAmp[7]) >> 8)
	)/4;

	currentChannel++;							// increments the channel for waveAmp processing on the next iteration
}

//class constructor
synth::synth(void){

}

/*
 * Init function. Must be called by the sketch that implement the library.
 *
 * It sets up the two timers used by the class. See Atmel doc for more information
 */

void synth::begin(void){

	cli();										// cancels interrupt durring setting up

/*
 * arduino uno / nano / mini / Atmega 238
 * uncomment the following lines and comment the PLL/TIMER4 lines to use with a regular arduino board
 * TIMER 2 setting (fast PWM, frequency generating)
*/
/*
	Timer2 fast PWM, OC2A and OC2B pins clear on match, set on bottom
	TCCR2A=B10100011;							// TIMER 2 fast pwm, OC2A and OC2B clear on match, set on bottom
	TCCR2B=B00000001;							// TIMER 2 no prescalling

	OCR2A=127;									// Initial value on A comparator
	OCR2B=127;									// ditto B

	bitSet(DDRB, 3);							// OC2A / PORTB3 / pin 11
	bitSet(DDRD, 3);							// OC2B / PORTD3 / pin 3
*/
	
	PLLCSR=B00010010;							// PLL prescaller setting. See Atmel atmega 32u4 doc, chapter 6.10
	delay(10);
	PLLCSR=B00010011;
	PLLFRQ=B00010100;

	TCCR4A=B01010011;							// Timer 4setting: OC4A and OC4B activated (Bxxxx0000, PWM4A and PWM4B activated (B000000xx)
	TCCR4B=B00000001;							// Timer 4: no dead time prescaller (B00xx0000), no prescalling (B0000xxxx).
//	TCCR4C										// beware, TCCR4C (Bxxxx0000) is the shadow register of TCCR4A: setting it will erase previous
	TCCR4D=B00000000;							// Timer 4: set fast PWM mode (B000000xx)

	OCR4A=127;									// default value: this is the middle value, for a sound of.
	OCR4B=127;									// ditto
	OCR4C=B11111111;							// valeur maxi du TIMER4 en mode fast PWM

//	bitSet(DDRB, 6);							// OC4B / pin 10	can also be used if it suits you better
	bitSet(DDRB, 5);							// /OC4B / pin 9
//	bitSet(DDRC, 7);							// OC4A / pin 13	can alos be used if it suits you better
	bitSet(DDRC, 6);							// /OC4A / pin 5


	//définition du TIMER 1 (CTC, fréquence d'échantillonage)
	TCNT1=0;									// Zeroing the counter
	TCCR1A=B00000000;							// Timer 1 CTC mode (defined by TCCR1A and TCCR1B)
	TCCR1B=B00001001;							// Timer 1 CTC, no prescaling
	TCCR1C=B00000000;
	OCR1A=CPUFrq/samplingFrq;					// valeur de tick // echantillonage de 22050Hz avec l'horloge de 16MHz.
	TIMSK1 |= (1 << OCIE1A);					// TIMER 1 enable the compare A match interrupt

	sei();										// sets interrupt again
}

/*
 * SetVoice function. It calls the functions that are needed to setup a voice on a channel.
 *
 * i is the number of channel the voice will be played on 	[0..7]
 * wave is the type of waveform to use 						[SIN, TRI, SQUARE, SAW, NOISE]
 * pitch is the note MIDI to play (C3 == 69)				[0..127]
 * env is the enveloppe to use 								[0..4]
 * length is the duration of the sound 						[0..127]
 */
void synth::setVoice(unsigned char i, unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length){

	if(i>7) i=0;
	setWave(i, wave);
	setPitch(i, pitch);
	setEnv(i, env);
	setLength(i, length);

}

/*
 * setWave function. It records on wave[] a pointer to the wave table wanted
 */
void synth::setWave(unsigned char i, unsigned char wave){

	switch(wave){
		case TRI:
			waves[i]=(unsigned int)triTable;
			break;
		case SQUARE:
			waves[i]=(unsigned int)squTable;
			break;
		case SAW:
			waves[i]=(unsigned int)sawTable;
			break;
		case NOISE:
			waves[i]=(unsigned int)noiseTable;
			break;
		default:
			waves[i]=(unsigned int)sinTable;

	}
}

/*
 * setPitch function. It records the pitch used for the channel, then records the matching waveform accumulator increment
 */
void synth::setPitch(unsigned char i, unsigned char pitch){

	pitchs[i]=pitch;									// records the right pitch
	waveTune[i]=pgm_read_word(pitchTable + pitch);		// records the waveform accumulator increment

}

/*
 * setEnv function. It records on envs[] a pointer to the enveloppe table wanted
 */
void synth::setEnv(unsigned char i, unsigned char env){

	switch(env){
		case 0:
			envs[i]=(unsigned int)env1;
			break;
		case 1:
			envs[i]=(unsigned int)env2;
			break;
		case 2:
			envs[i]=(unsigned int)env3;
			break;
		case 3:
			envs[i]=(unsigned int)env4;
			break;
		case 4:
			envs[i]=(unsigned int)env5;
			break;
		default:
			envs[i]=(unsigned int)env4;

	}
}

/*
 * setLength function. It records the length used for the channel, then records the matching enveloppe accumulator increment
 */
void synth::setLength(unsigned char i, unsigned char length){

	lengths[i]=length;									// records the right enveloppe
	envTune[i]=pgm_read_word(EFTWS + length);			// records the enveloppe accumulator increment

}

/*
 * play function. It start the play of a channel, by setting de envAcc[] to 0, then enabling the ISR to let it go trought the enveloppe table
 */
void synth::play(unsigned char i){

	if(i>7) i=0;
	lastPlay=i;
	envAcc[i]=0;

}

/*
 * getNextPlay function. This function gives the last channel whom play hs been started, + 1. (i.e. if the last channel played was 2, function returns 3)
 */
unsigned char synth::getNextPlay(void){

	if(lastPlay>6){
		return 0;
	} else {
		return lastPlay + 1;
	}

}

/*
 * getNextChannel function. This function gives the number of the channel whom play is more advanced.
 * it's a bit like getNextPlay, but it makes the difference between a sounf that is played fast and a sound that lasts longer.
 * taht way if the user wants to play a sound as all the channels are already used, it can choose the one on which the sound is the closer to the end.
 */
unsigned char synth::getNextChannel(void){

	byte nextChannel=0;
	byte lastWaveAmp=255;
	for(byte i=0; i<8; i++){
		if(envAcc[i] > 0x4000 && waveAmp[i]<lastWaveAmp){		// for sounds that have played more than half, sorts those which wave amplitude are the lowest
			lastWaveAmp=envAcc[i];
			nextChannel=i;
		}
	}
	return nextChannel;
}

/*
 * pause function. Stops the TIMER1 interrupt, and so the sound processing.
 * see Atmel documentation for more information
 */
boolean synth::pause(void){

	if(TIMSK1 & (1 << OCIE1A)){
		TIMSK1 &= ~(1 << OCIE1A);
		return true;
	} else {
		TIMSK1 |= (1 << OCIE1A);
		return false;
	}

}

/*
 * function. Not implemented yet
 */
/*
boolean synth::pulseBpm(void){

	if(tick==1){
		tick=0;
		return true;
	} else{
		return false;
	}

}

unsigned char synth::getBpm(){
	return bpm;
}

unsigned char synth::setBpm(unsigned char bpmToSet){
	bpm=bpmToSet;
	return bpm;
}
*/