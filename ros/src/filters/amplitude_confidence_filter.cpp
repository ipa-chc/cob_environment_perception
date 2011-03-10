/*
 * amplitude_filter.cpp
 *
 *  Created on: Mar 02, 2011
 *      Author: goa-wq
 */

//##################
//#### includes ####

// standard includes
//--

// ROS includes
#include <ros/ros.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

// ROS message includes
#include <sensor_msgs/PointCloud2.h>

// external includes
#include <boost/timer.hpp>
//#include <cob_vision_utils/VisionUtils.h>
#include "pcl/point_types.h"
#include <pluginlib/class_list_macros.h>
#include <pcl_ros/pcl_nodelet.h>
#include <cob_env_model/cpc_point.h>

//####################
//#### nodelet class####
class AmplitudeConfidenceFilter : public pcl_ros::PCLNodelet
{
public:
    // Constructor
	 AmplitudeConfidenceFilter()
	  :	filter_by_amplitude_(false),
	   	filter_by_confidence_(false)
	{
		t_check=1;
		filter_info=1;
	}

    // Destructor
    ~ AmplitudeConfidenceFilter()
    {
    	/// void
    }

    void onInit()
    {
    	PCLNodelet::onInit();
    	n_ = getNodeHandle();

		point_cloud_sub_ = n_.subscribe("point_cloud2", 1, & AmplitudeConfidenceFilter::PointCloudSubCallback, this);
		point_cloud_pub_= n_.advertise<sensor_msgs::PointCloud2>("point_cloud2_filtered",1);

		n_.param("/amplitude_confidence_filter_nodelet/filter_by_amplitude", filter_by_amplitude_, false);
	//	std::cout << "filter_by_amplitude: " << filter_by_amplitude_<< std::endl;
		n_.param("/amplitude_confidence_filter_nodelet/filter_by_confidence", filter_by_confidence_, false);
	//	std::cout << "filter_by_confidence: " << filter_by_confidence_<< std::endl;
		n_.param("/amplitude_confidence_filter_nodelet/amplitude_min_threshold", amplitude_min_threshold_, 1000);
		std::cout << "amplitude_min_threshold: " << amplitude_min_threshold_<< std::endl;
		n_.param("/amplitude_confidence_filter_nodelet/amplitude_max_threshold", amplitude_max_threshold_, 60000);
		std::cout << "amplitude_max_threshold: " << amplitude_max_threshold_<< std::endl;
		n_.param("/amplitude_confidence_filter_nodelet/confidence_threshold", confidence_threshold_, 60000);
		std::cout << "confidence_threshold: " << confidence_threshold_<< std::endl;
    }

    void PointCloudSubCallback(const pcl::PointCloud<CPCPoint>::Ptr& pc)
	{
		//ROS_INFO("PointCloudSubCallback");
		pcl::PointCloud<CPCPoint>::Ptr pc_out(new pcl::PointCloud<CPCPoint>());
		pc_out->points.resize(pc->points.size());
		pc_out->header = pc->header;

		cv::Mat xyz_mat_32F3 = cv::Mat(pc->height, pc->width, CV_32FC3);
		cv::Mat intensity_mat_32F1 = cv::Mat(pc->height, pc->width, CV_32FC1);

		//assumption: data order is always x->y->z in PC, datatype = float
		/*int x_offset = 0, i_offset = 0;
		for (size_t d = 0; d < pc->fields.size(); ++d)
		{
			if(pc->fields[d].name == "x")
				x_offset = pc->fields[d].offset;
			if(pc->fields[d].name == "intensity")
				i_offset = pc->fields[d].offset;
		}*/

		float* f_ptr = 0;
		float* i_ptr = 0;
		int pc_msg_idx=0;
		for (int row = 0; row < xyz_mat_32F3.rows; row++)
		{
			f_ptr = xyz_mat_32F3.ptr<float>(row);
			i_ptr = intensity_mat_32F1.ptr<float>(row);
			for (int col = 0; col < xyz_mat_32F3.cols; col++, pc_msg_idx++)
			{
				//memcpy(&f_ptr[3*col], &pc->data[pc_msg_idx * pc->point_step + x_offset], 3*sizeof(float));
				//memcpy(&i_ptr[col], &pc->data[pc_msg_idx * pc->point_step + i_offset], sizeof(float));
				memcpy(&f_ptr[3*col], &pc->points[pc_msg_idx].x, 3*sizeof(float));
				memcpy(&i_ptr[col], &pc->points[pc_msg_idx].intensity, sizeof(float));
			}
		}

		if(filter_by_amplitude_ || filter_by_confidence_)
		{
			FilterByAmplitude(pc, pc_out);
			point_cloud_pub_.publish(pc_out);
			if (t_check==1)
			{
				ROS_INFO("\tTime elapsed (Amplitude_Confidence_Filter) : %f", t.elapsed());
				t.restart();
				t_check=0;
			}
		}
	}

