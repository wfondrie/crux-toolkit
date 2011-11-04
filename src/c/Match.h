/**
 * \file Match.h
 * $Revision: 1.24 $ 
 * \brief Object for given a peptide and a spectrum, generate 
 * a preliminary score(ex, Sp)
 ****************************************************************************/
#ifndef MATCH_H
#define MATCH_H

#include "MatchFileWriter.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <set>
#include <map>
#include <ctype.h>
#include <float.h>
#include <unistd.h>
#include "carp.h"
#include "parse_arguments.h"
#include "Spectrum.h"
#include "SpectrumCollection.h"
#include "Ion.h"
#include "IonSeries.h"
#include "crux-utils.h"
#include "objects.h"
#include "parameter.h"
#include "Scorer.h"


/* Global variables */
static const FLOAT_T NOT_SCORED = FLT_MIN;
static const FLOAT_T P_VALUE_NA = -1.0;


class Match {
 protected:
  /**
   * README!
   * Issues on the overall_type field in match struct
   * 
   * Outstanding question: How do you determine the
   * peptide trypticity for multiple protein sources?
   * 
   * For example, if one protein src is tryptic while the other is 
   * not tryptic, what is the peptide trypticity used for feature and 
   * shuffling peptide sequence?
   *
   * Currently, we use the "Tryptic wins all" approach, where
   * if N-terminus is tryptic in any of the src proteins we claim it
   * tryptic on the N terminus. Same applies for C-terminus.
   * Thus, even if no peptide is tryptic in any of its src protein,
   * if there are one src protein where it is N tryptic and another where
   * it is C tryptic, overall we will call the peptide in the match tryptic.
   * 
   * There are for sure other methods, for example to randomly sample the src 
   * protein and consider that protein as its src or to shuffle the flanking 
   * sequence of the peptide in each of peptide and randomly sample from the 
   * shuffled flanking sequence to determine the shuffled peptide's trypticity.
   *
   */
  Spectrum* spectrum_; ///< the spectrum we are scoring with
  Peptide* peptide_;  ///< the peptide we are scoring
  FLOAT_T match_scores_[NUMBER_SCORER_TYPES]; 
    ///< array of scores, one for each type (index with SCORER_TYPE_T) 
  int match_rank_[NUMBER_SCORER_TYPES];  
    ///< rank of this match for each type scored (index with SCORER_TYPE_T)
  int pointer_count_; 
    ///< number of pointers to this match object (when reach 0, free memory)
  FLOAT_T b_y_ion_fraction_matched_; 
    ///< the fraction of the b, y ion matched while scoring for SP
  int b_y_ion_matched_; ///< number of b, y ion matched while scoring SP
  int b_y_ion_possible_; ///< number of possible b, y ion while scoring SP
  bool null_peptide_; ///< Is the match a null (decoy) peptide match?
  char* peptide_sequence_; ///< peptide sequence is that of peptide or shuffled
  MODIFIED_AA_T* mod_sequence_; ///< seq of peptide or shuffled if null peptide
  DIGEST_T digest_;
  SpectrumZState zstate_;
  // post_process match object features
  // only valid when post_process_match is true
  bool post_process_match_; ///< Is this a post process match object?
  FLOAT_T delta_cn_; ///< the difference in top and second Xcorr scores
  FLOAT_T ln_delta_cn_; ///< the natural log of delta_cn
  FLOAT_T ln_experiment_size_; 
     ///< natural log of total number of candidate peptides evaluated
  int num_target_matches_; ///< total target candidates for this spectrum
  int num_decoy_matches_;///< decoy candidates for this spectrum if decoy match
  bool best_per_peptide_; ///< Is this the best scoring PSM for this peptide?

