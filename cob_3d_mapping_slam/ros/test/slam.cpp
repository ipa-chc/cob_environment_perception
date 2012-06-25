/*
 * slam.cpp
 *
 *  Created on: 27.05.2012
 *      Author: josh
 */



#define DEBUG_

#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/registration/transforms.h>

#include <cob_3d_mapping_slam/curved_polygons/debug_interface.h>

#include <cob_3d_mapping_slam/slam/context.h>
#include <cob_3d_mapping_slam/slam/node.h>

#include <cob_3d_mapping_slam/dof/tflink.h>
#include <cob_3d_mapping_slam/dof/dof_uncertainty.h>
#include <cob_3d_mapping_slam/dof/dof_variance.h>

#include <cob_3d_mapping_slam/slam/dummy/objctxt.h>
#include <cob_3d_mapping_slam/slam/dummy/registration.h>
#include <cob_3d_mapping_slam/slam/dummy/key.h>
#include <cob_3d_mapping_slam/slam/dummy/robot.h>

#include <cob_3d_mapping_slam/curved_polygons/objctxt.h>
#include <cob_3d_mapping_slam/curved_polygons/key.h>

#include <cob_3d_mapping_msgs/ShapeArray.h>
#include "gnuplot_i.hpp"
#include <gtest/gtest.h>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

#define CYCLES 100
static const char *BAGFILES[][512]={
                                    //Test 1
                                    {//"test/cp_rgbd_dataset_freiburg2_desk_validation.bag",
                                     //"test/cp_rotate_real_cups4.bag",
                                     //"test/cp2_rot_real_cups1.bag",
                                     //"test/cp2_static_room.bag",
                                     "test/cp3_rot_real_cups.bag",
                                     //"test/cp3_rgbd_dataset_freiburg2_dishes.bag",
                                     //"test/cp3_rgbd_dataset_freiburg1_plant.bag",
                                     //"test/cp2_rgbd_dataset_freiburg2_dishes.bag",
                                     //"test/cp2_freiburg.bag",
                                     //"test/2012-06-14-09-04-44.bag",
                                     //"test/cp_rot_sim_kitchen.bag",
                                     //"test/cp_tr_real_cups.bag",
                                     //"test/cp_rotate_real_wall.bag",
                                     ""}
};

static const char *GROUNDTRUTH[][512]={
  {
   //"test/rgbd_dataset_freiburg2_dishes-groundtruth.txt",
   "test/rgbd_dataset_freiburg1_plant-groundtruth.txt",
    //"test/rgbd_dataset_freiburg2_dishes-groundtruth.txt",
    0
  }
};


template<typename Matrix>
float MATRIX_DISTANCE(const Matrix &a, const Matrix &b, const float thr=0.05) {
  Matrix c=a-b;
  float d=c.norm();

  if(d>thr) {
    std::cout<<"A\n"<<a<<"\n";
    std::cout<<"B\n"<<b<<"\n";
  }

  EXPECT_NEAR(d,0,thr);
  return d;
}


//TEST(Slam, inst)
void t2()
{
  {
    typedef DOF6::DOF6_Source<DOF6::TFLinkvf,DOF6::DOF6_Uncertainty<Dummy::RobotParameters,float> > DOF6;
    typedef Slam::Node<Dummy::OBJCTXT<DOF6> > Node;

    Slam::Context<Dummy::KEY, Node> ctxt(1,1);
  }
  {
    typedef DOF6::TFLinkvf DOF6;
    typedef Slam::Node<Dummy::OBJCTXT<DOF6> > Node;

    Slam::Context<Dummy::KEY, Node> ctxt(1,1);
  }

}

//TEST(Slam, dummy_add)
void t1()
{
  typedef DOF6::DOF6_Source<DOF6::TFLinkvf,DOF6::DOF6_Uncertainty<Dummy::RobotParameters,float> > DOF6;
  typedef Slam::Node<Dummy::OBJCTXT<DOF6> > Node;

  Slam::Context<Dummy::KEY, Node> ctxt(1,1);
  //ctxt.set_registration_fct( Dummy::registration_dummy<Node::OBJCTXT,Node::DOF6> );

  for(int i=0; i<CYCLES; i++) {
    ctxt.startFrame(i);
    for(int j=0; j<i; j++) {
      Node::OBJECT obj;
      ctxt+=obj.makeShared();
    }
    ctxt.finishFrame();
  }

}

