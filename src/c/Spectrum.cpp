/*************************************************************************//**
 * \file Spectrum.cpp
 * AUTHOR: Chris Park, cpp-ified by Barbara Frewen
 * CREATE DATE:  June 22 2006, turned into a class Sept 21, 2010
 * \brief code to support working with spectra
 ****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "objects.h"
#include "Spectrum.h"
#include "peak.h"
#include "utils.h"
#include "mass.h"
#include "parameter.h"
#include "scorer.h"
#include "carp.h"
#include <vector>
#include <string>
#include "MatchFileReader.h"
#include "MSToolkit/Spectrum.h"

using namespace std;

/**
 * Default constructor.
 */
Spectrum::Spectrum() :
   first_scan_(0),
   last_scan_(0),
   precursor_mz_(0),
   min_peak_mz_(0),
   max_peak_mz_(0),
   total_energy_(0),
   has_peaks_(false),
   sorted_by_mz_(false),
   sorted_by_intensity_(false),
   has_mz_peak_array_(false)
{
  mz_peak_array_ = NULL;
}

/**
 * Constructor initializes spectrum with given values.
 */ 
Spectrum::Spectrum
(int               first_scan,   ///< The number of the first scan -in
 int               last_scan,    ///< The number of the last scan -in
 FLOAT_T           precursor_mz, ///< The m/z of the precursor 
 const vector<int>& possible_z,  ///< The possible charge states 
 const char*       filename      ///< Optional filename
 ) : 
   first_scan_(first_scan),
   last_scan_(last_scan),
   precursor_mz_(precursor_mz), 
   min_peak_mz_(0),
   max_peak_mz_(0),
   total_energy_(0),
   filename_(filename),
   has_peaks_(false),
   sorted_by_mz_(false),
   sorted_by_intensity_(false),
   has_mz_peak_array_(false)
 {
  mz_peak_array_ = NULL;

  for (unsigned int idx=0;idx<possible_z.size();idx++) {
    SpectrumZState zstate;
    zstate.setMZ(precursor_mz, possible_z.at(idx));
    zstates_.push_back(zstate);
  }


}

/**
 * Default destructor.
 */
Spectrum::~Spectrum()
{
  free_peak_vector(peaks_);
  
  if(has_mz_peak_array_){
    free(mz_peak_array_);
  }
}

/**
 * \returns the peak iterator that signifies the start of the peaks 
 * in the spectrum
 */
PeakIterator Spectrum::begin() {

  return peaks_.begin();
}

/**
 * \returns the peak iterator that signifies the end of the peaks 
 * in the spectrum
 */
PeakIterator Spectrum::end() {
  return peaks_.end();
}

/**
 * Prints a spectrum object to file in ms2 format.
 */
void Spectrum::print(FILE* file) ///< output file to print at -out
{
  int mass_precision = get_int_parameter("mass-precision");
  fprintf(file, "S\t%06d\t%06d\t%.*f\n", 
         first_scan_,
         last_scan_,
         mass_precision,
         precursor_mz_);

  // print 'I' line
  for(size_t line_idx = 0; line_idx < i_lines_v_.size(); line_idx++){
    fprintf(file, "%s\n", (i_lines_v_[line_idx]).c_str());
  }
  
  // print 'Z', 'D' line
  for(size_t z_idx = 0; z_idx < zstates_.size(); z_idx++){
    fprintf(file, "Z\t%d\t%.*f\n", zstates_[z_idx].getCharge(), mass_precision,
            zstates_[z_idx].getSinglyChargedMass());
    // are there any 'D' lines to print?
    if(z_idx < d_lines_v_.size() ){
      fprintf(file, "%s", d_lines_v_[z_idx].c_str());
    }
  }

  // print peaks
  for(int peak_idx = 0; peak_idx < (int)peaks_.size(); ++peak_idx){
    fprintf(file, "%.2f %.*f\n", 
            get_peak_location(peaks_[peak_idx]),
            mass_precision,
            get_peak_intensity(peaks_[peak_idx]));
  }
}

/**
 * Prints a spectrum in ms2 format with the given intensities instead of the
 * observed peaks.  Assumes intensities are in m/z bins from 0 to
 * max_mz_bin.  Only prints non-zero intensities.
 */
