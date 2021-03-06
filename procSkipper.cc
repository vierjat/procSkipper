#include <string.h>
#include <stdio.h>
#include "fitsio.h"

#include <iostream>
#include <sstream>

#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <fstream>
#include <unistd.h>
#include <getopt.h>    /* for getopt_long; standard getopt is in unistd.h */
#include <vector>
#include <algorithm>
#include <ctime>
#include <climits>
#include <cmath>
#include <iomanip>
#include <numeric>

#include "globalConstants.h"

using namespace std;

const long imCols  = 1000;
const long ccdCols = 452*2;
const long nIgnore = 3;
const long nOS     = (imCols/2 - ccdCols/2)-nIgnore-1;
const long lStart  = ccdCols/2+nIgnore;
const long lEnd    = lStart+nOS;
const long rStart  = imCols/2;
const long rEnd    = rStart+nOS;

const int  nMeanTrim = 4;

const int kSaveSamplesFlag = 1;

int deleteFile(const char *fileName){
  cout << yellow;
  cout << "Will overwrite: " << fileName << endl << endl;
  cout << normal;
  return unlink(fileName);
}

bool fileExist(const char *fileName){
  ifstream in(fileName,ios::in);
  
  if(in.fail()){
    //cout <<"\nError reading file: " << fileName <<"\nThe file doesn't exist!\n\n";
    in.close();
    return false;
  }
  
  in.close();
  return true;
}

/*========================================================
  ASCII progress bar
==========================================================*/
void showProgress(unsigned int currEvent, unsigned int nEvent) {

  const int nProgWidth=50;

  if ( currEvent != 0 ) {
    for ( int i=0;i<nProgWidth+8;i++)
      cout << "\b";
  }

  double percent = (double) currEvent/ (double) nEvent;
  int nBars = (int) ( percent*nProgWidth );

  cout << " |";
  for ( int i=0;i<nBars-1;i++)
    cout << "=";
  if ( nBars>0 )
    cout << ">";
  for ( int i=nBars;i<nProgWidth;i++)
    cout << " ";
  cout << "| " << setw(3) << (int) (percent*100.) << "%";
  cout << flush;

}

void printCopyHelp(const char *exeName, bool printFullHelp=false){
  
  if(printFullHelp){
    cout << bold;
    cout << endl;
    cout << "This program process the raw Skipper CCD data. It computes overscan\n"
         << "mean for each sample and subtracts it line by line.\n"
         << "The output file will be a fits image containing the pixel value averaged\n"
         << "over all the samples (after subtration of the corresponding overscan value).\n"
         << "It's also possible to save an additional file that will contain the individual\n"
         << "values of all the samples. The file will have nSamples*nExt extensions.\n";
    cout << normal;
  }
  cout << "==========================================================================\n";
  cout << red;
  cout << "\nUsage:\n";
  cout << "  "   << exeName << " <input file> -o <output filename> \n\n";
  cout << "\nOptions:\n";
  cout << "  -s for saving the individual values of all the samples.\n\n";
  cout << normal;
  cout << blue;
  cout << "For any problems or bugs contact Javier Tiffenberg <javiert@fnal.gov>\n\n";
  cout << normal;
  cout << "==========================================================================\n\n";
}