float gen_random_float(float min, float max)
{
  static boost::mt19937 rng;
  boost::uniform_real<float> u(min, max);
  boost::variate_generator<boost::mt19937&, boost::uniform_real<float> > gen(rng, u);
  return gen();
}


std::vector<cob_3d_mapping_msgs::CurvedPolygon> generateRandomPlanes(const int N, const bool cors)
                    {
  std::vector<cob_3d_mapping_msgs::CurvedPolygon> r;

  for(int i=0; i<N; i++)
  {
    cob_3d_mapping_msgs::CurvedPolygon cp;

    cp.ID = i;

    cp.parameter[0] = gen_random_float(2.f, 4.f);
    cp.parameter[1] = gen_random_float(-1.f,1.f);
    cp.parameter[2] = 0;
    cp.parameter[3] = gen_random_float(-1.f,1.f);
    cp.parameter[4] = 0;
    cp.parameter[5] = 0;

    Eigen::Vector3f z,n,np;
    z.fill(0);z(2)=1.f;
    n=z;
    n(0) = -cp.parameter[1];
    n(1) = -cp.parameter[3];
    n.normalize();
    np = cp.parameter[0]*(z.dot(n))*n;

    cob_3d_mapping_msgs::feature ft;
    ft.ID = 1; //nearest point
    ft.x = np(0);
    ft.y = np(1);
    ft.z = np(2);
    cp.features.push_back(ft);

    if(cors)
    {
      cob_3d_mapping_msgs::simalarity_score ss;
      ss.ID = i;
      ss.prob = 1.f;
      cp.score.push_back(ss);
    }

    //outline
    float size = gen_random_float(0.5f, 2.f)/10;
    for(int j=0; j<10; j++)
    {
      cob_3d_mapping_msgs::polyline_point pt;
      pt.edge_prob = 1;

      pt.x = j*size;
      pt.y = 0;
      cp.polyline.push_back(pt);

      pt.x = j*size;
      pt.y = size*10;
      cp.polyline.push_back(pt);

      pt.y = j*size;
      pt.x = 0;
      cp.polyline.push_back(pt);

      pt.y = j*size;
      pt.x = size*10;
      cp.polyline.push_back(pt);
    }

    r.push_back(cp);
  }

  return r;
                    }

typedef void (*motion_fct)(Eigen::Matrix3f &, Eigen::Vector3f &);

void motion1(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  rot = Eigen::Matrix3f::Identity();
  tr.fill(0);
}

void motion2(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  rot = Eigen::Matrix3f::Identity();
  tr(0) = gen_random_float(0,0.2f);
  tr(1) = gen_random_float(0,0.2f);
  tr(2) = gen_random_float(0,0.2f);
}

void motion2s(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  rot = Eigen::Matrix3f::Identity();
  tr = 0.1*Eigen::Vector3f::Identity();
}

void motion3(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  Eigen::Vector3f n;
  n(0) = gen_random_float(-1,1);
  n(1) = gen_random_float(-1,1);
  n(2) = gen_random_float(-1,1);
  n.normalize();
  Eigen::AngleAxisf aa(gen_random_float(-0.02f,0.05f),n);
  rot = aa.toRotationMatrix();
  tr.fill(0);
}

void motion3s(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  Eigen::Vector3f n=Eigen::Vector3f::Identity();
  Eigen::AngleAxisf aa(0.05f,n);
  rot = aa.toRotationMatrix();
  tr.fill(0);
}

void motion4(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  Eigen::Matrix3f r;
  Eigen::Vector3f t;
  motion2(r,tr);
  motion3(rot,t);
}

void motion4s(Eigen::Matrix3f &rot, Eigen::Vector3f &tr)
{
  Eigen::Matrix3f r;
  Eigen::Vector3f t;
  motion2s(r,tr);
  motion3s(rot,t);
}

