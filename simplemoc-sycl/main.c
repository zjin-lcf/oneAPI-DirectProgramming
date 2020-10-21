#include "SimpleMOC-kernel_header.h"

int main( int argc, char * argv[] )
{
	int version = 4;

	#ifdef PAPI
	papi_serial_init();
	#endif

  #ifdef DEBUG
	srand(version);
  #else
	srand(time(NULL));
  #endif

	// Get Inputs
	Input * I = set_default_input();
	read_CLI( argc, argv, I );
	
	// Calculate Number of 3D Source Regions
	I->source_3D_regions = (int) ceil((double)I->source_2D_regions *
		I->coarse_axial_intervals / I->decomp_assemblies_ax);

	logo(version);

	// Build Source Data
	Source *S = initialize_sources(I); 

  // Build Source Data for device
	Source *S2 = copy_sources(I, S); 
	
	print_input_summary(I);

	center_print("SIMULATION", 79);
	border_print();
	printf("Attentuating fluxes across segments...\n");

	double start, stop;

	// Run Simulation Kernel Loop
	start = get_time();
	run_kernel(I, S, S2);
	stop = get_time();

	printf("Simulation Complete.\n");

	border_print();
	center_print("RESULTS SUMMARY", 79);
	border_print();

	double tpi = ((double) (stop - start) /
			(double)I->segments / (double) I->egroups) * 1.0e9;
	printf("%-25s%.3lf seconds\n", "Runtime:", stop-start);
	printf("%-25s%.3lf ns\n", "Time per Intersection:", tpi);
	border_print();

  free(S2->fine_source);
  free(S2->fine_flux);
  free(S2->sigT);
  free(S2);
  free(S[0].fine_source);
  free(S[0].fine_flux);
  free(S[0].sigT);
  free(S);
  free(I);
	return 0;
}