int procSkipperImage(const char *inFile, const char *outF, const int opt = 0){

  const bool saveSamples = (opt & kSaveSamplesFlag);
  const string sampOutF  = (string("samples_")+string(outF));

  /* Overwrite the output file if it already exist */
  if(fileExist(outF)){
    cout << yellow << "\nThe output file exist. " << normal;
    deleteFile(outF);
  }
  if(fileExist(sampOutF.c_str()) &&saveSamples ){
    cout << yellow << "\nThe output file exist. " << normal;
    deleteFile(sampOutF.c_str());
  }

  fitsfile *infptr;  
  fitsfile *outSampfptr;  
  fitsfile *outMeanfptr;  
  
  int status = 0;  
  int nkeys;
  int nhdu = 0;
  long totpix = 0;
  //char card[81];
  
  fits_open_file(&infptr, inFile, READONLY, &status); /* Open the input file */
  if (status != 0) return(status);
  
  fits_get_num_hdus(infptr, &nhdu, &status); // get the number of HDUs
  
  if(saveSamples) fits_create_file(&outSampfptr, sampOutF.c_str(), &status);/* Create the output file */
  if (status != 0) return(status);
  fits_create_file(&outMeanfptr, outF, &status);/* Create the output file */
  if (status != 0) return(status);
  
  for(int eI=1; eI<=nhdu; ++eI){  /* Main loop through each extension */
    
    int hdutype, bitpix, naxis = 0;
    long naxes[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    fits_movabs_hdu(infptr, eI, &hdutype, &status);
    
    fits_get_img_param(infptr, 9, &bitpix, &naxis, naxes, &status); /* get image dimensions and total number of pixels in image */
    totpix = naxes[0] * naxes[1] * naxes[2] * naxes[3] * naxes[4] * naxes[5] * naxes[6] * naxes[7] * naxes[8];
    
    if (hdutype != IMAGE_HDU || naxis == 0 || totpix == 0){
      /* just copy tables and null images */
      fits_copy_hdu(infptr, outMeanfptr, 0, &status);
      if (status != 0) return(status);
      
      if(saveSamples){
        fits_copy_hdu(infptr, outSampfptr, 0, &status);
        if (status != 0) return(status);  
      } 

    }
    else{
      fits_get_hdrspace(infptr, &nkeys, NULL, &status);
      long fpixel[2] = {1,1};
      double nulval = 0.;
      int anynul = 0;
      double* inArray = new double[totpix];
      fits_read_pix(infptr, TDOUBLE, fpixel, totpix, &nulval, inArray, &anynul, &status);


      /* create output image */
      const int nSamples = naxes[0]/imCols;
      long totpixSkp = totpix/nSamples;
      double* outArray = new double[totpixSkp];
      double* outMeanArray = new double[totpixSkp];
      for (int j = 0; j < totpixSkp; ++j) outMeanArray[j] = 0;
      naxes[0] /= nSamples;
      const int nRows = naxes[1];
      
      double osAuxV[nOS];
      for (int s = 0; s < nSamples; ++s){
        for (int j = 0; j < totpixSkp; ++j) outArray[j]=inArray[nSamples*j+s];
        for (int r = 0; r < nRows; ++r){
          double* rowPtr = outArray+imCols*r;
          /* compute stable mean for the OS pixels */
          // left side
          partial_sort_copy(rowPtr+lStart, rowPtr+lEnd, osAuxV, osAuxV+nOS);
          double lMean = (nOS>2*nMeanTrim)? accumulate(osAuxV+nMeanTrim, osAuxV+(nOS-nMeanTrim), 0.0) : accumulate(osAuxV, osAuxV, 0.0);
          lMean = (nOS>4)? lMean/(nOS-nMeanTrim*2) : lMean/nOS;
          // right side
          partial_sort_copy(rowPtr+rStart, rowPtr+rEnd, osAuxV, osAuxV+nOS);
          double rMean = (nOS>2*nMeanTrim)? accumulate(osAuxV+nMeanTrim, osAuxV+(nOS-nMeanTrim), 0.0) : accumulate(osAuxV, osAuxV, 0.0);
          rMean = (nOS>4)? rMean/(nOS-nMeanTrim*2) : rMean/nOS;
          // subtract OS mean for each sample
          for (int c = 0; c < imCols/2; ++c){
            *(rowPtr+c) -= lMean;
            *(rowPtr+imCols/2+c) -= rMean;
          }
        }
        for (int j = 0; j < totpixSkp; ++j) outMeanArray[j] += outArray[j];
        if(saveSamples){
          fits_create_img(outSampfptr, -32, naxis, naxes, &status);
          fits_movabs_hdu(outSampfptr, (eI-2)*nSamples+s+2+eI-2, &hdutype, &status);
          fits_write_pix(outSampfptr, TDOUBLE, fpixel, totpixSkp, outArray, &status);
        }

        if(gVerbosity) showProgress(s + (eI-1)*nSamples, nhdu*nSamples);
      }
      for (int j = 0; j < totpixSkp; ++j) outMeanArray[j] /= nSamples;
      if(saveSamples){
        fits_create_img(outSampfptr, -32, naxis, naxes, &status);
        fits_movabs_hdu(outSampfptr, (eI-1)*nSamples+2+eI-2, &hdutype, &status);
        fits_write_pix(outSampfptr, TDOUBLE, fpixel, totpixSkp, outMeanArray, &status);
      }

      fits_create_img(outMeanfptr, -32, naxis, naxes, &status);
      fits_movabs_hdu(outMeanfptr, eI, &hdutype, &status);
      fits_write_pix(outMeanfptr, TDOUBLE, fpixel, totpixSkp, outMeanArray, &status);

      delete[] inArray;
      delete[] outArray;
      delete[] outMeanArray;
    }
  }

  fits_close_file(infptr,   &status);
  fits_close_file(outMeanfptr,  &status);
  if(saveSamples) fits_close_file(outSampfptr,  &status);

  if(gVerbosity){
    showProgress(1, 1);
  }
  return status;
}


double mean(const double *v, const int &N){
  
  std::vector<double> temparray(v, v+N);
  std::sort(temparray.begin(), temparray.end());
  
  const int nMin = N/3;
  const int nMax = 2*N/3;
  
  double sum=0;
  for(int i=nMin;i<nMax;++i){
    sum+=temparray[i];
  }
  return sum/(nMax-nMin);
}


bool isSaturated(const double &pixVal, const int &bitpix, const double &bzero){
  
  float saturationVal = 0;
  
  switch(bitpix) {
      case BYTE_IMG:
          saturationVal = 128+bzero;
          break;
      case SHORT_IMG:
          saturationVal = 32768+bzero;
          break;
      default:
          saturationVal = kSatValue;
  }
  
  if(pixVal>=saturationVal*kSatMargin)
    return true;
  
  return false;
}

void checkArch(){
  if(sizeof(float)*CHAR_BIT!=32 || sizeof(double)*CHAR_BIT!=64){
    cout << red;
    cout << "\n ========================================================================================\n";
    cout << "   WARNING: the size of the float and double variables is non-standard in this computer.\n";
    cout << "   The program may malfunction or produce incorrect results\n";
    cout << " ========================================================================================\n";
    cout << normal;
  }
}

int processCommandLineArgs(const int argc, char *argv[], string &inFile, string &outFile, int &flags){
  
  if(argc == 1) return 1;
  
  bool outFileFlag = false;
  int opt=0;
  while ( (opt = getopt(argc, argv, "i:o:sqQhH?")) != -1) {
    switch (opt) {
    case 'o':
      if(!outFileFlag){
        outFile = optarg;
        outFileFlag = true;
      }
      else{
        cerr << red << "\nError, can not set more than one output file!\n\n" << normal;
        return 2;
      }
      break;
    case 's':
      flags |= kSaveSamplesFlag;
      break;
    case 'Q':
    case 'q':
      gVerbosity = 0;
      break;
    case 'h':
    case 'H':
    default: /* '?' */
      return 1;
    }
  }
  
  if(!outFileFlag){
    cerr << red << "\nError: output filename missing.\n" << normal;
    return 2;
  }

  inFile="";
  
  if(argc-optind==0){
    cerr << red << "Error: no input file provided!\n\n" << normal;
    return 1;
  }
  else if(argc-optind>1){
    cerr << red << "Error: more than one input file provided!\n\n" << normal;
    return 1;
  }
  
  inFile=argv[optind];
  if(!fileExist(inFile.c_str())){
    cout << red << "\nError reading input file: " << inFile <<"\nThe file doesn't exist!\n\n" << normal;
    return 1;
  }
  
  return 0;
}

int main(int argc, char *argv[])
{
  time_t start,end;
  double dif;
  time (&start);

  string outFile;
  string inFile;
  int opt = 0;
  
  int returnCode = processCommandLineArgs( argc, argv, inFile, outFile, opt);
  if(returnCode!=0){
    if(returnCode == 1) printCopyHelp(argv[0],true);
    if(returnCode == 2) printCopyHelp(argv[0]);
    return returnCode;
  }
  
  if(gVerbosity){
    cout << bold << "\nWill read the following file:\n" << normal;
    cout << "\t" << inFile << endl;
    cout << bold << "\nThe output will be saved in the file:\n\t" << normal << outFile << endl;
  }

  int status  = procSkipperImage(inFile.c_str(), outFile.c_str(), opt);

  if (status != 0){ 
    fits_report_error(stderr, status);
    return status;
  }
  
  /* Report */
  time (&end);
  dif = difftime (end,start);
  if(gVerbosity) cout << green << "\nAll done!\n" << bold << "-> It took me " << dif << " seconds to do it!\n\n" << normal;

  // return status;
  return 0;
}


