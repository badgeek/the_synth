#include <Arduino.h>
#include "soundmachine.h"

volatile unsigned char lastPlay = 0;			// this remains what was the last channel played.

volatile unsigned char current = CHANNELS;		// keeps the track of the channel beeing processed (see the ISR for more information)

volatile unsigned int waveAcc[CHANNELS];		//char
volatile unsigned int waveTune[CHANNELS];

volatile unsigned char waveAmp[CHANNELS];		//char

volatile unsigned int envAcc[CHANNELS];
volatile unsigned int envTune[CHANNELS];		//char

volatile unsigned int wave[CHANNELS];
volatile unsigned int env[CHANNELS];

unsigned char pitch[CHANNELS];
unsigned char length[CHANNELS];


ISR(TIMER1_COMPA_vect){
	current++;
	current &= 0x07;

	if(!(envAcc[current] & 0x8000)){			// 0x8000 is 128 (the enveloppe tables length) multiplied by 256 (fix point math)
		waveAmp[current] = pgm_read_byte(env[current] + (envAcc[current] += envTune[current]) / 256);
	} else {
		waveAmp[current] = 0;				// if the envAcc overlaps the enveloppe table lenght, then volume is set to 0.
	}

	#if defined(__AVR_ATmega32U4__)

	OCR4A=OCR4B=127;

	#else

	OCR2A = 127 +
	(
	(((signed char)pgm_read_byte(wave[0] + ((unsigned char*)&(waveAcc[0] += waveTune[0]))[1]) * waveAmp[0]) >> 8) +
	(((signed char)pgm_read_byte(wave[1] + ((unsigned char*)&(waveAcc[1] += waveTune[1]))[1]) * waveAmp[1]) >> 8) +
	(((signed char)pgm_read_byte(wave[2] + ((unsigned char*)&(waveAcc[2] += waveTune[2]))[1]) * waveAmp[2]) >> 8) +
	(((signed char)pgm_read_byte(wave[3] + ((unsigned char*)&(waveAcc[3] += waveTune[3]))[1]) * waveAmp[3]) >> 8) +
	(((signed char)pgm_read_byte(wave[4] + ((unsigned char*)&(waveAcc[4] += waveTune[4]))[1]) * waveAmp[4]) >> 8) +
	(((signed char)pgm_read_byte(wave[5] + ((unsigned char*)&(waveAcc[5] += waveTune[5]))[1]) * waveAmp[5]) >> 8) +
	(((signed char)pgm_read_byte(wave[6] + ((unsigned char*)&(waveAcc[6] += waveTune[6]))[1]) * waveAmp[6]) >> 8) +
	(((signed char)pgm_read_byte(wave[7] + ((unsigned char*)&(waveAcc[7] += waveTune[7]))[1]) * waveAmp[7]) >> 8)
	) / 4;

	#endif

}

//class constructor
SoundMachine::SoundMachine(void){

}

void SoundMachine::begin(){
	for(int i = 0; i < CHANNELS; i++){
		envAcc[i] = 0x8000;
	}
	_isrInit();
}

int SoundMachine::update(){

}

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
	OCR1A=F_CPU/SAMPLING;			// valeur de tick // echantillonage de 22050Hz avec l'horloge de 16MHz.
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

	if(i>7) i=0;
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
	envTune[i] = pgm_read_byte(&EFTWS[_length]);

}

/*
 * play function. It start the play of a channel, by setting de envAcc[] to 0, then enabling the ISR to let it go trought the enveloppe table
 */
void SoundMachine::play(unsigned char i){

	if(i>7) i=0;

	lastPlay=i;

	waveAcc[i] = 0;
	envAcc[i] = 0;

}

/*
 * getNextPlay function. This function gives the last channel whom play has been started, + 1. (i.e. if the last channel played was 2, function returns 3)
 */
unsigned char SoundMachine::getNextPlay(void){

	if(lastPlay>6){
		return 0;
	} else {
		return lastPlay + 1;
	}

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