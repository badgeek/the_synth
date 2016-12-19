#include <Arduino.h>
#include "soundmachine.h"

volatile unsigned char lastPlay = 0;			// this remains what was the last channel played.

volatile unsigned char current = CHANNELS;		// keeps the track of the channel beeing processed (see the ISR for more information)

volatile unsigned int tickCount = 0;			// Counter for ticks
volatile unsigned int tickTop = 0;
volatile bool tick = false;

volatile unsigned char bpmCount = 0;
volatile unsigned char bpmTop = 24;
volatile bool tickBpm = false;

volatile unsigned int waveAcc[CHANNELS];
volatile unsigned int waveTune[CHANNELS];

volatile unsigned char waveAmp[CHANNELS];

volatile unsigned int envAcc[CHANNELS];
volatile unsigned int envTune[CHANNELS];

volatile unsigned int wave[CHANNELS];
volatile unsigned int env[CHANNELS];

unsigned char pitch[CHANNELS];
unsigned char length[CHANNELS];



//This is the Interrupt routine. It's fired on a regular basis, given the sampling and CPU frequencies.
//It counts for MIDI ticks (24 per quarter) and for bpm ticks (given a time signature set by user)
//It also process the sound and update the output pins
ISR(TIMER1_COMPA_vect){

	//Increment tick counts
	//if top is reached, set back to 0, set a flag (tick) and increment bpm count
	tickCount++;
	if(tickCount == tickTop){
		tickCount = 0;
		tick = true;
		bpmCount++;
		//if top is reached, set back to 0 and set a flag (tickBpm)
		if(bpmCount == bpmTop){
			bpmCount = 0;
			tickBpm = true;
		}
	}

	current++;
	current &= 0x07;

	//Channels amplitude are processed one at each ISR.
	//It gives a bit more room for other things to happend between two ISR.

	if(!((envAcc[current]) & 0x8000)){			// 0x8000 is 128 (the enveloppe tables length) multiplied by 256 (fix point math)
		waveAmp[current] = pgm_read_byte(env[current] + ((envAcc[current] += envTune[current])) / 1024);
	} else {
		waveAmp[current] = 0;				// if the envAcc overlaps the enveloppe table lenght, then volume is set to 0.
	}

	//Timer that drives PWM on output pin is not the same on Arduino Uno and leonardo/micro.
	#if defined(__AVR_ATmega32U4__)
	OCR4A = OCR4B = 127 +
	#else
	OCR2A = 127 +
	#endif
	//Here each channels are added to compute the value of the PWM output pin
	(
	(((char)pgm_read_byte(wave[0] + ((unsigned char*)&(waveAcc[0] += waveTune[0]))[1]) * waveAmp[0]) >> 8) +
	(((char)pgm_read_byte(wave[1] + ((unsigned char*)&(waveAcc[1] += waveTune[1]))[1]) * waveAmp[1]) >> 8) +
	(((char)pgm_read_byte(wave[2] + ((unsigned char*)&(waveAcc[2] += waveTune[2]))[1]) * waveAmp[2]) >> 8) +
	(((char)pgm_read_byte(wave[3] + ((unsigned char*)&(waveAcc[3] += waveTune[3]))[1]) * waveAmp[3]) >> 8) +
	(((char)pgm_read_byte(wave[4] + ((unsigned char*)&(waveAcc[4] += waveTune[4]))[1]) * waveAmp[4]) >> 8) +
	(((char)pgm_read_byte(wave[5] + ((unsigned char*)&(waveAcc[5] += waveTune[5]))[1]) * waveAmp[5]) >> 8) +
	(((char)pgm_read_byte(wave[6] + ((unsigned char*)&(waveAcc[6] += waveTune[6]))[1]) * waveAmp[6]) >> 8) +
	(((char)pgm_read_byte(wave[7] + ((unsigned char*)&(waveAcc[7] += waveTune[7]))[1]) * waveAmp[7]) >> 8)
	) / 4;

}

//class constructor
SoundMachine::SoundMachine(void){

}

//Start the synth. Set default bpm and signature, init timers
void SoundMachine::begin(){
	for(int i = 0; i < CHANNELS; i++){
		envAcc[i] = 0x8000;
	}
	setBpm(60);
	setSignature(4);
	_isrInit();
}

int SoundMachine::update(){

}

