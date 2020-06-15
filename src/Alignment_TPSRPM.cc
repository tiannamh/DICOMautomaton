//Alignment_TPSRPM.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#ifdef DCMA_USE_EIGEN    
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/Eigenvalues>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/QR>
    #include <eigen3/Eigen/Cholesky>
#endif

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_Rigid.h"
#include "Alignment_TPSRPM.h"


thin_plate_spline::thin_plate_spline(const point_set<double> &ps,
                                     long int k_dim){
    const auto N = ps.points.size();
    this->control_points = ps;
    this->kernel_dimension = k_dim;
    this->W_A = num_array<double>(N + 3 + 1, 3, 0.0); // Initialize to all zeros.

    // Default to an identity affine transform without any warp components.
    this->W_A.coeff(N + 1, 0) = 1.0; // x-component.
    this->W_A.coeff(N + 2, 1) = 1.0; // y-component.
    this->W_A.coeff(N + 3, 2) = 1.0; // z-component.
}

double
thin_plate_spline::eval_kernel(const double &dist) const {
    double out = std::numeric_limits<double>::quiet_NaN();
    if(false){
    }else if(this->kernel_dimension == 2){
        // 2D case.
        //
        // Note: this is the 2D fundamental sol'n to biharmonic equation. It seems to also work well for the 3D case,
        // even better than the actual solution in 3D. Not sure why...
        const auto log_d2 = std::log(dist * dist);
        const auto u = log_d2 * dist * dist;
        // Note: If points overlap exactly, this assumes they are actually infinitesimally separated.
        out = std::isfinite(u) ? u : 0.0;
    }else if(this->kernel_dimension == 3){
        // 3D case.
        //
        // Note: this is the 3D fundamental sol'n to biharmonic equation. It doesn't work well in practice though.
        out = dist;
    }else{
        throw std::invalid_argument("Kernel dimension not currently supported. Cannot continue.");
        // Note: if this is truly desired, the kernel for arbitrary dimensions is available. But often for $3 < D$ the
        // $D = 3$ case is used since evaluation is problematic at the control points.
    }
    return out;
}

vec3<double> 
thin_plate_spline::transform(const vec3<double> &v) const {
    const auto N = static_cast<long int>(this->control_points.points.size());
    Stats::Running_Sum<double> x;
    Stats::Running_Sum<double> y;
    Stats::Running_Sum<double> z;

    // affine component.
    x.Digest(W_A.read_coeff(N + 0, 0));
    x.Digest(W_A.read_coeff(N + 1, 0) * v.x);
    x.Digest(W_A.read_coeff(N + 2, 0) * v.y);
    x.Digest(W_A.read_coeff(N + 3, 0) * v.z);

    y.Digest(W_A.read_coeff(N + 0, 1));
    y.Digest(W_A.read_coeff(N + 1, 1) * v.x);
    y.Digest(W_A.read_coeff(N + 2, 1) * v.y);
    y.Digest(W_A.read_coeff(N + 3, 1) * v.z);

    z.Digest(W_A.read_coeff(N + 0, 2));
    z.Digest(W_A.read_coeff(N + 1, 2) * v.x);
    z.Digest(W_A.read_coeff(N + 2, 2) * v.y);
    z.Digest(W_A.read_coeff(N + 3, 2) * v.z);

    // Warp component.
    for(long int i = 0; i < N; ++i){
        const auto P_i = this->control_points.points[i];
        const auto dist = P_i.distance(v);
        const auto ki = this->eval_kernel(dist);

        x.Digest(W_A.read_coeff(i, 0) * ki);
        y.Digest(W_A.read_coeff(i, 1) * ki);
        z.Digest(W_A.read_coeff(i, 2) * ki);
    }

    const vec3<double> f_v( x.Current_Sum(),
                            y.Current_Sum(),
                            z.Current_Sum() );
    if(!f_v.isfinite()){
        throw std::runtime_error("Failed to evaluate TPS mapping function. Cannot continue.");
    }
    return f_v;
}

void
thin_plate_spline::apply_to(point_set<double> &ps) const {
    for(auto &p : ps.points){
        p = this->transform(p);
    }
    return;
}