  /**
   * Print one field in the tab-delimited output file, based on column index.
   */
  void printOneMatchField(
    int      column_idx,             ///< Index of the column to print. -in
    MatchCollection* collection,  ///< collection holding this match -in 
    MatchFileWriter*    output_file,            ///< output stream -out
    int     scan_num,               ///< starting scan number -in
    FLOAT_T spectrum_precursor_mz,  ///< m/z of spectrum precursor -in
    int     num_target_matches,     ///< target matches for this spectrum -in
    int     num_decoy_matches, ///< decoy matches (if any) for this spectrum -in
    int     b_y_total,              ///< total b/y ions -in
    int     b_y_matched             ///< Number of b/y ions matched. -in
    );

  void init();

 public:

  /**
   * \returns a new memory allocated match
   */
  Match();

  /**
   *
   */
  Match(Peptide* peptide, ///< the peptide for this match
        Spectrum* spectrum, ///< the spectrum for this match
        SpectrumZState& zstate, ///< the charge/mass of the spectrum
        bool is_decoy);///< is the peptide a decoy or not

  /**
   * free the memory allocated match
   */
  static void freeMatch(
    Match* match ///< the match to free -in
  );

  virtual ~Match();

  /**
   * TODO: should be this be here or parameterized better?
   * shuffle the matches in the array between index start and end-1
   */
  static void shuffleMatches(
    Match** match_array, ///< the match array to shuffle  
    int start_idx,         ///< index of first element to shuffle
    int end_index          ///< index AFTER the last element to shuffle
    );

  /**
   * print the information of the match
   */
  void print(
    FILE* file, ///< output stream -out
    bool output_sequence, ///< should I output peptide sequence -in
    SCORER_TYPE_T output_mode  ///< the output mode -in
    );

  /**
   * \brief Print the match information in sqt format to the given file
   *
   * The main score goes in the position usually holding the xcorr.  The other
   * score goes in the position usually holding the preliminary Sp
   * score.  For searches analyzed by percolator, main and other should
   * be discriminant score and qvalue.
   */
  void printSqt(
    FILE* file      ///< output stream -out
    );



  /**
   * \brief Print the match information in xml format to the given file
   *
   * Prints out the match information in the format described as pep xml.
   * Fills out as much information as available.
   *
   */
  void printXml(
    FILE* output_file,
    const bool* scores_computed
  );

  /**
   * \brief Print the match information in tab delimited format to the given file
   *
   */
  void printTab(
    MatchCollection* collection,  ///< collection holding this match -in 
    MatchFileWriter*    file,                   ///< output stream -out
    int      scan_num,               ///< starting scan number -in
    FLOAT_T  spectrum_precursor_mz,  ///< m/z of spectrum precursor -in
    int      num_targetmatches,      ///< target matches for this spectrum -in
    int      num_decoy_matches ///< decoy matches (if any) for this spectrum -in
    );

  /*******************************************
   * match post_process extension
   ******************************************/
  /**
   * Constructs the 20 feature array that pass over to percolator registration
   *\returns the feature FLOAT_T array
   */
  double* getPercolatorFeatures(
    MatchCollection* match_collection ///< the match collection to iterate -in
    );

  /**
   *
   *\returns a match object that is parsed from the tab-delimited result file
   */
  static Match* parseTabDelimited(
    MatchFileReader& result_file,  ///< the result file to parse PSMs -in
    Database* database, ///< the database to which the peptides are created -in
    Database* decoy_database = NULL ///< optional database with decoy peptides
    );

  /****************************
   * match get, set methods
   ***************************/

  /**
   * Returns a heap allocated peptide sequence of the PSM
   * Sequence may not be the same as for the peptide if this is for a
   * decoy database.
   * User must free the sequence.
   *\returns the match peptide sequence
   */
  char* getSequence();

  /**
   * Returns a heap allocated peptide sequence of the PSM formatted with
   * the flanking amino acids and modifiation symbols.
   *
   * Sequence is in the form of X.SEQ.X where X is the flanking amino
   * acid or - if peptide is at the end of the protein.
   * Sequence may not be the same as for the peptide if this is for a
   * decoy database.
   *\returns The sqt-formatted peptide sequence for this match.
   */
  char* getSequenceSqt();

  /**
   * \brief Returns a newly allocated modified_aa sequence of the PSM
   * User must free the sequence.
   *\returns the match peptide sequence
   */
  MODIFIED_AA_T* getModSequence();

