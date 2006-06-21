/*****************************************************************************
 * \file peak.c
 * AUTHOR: William Stafford Noble
 * CREATE DATE: 6/14/04
 * VERSION: $Revision: 1.2 $
 * DESCRIPTION: Object for representing one peak in a spectrum.
 *****************************************************************************/
#include "peak.h"
#include "utils.h"
#include <string.h>

/**
 * \returns A PEAK_T object, 
 * \heap allocated, must be freed using free_peak method
 */
PEAK_T* new_peak (float intensity, float location){
  PEAK_T* fresh_peak =(PEAK_T*)mymalloc(sizeof(PEAK_T));
  fresh_peak->intensity = intensity;
  fresh_peak->location = location;
  return fresh_peak;
}
/**
 * \frees A PEAK_T object
 */
void free_peak (PEAK_T* garbage_peak){
  free(garbage_peak);
}

/**
 * \returns the intensity of PEAK_T object
 */
float peak_intensity(PEAK_T* working_peak){
  return working_peak->intensity;
}

/**
 * \returns the location of PEAK_T object
 */
float peak_location(PEAK_T* working_peak){
  return working_peak->location;
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
