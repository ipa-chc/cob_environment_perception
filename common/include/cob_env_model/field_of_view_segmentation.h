/****************************************************************
 *
 * Copyright (c) 2010
 *
 * Fraunhofer Institute for Manufacturing Engineering
 * and Automation (IPA)
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Project name: care-o-bot
 * ROS stack name: cob_vision
 * ROS package name: cob_env_model
 * Description:
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Author: Georg Arbeiter, email:georg.arbeiter@ipa.fhg.de
 * Supervised by:
 *
 * Date of creation: 02/2011
 * ToDo:
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Fraunhofer Institute for Manufacturing
 *       Engineering and Automation (IPA) nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License LGPL for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/

#ifndef FIELDOFVIEWSEGMENTATION_H_
#define FIELDOFVIEWSEGMENTATION_H_

//##################
//#### includes ####

// standard includes
//--

// PCL includes
#include <pcl/pcl_base.h>

#include <Eigen/Core>

namespace ipa_env_model
{
	template <typename PointT>
	class FieldOfViewSegmentation: public pcl::PCLBase<PointT> {
		public:
			FieldOfViewSegmentation() {};
			//virtual ~FieldOfViewSegmentation();
			void computeFieldOfView(double fovHorizontal, double fovVertical, double maxRange,
					Eigen::Vector3d &n_up, Eigen::Vector3d &n_down, Eigen::Vector3d &n_right, Eigen::Vector3d &n_left);
			void segment(pcl::PointIndices &indices,
					Eigen::Vector3d &n_up, Eigen::Vector3d &n_down, Eigen::Vector3d &n_right, Eigen::Vector3d &n_left, Eigen::Vector3d &n_origin, double maxRange);
		protected:
			using pcl::PCLBase<PointT>::input_;

	};

} // end namespace ipa_env_model

#endif /* FIELDOFVIEWSEGMENTATION_H_ */