bool
thin_plate_spline::write_to( std::ostream &os ) const {
    // Maximize precision prior to emitting any floating-point numbers.
    const auto original_precision = os.precision();
    os.precision( std::numeric_limits<double>::max_digits10 );

    os << this->control_points.points.size() << std::endl; 
    for(const auto &p : this->control_points.points){
        os << p << std::endl;
    }
    
    os << this->kernel_dimension << std::endl; 

    this->W_A.write_to(os);

    os.precision( original_precision );
    os.flush();
    return (!os.fail());
}

bool
thin_plate_spline::read_from( std::istream &is ){
    long int N_control_points = 0;
    is >> N_control_points;
    if( is.fail()
    ||  !isininc(1,N_control_points,1'000'000'000) ){
        FUNCWARN("Number of control points could not be read, or is invalid.");
        return false;
    }
    this->control_points.points.resize(N_control_points);

    try{
        for(long int i = 0; i < N_control_points; ++i){
            is >> this->control_points.points[i];
        }
    }catch(const std::exception &e){
        FUNCWARN("Failed to read control points: " << e.what());
        return false;
    }

    is >> this->kernel_dimension;
    if( is.fail()
    ||  !isininc(2,this->kernel_dimension,3) ){
        FUNCWARN("Kernel dimension could not be read, or is invalid.");
        return false;
    }

    if(!(this->W_A.read_from(is))){
        FUNCWARN("Transformation coefficients could not be read or are invalid.");
        return false;
    }

    if( (this->W_A.num_rows() != (N_control_points + 3 + 1))
    ||  (this->W_A.num_cols() != 3) ){
        FUNCWARN("Transformation coefficient matrix has invalid dimensions.");
        return false;
    }

    return (!is.fail());
}

#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using thin plate splines.
//
// Note that the point sets must be ordered and have the same number of points, and each pair (i.e., the nth moving
// point and the nth stationary point) correspond.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<thin_plate_spline>
AlignViaTPS(AlignViaTPSParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());
    if(N_move_points != N_stat_points){
        FUNCWARN("Unable to perform TPS alignment: point sets have different number of points");
        return std::nullopt;
    }

    thin_plate_spline t(moving, params.kernel_dimension);

    // Prepare working buffers.
    //
    // Main system matrix.
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N_move_points + 4, N_move_points + 4);
    // Corresponding points working buffer.
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_move_points + 4, 3); 

    // TPS model parameters.
    //
    // Will contain the 'warp' component (W) and an affine component (A) coefficients.
    // Note that, to avoid a later copy, these coefficients are directly mapped to the transform buffer.
    if(static_cast<long int>(t.W_A.size()) != (N_move_points + 4) * 3){
        throw std::logic_error("TPS coefficients allocated with incorrect size. Refusing to continue.");
    }
    Eigen::Map<Eigen::Matrix< double,
                              Eigen::Dynamic,
                              Eigen::Dynamic,
                              Eigen::ColMajor >> W_A(&(*(t.W_A.begin())),
                                                     N_move_points + 4,  3);
    if( (t.W_A.num_rows() != W_A.rows())
    ||  (t.W_A.num_cols() != W_A.cols()) ){
        throw std::logic_error("TPS coefficient matrix dimesions do not match. Refusing to continue.");
    }

    // Populate static elements.
    //
    // L matrix: "K" kernel part.
    //
    // Note: "K"s diagonals are later adjusted using the regularization parameter. They are set to zero initially.
    //for(long int i = 0; i < N_move_points; ++i) L(i, i) = params.lambda;
    for(long int i = 0; i < (N_move_points + 4); ++i) L(i, i) = params.lambda;
    for(long int i = 0; i < N_move_points; ++i){
        const auto P_i = moving.points[i];
        for(long int j = i + 1; j < N_move_points; ++j){
            const auto P_j = moving.points[j];
            const auto dist = P_i.distance(P_j);
            const auto kij = t.eval_kernel(dist);
            L(i, j) = kij;
            L(j, i) = kij;
        }
    }

    // L matrix: "P" and "PT" parts.
    for(long int i = 0; i < N_move_points; ++i){
        const auto P_moving = moving.points[i];
        L(i, N_move_points + 0) = 1.0;
        L(i, N_move_points + 1) = P_moving.x;
        L(i, N_move_points + 2) = P_moving.y;
        L(i, N_move_points + 3) = P_moving.z;

        L(N_move_points + 0, i) = 1.0;
        L(N_move_points + 1, i) = P_moving.x;
        L(N_move_points + 2, i) = P_moving.y;
        L(N_move_points + 3, i) = P_moving.z;
    }

    // Fill the Y vector with the corresponding points.
    for(long int j = 0; j < N_stat_points; ++j){ // column
        const auto P_stationary = stationary.points[j];
        Y(j, 0) = P_stationary.x;
        Y(j, 1) = P_stationary.y;
        Y(j, 2) = P_stationary.z;
    }

    // Use pseudo-inverse method.
    if(false){
    }else if(params.solution_method == AlignViaTPSParams::SolutionMethod::PseudoInverse){
        Eigen::MatrixXd L_pinv = L.completeOrthogonalDecomposition().pseudoInverse();

        // Update W_A.
        W_A = L_pinv * Y;

    // Use LDLT method.
    }else if(params.solution_method == AlignViaTPSParams::SolutionMethod::LDLT){
        Eigen::LDLT<Eigen::MatrixXd> LDLT;
        LDLT.compute(L.transpose() * L);
        if(LDLT.info() != Eigen::Success){
            throw std::runtime_error("Unable to update transformation: LDLT decomposition failed.");
        }
        
        W_A = LDLT.solve(L.transpose() * Y);
        if(LDLT.info() != Eigen::Success){
            throw std::runtime_error("Unable to update transformation: LDLT solve failed.");
        }
    }else{
        throw std::logic_error("Solution method not understood. Cannot continue.");
    }

    if(!W_A.allFinite()){
        FUNCWARN("Failed to solve for a finite-valued transform");
        return std::nullopt;
    }

    return t;
}
#endif // DCMA_USE_EIGEN