void Spectrum::printProcessedPeaks(
  SpectrumZState& zstate,           ///< print at this charge state
  FLOAT_T* intensities, ///< intensities of new peaks
  int max_mz_bin,       ///< num_bins in intensities
  FILE* file){          ///< print to this file

  int mass_precision = get_int_parameter("mass-precision");

  // print S line
  fprintf(file, "S\t%06d\t%06d\t%.*f\n", 
          first_scan_,
          last_scan_,
          mass_precision,
          precursor_mz_);

  // print I line(s)
  for(size_t line_idx = 0; line_idx < i_lines_v_.size(); line_idx++){
    fprintf(file, "%s\n", (i_lines_v_[line_idx]).c_str());
  }

  // print 'Z', 'D' line
  if( zstate.getCharge() != 0 ){  // print only one charge state
    fprintf(file, "Z\t%d\t%.*f\n", zstate.getCharge(), mass_precision,
            zstate.getSinglyChargedMass());
    // TODO find associated Z line and print
  } else {  // print all charge states

    for(size_t z_idx = 0; z_idx < zstates_.size(); z_idx++){
      fprintf(file, "Z\t%d\t%.*f\n", zstates_[z_idx].getCharge(), mass_precision,
              zstates_[z_idx].getSinglyChargedMass());
      // are there any 'D' lines to print?
      if(z_idx < d_lines_v_.size()){
        fprintf(file, "%s", d_lines_v_[z_idx].c_str());
      }
    }
  }

  // print peaks
  for(int bin_idx = 0; bin_idx < max_mz_bin; bin_idx++){
    if( intensities[bin_idx] != 0 ){
      fprintf(file, "%d %.*f\n", bin_idx, mass_precision, 
              intensities[bin_idx]); 
    }
  }
  return;
}

/**
 * Prints a spectrum object to file in xml format.
 */
void Spectrum::printXml(
  FILE* file,           ///< output file to print at -out
  SpectrumZState& zstate,            ///< charge used for the search -in
  int index              ///< used to output index to file
  ){
  int start_scan = first_scan_;
  int last_scan = last_scan_;
  const char* filepath = filename_.c_str();
  char** name_ext_array = NULL;
  const char* filename = NULL;
  if (filepath == NULL){
    filename = "NA";
  } else {
    name_ext_array = parse_filename_path_extension(filepath, ".ms2");
    filename = name_ext_array[0];
  }
  const char* period = ".";
  std::ostringstream spectrum_id;
  spectrum_id << filename << period << std::setw(5)  << std::setfill('0')
              << start_scan << period << std::setw(5) << std::setfill('0')
              << last_scan << period << zstate.getCharge() ;
  fprintf(file, "    <spectrum_query spectrum=\"%s\" start_scan=\"%i\""
          " end_scan=\"%i\" precursor_neutral_mass=\"%.*f\""
          " assumed_charge=\"%i\" index=\"%i\">\n",
          spectrum_id.str().c_str(),
          start_scan,
          last_scan,
          get_int_parameter("mass-precision"),
          zstate.getNeutralMass(),
          zstate.getCharge(),
          index
          );
  if (name_ext_array != NULL){
    if (name_ext_array[0] != NULL){
      free(name_ext_array[0]);
    }
    if (name_ext_array[1] != NULL){
      free(name_ext_array[1]);
    }
    free(name_ext_array);
  }
  
}


/**
 * Prints a spectrum object to file in sqt format.
 */
void Spectrum::printSqt(
  FILE* file,           ///< output file to print to -out
  int num_matches,      ///< number of peptides compared to this spec -in
  SpectrumZState& zstate            ///< charge used for the search -in
  ){

  fprintf(file,
          "S\t%d\t%d\t%d\t%.1f\t%s\t%.*f\t%.2f\t%.*f\t%d\n", 
          first_scan_, 
          last_scan_,
          zstate.getCharge(), 
          0.0, // FIXME dummy <process time>
          "server", // FIXME dummy <server>
          get_int_parameter("mass-precision"),
          zstate.getNeutralMass(), //this is used in search
          0.0, // FIXME dummy <total intensity>
          get_int_parameter("precision"),
          0.0, // FIXME dummy <lowest sp>
          num_matches);
}

/**
 * Copy constructor.  Deep copy--allocates new peaks for peak vector.
 */
 Spectrum::Spectrum(
  const Spectrum& old_spectrum ///< the spectrum to take values from
  ) :
 first_scan_(old_spectrum.first_scan_),
 last_scan_(old_spectrum.last_scan_),
 precursor_mz_(old_spectrum.precursor_mz_),
 zstates_(old_spectrum.zstates_),
 min_peak_mz_(old_spectrum.min_peak_mz_),
 max_peak_mz_(old_spectrum.max_peak_mz_),
 total_energy_(old_spectrum.total_energy_),
 filename_(old_spectrum.filename_),
 i_lines_v_(old_spectrum.i_lines_v_),
 d_lines_v_(old_spectrum.d_lines_v_),
 has_peaks_(old_spectrum.has_peaks_),
 sorted_by_mz_(old_spectrum.sorted_by_mz_),
 sorted_by_intensity_(old_spectrum.sorted_by_intensity_),
 has_mz_peak_array_(old_spectrum.has_mz_peak_array_)
{

  // copy each peak
  for(int peak_idx=0; peak_idx < (int)old_spectrum.peaks_.size(); ++peak_idx){
    this->addPeak(get_peak_intensity(old_spectrum.peaks_[peak_idx]),
                   get_peak_location(old_spectrum.peaks_[peak_idx])); 
  }

  /*  Should we do this??
  if( old_spectrum.mz_peak_array ){
    populateMzPeakArray();
  }
  */
}

