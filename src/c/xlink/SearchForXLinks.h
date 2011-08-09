/**
 * \file SearchForXLinks.h 
 * AUTHOR: Sean McIlwain
 * CREATE DATE: 6 December 2010
 * \brief Object for running search-for-xlinks
 *****************************************************************************/

#ifndef SEARCHFORXLINKS_H
#define SEARCHFORXLINKS_H

#include "MatchSearch.h"
#include "DelimitedFileReader.h"

#include <string>

class SearchForXLinks: public MatchSearch {

 public:

  /**
   * \returns a blank SearchForXLinks object
   */
  SearchForXLinks();
  
  /**
   * Destructor
   */
  ~SearchForXLinks();

  /**
   * main method for SearchForXLinks
   */
  virtual int main(int argc, char** argv);

  //New search main method.
  int xlink_search_main();


  /**
   * \returns the command name for SearchForXLinks
   */
  virtual std::string getName();

  /**
   * \returns the description for SearchForXLinks
   */
  virtual std::string getDescription();

  /**
   * \returns the enum of the application, default MISC_COMMAND
   */
  virtual COMMAND_T getCommand();

  virtual bool needsOutputDirectory();


};


#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