#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<thin_plate_spline>
AlignViaTPSRPM(AlignViaTPSRPMParams & params,
               const point_set<double> & moving,
               const point_set<double> & stationary ){

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    thin_plate_spline t(moving, params.kernel_dimension);

    // Compute the centroid for the stationary point cloud.
    // Stationary point outliers will be assumed to have this location.
    const auto com_stat = stationary.Centroid();

    // Estimate determinstic annealing parameters.
    //
    // Find the largest 'square distance' between (all) points and the average separation of nearest-neighbour points
    // (in the moving cloud). This info is needed to tune the annealing energy to ensure (1) deformations can initially
    // 'reach' across the point cloud, and (2) deformations are not considered much below the nearest-neighbour spacing
    // (i.e., overfitting).
    double mean_nn_sq_dist = std::numeric_limits<double>::quiet_NaN();
    double max_sq_dist = 0.0;
    {
        FUNCINFO("Locating mean nearest-neighbour separation in moving point cloud");
        Stats::Running_Sum<double> rs;
        {
            //asio_thread_pool tp;
            for(long int i = 0; i < N_move_points; ++i){
                //tp.submit_task([&,i](void) -> void {
                double min_sq_dist = std::numeric_limits<double>::infinity();
                for(long int j = 0; j < N_move_points; ++j){
                    if(i == j) continue;
                    const auto sq_dist = (moving.points[i]).sq_dist( moving.points[j] );
                    if(sq_dist < min_sq_dist) min_sq_dist = sq_dist;
                }
                if(!std::isfinite(min_sq_dist)){
                    throw std::runtime_error("Unable to estimate nearest neighbour distance.");
                }
                rs.Digest(min_sq_dist);
                //}); // thread pool task closure.
            }
        }
        mean_nn_sq_dist = rs.Current_Sum() / static_cast<double>( N_move_points );

        FUNCINFO("Locating max square-distance between all points");
        {
            //asio_thread_pool tp;
            //std::mutex saver_printer;
            for(long int i = 0; i < (N_move_points + N_stat_points); ++i){
                //tp.submit_task([&,i](void) -> void {
                    for(long int j = 0; j < i; ++j){
                        const auto A = (i < N_move_points) ? moving.points[i] : stationary.points[i - N_move_points];
                        const auto B = (j < N_move_points) ? moving.points[j] : stationary.points[j - N_move_points];
                        const auto sq_dist = A.sq_dist(B);
                        if(max_sq_dist < sq_dist){
                            //std::lock_guard<std::mutex> lock(saver_printer);
                            max_sq_dist = sq_dist;
                        }
                    }
                //}); // thread pool task closure.
            }
        } // Wait until all threads are done.
    }

    const double T_start = params.T_start_scale * max_sq_dist;
    const double T_end = params.T_end_scale * mean_nn_sq_dist;
    const double L_1_start = params.lambda_start * std::sqrt( mean_nn_sq_dist );
    const double L_2_start = params.zeta_start * L_1_start;

    if(!isininc(0.00001, params.T_step, 0.99999)){
        throw std::invalid_argument("Temperature step parameter is invalid. Cannot continue.");
    }
    if( (std::abs(T_start) == 0.0)
    ||  (std::abs(T_end) == 0.0)
    ||  (T_start <= T_end) ){
        throw std::invalid_argument("Start or end temperatures are invalid. Cannot continue.");
    }
    if( (L_1_start < 0.0)
    ||  (L_2_start < 0.0) ){
        throw std::invalid_argument("Regularization parameters are invalid. Cannot continue.");
    }
    FUNCINFO("T_start, T_step, and T_end are " << T_start << ", " << params.T_step << ", " << T_end);

    // Prepare working buffers.
    //
    // Main system matrix.
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N_move_points + 4, N_move_points + 4);
    // Corresponding points working buffer.
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_move_points + 4, 3); 
    // Identity matrix used for regularization.
    const Eigen::MatrixXd I_N4 = Eigen::MatrixXd::Identity(N_move_points + 4, N_move_points + 4);

    // Corresponence matrix.
    Eigen::MatrixXd M = Eigen::MatrixXd::Zero(N_move_points + 1, N_stat_points + 1);

    // TPS model parameters.
    //
    // Will contain the 'warp' component (W) and an affine component (A) coefficients.
    //
    // Note: To avoid a later copy, these coefficients are directly mapped to the transform buffer.
    //
    // Note: These are the parameters that get updated during the transformation update phase.
    if(static_cast<long int>(t.W_A.size()) != (N_move_points + 4) * 3){
        throw std::logic_error("TPS coefficients allocated with incorrect size. Refusing to continue.");
    }
    Eigen::Map<Eigen::Matrix< double,
                              Eigen::Dynamic,
                              Eigen::Dynamic,
                              Eigen::ColMajor >> W_A(&(*(t.W_A.begin())),
                                                     N_move_points + 4,  3);
    if( (t.W_A.num_rows() != W_A.rows())
    ||  (t.W_A.num_cols() != W_A.cols()) ){
        throw std::logic_error("TPS coefficient matrix dimesions do not match. Refusing to continue.");
    }

    // Populate static elements.
    //
    // L matrix: "K" kernel part.
    //
    // Note: "K"s diagonals are later adjusted using the regularization parameter. They are set to zero initially.
    for(long int i = 0; i < N_move_points; ++i) L(i, i) = 0.0;
    for(long int i = 0; i < N_move_points; ++i){
        const auto P_i = moving.points[i];
        for(long int j = i + 1; j < N_move_points; ++j){
            const auto P_j = moving.points[j];
            const auto dist = P_i.distance(P_j);
            const auto kij = t.eval_kernel(dist);
            L(i, j) = kij;
            L(j, i) = kij;
        }
    }

    // L matrix: "P" and "PT" parts.
    for(long int i = 0; i < N_move_points; ++i){
        const auto P_moving = moving.points[i];
        L(i, N_move_points + 0) = 1.0;
        L(i, N_move_points + 1) = P_moving.x;
        L(i, N_move_points + 2) = P_moving.y;
        L(i, N_move_points + 3) = P_moving.z;

        L(N_move_points + 0, i) = 1.0;
        L(N_move_points + 1, i) = P_moving.x;
        L(N_move_points + 2, i) = P_moving.y;
        L(N_move_points + 3, i) = P_moving.z;
    }

    // Prime the transformation with an identity affine component and no warp component.
    //
    // Note that the RPM-TPS method gradually progresses from global to local transformations, so if the initial
    // temperature is sufficiently high then something like centroid-matching and PCA-alignment will naturally occur.
    // Conversely, if the temperature is set below the threshold required for global transformations, then only local
    // transformations (waprs) will occur; this may be what the user intends!
    W_A(N_move_points + 1, 0) = 1.0; // x-component.
    W_A(N_move_points + 2, 1) = 1.0; // y-component.
    W_A(N_move_points + 3, 2) = 1.0; // z-component.

    if(params.seed_with_centroid_shift){
        // Seed the affine transformation with the output from a simpler rigid registration.
        auto t_com = AlignViaCentroid(moving, stationary);
        if(!t_com){
            FUNCWARN("Unable to compute centroid seed transformation");
            return std::nullopt;
        }

        W_A(N_move_points + 0, 0) = t_com.value().read_coeff(3,0);
        W_A(N_move_points + 0, 1) = t_com.value().read_coeff(3,1);
        W_A(N_move_points + 0, 2) = t_com.value().read_coeff(3,2);
    }

    // Invert the system matrix.
    //
    // Note: only necessary when regularization is disabled.
    Eigen::MatrixXd L_pinv;
    if( (std::abs(L_1_start) != 0.0)
    &&  (params.solution_method == AlignViaTPSRPMParams::SolutionMethod::PseudoInverse) ){
        L_pinv = L.completeOrthogonalDecomposition().pseudoInverse();
    }

    // Prime the correspondence matrix with uniform correspondence terms.
    for(long int i = 0; i < N_move_points; ++i){ // row
        for(long int j = 0; j < N_stat_points; ++j){ // column
            M(i, j) = 1.0 / static_cast<double>(N_move_points);
        }
    }
    {
        const auto i = N_move_points; // row
        for(long int j = 0; j < N_stat_points; ++j){ // column
            M(i, j) = 0.01 / static_cast<double>(N_move_points);
        }
    }
    for(long int i = 0; i < N_move_points; ++i){ // row
        const auto j = N_stat_points; // column
        M(i, j) = 0.01 / static_cast<double>(N_move_points);
    }
    M(N_move_points, N_stat_points) = 0.0;


    // Update the correspondence.
    //
    // Note: This sub-routine solves for the point cloud correspondence using the current TPS transformation.
    //
    // Note: This sub-routine implements a 'soft-assign' technique for evaluating the correspondence.
    //       It supports outliers in either point cloud set.
    const auto update_correspondence = [&](double T_now, double s_reg) -> void {
        // Non-outlier coefficients.
        Stats::Running_Sum<double> com_moved_x;
        Stats::Running_Sum<double> com_moved_y;
        Stats::Running_Sum<double> com_moved_z;
        for(long int i = 0; i < N_move_points; ++i){ // row
            const auto P_moving = moving.points[i];
            const auto P_moved = t.transform(P_moving); // Transform the point.
            com_moved_x.Digest(P_moved.x);
            com_moved_y.Digest(P_moved.y);
            com_moved_z.Digest(P_moved.z);
            for(long int j = 0; j < N_stat_points; ++j){ // column
                const auto P_stationary = stationary.points[j];
                const auto dP = P_stationary - P_moved;
                M(i, j) = std::sqrt( 1.0 / T_now ) * std::exp( (s_reg / T_now) - dP.Dot(dP) / (2.0 * T_now ) );
            }
        }
        const vec3<double> com_moved( com_moved_x.Current_Sum() / static_cast<double>(N_move_points), 
                                      com_moved_y.Current_Sum() / static_cast<double>(N_move_points), 
                                      com_moved_z.Current_Sum() / static_cast<double>(N_move_points) );

        // Moving outlier coefficients.
        {
            const auto i = N_move_points; // row
            const auto P_moving = com_moved;
            for(long int j = 0; j < N_stat_points; ++j){ // column
                const auto P_stationary = stationary.points[j];
                const auto dP = P_stationary - P_moving; // Note: intentionally not transformed.
                M(i, j) = std::sqrt( 1.0 / T_start ) * std::exp( - dP.Dot(dP) / (2.0 * T_start ) );
            }
        }

        // Stationary outlier coefficients.
        for(long int i = 0; i < N_move_points; ++i){ // row
            const auto P_moving = moving.points[i];
            const auto P_moved = t.transform(P_moving); // Transform the point.
            const auto j = N_stat_points; // column
            const auto P_stationary = com_stat;
            const auto dP = P_stationary - P_moved;
            M(i, j) = std::sqrt( 1.0 / T_start ) * std::exp( - dP.Dot(dP) / (2.0 * T_start ) );
        }

        // Normalize the rows and columns iteratively.
        {
            std::vector<double> row_sums(N_move_points + 1, 0.0);
            std::vector<double> col_sums(N_stat_points + 1, 0.0);
            for(long int norm_iter = 0; norm_iter < 5; ++norm_iter){

                // Tally the current column sums and re-scale the corespondence coefficients.
                for(long int j = 0; j < (N_stat_points+1); ++j){ // column
                    col_sums[j] = 0.0;
                    for(long int i = 0; i < (N_move_points+1); ++i){ // row
                        col_sums[j] += M(i,j);
                    }
                }
                for(long int j = 0; j < (N_stat_points+1); ++j){ // column
                    if(col_sums[j] < 1.0E-5){
                        // Forgo normalization.
                        continue;
                        // If too far away, nominate this point as an outlier.
                        //col_sums[j] += 1.0;
                        //M(N_move_points,j) += 1.0;
                    }
                    for(long int i = 0; i < N_move_points; ++i){ // row, intentionally ignoring the outlier coeff.
                        M(i,j) = M(i,j) / col_sums[j];
                    }
                }
                
                // Tally the current row sums and re-scale the corespondence coefficients.
                for(long int i = 0; i < (N_move_points+1); ++i){ // row
                    row_sums[i] = 0.0;
                    for(long int j = 0; j < (N_stat_points+1); ++j){ // column
                        row_sums[i] += M(i,j);
                    }
                }
                for(long int i = 0; i < (N_move_points+1); ++i){ // row
                    if(row_sums[i] < 1.0E-5){
                        // Forgo normalization.
                        continue;
                        // If too far away, nominate this point as an outlier.
                        //row_sums[i] += 1.0;
                        //M(i,N_stat_points) += 1.0;
                    }
                    for(long int j = 0; j < N_stat_points; ++j){ // column, intentionally ignoring the outlier coeff.
                        M(i,j) = M(i,j) / row_sums[i];
                    }
                }

                //FUNCINFO("On normalization iteration " << norm_iter << " the mean col sum was " << Stats::Mean(col_sums));
                //FUNCINFO("On normalization iteration " << norm_iter << " the mean row sum was " << Stats::Mean(row_sums));
            }
        }

        if(!M.allFinite()){
            throw std::runtime_error("Failed to compute coefficient matrix.");
        }
        return;
    };

    // Update the transformation.
    //
    // Note: This sub-routine solves for the TPS solution using the current correspondence.
    const auto update_transformation = [&](double lambda) -> void {

        // Fill the Y vector with the corresponding points.
        const auto softassign_outlier_min = 1.0E-4;
        for(long int i = 0; i < N_move_points; ++i){
            // Only needed for 'Double-sided outlier handling' approach from Yang et al (2011).
            //
            // TODO: Confirm if this row sum should include the gutter coefficient or not.
            //
            // TODO: Also determine if it is even appropriate to keep this term without adding the 'W' matrix that Yang describes.
            const auto col_sum = std::max(M.row(i).sum(), softassign_outlier_min);

            Stats::Running_Sum<double> c_x;
            Stats::Running_Sum<double> c_y;
            Stats::Running_Sum<double> c_z;
            for(long int j = 0; j < N_stat_points; ++j){ // column
                const auto P_stationary = stationary.points[j];

                // Original formulation from Chui and Rangaran.
                //const auto weight = M(i,j);

                // 'Double-sided outlier handling' approach from Yang et al (2011).
                const auto weight = M(i,j) / col_sum;

                const auto weighted_P = P_stationary * weight;
                c_x.Digest(weighted_P.x);
                c_y.Digest(weighted_P.y);
                c_z.Digest(weighted_P.z);
            }
            Y(i, 0) = c_x.Current_Sum();
            Y(i, 1) = c_y.Current_Sum();
            Y(i, 2) = c_z.Current_Sum();
        }

        // Use pseudo-inverse method.
        if(false){
        }else if(params.solution_method == AlignViaTPSRPMParams::SolutionMethod::PseudoInverse){
            // Update the L matrix inverse using current regularization lambda.
            if(std::abs(L_1_start) != 0.0){
                // TODO: Would it be easier to just add to the diagonal directly rather than allocate a temp R?
                Eigen::MatrixXd R = L + I_N4 * lambda; // Regularized version of L.
                R(N_move_points + 0, N_move_points + 0) = 0.0;
                R(N_move_points + 1, N_move_points + 1) = 0.0;
                R(N_move_points + 2, N_move_points + 2) = 0.0;
                R(N_move_points + 3, N_move_points + 3) = 0.0;

                L_pinv = R.completeOrthogonalDecomposition().pseudoInverse();
            }

            // Update W_A.
            W_A = L_pinv * Y;

        // Use LDLT method.
        }else if(params.solution_method == AlignViaTPSRPMParams::SolutionMethod::LDLT){
            Eigen::MatrixXd LHS;
            if(std::abs(L_1_start) != 0.0){
                // TODO: Would it be easier to just explicitly set the diagonal here?
                LHS = L + I_N4 * lambda; // Regularized version of L.
                LHS(N_move_points + 0, N_move_points + 0) = 0.0;
                LHS(N_move_points + 1, N_move_points + 1) = 0.0;
                LHS(N_move_points + 2, N_move_points + 2) = 0.0;
                LHS(N_move_points + 3, N_move_points + 3) = 0.0;

            }else{
                LHS = L;
            }
            
            Eigen::LDLT<Eigen::MatrixXd> LDLT;
            LDLT.compute(LHS.transpose() * LHS);
            if(LDLT.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: LDLT decomposition failed.");
            }
            
            W_A = LDLT.solve(LHS.transpose() * Y);
            if(LDLT.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: LDLT solve failed.");
            }
        }else{
            throw std::logic_error("Solution method not understood. Cannot continue.");
        }

        if(!W_A.allFinite()){
            throw std::runtime_error("Failed to update transformation.");
        }

        return;
    };

    // Print information about the optimization.
    const auto print_optimizer_progress = [&](double T_now, double /*lambda*/) -> void {

        // Correspondence coefficients.
        //
        // These will approach a binary state (min=0 and max=1) when the temperature is low.
        // Whether these are binary or not fully depends on the temperature, so they can be used to tweak the annealing
        // schedule.
        const auto mean_row_min_coeff = M.rowwise().minCoeff().sum() / static_cast<double>( M.rows() );
        const auto mean_row_max_coeff = M.rowwise().maxCoeff().sum() / static_cast<double>( M.rows() );

        FUNCINFO("Optimizer state: T = " << std::setw(12) << T_now 
                   << ", mean min,max corr coeffs = " << std::setw(12) << mean_row_min_coeff
                   << ", " << std::setw(12) << mean_row_max_coeff );
        return;
    };