/**
 * Parses a spectrum from a file, either mgf or ms2.
 */
 Spectrum* Spectrum::newSpectrumFromFile(FILE* file, const char* filename)
{
  if (get_boolean_parameter("use-mgf")) {
    return Spectrum::newSpectrumMgf(file, filename);
  } else {
    return Spectrum::newSpectrumMs2(file, filename);
  }
}

/**
 * Parses a spectrum from a file, either mgf or ms2.
 */
bool Spectrum::parseFile(FILE* file, const char* filename)
{
  if (get_boolean_parameter("use-mgf")) {
    return this->parseMgf(file, filename);
  } else {
    return this->parseMs2(file, filename);
  }
}

/**
 * Parses a spectrum from an .mgf file
 * \returns A newly allocated spectrum or NULL on error or EOF.
 */
Spectrum* Spectrum::newSpectrumMgf
(FILE* file, ///< the input file stream -in
 const char* filename) ///< filename of the spectrum
{
  Spectrum* spectrum = new Spectrum();
  if( spectrum->parseMgf(file, filename) ){
    return spectrum;
  } else {
    delete spectrum;
  }
  return false;
}

/**
 * Parses a spectrum from an .mgf file
 * \returns True if successfully parsed or false on error or EOF.
 */
// TODO: figure out a better way to handle spectrum count.  
// MGF doesn't really have
// a defined format for this.  If it does, then the programs that output 
// MGF don't always conform to this format. SJM
bool Spectrum::parseMgf
(FILE* file, ///< the input file stream -in
 const char* filename) ///< filename of the spectrum
{
  // TODO: delete any existing peaks
  static int spec_count = 1;
  char* new_line = NULL;
  int line_length;
  size_t buf_length = 0;
  FLOAT_T location_mz;
  FLOAT_T intensity;
  
  bool begin_found = FALSE;
  bool title_found = FALSE;
  bool charge_found = FALSE;
  bool pepmass_found = FALSE;
  bool peaks_found = FALSE;
  bool end_found = FALSE;
  
  int charge = -1;

  carp(CARP_DEBUG, "parsing MGF Scan");
  
  while( (line_length = getline(&new_line, &buf_length, file)) != -1){
    //scan until BEGIN IONS
    if (strncmp(new_line, "BEGIN IONS", 10) == 0) {
      begin_found = true;
      break;
    }
  }
  
  if (!begin_found) {
    carp(CARP_DEBUG,"Couldn't find any more scans");
    return false;
  }
  
  //scan for the header fields
  while( (line_length = getline(&new_line, &buf_length, file)) != -1){
    if (strncmp(new_line, "TITLE=",6) == 0) {
      title_found = true;
      int first_scan = spec_count;
      int last_scan = spec_count;
      //  TODO : figure out what to do here, the format is dependent 
      // upon the machine i think
      // parse the title line
      
      this->first_scan_ = first_scan;
      this->last_scan_ = last_scan;
    } else if (strncmp(new_line, "CHARGE=",7) == 0) {
      //parse the charge line
 
      char* plus_index = index(new_line,'+');
      *plus_index = '\0';
      carp(CARP_DETAILED_DEBUG,"Parsing %s",(new_line+7));
      charge = atoi(new_line+7);
      
      carp(CARP_DETAILED_DEBUG, "charge:%d", charge);
      
      charge_found = true;
    } else if (strncmp(new_line, "PEPMASS=",8) == 0) {
      //parse the pepmass line
      FLOAT_T pepmass;
      carp(CARP_DETAILED_DEBUG, "Parsing %s",(new_line+8));
      pepmass = atof(new_line+8);
      carp(CARP_DETAILED_DEBUG, "pepmass:%f",pepmass);
      //TODO - check to see if this is correct.
      this->precursor_mz_ = pepmass;
      pepmass_found = true;
    } else if (isdigit(new_line[0])) {
      //no more header lines, peak information is up
      peaks_found = true;
      break;
    } else if (strcmp(new_line, "END IONS") == 0) {
      //we found the end of the ions without any peaks.
      carp(CARP_WARNING,"No peaks found for mgf spectrum");
      return true;
    }
  }
  
  //TODO check to make sure we gleaned the information from
  //the headers.
  
  if (pepmass_found && charge_found) {
    SpectrumZState zstate;
    zstate.setMZ(precursor_mz_, charge);
  }


  //parse peak information
  do {
    if (strncmp(new_line, "END IONS", 8) == 0) {
      //we are done parsing this charged spectrum.
      end_found = true;
      break;
    }
#ifdef USE_DOUBLES
    else if(sscanf(new_line,"%lf %lf", &location_mz, &intensity) == 2)
#else
    else if(sscanf(new_line,"%f %f", &location_mz, &intensity) == 2)
#endif
    {
      carp(CARP_DETAILED_DEBUG,"adding peak %f %f",location_mz, intensity);
      //add the peak to the spectrum object
      this->addPeak(intensity, location_mz);
    } else {
      //file format error.
      carp(CARP_ERROR,
           "File format error\n"
           "At line: %s",
           new_line);
    }
  } while( (line_length = getline(&new_line, &buf_length, file)) != -1);
  
  spec_count++;
  
  if (end_found) {
    //we successfully parsed this spectrum.
    this->filename_ = filename;
    return true;
  } else {
    //something happened, bomb.
    return false;
  }
}