//Timer initialisation.
//Atemga 328 (Arduino uno, mini, nano, etc.) and 32u4 (leonardo, micro) are handled for now
void SoundMachine::_isrInit(void){
	//Cancel interrupts during setting up
	cli();

	//ISR definition for Atmega 32u4 (Arduino Leonardo, Micro)
	#if defined(__AVR_ATmega32U4__)
	//Timer 4
	PLLCSR=B00010010;				// PLL prescaller setting. See Atmel atmega 32u4 doc, chapter 6.10
	delay(10);
	PLLCSR=B00010011;
	PLLFRQ=B00010100;

	TCCR4A=B01010011;				// Timer 4setting: OC4A and OC4B activated (Bxxxx0000, PWM4A and PWM4B activated (B000000xx)
	TCCR4B=B00000001;				// Timer 4: no dead time prescaller (B00xx0000), no prescalling (B0000xxxx).
//	TCCR4C							// beware, TCCR4C (Bxxxx0000) is the shadow register of TCCR4A: setting it will erase previous
	TCCR4D=B00000000;				// Timer 4: set fast PWM mode (B000000xx)

	OCR4A=127;						// default value: this is the middle value, for a sound of.
	OCR4B=127;						// ditto
	OCR4C=B11111111;				// valeur maxi du TIMER4 en mode fast PWM

//	bitSet(DDRB, 6);				// OC4B / pin 10	can also be used if it suits you better
	bitSet(DDRB, 5);				// /OC4B / pin 9
//	bitSet(DDRC, 7);				// OC4A / pin 13	can also be used if it suits you better
	bitSet(DDRC, 6);				// /OC4A / pin 5

	//ISR definition for Atmega 328 (Arduino Uno, Nano, Pro, etc.)
	#else
	//Timer 2 setting (fast PWM, frequency generating)
	//Timer 2 fast PWM, OC2A and OC2B pins clear on match, set on bottom
	TCCR2A=B10100011;				// TIMER 2 fast pwm, OC2A and OC2B clear on match, set on bottom
	TCCR2B=B00000001;				// TIMER 2 no prescalling

	OCR2A=127;						// Initial value on A comparator
	OCR2B=127;						// ditto B

	bitSet(DDRB, 3);				// OC2A / PORTB3 / pin 11
	bitSet(DDRD, 3);				// OC2B / PORTD3 / pin 3

	#endif

	//définition du TIMER 1 (CTC, fréquence d'échantillonage)
	TCNT1=0;						// Zeroing the counter
	TCCR1A=B00000000;				// Timer 1 CTC mode (defined by TCCR1A and TCCR1B)
	TCCR1B=B00001001;				// Timer 1 CTC, no prescaling
	TCCR1C=B00000000;
	OCR1A=CPU/SAMPLING;				// valeur de tick
	TIMSK1 |= (1 << OCIE1A);		// TIMER 1 enable the compare A match interrupt

	sei();
}

/*
 * This ISR is used to update each channel on a regularly basis, depending on the CPU and the sampling frequency. See SoundMachine::begin()
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

/*
 * SetVoice function. It calls the functions that are needed to setup a voice on a channel.
 *
 * i is the number of channel the voice will be played on 	[0..7]
 * wave is the type of waveform to use 						[SIN, TRI, SQUARE, SAW, NOISE]
 * pitch is the note MIDI to play (A4 == 69)				[0..127]
 * env is the enveloppe to use 								[0..4]
 * length is the duration of the sound 						[0..127]
 */
void SoundMachine::setVoice(unsigned char i, unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length){
	//Set back to 0 if greater than 8
	i &= 0x7;
	_setWave(i, wave);
	_setPitch(i, pitch);
	_setEnv(i, env);
	_setLength(i, length);

}

/*
 * setWave function. It records on wave[] a pointer to the wave table wanted
 */
void SoundMachine::_setWave(unsigned char i, unsigned char _wave){

	switch(_wave){
		case TRI:
			wave[i] = (unsigned int)triTable;
			break;
		case SQUARE:
			wave[i] = (unsigned int)squTable;
			break;
		case SAW:
			wave[i] = (unsigned int)sawTable;
			break;
		case NOISE:
			wave[i] = (unsigned int)noiseTable;
			break;
		default:
			wave[i] = (unsigned int)sinTable;
	}

}

/*
 * setPitch function. It records the pitch used for the channel, then records the matching waveform accumulator increment
 */
void SoundMachine::_setPitch(unsigned char i, unsigned char _pitch){

	pitch[i] = _pitch;
	waveTune[i] = pgm_read_word(&pitchTable[_pitch]);

}