  /**
   * \brief Returns a newly allocated string of sequence including any
     * modifications represented as symbols (*,@,#, etc) following the
   * modified residue. 
   * \returns The peptide sequence of the match including modification
   * characters. 
   */
  char* getModSequenceStrWithSymbols();

  /**
   * \brief Returns a newly allocated string of sequence including any
   * modifications represented as mass values in brackets following the
   * modified residue. If merge_masses is true, the sum of multiple
   * modifications on one residue are printed.  If false, each mass is
   * printed in a comma-separated list.
   * \returns The peptide sequence of the match including modification
   * masses. 
   */
  char* getModSequenceStrWithMasses( 
   bool merge_masses
    );

  /**
   * Must ask for score that has been computed
   *\returns the match_mode score in the match object
   */
  FLOAT_T getScore(
    SCORER_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
    );

  /**
   * sets the match score
   */
  void setScore(
    SCORER_TYPE_T match_mode, ///< the working mode (SP, XCORR) -in
    FLOAT_T match_score ///< the score of the match -in
    );

  /**
   * Must ask for score that has been computed
   *\returns the match_mode rank in the match object
   */
  int getRank(
    SCORER_TYPE_T match_mode ///< the working mode (SP, XCORR) -in
    );

  /**
   * sets the rank of the match
   */
  void setRank(
    SCORER_TYPE_T match_mode, ///< the working mode (SP, XCORR) -in
    int match_rank ///< the rank of the match -in
    );

  /**
   *\returns the spectrum in the match object
   */
  Spectrum* getSpectrum();

  /**
   *\returns the peptide in the match object
   */
  Peptide* getPeptide();

  /**
   * sets the match charge and mass
   */

  void setZState(
    SpectrumZState& zstate
    );

  SpectrumZState& getZState();

  /**
   * gets the match charge
   */

  int getCharge();

  /**
   * gets the match neutral mass
   */
  FLOAT_T getNeutralMass();

  /**
   * sets the match delta_cn
   */
  void setDeltaCn(
    FLOAT_T delta_cn  ///< the delta cn value of PSM -in
    );

  /**
   * gets the match delta_cn
   */
  FLOAT_T getDeltaCn();

  /**
   * sets the match ln_delta_cn
   */
  void setLnDeltaCn(
    FLOAT_T ln_delta_cn  ///< the ln delta cn value of PSM -in
    );

  /**
   * gets the match ln_delta_cn
   */
  FLOAT_T getLnDeltaCn();

  /**
   * sets the match ln_experiment_size
   */
  void setLnExperimentSize(
    FLOAT_T ln_experiment_size ///< the ln_experiment_size value of PSM -in
    );

  /**
   * gets the match ln_experiment_size
   */
  FLOAT_T getLnExperimentSize();

  /**
   * \returns The total number of target matches searched for this spectrum.
   */
  int getTargetExperimentSize();

  /**
   * \returns The total number of decoy matches searched for this
   * spectrum if this is a match to a decoy spectrum.
   */
  int getDecoyExperimentSize();

  /**
   *Increments the pointer count to the match object
   */
  void incrementPointerCount();

  /**
   * gets the match if it is a null_peptide match
   *\returns true if match is null peptide, else false
   */
  bool getNullPeptide();

  /**
   * sets the match b_y_ion info
   */
  void setBYIonInfo(
    Scorer* scorer ///< the scorer from which to extract information -in
    );

  /**
   * gets the match b_y_ion_match
   */
  FLOAT_T getBYIonFractionMatched();

  /**
   * gets the match b_y_ion_matched
   */
  int getBYIonMatched();

  /**
   * gets the match b_y_ion_possible
   */
  int getBYIonPossible();

  /**
   * Set the best-per-peptide Boolean to true.
   */
  void setBestPerPeptide();

};

/**
 * Comparator class for Match objects
 * Used for sorting.
 */