/*
    const auto write_to_xyz_file = [&](const std::string &base){
        const auto fname = Get_Unique_Sequential_Filename(base, 6, ".xyz");

        std::ofstream of(fname);
        for(long int i = 0; i < N_move_points; ++i){
            const auto P_moving = moving.points[i];
            const auto P_moved = t.transform(P_moving);
            of << P_moved.x << " " << P_moved.y << " " << P_moved.z << std::endl;
        }
        return;
    };
*/

    // Anneal deterministically.
    for(double T_now = T_start; T_now >= T_end; T_now *= params.T_step){
        // Regularization parameter: controls how smooth the TPS interpolation is.
        const double L_1 = T_now * L_1_start;

        // Regularization parameter: controls bias toward declaring a point an outlier. Chui and Rangarajan recommend
        // setting it "close to zero."
        const double L_2 = T_now * L_2_start;

        for(long int iter_at_fixed_T = 0; iter_at_fixed_T < params.N_iters_at_fixed_T; ++iter_at_fixed_T){

            // Update correspondence matrix.
            update_correspondence(T_now, L_2);

            // Update transformation.
            update_transformation(L_1);
        }

        print_optimizer_progress(T_now, L_1);

//        write_to_xyz_file("warped_tps-rpm_");
    }
//    write_to_xyz_file("warped_tps-rpm_");

    return t;
}
#endif // DCMA_USE_EIGEN