//TEST(Slam,sim_run)
void t4()
{
  //*s means simple motion (for debug purpose)
  motion_fct motion[]={motion1,motion2s,motion3s,motion4s,motion2,motion3,motion4,0};

  int mp=0;
  while(motion[mp])
  {
    Eigen::Matrix3f path_rot=Eigen::Matrix3f::Identity(),rot=Eigen::Matrix3f::Identity(),tmp_rot,tmp_rot2;
    Eigen::Vector3f path_tr=Eigen::Vector3f::Zero(),tr=Eigen::Vector3f::Zero(),tmp_tr,tmp_tr2;

    //input data
    std::vector<cob_3d_mapping_msgs::CurvedPolygon> planes = generateRandomPlanes(5, true);

    //setup slam
    typedef DOF6::DOF6_Source<DOF6::TFLinkvf,DOF6::DOF6_Uncertainty<Dummy::RobotParameters,float> > DOF6;
    typedef Slam::Node<Slam_CurvedPolygon::OBJCTXT<DOF6> > Node;

    Slam::Context<Slam_CurvedPolygon::KEY<DOF6>, Node> ctxt(1,1);

    //run
    for(int i=0; i<15; i++)
    {
      std::cout<<"real rot\n"<<rot<<"\n";
      std::cout<<"real tr\n"<<tr<<"\n";
      path_rot = rot*path_rot;
      path_tr += tr;

      ctxt.startFrame(i);

      for(size_t j=0; j<planes.size(); j++)
      {
        ctxt += planes[j];
      }

      ctxt.finishFrame();

      //get motion
      motion[mp](rot,tr);
      //transform
      for(size_t j=0; j<planes.size(); j++)
      {
        Eigen::Vector3f v,t;
        v(0)=planes[j].features[0].x;
        v(1)=planes[j].features[0].y;
        v(2)=planes[j].features[0].z;
        v=rot*v;
        t=v*(tr.dot(v))/(v.squaredNorm());
        ROS_ASSERT(t.squaredNorm()<=tr.squaredNorm());
        v+=t;
        planes[j].features[0].x=v(0);
        planes[j].features[0].y=v(1);
        planes[j].features[0].z=v(2);

        Eigen::Vector3f n=v;

        planes[j].parameter[0] = n.norm();
        n/=n(2);

        planes[j].parameter[1] = -n(0);
        planes[j].parameter[3] = -n(1);
        Eigen::Vector3f z;
        z.fill(0);z(2)=1.f;
        n.normalize();
        planes[j].parameter[0] /= (n.dot(z));
      }

      //check path
      tmp_rot = Eigen::Matrix3f::Identity();
      tmp_tr  = Eigen::Vector3f::Zero();
      tmp_rot2 = Eigen::Matrix3f::Identity();
      tmp_tr2  = Eigen::Vector3f::Zero();
      const Slam::SWAY<Node> *n = &ctxt.getPath().getLocal();
      while(n)
      {
        tmp_tr += n->link_.getTranslation();
        tmp_rot = ((Eigen::Matrix3f)n->link_.getRotation())*tmp_rot;

        tmp_tr2 += tmp_rot2.inverse()*n->link_.getSource1()->getTranslation();
        tmp_rot2 = ((Eigen::Matrix3f)n->link_.getSource1()->getRotation())*tmp_rot2;

        std::cout<<"con\n";
        n = n->node_->getConnections().size()?&n->node_->getConnections()[0]:NULL;
      }
      std::cout<<"ROT1\n"<<tmp_rot<<"\n";
      std::cout<<"ROT2\n"<<tmp_rot2<<"\n";
      std::cout<<"ROT3\n"<<path_rot<<"\n";
      std::cout<<"TR1\n"<<tmp_tr<<"\n";
      std::cout<<"TR2\n"<<tmp_tr2<<"\n";
      std::cout<<"TR3\n"<<path_tr<<"\n";
      MATRIX_DISTANCE(tmp_rot, path_rot);
      MATRIX_DISTANCE(tmp_tr,  path_tr);

    }

    mp++;
  }

}


struct SOdomotry_Data
{
  double timestamp, tx, ty, tz, qx, qy, qz, qw;
};