/**
 * Parses a spectrum from an ms2 file.
 * \returns A newly allocated Spectrum or NULL on error or EOF.
 */
Spectrum* Spectrum::newSpectrumMs2
  (FILE* file, ///< the input file stream -in
   const char* filename) ///< filename of the spectrum
{
  Spectrum* spectrum = new Spectrum();
  if( spectrum->parseMs2(file, filename)){
    return spectrum;
  } else {
    delete spectrum;
  }
  return NULL;
}

/**
 * Parses a spectrum from an ms2 file.
 * \returns True if successfully parsed or false on error or EOF.
 */
bool Spectrum::parseMs2
  (FILE* file, ///< the input file stream -in
   const char* filename) ///< filename of the spectrum
{
  long file_index = ftell(file); // stores the location of the current working line in the file
  char* new_line = NULL;
  int line_length;
  size_t buf_length = 0;
  FLOAT_T location_mz;
  FLOAT_T intensity;
  bool record_S = false; // check's if it read S line
  bool record_Z = false; // check's if it read Z line
  bool start_addPeaks = false; // check's if it started reading peaks
  bool file_format = false; // is the file format correct so far
  
  FLOAT_T test_float;
  char test_char;
  
  while( (line_length = getline(&new_line, &buf_length, file)) != -1){
    // checks if 'S' is not the first line
    if((!record_S || (record_S && start_addPeaks)) && 
            (new_line[0] == 'Z' ||  
             new_line[0] == 'I' ||
             new_line[0] == 'D' )){
      file_format = false;
      carp(CARP_ERROR, 
           "Incorrect order of line (S,Z, Peaks)\n"
           "At line: %s", 
           new_line);
      break; // File format incorrect
    }
    // Reads the 'S' line
    else if(new_line[0] == 'S' && !record_S){
      record_S = true;
      if(!this->parseSLine(new_line, buf_length)){
        file_format = false;
        break; // File format incorrect
      }
    }
    // Reads the 'Z' line 
    else if(new_line[0] == 'Z'){
      record_Z = true;
      if(!this->parseZLine(new_line)){
        file_format = false;
        break; // File format incorrect
      }
    }

    // Reads the 'D' line 
    else if(new_line[0] == 'D'){
      if(!this->parseDLine(new_line)){
        file_format = false;
        break; // File format incorrect
      }
    }

    // Reads the 'I' line 
    else if(new_line[0] == 'I'){
      if(!this->parseILine(new_line)){
        file_format = false;
        break; // File format incorrect
      }
    }
    
    // Stops, when encounters the start of next spectrum 'S' line
    else if(new_line[0] == 'S' && start_addPeaks){ // start of next spectrum
      break;
    }

    // *****parse peak line******
    else if(new_line[0] != 'Z' &&  
            new_line[0] != 'I' &&
            new_line[0] != 'D' &&
            new_line[0] != '\n')
      {
        // checks if the peaks are in correct order of lines
        if((!record_Z || !record_S)){
          file_format = false;
          carp(CARP_ERROR,
               "Incorrect order of line (S,Z, Peaks)\n"
               "At line: %s", 
               new_line);
          break; // File format incorrect
        }
        // check for peak line format
        #ifdef USE_DOUBLES
        // test format: does peak line have more than 2 fields
        else if((sscanf(new_line,"%lf %lf %lf",
                        &test_float, &test_float, &test_float) > 2)||
                (sscanf(new_line,"%lf %lf %c",
                        &test_float, &test_float, &test_char) > 2)||
                (sscanf(new_line,"%lf %lf",
                        &test_float, &test_float) != 2))
        #else
        else if((sscanf(new_line,"%f %f %f",
                        &test_float, &test_float, &test_float) > 2)||
                (sscanf(new_line,"%f %f %c",
                        &test_float, &test_float, &test_char) > 2)||
                (sscanf(new_line,"%f %f",
                        &test_float, &test_float) != 2))
        #endif
          {
          file_format = false;
          carp(CARP_ERROR,
               "Incorrect peak line\n"
               "At line: %s", 
               new_line);
          break; // File format incorrect
        }
        // Reads the 'peak' lines, only if 'Z','S' line has been read
        #ifdef USE_DOUBLES
        else if(record_Z && record_S &&
                (sscanf(new_line,"%lf %lf", &location_mz, &intensity) == 2))
        #else
        else if(record_Z && record_S &&
                (sscanf(new_line,"%f %f", &location_mz, &intensity) == 2))
        #endif
        {
          file_format = true;
          start_addPeaks = true;
          this->addPeak(intensity, location_mz);
        }
      }
    // *************************
    file_index = ftell(file); // updates the current working line location
  }

  // set the file pointer back to the start of the next 's' line
  fseek(file, file_index, SEEK_SET);
  myfree(new_line);
  
  // set filename of empty spectrum
  this->filename_ = filename;

  // No more spectrum in .ms file
  if(!record_S && !file_format){
    return false;
  }
  
  // File format incorrect
  if(!file_format){ 
    carp(CARP_ERROR, "Incorrect ms2 file format.");
    return false;
  }
  return true;
}

