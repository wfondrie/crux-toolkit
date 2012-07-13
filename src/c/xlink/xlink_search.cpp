//#include "xhhc_scorer.h"
//#include "xhhc_ion_series.h"
//#include "xhhc_search.h"
#include "xlink_compute_qvalues.h"

/**
 * \file xlink_search.cpp
 * \brief Object for running search-for-xlinks (new code)
 *****************************************************************************/
#include "SearchForXLinks.h"
#include "XLinkMatch.h"
#include "XLinkMatchCollection.h"
#include "XLinkBondMap.h"
#include "XLinkPeptide.h"


//CRUX INCLUDES
#include "objects.h"
#include "FilteredSpectrumChargeIterator.h"
#include "MatchSearch.h"
#include "OutputFiles.h"
#include "SpectrumCollectionFactory.h"
#include "QRanker.h"

//C++ Includes
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <ctime>



using namespace std;

void buildArguments(
  vector<string>& args_vec, 
  int &argc, 
  char** &argv) {

  argc = args_vec.size();
  argv = new char*[argc];

  argv[0] = (char*)args_vec[0].c_str();
  carp(CARP_INFO, "argv[0]=%s", argv[0]);
  for (int idx = 1; idx < argc; idx++) {
    argv[idx] = (char*)args_vec[idx].c_str();
    carp(CARP_INFO, "argv[%i]=%s", idx, argv[idx]);
  }

}


/**
 * main method for SearchForXLinks that implements to refactored code
 */