#include <visualization_msgs/Marker.h>
#include <nav_msgs/Odometry.h>
#include "pcl/visualization/pcl_visualizer.h"
#include <pcl/visualization/cloud_viewer.h>
TEST(Slam,bag_run)
//void t3()
{
  pcl::visualization::CloudViewer viewer("abc");
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr rgb(new pcl::PointCloud<pcl::PointXYZRGB>);

  int bg_file=0;
  while(strlen(BAGFILES[0][bg_file])>0) {

    std::vector<SOdomotry_Data> odos;
    if(GROUNDTRUTH[0][bg_file]) {
      std::ifstream infile(GROUNDTRUTH[0][bg_file]);

      std::string line;
      while (std::getline(infile, line))
      {
        if(line.size()<1 || line[0]=='#') continue;

        std::istringstream iss(line);
        SOdomotry_Data odo;
        if (!(iss >> odo.timestamp >> odo.tx >> odo.ty >> odo.tz >> odo.qx >> odo.qy >> odo.qz >> odo.qw)) {
          ROS_ERROR("parsing groundtruth");
          ROS_ASSERT(0);
          break;
        } // error

        odos.push_back(odo);
      }

    }

    try {
      //read file
      rosbag::Bag bag, bag_out;
      bag.    open(BAGFILES[0][bg_file],                         rosbag::bagmode::Read);
      bag_out.open(std::string(BAGFILES[0][bg_file])+".odo.bag", rosbag::bagmode::Write);

      //setup topics
      std::vector<std::string> topics;
      topics.push_back(std::string("curved_polygons"));
      topics.push_back(std::string("/curved_polygons"));
      topics.push_back(std::string("shapes_array"));
      topics.push_back(std::string("/shapes_array"));

      rosbag::View view(bag, rosbag::TopicQuery(topics));

      visualization_msgs::Marker marker_text, marker_points, marker_planes, marker_cor1, marker_cor2, marker_del;
      marker_text.header.frame_id = "/openni_rgb_frame";
      marker_text.pose.position.x = 0;
      marker_text.pose.position.y = 0;
      marker_text.pose.position.z = 0;
      marker_text.pose.orientation.x = 0.0;
      marker_text.pose.orientation.y = 0.0;
      marker_text.pose.orientation.z = 0.0;
      marker_text.pose.orientation.w = 1.0;
      marker_text.action = visualization_msgs::Marker::ADD;
      marker_text.type = visualization_msgs::Marker::LINE_LIST;
      marker_text.scale.x = marker_text.scale.y = marker_text.scale.z = 0.01;
      marker_text.color.r = marker_text.color.g = marker_text.color.b =  marker_text.color.a = 1;

      marker_del = marker_cor1 =  marker_cor2 = marker_points = marker_planes = marker_text;
      marker_text.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
      marker_text.scale.x = marker_text.scale.y = marker_text.scale.z = 0.35;
      marker_points.color.g = marker_points.color.b =  0;
      marker_planes.color.g = marker_planes.color.r =  0;
      marker_cor2.color.b = marker_cor2.color.r =  0;
      marker_del.action = visualization_msgs::Marker::DELETE;

      marker_text.id = 0;
      marker_points.id = 1;
      marker_planes.id = 2;
      marker_cor1.id = 3;
      marker_cor2.id = 4;

      //setup slam
      typedef DOF6::DOF6_Source<DOF6::TFLinkvf,DOF6::DOF6_Uncertainty<Dummy::RobotParameters,float> > DOF6;
      typedef Slam::Node<Slam_CurvedPolygon::OBJCTXT<DOF6> > Node;

      Slam::Context<Slam_CurvedPolygon::KEY<DOF6>, Node> ctxt(.50,.50);

      Eigen::Vector3f last_tr = Eigen::Vector3f::Zero();
      Eigen::Matrix3f last_rot= Eigen::Matrix3f::Identity();

      Eigen::Vector3f last_odo_tr = Eigen::Vector3f::Zero();

      //load data
      int ctr=0;
      ros::Time last, start;
      bool first=true;
      std::vector<cob_3d_mapping_msgs::ShapeArray> memory_sa;
      BOOST_FOREACH(rosbag::MessageInstance const m, view)
      {

        cob_3d_mapping_msgs::ShapeArray::ConstPtr sa = m.instantiate<cob_3d_mapping_msgs::ShapeArray>();
        if (sa != NULL)
        {
          memory_sa.push_back(*sa);
          continue;
        }

        geometry_msgs::Point line_p, line_p0;
        line_p0.x = line_p0.y = line_p0.z = 0;

        cob_3d_mapping_msgs::CurvedPolygon::ConstPtr s = m.instantiate<cob_3d_mapping_msgs::CurvedPolygon>();
        if (s != NULL)
        {
//          if(!(std::abs((s->stamp-start).toSec()-0)<0.001 || first || std::abs((s->stamp-start).toSec()-3.15)<0.001 || std::abs((s->stamp-start).toSec()-3.7)<0.1))
//            continue;

          if(last!=s->stamp) {
            marker_cor1.header.stamp = marker_cor2.header.stamp = marker_points.header.stamp = marker_planes.header.stamp = marker_text.header.stamp = s->stamp;

            ROS_INFO("new frame");

            last=s->stamp;
            if(first) {
              first=false;
              start = s->stamp;
            }
            else
              ctxt.finishFrame();

            ROS_INFO("timestamp %f (%d)", (last-start).toSec(), ctr);
            ctr++;

            pcl::PointXYZRGB p;
            Eigen::Vector3f from,to,temp;
            while(Debug::Interface::get().getArrow(from,to,p.r,p.g,p.b))
            {
              line_p.x = from(0);
              line_p.y = from(1);
              line_p.z = from(2);
              p.r>100?marker_cor1.points.push_back(line_p):marker_cor2.points.push_back(line_p);
              line_p.x = to(0);
              line_p.y = to(1);
              line_p.z = to(2);
              p.r>100?marker_cor1.points.push_back(line_p):marker_cor2.points.push_back(line_p);
              temp = to-from;
//              from -= temp*0.1;
//              temp *= 1.2;
              for(float ms=0; ms<=1; ms+=0.01)
              {
                p.x=ms*temp(0)+from(0);
                p.y=ms*temp(1)+from(1);
                p.z=ms*temp(2)+from(2);
                if(p.getVector3fMap().squaredNorm()<100)
                  rgb->push_back(p);
              }
            }

            //OUTPUT--------------

            //check path
            Eigen::Matrix3f tmp_rot = Eigen::Matrix3f::Identity();
            Eigen::Vector3f tmp_tr  = Eigen::Vector3f::Zero();
            const Slam::SWAY<Node> *n = &ctxt.getPath().getLocal();
            while(n)
            {

              tmp_tr += n->link_.getTranslation();
              tmp_rot = ((Eigen::Matrix3f)n->link_.getRotation())*tmp_rot;

              std::cout<<"con\n";
              n = n->node_->getConnections().size()?&n->node_->getConnections()[0]:NULL;
            }
            std::cout<<"ROT1\n"<<(::DOF6::EulerAnglesf)tmp_rot<<"\n";
            std::cout<<"TR1\n"<<tmp_tr<<"\n";

            Eigen::Quaternionf quat((Eigen::Matrix3f)tmp_rot.inverse());

            nav_msgs::Odometry odo;
            odo.header.frame_id = "/openni_rgb_frame";
            odo.header.stamp = last;
            odo.pose.pose.position.x = -tmp_tr(0);
            odo.pose.pose.position.y = -tmp_tr(1);
            odo.pose.pose.position.z = -tmp_tr(2);
            odo.pose.pose.orientation.x = quat.x();
            odo.pose.pose.orientation.y = quat.y();
            odo.pose.pose.orientation.z = quat.z();
            odo.pose.pose.orientation.w = quat.w();

            bag_out.write("odometry", last, odo);

            while(odos.size()>0 && odos.front().timestamp<last.toSec())
              odos.erase(odos.begin());
            if(odos.size()>0)
            {
              odo.header.stamp = ros::Time(odos.front().timestamp);
              odo.pose.pose.position.x = odos.front().tx;
              odo.pose.pose.position.y = odos.front().ty;
              odo.pose.pose.position.z = odos.front().tz;
              Eigen::Vector3f tr;
              tr(0) = odos.front().tx;
              tr(1) = odos.front().ty;
              tr(2) = odos.front().tz;
              ROS_INFO("odometry tr jump with length %f", (tr-last_odo_tr).norm());
              last_odo_tr = tr;
              odo.pose.pose.orientation.x = odos.front().qx;
              odo.pose.pose.orientation.y = odos.front().qy;
              odo.pose.pose.orientation.z = odos.front().qz;
              odo.pose.pose.orientation.w = odos.front().qw;

              bag_out.write("groundtruth", ros::Time(odos.front().timestamp), odo);
            }

            while(memory_sa.size()>0 && memory_sa.front().header.stamp<=last)
            {
              bag_out.write("shapes_array", last, memory_sa.front());
              memory_sa.erase(memory_sa.begin());
            }
            for(int i=0; i<5; i++)
            {
              marker_del.id = i;
              bag_out.write("/markers", last, marker_del);
            }
            bag_out.write("/markers", last, marker_points);
            bag_out.write("/markers", last, marker_planes);
            bag_out.write("/markers", last, marker_cor1);
            bag_out.write("/markers", last, marker_cor2);
            marker_points.points.clear();
            marker_planes.points.clear();
            marker_cor1.points.clear();
            marker_cor2.points.clear();
            //OUTPUT-------------------


            if((last_tr-tmp_tr).norm()>0.2 || ((::DOF6::EulerAnglesf)(last_rot.inverse()*tmp_rot)).getVector().squaredNorm()>0.15f*0.15f) {
              ROS_INFO("BIG JUMP");
              ROS_ERROR("BIG JUMP");

              marker_text.text = "BIG JUMP";
              bag_out.write("/markers", last, marker_text);
              //ASSERT_TRUE(false);
              //while(1) getchar();
            }
            last_tr=tmp_tr;
            last_rot=tmp_rot;

            rgb->width=1;
            rgb->height=rgb->size();
            if(rgb->size())
            {
//              Eigen::Quaternionf q((Eigen::Matrix3f)tmp_rot.inverse());
//              pcl::transformPointCloud(*rgb,*rgb, -tmp_tr, q);
              viewer.showCloud(rgb);
              std::cerr<<"press any key\n";
//              if((last-start).toSec()>31.8)
                getchar();
              rgb.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
            }

            ctxt.startFrame(s->stamp.toSec());
          }
        }

        if(s->weight<500) continue;

        Slam_CurvedPolygon::ex_curved_polygon xp = *s;

        if(!pcl_isfinite(xp.getNearestPoint().sum()))
          continue;
//                        if(s->polyline.size()<10)
//                          continue;
//                if(xp.getEnergy()<5)
//                  continue;
        //        if(!xp.isPlane())
        //          continue;
//        if(xp.getNearestPoint().norm()>4)
//          continue;

        ctxt += *s;

        line_p.x = xp.getNearestPoint()(0);
        line_p.y = xp.getNearestPoint()(1);
        line_p.z = xp.getNearestPoint()(2);
        if(xp.isPlane())
        {
          marker_planes.points.push_back(line_p0);
          marker_planes.points.push_back(line_p);
        }
        else
        {
          marker_points.points.push_back(line_p0);
          marker_points.points.push_back(line_p);
        }

        pcl::PointXYZRGB p;
        std::cerr<<"param: "
            <<s->parameter[0]<<" "
            <<s->parameter[1]<<" "
            <<s->parameter[2]<<" "
            <<s->parameter[3]<<" "
            <<s->parameter[4]<<" "
            <<s->parameter[5]<<"\n";
        for(float ms=0; ms<=1; ms+=0.01)
        {
          p.r=p.g=p.b=0;
          if(!xp.isPlane())
            p.r=255;
          else
            p.b=255;
          p.x=ms*xp.getNearestPoint()(0);
          p.y=ms*xp.getNearestPoint()(1);
          p.z=ms*xp.getNearestPoint()(2);
          if(p.getVector3fMap().squaredNorm()<100)
            rgb->push_back(p);
        }

        Eigen::Vector3f temp;
        temp=xp.getFeatures()[1].v_;
        temp.normalize();
        temp*=0.1;
        for(float ms=0; ms<=1; ms+=0.01)
        {
          p.r=p.g=p.b=0;
          p.g=255;
          p.b=255;
          p.x=ms*temp(0)+xp.getNearestPoint()(0);
          p.y=ms*temp(1)+xp.getNearestPoint()(1);
          p.z=ms*temp(2)+xp.getNearestPoint()(2);
          if(p.getVector3fMap().squaredNorm()<100)
            rgb->push_back(p);
        }
        temp=xp.getFeatures()[3].v_;
        temp.normalize();
        temp*=0.1;
        for(float ms=0; ms<=1; ms+=0.01)
        {
          p.r=p.g=p.b=0;
          p.r=255;
          p.b=255;
          p.x=ms*temp(0)+xp.getNearestPoint()(0);
          p.y=ms*temp(1)+xp.getNearestPoint()(1);
          p.z=ms*temp(2)+xp.getNearestPoint()(2);
          if(p.getVector3fMap().squaredNorm()<100)
            rgb->push_back(p);
        }
        temp=xp.getFeatures()[4].v_;
        temp.normalize();
        temp*=0.1;
        for(float ms=0; ms<=1; ms+=0.01)
        {
          p.r=p.g=p.b=0;
          p.r=255;
          p.g=255;
          p.x=ms*temp(0)+xp.getNearestPoint()(0);
          p.y=ms*temp(1)+xp.getNearestPoint()(1);
          p.z=ms*temp(2)+xp.getNearestPoint()(2);
          if(p.getVector3fMap().squaredNorm()<100)
            rgb->push_back(p);
        }

      }

      ctxt.finishFrame();

      bag.close();
    }
    catch(...) {
      std::cout<<"failed to load: "<<BAGFILES[0][bg_file]<<"\n";
      ASSERT_TRUE(false);
    }

    bg_file++;
  }
}

int main(int argc, char **argv){
  ros::Time::init();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