/**
 * Parses the 'S' line of the a spectrum
 * \returns true if success. false is failure.
 * 
 */
bool Spectrum::parseSLine
  (char* line, ///< 'S' line to parse -in
   int buf_length ///< line length -in
   )
{
  char spliced_line[buf_length];
  int line_index = 0;
  int spliced_line_index = 0;
  int read_first_scan;
  int read_last_scan;
  FLOAT_T read_precursor_mz;
  FLOAT_T test_float;
  char test_char;
  
  // deletes empty space & 0
  while((line[line_index] !='\0') && 
        (line[line_index] == 'S' || 
         line[line_index] == '\t'||
         line[line_index] == ' ' || 
         line[line_index] == '0')){
    ++line_index;
  }
  // reads in line value
  while(line[line_index] !='\0' && 
        line[line_index] != ' ' && 
        line[line_index] != '\t'){
    spliced_line[spliced_line_index] =  line[line_index];
    ++spliced_line_index;
    ++line_index;
  }
  spliced_line[spliced_line_index] =  line[line_index];
  ++spliced_line_index;
  ++line_index;
  // deletes empty space & zeros
  while((line[line_index] !='\0') && 
        (line[line_index] == '\t' || 
         line[line_index] == ' ' || 
         line[line_index] == '0')){
    ++line_index;
  }
  // read last scan & precursor m/z
  while(line[line_index] !='\0'){
    spliced_line[spliced_line_index] =  line[line_index];
    ++spliced_line_index;
    ++line_index;
  }
  spliced_line[spliced_line_index] = '\0';
  
  // check if S line is in correct format
#ifdef USE_DOUBLES
  // test format:S line has more than 3 fields
  if ( (sscanf(spliced_line,"%lf %lf %lf %lf",
               &test_float, &test_float, &test_float, &test_float) > 3) ||
       (sscanf(spliced_line,"%lf %lf %lf %c",
               &test_float, &test_float, &test_float, &test_char) > 3) ||
       (sscanf(spliced_line,"%i %i %lf", // S line is parsed here
               &read_first_scan, &read_last_scan, &read_precursor_mz) != 3)) 
#else
    if ( (sscanf(spliced_line,"%f %f %f %f",
                 &test_float, &test_float, &test_float, &test_float) > 3) ||
         (sscanf(spliced_line,"%f %f %f %c",
                 &test_float, &test_float, &test_float, &test_char) > 3) ||
         (sscanf(spliced_line,"%i %i %f", // S line is parsed here
                 &read_first_scan, &read_last_scan, &read_precursor_mz) != 3)) 
#endif
    {
      carp(CARP_ERROR,"Failed to parse 'S' line:\n %s",line);
      return false;
    }
  first_scan_ = read_first_scan;
  last_scan_ = read_last_scan;
  precursor_mz_ = read_precursor_mz;
  
  return true;
}

/**
 * Parses the 'Z' line of the a spectrum
 * \returns TRUE if success. FALSE is failure.
 * 
 */
bool Spectrum::parseZLine(char* line)  ///< 'Z' line to parse -in
{
  int tokens;
  char line_name;
  int charge;
  FLOAT_T m_h_plus;
  FLOAT_T test_float;
  char test_char;
  
  // check if Z line is in correct format
#ifdef USE_DOUBLES
  if( ((tokens =  // test format: Z line has less than 3 fields
        sscanf(line, "%c %lf %lf", &test_char, &test_float, &test_float)) < 3)
      || ((tokens =   // test format: Z line has more than 3 fields
           sscanf(line, "%c %lf %lf %lf", &test_char, &test_float, &test_float,
                  &test_float)) >  3) 
      || ((tokens =  // test format: Z line has more than 3 fields
           sscanf(line, "%c %lf %lf %c", &test_char, &test_float, &test_float, 
                  &test_char)) >  3) 
      || (tokens = // Z line is parsed here
          sscanf(line, "%c %d %lf", &line_name, &charge, &m_h_plus)) != 3)
#else
    if( ((tokens =  // test format: Z line has less than 3 fields
          sscanf(line, "%c %f %f", &test_char, &test_float, &test_float)) < 3)
        || ((tokens =   // test format: Z line has more than 3 fields
             sscanf(line, "%c %f %f %f", &test_char, &test_float, &test_float,
                    &test_float)) >  3) 
        || ((tokens =  // test format: Z line has more than 3 fields
             sscanf(line, "%c %f %f %c", &test_char, &test_float, &test_float, 
                    &test_char)) >  3) 
        || (tokens = // Z line is parsed here
            sscanf(line, "%c %d %f", &line_name, &charge, &m_h_plus)) != 3)
   #endif
   {
     carp(CARP_ERROR,"Failed to parse 'Z' line:\n %s",line);
     return false;
   }  


  SpectrumZState zstate;
  zstate.setSinglyChargedMass(m_h_plus, charge);

  zstates_.push_back(zstate);

  return true;
 }

