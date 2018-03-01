
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <algorithm>
#include <string>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"

#include "ImagePartialDerivative.h"

bool ImagePartialDerivative(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any user_data){

    ImagePartialDerivativeUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ImagePartialDerivativeUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //This routine computes first- and second-order partial derivatives (using centered finite differences) along the
    // row- and column-aligned axes, as well as a 'cross' second-order partial derivative. All use pixel coordinates
    // (i.e., ignoring pixel shape/extent and real-space coordinates, which can be found by an appropriate multiplicative
    // factor if desired). The 'cross' partial derivative is:
    //
    //    $\frac{\partial^{2} P(row,col)}{\partial_{row} \partial_{col}}$
    //
    // These derivatives are not directly suitable for physical calculations due to the use of pixel coordinates, but
    // are suitable for boundary visualization and edge detection.

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only.");

    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working = *first_img_it;

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){

                auto newval = std::numeric_limits<float>::quiet_NaN();

                if(false){
                }else if(user_data_s->order == PartialDerivativeOrder::first){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);
                        newval = std::atan2(ra,ca) + M_PI;

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeOrder::Roberts_cross){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::prow_pcol_aligned){
                        newval = first_img_it->prow_pcol_aligned_Roberts_cross(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::nrow_pcol_aligned){
                        newval = first_img_it->nrow_pcol_aligned_Roberts_cross(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->prow_pcol_aligned_Roberts_cross(row, col, chan);
                        const auto ca = first_img_it->nrow_pcol_aligned_Roberts_cross(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto prpca = first_img_it->prow_pcol_aligned_Roberts_cross(row, col, chan);
                        const auto ncpca = first_img_it->nrow_pcol_aligned_Roberts_cross(row, col, chan);
                        newval = std::atan2(ncpca,prpca) - M_PI/8.0 + M_PI; // For consistency with others.
                        newval = std::fmod(newval + 2.0*M_PI, 2.0*M_PI); 

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeOrder::Prewitt){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Prewitt_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Prewitt_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Prewitt_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Prewitt_derivative(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Prewitt_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Prewitt_derivative(row, col, chan);
                        newval = std::atan2(ra,ca) + M_PI;

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeOrder::Sobel){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Sobel_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Sobel_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative(row, col, chan);
                        newval = std::atan2(ra,ca) + M_PI;

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeOrder::Scharr){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Scharr_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Scharr_derivative(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative(row, col, chan);
                        newval = std::atan2(ra,ca) + M_PI;

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeOrder::second){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::cross){
                        newval = first_img_it->cross_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        newval = std::atan2(ra,ca) + M_PI;

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }
                }else{
                    throw std::invalid_argument("Unrecognized user-provided derivative order.");
                }

                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Replace the old image data with the new image data.
    *first_img_it = working;

    //Update the image metadata. 
    std::string img_desc;
    if(false){
    }else if(user_data_s->order == PartialDerivativeOrder::first){
        img_desc += "First-order partial deriv.,";

    }else if(user_data_s->order == PartialDerivativeOrder::Roberts_cross){
        img_desc += "Roberts' cross estimator,";

    }else if(user_data_s->order == PartialDerivativeOrder::Prewitt){
        img_desc += "Prewitt estimator,";

    }else if(user_data_s->order == PartialDerivativeOrder::Sobel){
        img_desc += "Sobel estimator,";

    }else if(user_data_s->order == PartialDerivativeOrder::Scharr){
        img_desc += "Scharr estimator,";

    }else if(user_data_s->order == PartialDerivativeOrder::second){
        img_desc += "Second-order partial deriv.,";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative order.");
    }

    if(false){
    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
        img_desc += " row-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
        img_desc += " column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::prow_pcol_aligned){
        img_desc += " +row,+column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::nrow_pcol_aligned){
        img_desc += " -row,+column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::cross){
        img_desc += " cross";

    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
        img_desc += " magnitude";

    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
        img_desc += " orientation";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative method.");
    }
    img_desc += " (in pixel coord.s)";

    UpdateImageDescription( std::ref(*first_img_it), img_desc );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