    void FilterByAmplitude(const pcl::PointCloud<CPCPoint>::Ptr& pc, const pcl::PointCloud<CPCPoint>::Ptr& pc_out)
    {
    	//ROS_INFO("Filter by amplitude");
		//assumption: data order is always x->y->z in PC, datatype = float
		/*int x_offset = 0, c_offset = 0;
		for (size_t d = 0; d < pc->fields.size(); ++d)
		{
			if(pc->fields[d].name == "x")
				x_offset = pc->fields[d].offset;
			if(pc->fields[d].name == "intensity")
				c_offset = pc->fields[d].offset;
		}*/
		int nr_p = 0;

		if(filter_by_amplitude_==true && filter_by_confidence_==true)
		{
			if (filter_info==1)
			{
				ROS_INFO("Applying Amplitude and confidence Filter");
			    filter_info=0;
			}
			for( unsigned int i = 0; i < pc->points.size(); i++)
			{
				if( pc->points[i].intensity > amplitude_min_threshold_ && pc->points[i].intensity < amplitude_max_threshold_ && pc->points[i].confidence > confidence_threshold_)
				pc_out->points[nr_p++] = pc->points[i];
			}
		}

		if(filter_by_amplitude_==true && filter_by_confidence_==false)
		{
			if (filter_info==1)
			{
				ROS_INFO("Applying Amplitude Filter");
				filter_info=0;
			}
			for( unsigned int i = 0; i < pc->points.size(); i++)
			{
				if( pc->points[i].intensity > amplitude_min_threshold_ && pc->points[i].intensity < amplitude_max_threshold_)
				pc_out->points[nr_p++] = pc->points[i];
			}
    	}
		if(filter_by_amplitude_==false && filter_by_confidence_==true)
		{
			if (filter_info==1)
			{
				ROS_INFO("Applying confidence Filter");
				filter_info=0;
			}
			for( unsigned int i = 0; i < pc->points.size(); i++)
			{
				if( pc->points[i].confidence > confidence_threshold_)
				pc_out->points[nr_p++] = pc->points[i];
			}
		}
		/*float c_ptr;
		float f_ptr[3];
		f_ptr[0] = f_ptr[1] = 0;
		f_ptr[2] = -5;
		for ( unsigned int pc_msg_idx = 0; pc_msg_idx < pc->height*pc->width; pc_msg_idx++)
		{
			//memcpy(&c_ptr, &pc->data[pc_msg_idx * pc->point_step + c_offset], sizeof(float));
			if(*(float*)&pc->data[pc_msg_idx * pc->point_step + c_offset] > 1000 && *(float*)&pc->data[pc_msg_idx * pc->point_step + c_offset] < 60000)
			{
				memcpy(&pc_out->data[pc_msg_idx * pc->point_step], &pc->data[pc_msg_idx * pc->point_step], 3*sizeof(float));
				nr_p++;
			}
		}*/
		pc_out->width = nr_p;
		pc_out->height = 1;
		pc_out->points.resize(nr_p);
		pc_out->is_dense = true;
    }

    ros::NodeHandle n_;
    boost::timer t;
    bool t_check;
    bool filter_info;

protected:
    ros::Subscriber point_cloud_sub_;
    ros::Publisher point_cloud_pub_;

    bool filter_by_amplitude_;
    bool filter_by_confidence_;

    int amplitude_min_threshold_;
    int amplitude_max_threshold_;
    int confidence_threshold_;

};

//#######################
//#### main programm ####
//int main(int argc, char** argv)
//{
//	/// initialize ROS, specify name of node
//	ros::init(argc, argv, "point_cloud_filter");
//
//	/// Create a handle for this node, initialize node
//	ros::NodeHandle nh;
//
//	/// Create camera node class instance
//	PointCloudFilter point_cloud_filter(nh);
//
//	ros::spin();
//
//	return 0;
//}

PLUGINLIB_DECLARE_CLASS(cob_env_model,  AmplitudeConfidenceFilter,  AmplitudeConfidenceFilter, nodelet::Nodelet)


