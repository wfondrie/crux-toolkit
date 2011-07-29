/**
 * \file Protein.h 
 * $Revision: 1.25 $
 * \brief Object for representing one protein sequence.
 *****************************************************************************/
#ifndef PROTEIN__H 
#define PROTEIN__H

#include <stdio.h>
#include "utils.h"
#include "objects.h"
#include "peptide.h"
#include "peptide_src.h"
#include "carp.h"
#include "PeptideConstraint.h"


/* CHRIS This is probably an object for which you can crib code for from an outside source. Even from in-house (like Charles).*/

class Protein {
 protected:
  Database*  database_; ///< Which database is this protein part of
  unsigned long int offset_; ///< The file location in the database source file
  unsigned int protein_idx_; ///< The index of the protein in it's database.
  bool    is_light_; ///< is the protein a light protein?
  bool    is_memmap_; ///< is the protein produced from memory mapped file
  char*              id_; ///< The protein sequence id.
  char*        sequence_; ///< The protein sequence.
  unsigned int   length_; ///< The length of the protein sequence.
  char*      annotation_; ///< Optional protein annotation.

  /**
   * Find the beginning of the next sequence, and read the sequence ID
   * and the comment.
   */
  bool readTitleLine
    (FILE* fasta_file,
     char* name,
     char* description);

  
  /****************************************************************************
   * Read raw sequence until a '>' is encountered or too many letters
   * are read.  The new sequence is appended to the end of the given
   * sequence.
   *
   * Return: Was the sequence read completely?
   ****************************************************************************/
  static bool readRawSequence
    (FILE* fasta_file,   // Input Fasta file.
     char* name,         // Sequence ID (used in error messages).
     unsigned int   max_chars,    // Maximum number of allowed characters.
     char* raw_sequence, // Pre-allocated sequence.
     unsigned int* sequence_length // the sequence length -chris added
   );
 
 public:

  void init();
  /**
   * \returns An (empty) protein object.
   */
  Protein();

  /**
   * \returns A new protein object(heavy).
   */
  Protein(
    const char*         id, ///< The protein sequence id.
    const char*   sequence, ///< The protein sequence.
    unsigned int length, ///< The length of the protein sequence.
    const char* annotation,  ///< Optional protein annotation.  -in
    unsigned long int offset, ///< The file location in the source file in the database -in
    unsigned int protein_idx, ///< The index of the protein in it's database. -in
    Database* database ///< the database of its origin
  );         

  /**
   * \returns A new light protein object.
   */
  static Protein* newLightProtein(
    unsigned long int offset, ///< The file location in the source file in the database -in
    unsigned int protein_idx ///< The index of the protein in it's database. -in
  );

  /**
   * convert light protein to heavy, by parsing all the sequence from fasta file
   * \returns TRUE if successfully converts the protein to heavy 
   */
  bool toHeavy();

  /**
   * covert heavy protein back to light
   * \returns TRUE if successfully converts the protein to light
   */
  bool toLight();

  /**
   * Frees an allocated protein object.
   */
  
  virtual ~Protein();

  /**
   * Prints a protein object to file.
   */
  void print(
    FILE* file ///< output stream -out
  );

  /**
   * Copies protein object src to dest.
   * dest must be a heap allocated object 
   */
  static void copy(
    Protein* src,///< protein to copy -in
    Protein* dest ///< protein to copy to -out
  );

  /**
   * Parses a protein from an open (FASTA) file.
   * \returns TRUE if success. FALSE is failure.
   */
  bool parseProteinFastaFile(
    FILE* file ///< fasta file -in
  );

  /**
   * Parses a protein from an memory mapped binary fasta file
   * the protein_idx field of the protein must be added before or after you parse the protein
   * \returns TRUE if success. FALSE is failure.
   * protein must be a heap allocated
   * 
   * Assume memmap pointer is set at beginning of protein
   * Assume protein binary format
   * <int: id length><char: id><int: annotation length><char: annotation><int: sequence length><char: sequence>
   *
   * modifies the *memmap pointer!
   */
  bool parseProteinBinaryMemmap(
    char** memmap ///< a pointer to a pointer to the memory mapped binary fasta file -in
  );

  /**
   * Change the sequence of a protein to be a randomized version of
   * itself.  The method of randomization is dependant on the
   * decoy_type (shuffle or reverse).  The name of the protein is also
   * changed by prefixing with reverse_ or rand_, depending on how it
   * was randomized. 
   */
  void shuffle(DECOY_TYPE_T decoy_type);

  /** 
   * Access routines of the form get_<object>_<field> and set_<object>_<field>. 
   * FIXME Chris, could you create the get and set methods for the object fields?
   */

  /**
   * Additional get and set methods
   */

  /*PEPTIDE_T** get_protein_peptides(PROTEIN_T* protein, PEPTIDE_CONSTRAINT*
   * peptide_constraint);*/

  /**
   *\returns the id of the protein
   * returns a heap allocated new copy of the id
   * user must free the return id
   */
  char* getId();

  /**
   *\returns a pointer to the id of the protein
   */
  char* getIdPointer();

  /**
   * sets the id of the protein
   */
  void setId(
    const char* id ///< the sequence to add -in
    );

  /**
   *\returns the sequence of the protein
   * returns a heap allocated new copy of the sequence
   * user must free the return sequence 
   */
  char* getSequence();

  /**
   *\returns a pointer to the sequence of the protein
   */
  char* getSequencePointer();

  /**
   * sets the sequence of the protein
   */
  void setSequence(
    const char* sequence ///< the sequence to add -in
  );

  /**
   *\returns the length of the protein
   */
  unsigned int getLength();

  /**
   * sets the id of the protein
   */
  void setLength(
    unsigned int length ///< the length to add -in
  );

  /**
   *\returns the annotation of the protein
   * returns a heap allocated new copy of the annotation
   * user must free the return annotation
   */
  char* getAnnotation();

  /**
   * sets the annotation of the protein
   */
  void setAnnotation(
    const char* annotation ///< the sequence to add -in
  );


  /**
   * sets the offset of the protein in the fasta file
   */
  void setOffset(
    unsigned long int offset ///< The file location in the source file in the database -in
    );

  /**
   *\returns the offset the protein
   */
  unsigned long int getOffset();

  /**
   * sets the protein_idx (if, idx=n, nth protein in the fasta file)
   */
  void setProteinIdx(
    unsigned int protein_idx ///< The index of the protein in it's database. -in
  );

  /**
   *\returns the protein_idx field
   */
  unsigned int getProteinIdx();

  /**
   * sets the is_light field (is the protein a light protein?)
   */
  void setIsLight(
    bool is_light ///< is the protein a light protein? -in
    );

  /**
   *\returns TRUE if the protein is light protein
   */
  bool getIsLight();

  /**
   * sets the database for protein
   */
  void setDatabase(
    Database*  database ///< Which database is this protein part of -in
    );

  /**
   *\returns Which database is this protein part of
   */
  Database* getDatabase();

  /**
   * prints a binary representation of the protein
   * 
   * FORMAT
   * <int: id length><char: id><int: annotation length><char: annotation><int: sequence length><char: sequence>
   *
   * make sure when rading the binary data, add one to the length so that it will read in the terminating char as well
   */
  void serialize(
    FILE* file ///< output stream -out
    );

};

/** 
 * Comparison function for sorting proteins by protein id.
 */
bool protein_id_less_than(Protein* protein_one, Protein* protein_two);


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
#endif
