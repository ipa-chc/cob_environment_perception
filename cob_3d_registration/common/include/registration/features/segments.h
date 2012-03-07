/*
 * segments.h
 *
 *  Created on: Nov 28, 2011
 *      Author: goa-jh
 */

#ifndef SEGMENTS_H_
#define SEGMENTS_H_

#include <pcl/filters/extract_indices.h>

#ifdef VISUALIZE_SEGMENTS_
#include <pcl/visualization/cloud_viewer.h>
#endif

struct EIGEN_ALIGN16 PCAPoint
{
  PCL_ADD_POINT4D; // This adds the members x,y,z which can also be accessed using the point (which is float[4])

  float data_c[9];

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

template<typename Point>
class Keypoints_Segments : public RegKeypointCorrespondence<Point, PCAPoint>
{
  float radius_;
  float thr_;
  float dist_threshold_;

  Point keypoint2point(const PCAPoint &kp) {
    Point p;
    p.x=kp.x;
    p.y=kp.y;
    p.z=kp.z;
    return p;
  }

public:
  Keypoints_Segments():
    radius_(0.05), thr_(0.1), dist_threshold_(0.1)
  {}

  void setRadius(float v) {radius_ = v;}
  void setThreshold(float v) {thr_ = v;}
  void setDisThreshold(float v) {dist_threshold_ = v;}

  void extractFeatures(const pcl::PointCloud<Point>& point_cloud, pcl::PointCloud<Point> &narf_descriptors, pcl::PointCloud<PCAPoint> &mids);
#ifdef VISUALIZE_SEGMENTS_
  void
  show (pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud2)
  {
    //... populate cloud
    pcl::visualization::CloudViewer viewer ("Simple Cloud Viewer");
    viewer.showCloud (cloud);
    viewer.showCloud (cloud2);
    while (!viewer.wasStopped ())
    {
    }
  }
#endif

  virtual bool compute(const pcl::PointCloud<Point> &src, const pcl::PointCloud<Point> &tgt) {
    pcl::PointCloud<Point> dummy, dummy2;
    extractFeatures(src, dummy, this->keypoints_src_);
    extractFeatures(tgt, dummy2, this->keypoints_tgt_);

    ROS_INFO("found %d keypoints in src", this->keypoints_src_.size());
    ROS_INFO("found %d keypoints in tgt", this->keypoints_tgt_.size());

#ifdef VISUALIZE_SEGMENTS_
    show(dummy.makeShared(), dummy2.makeShared());
#endif
  }

  inline Eigen::Vector3f getEigenVector(const PCAPoint &kp, const int r) {
    Eigen::Vector3f ret;
    ret(0) = kp.data_c[3*r+0];
    ret(1) = kp.data_c[3*r+1];
    ret(2) = kp.data_c[3*r+2];
    return ret;
  }

  virtual void getCorrespondences(std::vector<pcl::registration::Correspondence> &correspondences) {

    pcl::registration::Correspondence corr;
    correspondences.clear();

    const float thr = 0.001;
    const float max_dis = 0.5*0.5;

    for(int i=0; i<this->keypoints_src_.size(); i++) {
      int mi=0;
      float f=111111;

      Eigen::Vector3f ev1=getEigenVector(this->keypoints_src_[i],0);
      Eigen::Vector3f ev2=getEigenVector(this->keypoints_src_[i],1);
      Eigen::Vector3f ev3=getEigenVector(this->keypoints_src_[i],2);

      for(int j=0; j<this->keypoints_tgt_.size(); j++) {
        Eigen::Vector3f ev1t=getEigenVector(this->keypoints_tgt_[j],0);
        Eigen::Vector3f ev2t=getEigenVector(this->keypoints_tgt_[j],1);
        Eigen::Vector3f ev3t=getEigenVector(this->keypoints_tgt_[j],2);
        Eigen::Vector3f v=this->keypoints_src_[i].getVector3fMap()-this->keypoints_tgt_[j].getVector3fMap();
        float dis = (std::abs(acosf(ev3.dot(ev3t)/(ev3.norm()*ev3t.norm())))+0.1)
            *(std::abs(ev1.squaredNorm()-ev1t.squaredNorm())+std::abs(ev2.squaredNorm()-ev2t.squaredNorm()))*v.norm();//(ev1-ev1t).squaredNorm()*(ev2-ev2t).squaredNorm()*v.squaredNorm();

        if(dis<f && v.squaredNorm()<max_dis) {
          f=dis;
          mi=j;
        }

      }

      if(f>thr)
        continue;

      corr.indexQuery = i;
      corr.indexMatch = mi;
      corr.distance = f;
      correspondences.push_back(corr);

      Eigen::Vector3f v=this->keypoints_src_[i].getVector3fMap()-this->keypoints_tgt_[mi].getVector3fMap();
      ROS_INFO("corr %d -> %d with %f %f", i, mi, f, v.norm());
    }
  }


  Point getPointForKeypointSrc(const int ind) {return keypoint2point(this->keypoints_src_[ind]);}
  Point getPointForKeypointTgt(const int ind) {return keypoint2point(this->keypoints_tgt_[ind]);}
};

#include "impl/segments.hpp"

#endif /* SEGMENTS_H_ */