class CompareMatch {
 protected:
  int (*sort_by_)(const void*, const void*);
 public:  
  
  
  CompareMatch(
    int (*sort_by)(const void*, const void*)); ///<sort key

  bool operator()(
    const Match*a, 
    const Match*b
  );

};

/**
 * sort the match array with the corresponding compare method
 */
/*
void sortMatches(
  Match** match_array, ///< the match arrray to sort -in
  int match_total, ///< the total number of match objects -in
  int (*compare_method)(const void*, const void*) ///< the compare method to use -in
  );
*/  

/**
 * sort the match array with the corresponding compare method
 */
void qsortMatch(
  Match** match_array, ///< the match array to sort -in  
  int match_total,  ///< the total number of match objects -in
  int (*compare_method)(const void*, const void*) ///< the compare method to use -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between sp score in match_a and match_b
 */
int compareSp(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between xcorr score in match_a and match_b
 */
int compareXcorr(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between p_value (LOGP_BONF_WEIBULL_XCORR)
 * score in match_a and match_b 
 */
int comparePValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

int comparePercolatorQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
);

int compareQRankerQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between xcorr score in match_a and match_b
 */
int compareQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for qsort
 * \returns the difference between qranker qvalue in match_a and match_b
 */
int compareQRankerQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for PERCOLATOR_SCORE
 * \returns the difference between PERCOLATOR_SCORE score in match_a and match_b
 */
int comparePercolatorScore(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * compare two matches, used for QRANKER_SCORE
 * \returns the difference between QRANKER_SCORE score in match_a and match_b
 */
int compareQRankerScore(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and sp score, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if sp score of match a is less than
 * match b.  1 if scan number and sp are equal, else 0.
 */
int compareSpectrumSp(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and xcorr, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumXcorr(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and q-value, used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumPercolatorQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and qranker q-value, 
 * used for qsort.
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumQRankerQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and percolator score,
 * used for qsort. 
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumPercolatorScore(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and qranker score,
 * used for qsort. 
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumQRankerScore(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and q-value (from the decoys and xcorr score),
 * used for qsort. 
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumDecoyXcorrQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );

/**
 * Compare two matches by spectrum scan number and q-value (from the decoys and weibull est p-values),
 * used for qsort. 
 * \returns -1 if match a spectrum number is less than that of match b
 * or if scan number is same, if score of match a is less than
 * match b.  1 if scan number and score are equal, else 0.
 */
int compareSpectrumDecoyPValueQValue(
  Match** match_a, ///< the first match -in  
  Match** match_b  ///< the scond match -in
  );


/************************************************
 * TODO: Why are these here?
 ************************************************/

/**
 * \brief Counts the number of internal cleavages
 * why is this here?
 */
int get_num_internal_cleavage(
  char* peptide_sequence, 
  ENZYME_T enzyme
);

/**
 * \brief Counts the number of terminal cleavage. Either 0, 1, or 2
 *
 */
int get_num_terminal_cleavage(
  char* peptide_sequence, 
  char flanking_aas_prev,
  char flanking_aas_next,
  ENZYME_T enzyme
);


/**
 * \brief prints both variable and static modifications for 
 *  peptide sequence
 *
 */
void print_modifications_xml(
  char* mod_seq,
  char* sequence,
  FILE* output_file
);

/**
 * \brief takes an empty mapping of index to mass
 * of static mods and a full mapping of var mods
 * to fill up the mapping of static mods
 */
void find_static_modifications(
  std::map<int, double>& static_mods,
  std::map<int, double>& var_mods,
  char* sequence
);

/**
 * \brief takes an empty mapping of index to mass
 * and extract information from mod sequence fill
 * up map
 */
void find_variable_modifications(
 std::map<int, double>& mods,
 char* mod_seq
);


/**
 * \brief Takes a empty set of pairs of strings and a peptide
 *  and fills the set with protein id paired with protein annotation
 *
 */
void get_information_of_proteins(
  std::set<std::pair<char*, char*> >& protein_info,
  Peptide* peptide
);

#endif //MATCH_H

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */