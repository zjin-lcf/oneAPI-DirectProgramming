#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cmath>

typedef struct {
  float s0;
  float s1;
  float s2;
  float s3;
} float4;

float  distance_host ( int i, float  latitude_1, float  longitude_1, float  latitude_2, float  longitude_2 )
{
  float  dist ;
  float  rad_latitude_1 ;
  float  rad_latitude_2 ;
  float  rad_longitude_1 ;
  float  rad_longitude_2 ;

  float  BAZ , C , C2A , CU1 , CU2 , CX , CY , CZ ,
         D , E , FAZ , SA , SU1 , SX  , SY , TU1 , TU2 , X , Y ; 

  const float  GDC_DEG_TO_RAD = 3.141592654 / 180.0 ;  /* Degrees to radians      */
  const float GDC_FLATTENING = 1.0 - ( 6356752.31424518 / 6378137.0 ) ; 
  const float GDC_ECCENTRICITY = ( 6356752.31424518 / 6378137.0 ) ; 
  const float  GDC_ELLIPSOIDAL =  1.0 / ( 6356752.31414 / 6378137.0 ) / ( 6356752.31414 / 6378137.0 ) - 1.0 ;
  const float GDC_SEMI_MINOR = 6356752.31424518f;
  const float EPS = 0.5e-5f;


  rad_longitude_1 = longitude_1 * GDC_DEG_TO_RAD ;
  rad_latitude_1 = latitude_1 * GDC_DEG_TO_RAD ;
  rad_longitude_2 = longitude_2 * GDC_DEG_TO_RAD ;
  rad_latitude_2 = latitude_2 * GDC_DEG_TO_RAD ;

  TU1 = GDC_ECCENTRICITY * sinf ( rad_latitude_1 ) /
    cosf ( rad_latitude_1 ) ;
  TU2 = GDC_ECCENTRICITY * sinf ( rad_latitude_2 ) /
    cosf ( rad_latitude_2 ) ;

  CU1 = 1.0f / sqrtf ( TU1 * TU1 + 1.0f ) ;
  SU1 = CU1 * TU1 ;
  CU2 = 1.0f / sqrtf ( TU2 * TU2 + 1.0f ) ;
  dist = CU1 * CU2 ;
  BAZ = dist * TU2 ;
  FAZ = BAZ * TU1 ;
  X = rad_longitude_2 - rad_longitude_1 ;

  do {
    SX = sinf ( X ) ;
    CX = cosf ( X ) ;
    TU1 = CU2 * SX ;
    TU2 = BAZ - SU1 * CU2 * CX ;
    SY = sqrtf ( TU1 * TU1 + TU2 * TU2 ) ;
    CY = dist * CX + FAZ ;
    Y = atan2f ( SY, CY ) ;
    SA = dist * SX / SY ;
    C2A = - SA * SA + 1.0f;
    CZ = FAZ + FAZ ;
    if ( C2A > 0.0f ) CZ = -CZ / C2A + CY ;
    E = CZ * CZ * 2.0f - 1.0f ;
    C = ( ( -3.0f * C2A + 4.0f ) * GDC_FLATTENING + 4.0f ) * C2A *
      GDC_FLATTENING / 16.0f ;
    D = X ;
    X = ( ( E * CY * C + CZ ) * SY * C + Y ) * SA ;
    X = ( 1.0f - C ) * X * GDC_FLATTENING + rad_longitude_2 - rad_longitude_1 ;
  } while ( fabsf ( D - X ) > EPS );

  X = sqrtf ( GDC_ELLIPSOIDAL * C2A + 1.0f ) + 1.0f ;
  X = ( X - 2.0f ) / X ;
  C = 1.0f - X ;
  C = ( X * X / 4.0f + 1.0f ) / C ;
  D = ( 0.375f * X * X - 1.0f ) * X ;
  X = E * CY ;
  dist = 1.0f - E - E ;
  dist = ( ( ( ( SY * SY * 4.0f - 3.0f ) * dist * CZ * D / 6.0f -
          X ) * D / 4.0f + CZ ) * SY * D + Y ) * C * GDC_SEMI_MINOR ;
  return dist;
}