/*
 * setEnv function. It records on envs[] a pointer to the enveloppe table wanted
 */
void SoundMachine::_setEnv(unsigned char i, unsigned char _env){

	switch(_env){
		case 0:
			env[i] = (unsigned int)env1;
			break;
		case 1:
			env[i] = (unsigned int)env2;
			break;
		case 2:
			env[i] = (unsigned int)env3;
			break;
		case 3:
			env[i] = (unsigned int)env4;
			break;
		case 4:
			env[i] = (unsigned int)env5;
			break;
		default:
			env[i] = (unsigned int)env1;
	}

}

/*
 * setLength function. It records the length used for the channel, then records the matching enveloppe accumulator increment
 */
void SoundMachine::_setLength(unsigned char i, unsigned char _length){

	length[i] = _length;
	envTune[i] = pgm_read_word(&EFTWS[_length]);

}

/*
 * play function. It start the play of a channel, by setting de envAcc[] to 0, then enabling the ISR to let it go trought the enveloppe table
 */
void SoundMachine::play(unsigned char i){

	//Set back to 0 if greater than 8
	i &= 0x7;

	lastPlay = i;

	waveAcc[i] = 0;
	envAcc[i] = 0;

}

//Direct play of a note with parameters
unsigned char SoundMachine::play(unsigned char wave, unsigned char pitch, unsigned char env, unsigned char length){
	//Get the new channel to play
	unsigned char current = lastPlay + 1;

	//Set voice, then play it.
	setVoice(current, wave, pitch, env, length);
	play(current);

	//Finally return the number of the channel that has been played.
	return current;
}

//stop a note that is being played
void SoundMachine::stop(unsigned char i){
	waveAcc[i] = 0x8000;
}

/*
 * getNextPlay function. This function gives the last channel whom play has been started, + 1. (i.e. if the last channel played was 2, function returns 3)
 */
unsigned char SoundMachine::getNextPlay(void){
		return (lastPlay + 1) & 0x7;
}

/*
 * getNextChannel function. This function gives the number of the channel whom play is more advanced.
 * it's a bit like getNextPlay, but it makes the difference between a sound that is played fast and a sound that lasts longer.
 * taht way if the user wants to play a sound as all the channels are already used, it can choose the one on which the sound is the closer to the end.
 */
unsigned char SoundMachine::getNextChannel(void){

	byte nextChannel=0;
	byte lastWaveAmp=255;
	for(byte i=0; i<8; i++){
		if(envAcc[i] > 0x4000 && waveAmp[i] < lastWaveAmp){		// for sounds that have played more than half, sorts those which wave amplitude are the lowest
			lastWaveAmp = envAcc[i];
			nextChannel=i;
		}
	}
	return nextChannel;
}

/*
 * pause function. Stops the TIMER1 interrupt, and so the sound processing.
 * see Atmel documentation for more information
 */
boolean SoundMachine::pause(void){

	if(TIMSK1 & (1 << OCIE1A)){
		TIMSK1 &= ~(1 << OCIE1A);
		return true;
	} else {
		TIMSK1 |= (1 << OCIE1A);
		return false;
	}

}

//Set the bpm wanted. Under the hood, it sets a tick as in MIDI, that fires 24 times per quarter note.
void SoundMachine::setBpm(unsigned char _bpm){
	if (_bpm > 239){
		bpm=239;
	} else if (_bpm < 20){
		bpm = 20;
	}
	bpm = _bpm;
	tickTop = pgm_read_word(tickBPM + (bpm - 20));
	tickCount = 0;
	tick = false;

	bpmCount = 0;

}

//Get the bpm value set
unsigned char SoundMachine::getBpm(){
	return bpm;
}

//Get a tick. This function must be polled on a regular basis
bool SoundMachine::getTick(){
	if (tick == false){
		return false;
	} else {
		tick = false;
		return true;
	}

}

//Set the time signature. Must be a power of 2. 1 is for 4 times, 4 is for quarter, aso.
void SoundMachine::setSignature(unsigned char _sign){
	//test the value received: it must be a power of 2
	for (int i = 0; i < 6; i++){
		if(!(_sign ^ (1 << i))){
			bpmTop = (24 * 4) / _sign;
			return;
		}
	}
	//If wrong value, set defaut to 24 (time is based on quarter note)
	bpmTop = 24;

}

//Get a metronome time. Must be polled on a regluar basis
bool SoundMachine::getTime(){
	if (tickBpm == false){
		return false;
	} else {
		tickBpm = false;
		return true;
	}

}