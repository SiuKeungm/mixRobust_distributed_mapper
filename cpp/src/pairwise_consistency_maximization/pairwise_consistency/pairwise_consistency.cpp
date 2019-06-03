// Copyright (C) 2018 by Pierre-Yves Lajoie <lajoie.py@gmail.com>
//modified by Siukeung <siukeung9527@gmail.com>
#include "pairwise_consistency/pairwise_consistency.h"

namespace pairwise_consistency {

double PairwiseConsistency::getChiSquaredThreshold(){
    double threshold;
    if (nb_degree_freedom_ == 6) {
        switch ((int)(confidence_probability_*100)){
            case 99:
                threshold = 0.872;
                break;
            case 95:
                threshold = 1.635;
                break;
            case 90:
                threshold = 2.20;
                break;
            default:
                std::cerr << std::endl << "Confidence probability of " << confidence_probability_ << " is not supported" << std::endl;
                std::abort();
        }
    } else {
        std::cerr << std::endl << nb_degree_freedom_ << " dof is not supported" << std::endl;
        std::abort();
    }
    return threshold;
}

Eigen::MatrixXi PairwiseConsistency::computeConsistentMeasurementsMatrix() {

    // Determination of the chi squared threshold (numbers from chi-squared table)
    double threshold = getChiSquaredThreshold();

    // Preallocate consistency matrix
    Eigen::MatrixXi consistency_matrix(loop_closures_.size(), loop_closures_.size());

    // Iterate on loop closures
    size_t u = 0;
    for (const auto& loop_closure_1: loop_closures_) {
        size_t v = 0;
        for (const auto& loop_closure_2: loop_closures_) {
            if (u < v) {
                // Extract pose indexes
                size_t i,j,k,l;
                i = loop_closure_1.first;
                k = loop_closure_1.second;
                j = loop_closure_2.first;
                l = loop_closure_2.second;

                // Check if the loop closures are interrobot. 
                // It is the case if {i,j} are elements of trajectory_robot1 and {k,l} are elements of trajectory_robot2.
                // Or the inverse.
                bool is_config_r12 = graph_utils::isInTrajectory(trajectory_robot1_, i) && graph_utils::isInTrajectory(trajectory_robot1_, j) &&
                    graph_utils::isInTrajectory(trajectory_robot2_, k) && graph_utils::isInTrajectory(trajectory_robot2_, l);
                bool is_config_r21 = graph_utils::isInTrajectory(trajectory_robot2_, i) && graph_utils::isInTrajectory(trajectory_robot2_, j) &&
                    graph_utils::isInTrajectory(trajectory_robot1_, k) && graph_utils::isInTrajectory(trajectory_robot1_, l);
                // Compute only if they are interrobot loop closures
                if (is_config_r12 || is_config_r21) {
                    // Extract transforms
                    graph_utils::PoseWithCovariance abZik = (*transforms_interrobot_.transforms.find(loop_closure_1)).second.pose;
                    graph_utils::PoseWithCovariance abZjl = (*transforms_interrobot_.transforms.find(loop_closure_2)).second.pose; 
                    graph_utils::PoseWithCovariance aXij, bXlk;
                    if (is_config_r12) {
                        aXij = composeOnTrajectory(i, j, 1);  
                        bXlk = composeOnTrajectory(l, k, 2); 
                    } else {
                        aXij = composeOnTrajectory(i, j, 2);  
                        bXlk = composeOnTrajectory(l, k, 1); 
                    }
                    // Compute the consistency pose (should be near Identity if consistent)
                    graph_utils::ConsistencyErrorData consistency_error = computeConsistencyError(aXij, bXlk, abZik, abZjl);
                    //std::cout<< consistency_error.second<<std::endl;
                    // Compute the Mahalanobis distance
                    double distance = computeSquaredMahalanobisDistance(consistency_error);
                    // Apply threshold on the chi-squared distribution
                    if (distance < threshold) {
                        consistency_matrix(u,v) = 1;
                    } else {
                        consistency_matrix(u,v) = 0;
                    }
                }
            }
            v++;
        }
        u++;
    }
    //std::cout<< consistency_matrix<<std::endl;

    return consistency_matrix;
}

Eigen::MatrixXi PairwiseConsistency::computeAdjointMatrix() {
    // Determination of the chi squared threshold (numbers from chi-squared table)
    double threshold = getChiSquaredThreshold();

    // Preallocate consistency matrix
    Eigen::MatrixXi consistency_matrix(loop_closures_.size(), loop_closures_.size());
    for(int i = 0; i < loop_closures_.size(); i++){
        consistency_matrix(i,i) = 1;
    }

    // Iterate on loop closures
    size_t u = 0;
    for (const auto& loop_closure_1: loop_closures_) {
        size_t v = 0;
        for (const auto& loop_closure_2: loop_closures_) {
            if (u < v) {
                // Extract pose indexes
                size_t i,j,k,l;
                i = loop_closure_1.first;
                k = loop_closure_1.second;
                j = loop_closure_2.first;
                l = loop_closure_2.second;

                // Check if the loop closures are interrobot.
                // It is the case if {i,j} are elements of trajectory_robot1 and {k,l} are elements of trajectory_robot2.
                // Or the inverse.
                bool is_config_r12 = graph_utils::isInTrajectory(trajectory_robot1_, i) && graph_utils::isInTrajectory(trajectory_robot1_, j) &&
                                     graph_utils::isInTrajectory(trajectory_robot2_, k) && graph_utils::isInTrajectory(trajectory_robot2_, l);
                bool is_config_r21 = graph_utils::isInTrajectory(trajectory_robot2_, i) && graph_utils::isInTrajectory(trajectory_robot2_, j) &&
                                     graph_utils::isInTrajectory(trajectory_robot1_, k) && graph_utils::isInTrajectory(trajectory_robot1_, l);
                // Compute only if they are interrobot loop closures
                if (is_config_r12 || is_config_r21) {
                    // Extract transforms
                    graph_utils::PoseWithCovariance abZik = (*transforms_interrobot_.transforms.find(loop_closure_1)).second.pose;
                    graph_utils::PoseWithCovariance abZjl = (*transforms_interrobot_.transforms.find(loop_closure_2)).second.pose;
                    graph_utils::PoseWithCovariance aXij, bXlk;
                    if (is_config_r12) {
                        aXij = composeOnTrajectory(i, j, 1);
                        bXlk = composeOnTrajectory(l, k, 2);
                    } else {
                        aXij = composeOnTrajectory(i, j, 2);
                        bXlk = composeOnTrajectory(l, k, 1);
                    }
                    // Compute the consistency pose (should be near Identity if consistent)
                    graph_utils::ConsistencyErrorData consistency_error = computeConsistencyError(aXij, bXlk, abZik, abZjl);
                    //std::cout<< consistency_error.second<<std::endl;
                    // Compute the Mahalanobis distance
                    double distance = computeSquaredMahalanobisDistance(consistency_error);
                    // Apply threshold on the chi-squared distribution
                    if (distance < 0.05) {
                        consistency_matrix(u,v) = 1;
                        consistency_matrix(v,u) = 1;

                    } else {
                        consistency_matrix(u,v) = 0;
                        consistency_matrix(v,u) = 0;
                    }
                }
            }
            v++;
        }
        u++;
    }
    std::cout<< "consistency_matrix calculate completed" <<std::endl;

    return consistency_matrix;
}


graph_utils::ConsistencyErrorData PairwiseConsistency::computeConsistencyError(
                                                        const graph_utils::PoseWithCovariance& aXij,
                                                        const graph_utils::PoseWithCovariance& bXlk,
                                                        const graph_utils::PoseWithCovariance& abZik, 
                                                        const graph_utils::PoseWithCovariance& abZjl) {
    /*  Consistency loop : aXij + abZjl + bXlk - abZik
     *
     *  *   :   robot poses
     *  |   :   odometry measurements
     *  --  :   interrobot measurements
     *
     *                  abZik
     *        Xai*---------------->Xbk*
     *         |                    ^
     *         |                    |
     *    aXij |                    | bXlk
     *         |                    |
     *         v                    |
     *        Xaj*---------------->Xbl*
     *                  abZjl
     *
     */
    graph_utils::PoseWithCovariance out1, out2, result;
    graph_utils::poseCompose(aXij, abZjl, out1);
    graph_utils::poseCompose(out1, bXlk, out2);
    graph_utils::poseBetween(abZik, out2, result);
    gtsam::Matrix Hr;
//×××××××××××××××××××××××××××××××××××××××××××***********此处改动协方差的计算方式××××××××********************8
    gtsam::Vector6 consistency_error = gtsam::Pose3::Logmap( result.pose, Hr);
    //result.covariance_matrix = Hr * result.covariance_matrix * Hr.transpose();
    return std::make_pair(consistency_error, result.covariance_matrix);
}
//****************************************************此处改动用协方差的逆×××××××××××××××××****************************
double PairwiseConsistency::computeSquaredMahalanobisDistance(const graph_utils::ConsistencyErrorData& consistency_error) {
    double distance = std::sqrt(  consistency_error.first.transpose() * consistency_error.second.inverse() * consistency_error.first  );
    //double distance = std::sqrt(  consistency_error.first.transpose() * graph_utils::FIXED_COVARIANCE * consistency_error.first);
    return distance;
}

graph_utils::PoseWithCovariance PairwiseConsistency::composeOnTrajectory(const size_t& id1, const size_t& id2, const size_t& robot_id) {
    // Select trajectory
    graph_utils::Trajectory trajectory;
    if (robot_id == 1) {
        trajectory = trajectory_robot1_;
    } else {
        trajectory = trajectory_robot2_;
    }
    
    // Extraction of the poses on the trajectory
    graph_utils::TrajectoryPose pose1 = (*trajectory.trajectory_poses.find(id1)).second;
    graph_utils::TrajectoryPose pose2 = (*trajectory.trajectory_poses.find(id2)).second;
    // Computation of the transformation
    graph_utils::PoseWithCovariance result;
    graph_utils::poseBetween(pose1.pose, pose2.pose, result);
    return result;
}

const graph_utils::LoopClosures& PairwiseConsistency::getLoopClosures() const {
    return loop_closures_;
}

const graph_utils::Transforms& PairwiseConsistency::getTransformsRobot1() const{
    return transforms_robot1_;
}

const graph_utils::Transforms& PairwiseConsistency::getTransformsRobot2() const{
    return transforms_robot2_;
}

const graph_utils::Transforms& PairwiseConsistency::getTransformsInterRobot() const{
    return transforms_interrobot_;
}

}