void distance_device(const float4* VA, float* VC, const size_t N, const int iteration) {

#pragma omp target data map(to: VA[0:N]) map(from: VC[0:N])
  {
    for (int n = 0; n < iteration; n++) {
      #pragma omp target teams distribute parallel for thread_limit(256)
      for (int wiID = 0; wiID < N; wiID++) {

        const float  GDC_DEG_TO_RAD = 3.141592654 / 180.0 ;  /* Degrees to radians      */
        const float GDC_FLATTENING = 1.0 - ( 6356752.31424518 / 6378137.0 ) ; 
        const float GDC_ECCENTRICITY = ( 6356752.31424518 / 6378137.0 ) ; 
        const float  GDC_ELLIPSOIDAL =  1.0 / ( 6356752.31414 / 6378137.0 ) / ( 6356752.31414 / 6378137.0 ) - 1.0 ;
        const float GDC_SEMI_MINOR = 6356752.31424518f;
        const float EPS                    = 0.5e-5f;
        float  dist, BAZ , C , C2A , CU1 , CU2 , CX , CY , CZ ,
               D , E , FAZ , SA , SU1 , SX  , SY , TU1 , TU2 , X , Y ; 

        const float latitude_1 = VA[wiID].s0 ;
        const float longitude_1 = VA[wiID].s1;
        const float latitude_2 = VA[wiID].s2 ;
        const float longitude_2 = VA[wiID].s3;
        const float rad_longitude_1 = longitude_1 * GDC_DEG_TO_RAD ;
        const float rad_latitude_1 = latitude_1 * GDC_DEG_TO_RAD ;
        const float rad_longitude_2 = longitude_2 * GDC_DEG_TO_RAD ;
        const float rad_latitude_2 = latitude_2 * GDC_DEG_TO_RAD ;
        TU1 = GDC_ECCENTRICITY * sinf ( rad_latitude_1 ) /
          cosf ( rad_latitude_1 ) ;
        TU2 = GDC_ECCENTRICITY * sinf ( rad_latitude_2 ) /
          cosf ( rad_latitude_2 ) ;

        CU1 = 1.0f / sqrtf ( TU1 * TU1 + 1.0f ) ;
        SU1 = CU1 * TU1 ;
        CU2 = 1.0f / sqrtf ( TU2 * TU2 + 1.0f ) ;
        dist = CU1 * CU2 ;
        BAZ = dist * TU2 ;
        FAZ = BAZ * TU1 ;
        X = rad_longitude_2 - rad_longitude_1 ;

        do {
          SX = sinf ( X ) ;
          CX = cosf ( X ) ;
          TU1 = CU2 * SX ;
          TU2 = BAZ - SU1 * CU2 * CX ;
          SY = sqrtf ( TU1 * TU1 + TU2 * TU2 ) ;
          CY = dist * CX + FAZ ;
          Y = atan2f ( SY, CY ) ;
          SA = dist * SX / SY ;
          C2A = - SA * SA + 1.0f;
          CZ = FAZ + FAZ ;
          if ( C2A > 0.0f ) CZ = -CZ / C2A + CY ;
          E = CZ * CZ * 2.0f - 1.0f ;
          C = ( ( -3.0f * C2A + 4.0f ) * GDC_FLATTENING + 4.0f ) * C2A *
            GDC_FLATTENING / 16.0f ;
          D = X ;
          X = ( ( E * CY * C + CZ ) * SY * C + Y ) * SA ;
          X = ( 1.0f - C ) * X * GDC_FLATTENING + rad_longitude_2 - rad_longitude_1 ;
        } while ( fabsf ( D - X ) > EPS ) ;

        X = sqrtf ( GDC_ELLIPSOIDAL * C2A + 1.0f ) + 1.0f ;
        X = ( X - 2.0f ) / X ;
        C = 1.0f - X ;
        C = ( X * X / 4.0f + 1.0f ) / C ;
        D = ( 0.375f * X * X - 1.0f ) * X ;
        X = E * CY ;
        dist = 1.0f - E - E ;
        dist = ( ( ( ( SY * SY * 4.0f - 3.0f ) * dist * CZ * D / 6.0f -
                X ) * D / 4.0f + CZ ) * SY * D + Y ) * C * GDC_SEMI_MINOR ;
        VC[wiID] = dist;
      }
    }
  }
}

void verify(unsigned int size, const float *output, const float *expected_output) {
  float error_rate = 0;
  for (unsigned int i = 0; i < size; i++) {
    if (fabs(output[i] - expected_output[i]) > error_rate) {
      error_rate = fabs(output[i] - expected_output[i]);
    }
  }
  printf("The maximum error in distance for single precision is %f\n", error_rate); 
}

int main(int argc, char** argv) {

  int iteration = atoi(argv[1]);

  unsigned int num_cities = 2097152; // 2 ** 21
  unsigned int num_ref_cities = 6; // bombay, melbourne, waltham, moscow, glasgow, morocco
  unsigned int index_map[] ={436483, 1952407, 627919, 377884, 442703, 1863423};
  unsigned int N = num_cities * num_ref_cities;
  unsigned int city = 0;
  float lat, lon;

  const char* filename = "locations.txt";
  printf("Reading city locations from file %s...\n", filename);
  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    perror ("Error opening the file");
    exit(-1);
  }

  float4* input  = (float4*) aligned_alloc(4096, N*sizeof(float4));
  float*  output = (float*) aligned_alloc(4096, N*sizeof(float));
  float*  expected_output = (float*) malloc(N*sizeof(float));

  while (fscanf(fp, "%f %f\n", &lat, &lon) != EOF) { 
    input[city].s0 = lat;
    input[city].s1 = lon;
    city++;
    if (city == num_cities) break;  
  }
  fclose(fp);

  // duplicate for "num_ref_cities"
  for (int c = 1;  c < num_ref_cities; c++) {
    std::copy(input, input+num_cities, input+c*num_cities);
  }
  // each reference city is compared with 'num_cities' cities
  for (int c = 0;  c < num_ref_cities; c++) {
    unsigned index = index_map[c] - 1;
    for(unsigned j = c*num_cities; j < (c+1)*num_cities; ++j) {
      input[j].s2 = input[index].s0;
      input[j].s3 = input[index].s1;
    }
  }

  // run on the host for verification
  for (int i = 0; i < N; i++)
  {
    float lat1 = input[i].s0;
    float lon1 = input[i].s1;
    float lat2 = input[i].s2;
    float lon2 = input[i].s3;
    expected_output[i] = distance_host(i, lat1, lon1, lat2, lon2);
  }

  distance_device(input, output, N, iteration);

  verify(N, output, expected_output);

  free(input);
  free(output);
  free(expected_output);
  return 0;
}