/**
 * FIXME currently does not parse D line, just copies the entire line
 * Parses the 'D' line of the a spectrum
 * \returns TRUE if success. FALSE is failure.
 */
bool Spectrum::parseDLine(char* line)  ///< 'D' line to parse -in 
{
  string d_line = line;
  d_lines_v_.push_back(d_line);
  return true;
}

/**
 * FIXME currently does not parse I line, just copies the entire line
 * Parses the 'I' line of the a spectrum
 * \returns TRUE if success. FALSE is failure.
 */
bool Spectrum::parseILine(char* line)  ///< 'I' line to parse -in
{
   string line_str(line);
   // remove the newline (windows or unix style)
   line_str.erase( line_str.find_first_of("\r\n") );
   i_lines_v_.push_back(line_str);

  return TRUE;
}

/**
 * Transfer values from an MSToolkit spectrum to the crux Spectrum.
 * \returns TRUE if success. FALSE is failure.
 */
bool Spectrum::parseMstoolkitSpectrum
  (MSToolkit::Spectrum* mst_spectrum, ///< the input MSToolkit spectrum -in
  const char* filename ///< filename of the spectrum
  ) {

  // clear any existing values
  zstates_.clear();

  free_peak_vector(peaks_);
  i_lines_v_.clear();
  d_lines_v_.clear();
  if( mz_peak_array_ ){ free(mz_peak_array_); }

  MSToolkit::Spectrum* mst_real_spectrum = (MSToolkit::Spectrum*)mst_spectrum;

  //set first_scan, last_scan, and precursor_mz.
  first_scan_ = mst_real_spectrum->getScanNumber();
  last_scan_ = mst_real_spectrum->getScanNumber();
  precursor_mz_ = mst_real_spectrum->getMZ();

  // setfilename of empty spectrum
  filename_ = filename;

  //add all peaks.
  for(int peak_idx = 0; peak_idx < (int)mst_real_spectrum->size(); peak_idx++){
    this->addPeak(mst_real_spectrum->at(peak_idx).intensity,
                   mst_real_spectrum->at(peak_idx).mz);
  }
  
  //add possible charge states.
  if(  mst_real_spectrum->sizeZ() > 0 ){
    for (int z_idx = 0; z_idx < mst_real_spectrum -> sizeZ(); z_idx++) {
      SpectrumZState zstate;
      zstate.setSinglyChargedMass(
        mst_real_spectrum->atZ(z_idx).mz,
        mst_real_spectrum->atZ(z_idx).z);
      zstates_.push_back(zstate);
    }
  } else { // if no charge states detected, decide based on spectrum
    int charge = choose_charge(precursor_mz_, peaks_);

    // add either +1 or +2, +3
    
    if( charge == 1 ){
      SpectrumZState zstate;
      zstate.setMZ(precursor_mz_, 1);
      zstates_.push_back(zstate);

    } else if( charge == 0 ){
      SpectrumZState zstate;
      zstate.setMZ(precursor_mz_, 2);
      zstates_.push_back(zstate);
      zstate.setMZ(precursor_mz_, 3);
      zstates_.push_back(zstate);
    } else {
      carp(CARP_ERROR, "Could not determine charge state for spectrum %d.", 
           first_scan_);
    }
  }

  return true;
}

/**
 * Adds a peak to the spectrum given a intensity and location
 * calls update_spectrum_fields to update num_peaks, min_peak ...
 */
bool Spectrum::addPeak
( FLOAT_T intensity, ///< the intensity of peak to add -in
  FLOAT_T location_mz ///< the location of peak to add -in
  )
{

  PEAK_T* peak = new_peak(intensity, location_mz);
  peaks_.push_back(peak);

  updateFields(intensity, location_mz);
  has_peaks_ = true;
  return true;

}

/**
 * Creates and fills mz_peak_array_, the array of pointers to peaks
 * in the Spectrum's vector of peaks.  Peaks in the array are
 * indexed by ???
 */
