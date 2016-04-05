/*
 * assignment1_crossover
 * RTDSP 2016
 *
 * First assignment for ECS732 RTDSP, to implement a 2-way audio crossover
 * using the BeagleBone Black.
 *
 * Andrew McPherson and Victor Zappi
 * Modified by Becky Stewart
 * Queen Mary, University of London
 */

#include <BeagleRT.h>
#include <cmath>
#include <Utilities.h>
#include <rtdk.h>

#define FILTER_ORDER 4


// previous sample buffers & write pointer
float gYHi[FILTER_ORDER] = {0.0};
float gX[FILTER_ORDER] = {0.0};
float gYLo[FILTER_ORDER] = {0.0};
int gWritePointer = 0;

// parameters passed from the command line
extern float extFrequency;
extern int extFilterClass;

// structure for passing filter coefficients
struct FilterCoefficients {
	double b[5];
	double a[5];
};


// enumerators for filter class and type
enum { 
	filterTypeLoPass = 0,
	filterTypeHiPass = 1
};

enum{
	filterClassButterworth = 0,
	filterClassLinkwitz = 1
};


// filter coefficients for both high and low pass
FilterCoefficients hpCoefficients;
FilterCoefficients lpCoefficients;


FilterCoefficients makeFilterCoefficients(int filterType, float cutoff, int filterClass, float sampleRate) {

	// function for setting filter coefficients

	FilterCoefficients coeffs; // coefficients to be returned 

	float wc = 2 * M_PI * cutoff; 	// cutoff frequency in radians
	float Q = sqrt(2) / 2; // Q of a butterworth filter

	float wca = tanf(wc/(2*sampleRate)); // warped analogue frequency (without the 2/T term)

	if (filterClass == filterClassButterworth) {
		float a0 = 1 + wca/Q + pow(wca,2); // denominator for all coefficients for the butterworth filters

		if(filterType == filterTypeLoPass) {
			
			coeffs.a[1] = (-2 + 2 * pow(wca,2) )/ a0;
			coeffs.a[2] = (1 - wca/Q + pow(wca,2))/ a0;
			coeffs.a[3] = coeffs.a[4] = 0.0;

			coeffs.b[0] = pow(wca, 2)/ a0;
			coeffs.b[1] = 2 * pow(wca, 2)/ a0;
			coeffs.b[2] = pow(wca, 2)/ a0;
			coeffs.b[3] = coeffs.b[4] = 0.0;


		} else if(filterType ==  filterTypeHiPass) {
		
			coeffs.b[0] = 1/ a0;
			coeffs.b[1] = -2/ a0;
			coeffs.b[2] = 1/ a0;
			coeffs.b[3] = coeffs.b[4] = 0.0;

			coeffs.a[1] = (-2 + 2 * pow(wca,2))/ a0;
			coeffs.a[2] = (1 - wca/Q + pow(wca,2))/ a0;
			coeffs.a[3] = coeffs.a[4] = 0.0;

		}
	} else if (filterClass == filterClassLinkwitz) {
		float a0 = pow(1 + wca/Q + pow(wca,2), 2); // denominator for all coefficeints for the linkwitz filters

		if(filterType == filterTypeLoPass) {
			
			coeffs.b[0] = pow(wca, 4) / a0;
			coeffs.b[1] = 4 * pow(wca, 4) / a0;
			coeffs.b[2] = 2 * pow(wca, 4) + 4 * pow(wca, 4) / a0;
			coeffs.b[3] = 4 * pow(wca, 4) / a0;
			coeffs.b[4] = pow(wca, 4) / a0;

			coeffs.a[1] = 2 * (1 + wca/Q + pow(wca,2))*( -2 + 2 * pow(wca,2)) / a0;
			coeffs.a[2] =( 2 * (1 + wca/Q + pow(wca,2)) * (1 - wca/Q + pow(wca,2)) + pow(-2 + 2 * pow(wca,2), 2)) / a0;
			coeffs.a[3] = 2 * (-2 + 2 * pow(wca,2)) * (1 - wca/Q + pow(wca,2)) / a0;
			coeffs.a[4] = pow(1 - wca/Q + pow(wca,2), 2) / a0;


		} else if(filterType ==  filterTypeHiPass) {
		
			coeffs.b[0] = 1 / a0;
			coeffs.b[1] = -4 / a0;
			coeffs.b[2] = 6 / a0;
			coeffs.b[3] = -4 / a0;
			coeffs.b[4] = 1 / a0;


			coeffs.a[1] = 2 * (1 + wca/Q + pow(wca,2))*( -2 + 2 * pow(wca,2)) / a0;
			coeffs.a[2] = (2 * (1 + wca/Q + pow(wca,2)) * (1 - wca/Q + pow(wca,2)) + pow(-2 + 2 * pow(wca,2), 2)) / a0;
			coeffs.a[3] = 2 * (-2 + 2 * pow(wca,2)) * (1 - wca/Q + pow(wca,2)) / a0;
			coeffs.a[4] = pow(1 - wca/Q + pow(wca,2), 2) / a0;


		}
	}

	// print coefficiens - debugging
	rt_printf("Filter Coefficients:\n");

	for (int i = 0; i <= FILTER_ORDER; i++)
	{
		rt_printf("A(%d) = %f", i, coeffs.a[i]);
		rt_printf("\t B(%d) = %f \n", i, coeffs.b[i]);

	}
	return coeffs;

}



// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.

bool setup(BeagleRTContext *context, void *userData)
{
	// Retrieve a parameter passed in from the initAudio() call
	if(userData != 0)
		extFrequency = *(float *)userData;
	
	// filter coefficients
	hpCoefficients = makeFilterCoefficients(filterTypeHiPass, extFrequency, extFilterClass, context->audioSampleRate);
	lpCoefficients = makeFilterCoefficients(filterTypeLoPass, extFrequency, extFilterClass, context->audioSampleRate);

	return true;
}



// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(BeagleRTContext *context, void *userData)
{

	for(unsigned int n = 0; n < context->audioFrames; n++) {
		// loop through each sample in the buffer

		// initialize input and outputs
		float outputHi = 0.0;
		float outputLo = 0.0;
		float input = 0.0;

		for(unsigned int channel = 0; channel < context->audioChannels; channel++) {
			// sum the inputs
			input += 0.5 * audioReadFrame(context, n, channel);
		}
		

		// apply filter
		outputHi += hpCoefficients.b[0] * input;
		outputLo += lpCoefficients.b[0] * input;
		for (int i = 1; i <= FILTER_ORDER; i++)
		{

			outputHi += hpCoefficients.b[i] * gX[(gWritePointer - i + FILTER_ORDER) % FILTER_ORDER];
			outputHi -= hpCoefficients.a[i] * gYHi[(gWritePointer - i + FILTER_ORDER) % FILTER_ORDER];

			outputLo += lpCoefficients.b[i] * gX[(gWritePointer - i + FILTER_ORDER) % FILTER_ORDER];
			outputLo -= lpCoefficients.a[i] * gYLo[(gWritePointer - i + FILTER_ORDER) % FILTER_ORDER];
		}

		// write low pass output to left channel
		audioWriteFrame(context, n, 0, outputLo); 

		// write high pass output to right channel
		audioWriteFrame(context, n, 1, outputHi);

		// write inputs and outputs to buffers
		gX[gWritePointer] = input;	
		gYLo[gWritePointer] = outputLo;
		gYHi[gWritePointer] = outputHi;

		// advance pointer and loop in pointer if necessary
		gWritePointer++;
		if (gWritePointer == FILTER_ORDER) {
			gWritePointer = 0;
		}
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BeagleRTContext *context, void *userData)
{

}
