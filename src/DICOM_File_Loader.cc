//DICOM_File_Loader.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program loads data from DICOM files without involving a PACS or other entity.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
//#include <future>             //Needed for std::async(...)
#include <limits>
#include <cmath>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <pqxx/pqxx>          //PostgreSQL C++ interface.

#include "Structs.h"
#include "Imebra_Shim.h"      //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.
#include "Demarcator.h"       //Needed for Demarcator class.

//#include "Distinguisher.h"    //Needed for Distinguisher class.            //Needed?

#include "YgorDICOMTools.h"   //Needed for Is_File_A_DICOM_File(...);

#include "Serialization.pb.h" //Is generated by the protocol buffer compiler. 

bool Load_From_DICOM_Files( Drover &DICOM_data,
                            std::map<std::string,std::string> & /* InvocationMetadata */,
                            std::string &FilenameLex,
                            std::list<boost::filesystem::path> &Filenames ){


        ////////////////////////////
        // TODO.
        // Remember: *consume* the files that have been loaded, so we can pass through to another file loader type if
        // they fail.
        ////////////////////////////




    return true;
}