int SearchForXLinks::xlinkSearchMain() {
  
  carp(CARP_INFO, "Beginning crux xlink-search-mods");
  

  //int num_missed_cleavages = 0;

  /* Get parameters */
  carp(CARP_INFO, "Getting parameters");
  char* ms2_file = get_string_parameter("ms2 file");
  char* input_file = get_string_parameter("protein database");
  const char* output_directory = get_string_parameter_pointer("output-dir");
  //int top_match = get_int_parameter("top-match");
  XLinkPeptide::setLinkerMass(get_double_parameter("link mass"));
  //int min_weibull_points = get_int_parameter("min-weibull-points");
  bool compute_pvalues = get_boolean_parameter("compute-p-values");

  XLinkBondMap bondmap;

  /* Prepare input, fasta or index */
  carp(CARP_INFO, "Preparing database");
  Index* index = NULL;
  Database* database = NULL;
  int num_proteins = prepare_protein_input(input_file, &index, &database);
  carp(CARP_INFO,"Number of proteins:%d",num_proteins);
  free(input_file);
  PEPTIDE_MOD_T** peptide_mods = NULL;
  int num_peptide_mods = generate_peptide_mod_list( &peptide_mods );

  /* Usually for debugging purposes, print out the database of canddiates */
  if (get_boolean_parameter("xlink-print-db"))
  {
    carp(CARP_INFO, "generating and printing xlink database");
    char* output_directory = get_string_parameter("output-dir");
    ostringstream oss;
    oss << output_directory << "/" << "xlink_peptides.txt";
    string temp = oss.str();
    ofstream peptides_file(temp.c_str());

    peptides_file << setprecision(8);

    XLinkMatchCollection* all_candidates = 
      new XLinkMatchCollection(bondmap, peptide_mods, num_peptide_mods, index, database);

    
    peptides_file << "mass\tsequence"<<endl;

    for (int idx=0;idx < all_candidates->getMatchTotal();idx++) {
      XLinkMatch* candidate = all_candidates->at(idx);
      peptides_file << candidate->getMass(MONO) << "\t";
      peptides_file << candidate->getSequenceString() << endl;
    }
    peptides_file.flush();
    delete all_candidates;
    XLink::deleteAllocatedPeptides();
  }


  int scan_num = 0;

  SpectrumZState zstate;


  //int max_ion_charge = get_max_ion_charge_parameter("max-ion-charge");



  carp(CARP_INFO,"Loading Spectra");
  Spectrum* spectrum = new Spectrum();
  SpectrumCollection* spectra = SpectrumCollectionFactory::create(ms2_file);
  spectra->parse();

  FilteredSpectrumChargeIterator* spectrum_iterator =
    new FilteredSpectrumChargeIterator(spectra);

  /* Prepare output files */
  carp(CARP_INFO, "Preparing output files");
  OutputFiles output_files(this);
  output_files.writeHeaders(num_proteins);

  // main loop over spectra in ms2 file


  int search_count = 0;
  // for every observed spectrum 
  carp(CARP_INFO, "Searching Spectra");


  while (spectrum_iterator->hasNext()) {

    spectrum = spectrum_iterator->next(zstate);
    scan_num = spectrum->getFirstScan();

    carp(CARP_DEBUG,"count %d scan %d charge %d", search_count, scan_num, zstate.getCharge());

    if (search_count % 10 == 0)
      carp(CARP_INFO,"count %d scan %d charge %d", search_count, scan_num, zstate.getCharge());
    search_count++;

    FLOAT_T precursor_mz = spectrum->getPrecursorMz();

    carp(CARP_INFO, "Getting candidates");  

    XLinkMatchCollection* target_candidates = new XLinkMatchCollection(precursor_mz,
                                           zstate,
					   bondmap,
					   index,
					   database,
					   peptide_mods,
					   num_peptide_mods,
					   false);
   

    carp(CARP_INFO, "Done getting candidates:%d", target_candidates->getMatchTotal());
    //target_candidates->setScan(spectrum->getFirstScan());

    if (target_candidates->getMatchTotal() < 1) {
      carp(CARP_INFO, "not enough precursors found, skipping scan %d charge %d", scan_num, zstate.getCharge());
      continue;
    }

    //score targets
    carp(CARP_INFO, "scoring candidates:%d", target_candidates->getMatchTotal());
    target_candidates->scoreSpectrum(spectrum);


    /*
    
    carp(CARP_DEBUG, "Getting decoy candidates");

    XLinkMatchCollection* decoy_candidates = new XLinkMatchCollection();
    target_candidates->shuffle(*decoy_candidates);

    carp(CARP_DEBUG,"scoring decoys");
    decoy_candidates->scoreSpectrum(spectrum);
    */
    /*
    if (compute_pvalues) {

      if (target_candidates->getMatchTotal() >= min_weibull_points) {
        carp(CARP_DEBUG, "Fitting weibull to targets");
        target_candidates->fitWeibull();
        MatchCollection::transferWeibull(target_candidates, decoy_candidates);
      //TODO
      //} else if (target_candidates.size() + decoy_candidates.size() >= min_wiebull_points) {
      //  fit_weibull(target_candidates, decoy_candidates, shift, eta, beta, corr);
      } else {
    

        carp(CARP_DEBUG, "Getting weibull training candidates");
        XLinkMatchCollection* train_target_candidates =
          new XLinkMatchCollection(precursor_mz,
                                                   zstate,
                                                   bondmap,
                                                   index,
                                                   database,
                                                   peptide_mods,
                                                   num_peptide_mods,
                                                   true);
   
        XLinkMatchCollection* train_candidates = new XLinkMatchCollection(*train_target_candidates);
        //get enough weibull training candidates by shuffling.
        carp(CARP_DEBUG,"Shuffling %d:%d", 
          train_candidates->getMatchTotal(), 
          min_weibull_points);
    
        while(train_candidates->getMatchTotal() < min_weibull_points) {
          train_target_candidates->shuffle(*train_candidates);
        }
        carp(CARP_DEBUG, "Have %d training candidates", train_candidates->getMatchTotal());
        carp(CARP_DEBUG, "scoring training points");
        train_candidates->scoreSpectrum(spectrum);
        train_candidates->fitWeibull();
        MatchCollection::transferWeibull(train_candidates, target_candidates);
        MatchCollection::transferWeibull(train_candidates, decoy_candidates);
        delete train_candidates;
        delete train_target_candidates;
      }

      target_candidates->sort(XCORR);


      int nprint = min(top_match,target_candidates->getMatchTotal());

      carp(CARP_DEBUG, "Calculating %d target p-values", nprint);
 
      //calculate pvalues.
      for (int idx=0;idx < nprint;idx++) {
        target_candidates->computeWeibullPValue(idx);
      }

      nprint = min(top_match,(int)decoy_candidates->getMatchTotal());

      carp(CARP_INFO,"Calculating %d decoy p-valuess", nprint);

      decoy_candidates->sort(XCORR);

      for (int idx=0;idx < nprint;idx++) {
        decoy_candidates->computeWeibullPValue(idx);
      }

    } // if (compute_p_values)

*/

    //print out

    vector<MatchCollection*> decoy_vec;
    //decoy_vec.push_back(decoy_candidates);
/*
    if (decoy_candidates->getScoredType(SP) == true) {
      decoy_candidates->populateMatchRank(SP);
    }
    decoy_candidates->populateMatchRank(XCORR);
    decoy_candidates->sort(XCORR);
*/

    carp(CARP_INFO, "Ranking");

    if (target_candidates->getScoredType(SP) == true) {
      target_candidates->populateMatchRank(SP);
    }
    target_candidates->populateMatchRank(XCORR);
    target_candidates->sort(XCORR);

    
    carp(CARP_INFO, "Writing results");
    output_files.writeMatches(
      (MatchCollection*)target_candidates, 
      decoy_vec,
      XCORR,
      spectrum);

    /* Clean up */
    //delete decoy_candidates;
    delete target_candidates;
    XLink::deleteAllocatedPeptides();
    

    //free_spectrum(spectrum);

    carp(CARP_INFO,"Done with spectrum %d", scan_num);
    carp(CARP_INFO, "=====================================");
  } // get next spectrum

  output_files.writeFooters();

  // clean up


  delete spectrum_iterator;
  delete spectra;
  for(int mod_idx = 0; mod_idx < num_peptide_mods; mod_idx++){
    free_peptide_mod(peptide_mods[mod_idx]);
  }
  free(peptide_mods);
  Index::free(index);
  Database::freeDatabase(database);

  //Calculate q-values via p-values from weibull fit.
  if (compute_pvalues) {
    carp(CARP_DEBUG, "Computing Q-Values using P-values");
    xlink_compute_qvalues();
  }

  //Calculate q-values via x-ranker.
  carp(CARP_INFO, "Computing Q-Values using X-Ranker");

  vector<string> xr_args_vec;
  xr_args_vec.push_back("q-ranker");

  xr_args_vec.push_back("--num-hu=7");

  xr_args_vec.push_back("--mu=0.01");

  xr_args_vec.push_back("--wd=1e-7");
/*
  {
    ostringstream oss;
    oss << "--ms2file=";
    oss << ms2_file;
    xr_args_vec.push_back(oss.str());
  }
*/
  {
    ostringstream oss;
    oss << "--xlink-mass=";
    oss << get_double_parameter("link mass");
    xr_args_vec.push_back(oss.str());
  }

  xr_args_vec.push_back("--bootstrap=5");

  {
    ostringstream oss;
    oss << output_directory << "/search.target.txt";
    string target_file = oss.str();
    xr_args_vec.push_back(target_file);
  }
  /*
  {
    ostringstream oss;
    oss << output_directory << "/search.decoy.txt";
    string decoy_file = oss.str();
    xr_args_vec.push_back(decoy_file);
  }
  */
  int xranker_argc;
  char** xranker_argv;
  buildArguments(xr_args_vec, xranker_argc, xranker_argv);

  carp(CARP_INFO, "argc:%i",xranker_argc);

  for (int idx = 0;idx < xranker_argc;idx++) {
    carp(CARP_INFO, "argv[%i]:%s",idx, xranker_argv[idx]);
  }


  QRanker* xranker = new QRanker();
  xranker->main(xranker_argc, xranker_argv);
  delete xranker;

  carp(CARP_INFO, "Elapsed time: %.3g s", wall_clock() / 1e6);
  carp(CARP_INFO, "Finished crux search-for-xlink-mods.");

  return(0);
}