void Spectrum::populateMzPeakArray()
{
  if (has_mz_peak_array_ == true){
    return;
  }
  
  int array_length = MZ_TO_PEAK_ARRAY_RESOLUTION * MAX_PEAK_MZ;
  mz_peak_array_ = (PEAK_T**)mymalloc(array_length * sizeof(PEAK_T*));
  for (int peak_idx = 0; peak_idx < array_length; peak_idx++){
    mz_peak_array_[peak_idx] = NULL;
  }
  for(int peak_idx = 0; peak_idx < (int)peaks_.size(); peak_idx++){
    PEAK_T* peak = peaks_[peak_idx];
    FLOAT_T peak_mz = get_peak_location(peak);
    int mz_idx = (int) (peak_mz * MZ_TO_PEAK_ARRAY_RESOLUTION);
    if (mz_peak_array_[mz_idx] != NULL){
      carp(CARP_INFO, "Peak collision at mz %.3f = %i", peak_mz, mz_idx);
      if(get_peak_intensity(mz_peak_array_[mz_idx])< get_peak_intensity(peak)){
        mz_peak_array_[mz_idx] = peak;
      }
    } else {
      mz_peak_array_[mz_idx] = peak; 
    }
  }
  has_mz_peak_array_ = true;
}

/**
 * \returns The closest intensity within 'max' of 'mz' in 'spectrum'
 * NULL if no peak.
 * This should lazily create the data structures within the
 * spectrum object that it needs.
 * TODO: reimplement with faster peak lookup
 */
PEAK_T* Spectrum::getNearestPeak(
  FLOAT_T mz, ///< the mz of the peak around which to sum intensities -in
  FLOAT_T max ///< the maximum distance to get intensity -in
  )
{
  this->populateMzPeakArray(); // for rapid peak lookup by mz

  FLOAT_T min_distance = BILLION;
  int min_mz_idx = (int)((mz - max) * MZ_TO_PEAK_ARRAY_RESOLUTION + 0.5);
  min_mz_idx = min_mz_idx < 0 ? 0 : min_mz_idx;
  int max_mz_idx = (int)((mz + max) * MZ_TO_PEAK_ARRAY_RESOLUTION + 0.5);
  int absolute_max_mz_idx = MAX_PEAK_MZ * MZ_TO_PEAK_ARRAY_RESOLUTION - 1;
  max_mz_idx = max_mz_idx > absolute_max_mz_idx 
    ? absolute_max_mz_idx : max_mz_idx;
  PEAK_T* peak = NULL;
  PEAK_T* nearest_peak = NULL;
  int peak_idx;
  for (peak_idx=min_mz_idx; peak_idx < max_mz_idx + 1; peak_idx++){
    if ((peak = mz_peak_array_[peak_idx]) == NULL){
      continue;
    }
    FLOAT_T peak_mz = get_peak_location(peak);
    FLOAT_T distance = fabs(mz - peak_mz);
    if (distance > max){
      continue;
    }
    if (distance < min_distance){
      nearest_peak = peak;
      min_distance = distance;
    }
  }
  return nearest_peak;
}

/**
 * Updates num_peaks, min_peak_mz, max_peak_mz, total_energy.
 */
void Spectrum::updateFields(
  FLOAT_T intensity, ///< the intensity of the peak that has been added -in
  FLOAT_T location ///< the location of the peak that has been added -in
  )
{
 
  // is new peak the smallest peak
  if(peaks_.size() == 1 || min_peak_mz_ > location){
    min_peak_mz_ = location;
  }
  // is new peak the largest peak
  if(peaks_.size() == 1 || max_peak_mz_ < location){
    max_peak_mz_ = location;
  }
  // update total_energy
  total_energy_ += intensity;
}

/**
 * \returns The number of the first scan.
 */
int Spectrum::getFirstScan()
{
  return first_scan_;
}

/**
 * \returns The number of the last scan.
 */
int Spectrum::getLastScan()
{
  return last_scan_;
}

/**
 * \returns The m/z of the precursor.
 */
FLOAT_T Spectrum::getPrecursorMz()
{
  return precursor_mz_;
}

/**
 * \returns The minimum m/z of all peaks.
 */
FLOAT_T Spectrum::getMinPeakMz()
{
  return min_peak_mz_;
}

/**
 * \returns The maximum m/z of all peaks.
 */
FLOAT_T Spectrum::getMaxPeakMz()
{
  return max_peak_mz_;
}

/**
 * \returns The number of peaks.
 */
int Spectrum::getNumPeaks()
{
  return (int)peaks_.size();
}


/**
 * \returns The sum of intensities in all peaks.
 */
double Spectrum::getTotalEnergy()
{
  return total_energy_;
}

/**
 * \returns A read-only reference to the vector of possible chare
 * states for this spectrum.
 */
const vector<SpectrumZState>& Spectrum::getZStates() {
  return zstates_;
}


/**
 *  Considers the spectrum-charge parameter and returns the
 *  appropriate charge states that should be searched for this
 *  spectrum: all of them or the one selected by the parameter.
 * /returns A vector of charge states to consider for this spectrum.
 */ 
/*
vector<int> Spectrum::getChargesToSearch(){

  vector<int> select_charges;
  const char* charge_str = get_string_parameter_pointer("spectrum-charge");

  
  if( strcmp( charge_str, "all") == 0){ // return full array of charges
    select_charges = possible_z_;
  } else { // return one charge

    int param_charge = atoi(charge_str);
    
    if( (param_charge < 1) || (param_charge > MAX_CHARGE) ){
      carp(CARP_FATAL, "spectrum-charge option must be 1,2,3,.. %d or 'all'.  "
           "'%s' is not valid", MAX_CHARGE, charge_str);
    }
    
    select_charges.push_back(param_charge);
  }
  return select_charges;
}
*/
vector<SpectrumZState> Spectrum::getZStatesToSearch() {

  vector<SpectrumZState> select_zstates;
  const char* charge_str = get_string_parameter_pointer("spectrum-charge");

  
  if( strcmp( charge_str, "all") == 0){ // return full array of charges
    select_zstates = zstates_;
  } else { // return a single charge state.

    int param_charge = atoi(charge_str);
    
    if( (param_charge < 1) || (param_charge > MAX_CHARGE) ){
      carp(CARP_FATAL, "spectrum-charge option must be 1,2,3,.. %d or 'all'.  "
           "'%s' is not valid", MAX_CHARGE, charge_str);
    }

    for (unsigned int zstate_idx=0;zstate_idx < zstates_.size();zstate_idx++) {
      if (zstates_[zstate_idx].getCharge() == param_charge) {
        select_zstates.push_back(zstates_[zstate_idx]);
      }
    }
  }

  return select_zstates;

}


/**
 * \returns The number of possible charge states of this spectrum.
 */
int Spectrum::getNumZStates()
{
  return (int)zstates_.size();
}

/**
 * \returns The intensity of the peak with the maximum intensity.
 */
FLOAT_T Spectrum::getMaxPeakIntensity()
{
  FLOAT_T max_intensity = -1;

  for(int peak_idx = 0; peak_idx < (int)peaks_.size(); ++peak_idx){
    if(max_intensity <= get_peak_intensity(peaks_[peak_idx])){
      max_intensity = get_peak_intensity(peaks_[peak_idx]);
    }
  }
  return max_intensity; 
}


/**
 * Parse the spectrum from the tab-delimited result file.
 *\returns The parsed spectrum, else returns NULL for failed parse.
 */
Spectrum* Spectrum::parseTabDelimited(
  MatchFileReader& file ///< output stream -out
  ) {

  Spectrum* spectrum = new Spectrum();

  spectrum->first_scan_ = file.getInteger(SCAN_COL);
  spectrum->last_scan_ = spectrum->first_scan_;

  spectrum->precursor_mz_ = file.getFloat(SPECTRUM_PRECURSOR_MZ_COL);
  //Is it okay to assign an individual spectrum object for each charge?

  int charge = file.getInteger(CHARGE_COL);

  FLOAT_T neutral_mass = file.getFloat(SPECTRUM_NEUTRAL_MASS_COL);
  
  SpectrumZState zstate;
  zstate.setNeutralMass(neutral_mass, charge);

  spectrum->zstates_.push_back(zstate);


  /*
  TODO : Implement these in the tab delimited file?
  spectrum -> min_peak_mz = file.getFloat("spectrum min peak mz");
  spectrum -> max_peak_mz = file.getFloat("spectrum max peak mz");
  spectrum -> num_peaks = file.getInteger("spectrum num peaks");
  spectrum -> total_energy = file.getInteger("spectrum total energy");
  */

  spectrum->has_peaks_ = false;
  return spectrum;

}

/**
 * Normalize peak intensities so that they sum to unity.
 */
void Spectrum::sumNormalize()
{
  for(int peak_idx = 0; peak_idx < (int)peaks_.size(); peak_idx++){
    PEAK_T* peak = peaks_[peak_idx];
    FLOAT_T new_intensity = get_peak_intensity(peak) / total_energy_;
    set_peak_intensity(peak, new_intensity);
  }
}

/**
 * Populate peaks with rank information.
 */
void Spectrum::rankPeaks()
{
  sort_peaks(peaks_, _PEAK_INTENSITY);
  sorted_by_intensity_ = true;
  sorted_by_mz_ = false;
  int rank = (int)peaks_.size();
  for(int peak_idx = 0; peak_idx < (int) peaks_.size(); peak_idx++){
    PEAK_T* peak = peaks_[peak_idx];
    FLOAT_T new_rank = rank/(float)peaks_.size();
    rank--;
    set_peak_intensity_rank(peak, new_rank); 
  }

}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
