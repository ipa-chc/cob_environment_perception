
#include "../sub_structures/poly2d.hpp"
#include "../sub_structures/labeling.h"
#include "../sub_structures/debug.h"

#include <cob_3d_segmentation/eval.h>

#include <cob_3d_mapping_common/stop_watch.h>


template <typename Point, typename PointLabel>
Segmentation_QuadRegression<Point,PointLabel>::Segmentation_QuadRegression():
MIN_LOD(8), FINAL_LOD(0), GO_DOWN_TO_LVL(3),
ch_(NULL), outline_check_(0), outline_check_size_(0),
filter_(-1.f), only_planes_(false)
{
  kinect_params_.f = 0.f;
  Contour2D::generateSpline2D();
}


template <typename Point, typename PointLabel>
void Segmentation_QuadRegression<Point,PointLabel>::prepare(const pcl::PointCloud<Point> &pc) {
  getKinectParams(*input_);

#ifdef DO_NOT_DOWNSAMPLE_
  int w=input_->width;
  int h=input_->height;
#else
  int w=input_->width/2;
  int h=input_->height/2;
#endif

#ifdef DO_NOT_DOWNSAMPLE_
  for(int i=0; i<5; i++){
#else
    for(int i=0; i<4; i++){
#endif
      levels_.push_back(SubStructure::ParamC(w,h));

      w/=2;
      h/=2;
    }

    ch_=new int[levels_[0].w*levels_[0].h];
    memset(ch_,0,levels_[0].w*levels_[0].h*4);
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::getKinectParams(const pcl::PointCloud<Point> &pc) {
    Point p1, p2;
    p1=p1;
    p2=p2;
    int i1=-1, i2=-1;

    for(size_t x=0; x<pc.width; x+=8) {
      for(size_t y=0; y<pc.height; y+=8) {
        int ind = getIndPC(x,y);
        if(pcl_isfinite(pc[ind].z)&&pc[ind].z<10.f) {
          p1=pc[ind];
          i1=ind;
          x=pc.width;
          break;
        }
      }
    }

    for(int x=pc.width-1; x>=0; x-=8) {
      for(int y=pc.height-1; y>=0; y-=8) {
        int ind = getIndPC(x,y);
        if(pcl_isfinite(pc[ind].z)&&pc[ind].z!=p1.z&&pc[ind].z<10.f) {
          p2=pc[ind];
          i2=ind;
          x=-1;
          break;
        }
      }
    }

    if(i1==-1||i2==-1) {
      ROS_WARN("no valid points");
      return;
    }

    int x=i1%pc.width;
    int y=i1/pc.width;
    float ax1,ax2, bx1,bx2;
    float ay1,ay2, by1,by2;

    ax1=p1.z/p1.x*x;
    bx1=p1.z/p1.x;
    ay1=p1.z/p1.y*y;
    by1=p1.z/p1.y;

    x=i2%pc.width;
    y=i2/pc.width;
    ax2=p2.z/p2.x*x;
    bx2=p2.z/p2.x;
    ay2=p2.z/p2.y*y;
    by2=p2.z/p2.y;

    kinect_params_.dx = (ax1-ax2)/(bx1-bx2);
    kinect_params_.dy = (ay1-ay2)/(by1-by2);
    kinect_params_.f = ax1 - bx1*kinect_params_.dx;
  }


  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::buildTree(const pcl::PointCloud<Point> &pc) {
#ifdef STOP_TIME
    PrecisionStopWatch ssw;
    ssw.precisionStart();
#endif

    //go through all levels
    for(size_t i=0; i<levels_.size(); i++) {

      SubStructure::ParamC *lvl = &levels_[i];
      if(i==0) {  ///lowest-level: take points
        Eigen::Vector3f p,t;
        int j = 0;
        for(size_t y=0; y<lvl->h; y++) {
          for(size_t x=0; x<lvl->w; x++) {
#ifdef DO_NOT_DOWNSAMPLE_
            if(pcl_isfinite(pc[j].z))
            {
              lvl->data[j]=pc[j].getVector3fMap();
            }
            else
            {
              lvl->data[j]=Eigen::Vector3f::Zero();
            }

            lvl->data[j].occopied=-1;

            ++j;
#else
            int num=0;
            p(0)=p(1)=p(2)=0.f;

            // sub-sample 4 points to one
            j=getIndPC(2*x,2*y);
            if(pcl_isfinite(pc[j].z))
            {
              p(0)+=pc[j].x;
              p(1)+=pc[j].y;
              p(2)+=pc[j].z;
              ++num;
            }

            j=getIndPC(2*x,2*y+1);
            if(pcl_isfinite(pc[j].z))
            {
              p(0)+=pc[j].x;
              p(1)+=pc[j].y;
              p(2)+=pc[j].z;
              ++num;
            }

            j=getIndPC(2*x+1,2*y);
            if(pcl_isfinite(pc[j].z))
            {
              p(0)+=pc[j].x;
              p(1)+=pc[j].y;
              p(2)+=pc[j].z;
              ++num;
            }

            j=getIndPC(2*x+1,2*y+1);
            if(pcl_isfinite(pc[j].z))
            {
              p(0)+=pc[j].x;
              p(1)+=pc[j].y;
              p(2)+=pc[j].z;
              ++num;
            }

            //revalidate
            j=getIndPC(2*x,2*y);
            if((pc[j].getVector3fMap()-p/num).squaredNorm()>0.005f)
            {
              p(0)-=pc[j].x;
              p(1)-=pc[j].y;
              p(2)-=pc[j].z;
              --num;
            }

            j=getIndPC(2*x,2*y+1);
            if((pc[j].getVector3fMap()-p/num).squaredNorm()>0.005f)
            {
              p(0)-=pc[j].x;
              p(1)-=pc[j].y;
              p(2)-=pc[j].z;
              --num;
            }

            j=getIndPC(2*x+1,2*y);
            if((pc[j].getVector3fMap()-p/num).squaredNorm()>0.005f)
            {
              p(0)-=pc[j].x;
              p(1)-=pc[j].y;
              p(2)-=pc[j].z;
              --num;
            }

            j=getIndPC(2*x+1,2*y+1);
            if((pc[j].getVector3fMap()-p/num).squaredNorm()>0.005f)
            {
              p(0)-=pc[j].x;
              p(1)-=pc[j].y;
              p(2)-=pc[j].z;
              --num;
            }

            j=getInd(x,y);
            lvl->data[j]=p/num;

            lvl->data[j].occopied=-1;
#endif
          }
        }
      }
      else { //other levels take lower level
        SubStructure::ParamC *lvl_prev = &levels_[i-1];

        int j=0, k=0;
        for(size_t y=0; y<lvl->h; y++) {
          for(size_t x=0; x<lvl->w; x++) {
            //j=getInd(x,y);
            lvl->data[j] = lvl_prev->data[k];    //getInd1(x*2,y*2)
            lvl->data[j]+= lvl_prev->data[k+1]; //getInd1(x*2,y*2+1)
            lvl->data[j]+= lvl_prev->data[k+levels_[i-1]];    //getInd1(x*2+1,y*2)
            lvl->data[j]+= lvl_prev->data[k+levels_[i-1]+1];        //getInd1(x*2+1,y*2+1)
            ++j;
            k+=2;
          }
        }
      }

    }

#ifdef STOP_TIME
    execution_time_quadtree_ = ssw.precisionStop();
#endif
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::calc() {

#ifdef STOP_TIME
    execution_time_polyextraction_=0.;
    PrecisionStopWatch ssw;
    ssw.precisionStart();
#endif

    for(int i=(int)levels_.size()-1; i>(int)(levels_.size()-GO_DOWN_TO_LVL); i--) {

      //from one corner to the other
      size_t sx=0, sy=0;
      while(sx<levels_[i].w-1 && sy<levels_[i].h-1) {
        for(int x=sx+1; x<(int)levels_[i].w-1; x++) {
          //if(isOccupied2(i,x,sy)==-1&&!checkOccupiedDeep(i,x,sy))
          if(isOccupied(i,x,sy)==-1)
          {
            SubStructure::Model m;
            grow(m, i, x,sy);
          }
        }
        sx+=2;

        for(size_t y=sy+1; y<levels_[i].h-1; y++) {
          //if(isOccupied2(i,sx,y)==-1&&!checkOccupiedDeep(i,sx,y))
          if(isOccupied(i,sx,y)==-1)
          {
            SubStructure::Model m;
            grow(m, i, sx,y);
          }
        }
        sy+=2;
      }

    }

#ifdef STOP_TIME
    execution_time_growing_ = ssw.precisionStop();
#endif

    //preparePolygons();
#ifdef BACK_CHECK_REPEAT
    back_check_repeat();
#endif
  }

#ifdef DO_NOT_DOWNSAMPLE_
#define SHIFT   0
#else
#define SHIFT   1
#endif

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::back_check_repeat() {
#ifdef BACK_CHECK_REPEAT
    for(size_t i=0; i<polygons_.size(); i++) {
      if(polygons_[i].segments_.size()<1) continue;

      //ROS_ERROR("start");

      for(size_t j=0; j<polygons_[i].segments2d_.size(); j++) {

        for(size_t k=0; k<polygons_[i].segments2d_[j].size(); k++)
        {

          Eigen::Vector2i start = polygons_[i].segments2d_[j][(k-1+polygons_[i].segments2d_[j].size())%polygons_[i].segments2d_[j].size()],
              end = polygons_[i].segments2d_[j][k], pos;

          const size_t n = std::max((size_t)1, (size_t)(sqrtf((end-start).squaredNorm())/5) );
          float prob = 0.f;

          //ROS_ERROR("n %d",n);
          //int m=0;

          for(size_t p=0; p<n; p++) {
            //pos
            pos = start + (end-start)*(p+1)/(float)(n+1);

            //ROS_ERROR("pos %d %d",pos(0),pos(1));

            if(pos(0)<0 || pos(1)<0 || pos(0)>=(int)levels_[0].w || pos(1)>=(int)levels_[0].h) continue;

            int o;
            for(int x=-12; x<=12; x+=3)
              for(int y=-12; y<=12; y+=3)
                if( (o = otherOccupied(0, pos(0)+x, pos(1)+y, polygons_[i].mark_)) != -1)
                  break;

            if(o<0||o==polygons_[i].mark_) continue;
            //ROS_ERROR("o %d",o);
            //++m;

            //get here
            Eigen::Vector2f pHere = polygons_[i].project2plane(((pos(0)<<(SHIFT))-kinect_params_.dx)/kinect_params_.f,
                                                               ((pos(1)<<(SHIFT))-kinect_params_.dy)/kinect_params_.f,
                                                               polygons_[i].model_,0.f).head(2);
            Eigen::Vector3f vHere = polygons_[i].project2world(pHere), nHere = polygons_[i].normalAt(pHere);

            //get there
            Eigen::Vector2f pThere = polygons_[o].project2plane(((pos(0)<<(SHIFT))-kinect_params_.dx)/kinect_params_.f,
                                                                ((pos(1)<<(SHIFT))-kinect_params_.dy)/kinect_params_.f,
                                                                polygons_[o].model_,0.f).head(2);
            Eigen::Vector3f vThere = polygons_[o].project2world(pHere), nThere = polygons_[o].normalAt(pHere);

            const float d = std::min(vThere(2),vHere(2));

            prob += (
                (vThere(2)-vHere(2))>0.01f*d*d && std::abs(nHere(2))>0.7f) ||
                (std::abs(vThere(2)-vHere(2))<0.1f*d*d && nHere.dot(nThere)<0.8f)
                ? 1.f:0.f;
          }

          polygons_[i].segments_[j][k](2) = prob/n;
          //ROS_ERROR("m %d",m);

        }
      }
    }

#endif
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::grow(SubStructure::Model &model, const int i, const int x, const int y) {
    static SubStructure::VISITED_LIST<SubStructure::SVALUE> list;
    list.init();
    list.add( SubStructure::SVALUE(getInd(x,y),levels_[i].w*levels_[i].h) );

    grow(list, model, i, polygons_.size(), true);
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::grow(SubStructure::VISITED_LIST<SubStructure::SVALUE> &list, SubStructure::Model &model, const int i, const int mark, bool first_lvl) {
    int x, y, hops, occ;
    unsigned int found=0;
    bool bNew,bNew2;
    int last_size=-1;
#ifdef CHECK_CONNECTIVITY
    S_POLYGON_CONNECTIVITY connectivity;
#endif

    do {
      bNew=false;
      bNew2=false;
      while(list.pos+1<list.size) {
        list.move();

        x= list.vals[list.pos].v%levels_[i].w;
        y= list.vals[list.pos].v/levels_[i].w;
        hops = list.vals[list.pos].hops;

        occ = isOccupied(i,x,y);
        if(occ==mark) {
          if(list.pos>last_size&&hops) list.remove();
          continue;
        }
        else if(occ!=-1) {
#ifdef CHECK_CONNECTIVITY
          if(/*i==FINAL_LOD && */checkModelAt(model, i,x,y,
                                              0.02)) { //TODO: check threshold
            while(occ>=connectivity.connections_.size())
              connectivity.connections_.push_back(0);
            connectivity.connections_[occ]+=(1<<i);
          }
#endif
          continue;
        }

        float d=std::min(
            std::abs(levels_[i].data[getInd(x,y)].z_(0)/levels_[i].data[getInd(x,y)].model_(0,0)),
            std::abs(model.param.z_(0)/model.param.model_(0,0))
        );

#if defined(SIMULATION_)
        if(d>20.f) continue;
        const float thr=(d+2.f)*0.0015f;
#elif defined(DO_NOT_DOWNSAMPLE_)
        const float thr=(d*d+1.2f)*0.004f;      //TODO: check
#else
        const float thr=(d*d+1.2f)*0.004f;
#endif

        if( hops>0 && x>0&&y>0&&x+1<(int)levels_[i].w&&y+1<(int)levels_[i].h &&
            d!=0.f && ((found<1&&first_lvl
//#ifdef USE_MIN_MAX_RECHECK_
//                &&(levels_[i].data[getInd(x,y)].v_max_-levels_[i].data[getInd(x,y)].v_min_)<0.02f*(1<<i)*std::abs(levels_[i].data[getInd(x,y)].z_(0)/levels_[i].data[getInd(x,y)].model_(0,0))
//#endif
        ) ||
                (
#ifdef USE_MIN_MAX_RECHECK_
#ifdef DO_NOT_DOWNSAMPLE_
                    (levels_[i].data[getInd(x,y)].v_max_-levels_[i].data[getInd(x,y)].v_min_)< (model.get_max_gradient(levels_[i].data[getInd(x,y)])*d*(1<<i)/kinect_params_.f+4*thr) /*std::min(0.5f,std::max(0.02f,0.05f*d))*/ //TODO: in anbhaengigkeit der steigung
#else
                    (levels_[i].data[getInd(x,y)].v_max_-levels_[i].data[getInd(x,y)].v_min_)< 2*(model.get_max_gradient(levels_[i].data[getInd(x,y)])*d*(1<<i)/kinect_params_.f+4*thr) /*std::min(0.5f,std::max(0.02f,0.05f*d))*/ //TODO: in anbhaengigkeit der steigung
#endif
                    //&& std::abs(model.model(levels_[i].data[getInd(x,y)].v_min_(0),levels_[i].data[getInd(x,y)].v_min_(1))-levels_[i].data[getInd(x,y)].v_min_(2))<thr
                    //&& std::abs(model.model(levels_[i].data[getInd(x,y)].v_max_(0),levels_[i].data[getInd(x,y)].v_max_(1))-levels_[i].data[getInd(x,y)].v_max_(2))<thr
                    &&
#endif
                    (checkModelAt(model, i,x,y,
                                  thr)
                                  /*||
                    checkModelAtZ(model, i,x,y,
                                  0.01f)*/)
                                  //d*(0.04-0.005*levels_[i].data[getInd(x,y)].model_(0,0)/model.param.model_(0,0))))
                                  //0.015)) {
                                  //d*(0.01-0.005*levels_[i].data[getInd(x,y)].model_(0,0)/model.param.model_(0,0)))
                )
#ifdef USE_NORMAL_CHECK
                && ( (i!=1&&i!=2) || std::abs(SubStructure::Model(levels_[i].data[getInd(x,y)]).getLinearNormal().dot(
                    model.getNormal(levels_[i].data[getInd(x,y)].model_(1,0)/levels_[i].data[getInd(x,y)].model_(0,0),
                                    levels_[i].data[getInd(x,y)].model_(3,0)/levels_[i].data[getInd(x,y)].model_(0,0)) ))>0.8 ) //TODO: optimize normalize out
#endif
            )
        ) {
          ++found;
          levels_[i].data[getInd(x,y)].occopied=mark;
          model+=levels_[i].data[getInd(x,y)];
          if(hops>10 || found%20==0)
            model.get();
          bNew=bNew2;

          list.add(SubStructure::SVALUE(getInd(x+1,y),hops-1));
          list.add(SubStructure::SVALUE(getInd(x-1,y),hops-1));
          list.add(SubStructure::SVALUE(getInd(x,y+1),hops-1));
          list.add(SubStructure::SVALUE(getInd(x,y-1),hops-1));

        }
        else
          bNew2=true;
      }
      last_size=list.pos;
      list.pos=-1;
      //break;
    } while(bNew);

    if(first_lvl && found<MIN_LOD) {
      for(int j=0; j<list.size; j++) {
        if(levels_[i].data[list.vals[j].v].occopied==mark)
          levels_[i].data[list.vals[j].v].occopied=-1;
      }
      return;
    }

    if(i==(int)FINAL_LOD) {
      if(!list.size)
      {
        ROS_ASSERT(0);
        return;
      }

      if(only_planes_ && !model.isLinearAndTo())
        return;

      S_POLYGON poly;
      //model.isLinearAndTo();
      poly=model;
      poly.mark_ = mark;

#ifdef CHECK_CONNECTIVITY
      poly.connectivity_=connectivity;
#endif

      static std::vector<SubStructure::SXY> outs;
      SubStructure::SXY pt;

      outs.clear();

      for(int j=0; j<list.size; j++) {
        x= list.vals[j].v%levels_[i].w;
        y= list.vals[j].v/levels_[i].w;

        if(x>0 && y>0 && x<(int)levels_[i].w && y<(int)levels_[i].h && isOccupied(i,x,y)==mark) continue;

        pt.x=x;pt.y=y;
#ifdef USE_MIN_MAX_RECHECK_
        //        const float delta = (levels_[i+2].data[getInd2(x/4,y/4)].v_max_-
        //            model.model(levels_[i].data[getInd(x,y)].model_(1)/levels_[i].data[getInd(x,y)].model_(0,0),
        //                        levels_[i].data[getInd(x,y)].model_(3)/levels_[i].data[getInd(x,y)].model_(0,0)));
        //        pt.back = (delta > -0.01f && levels_[i+2].data[getInd2(x/4,y/4)].v_min_<0.1f) || delta>2*(model.get_max_gradient(levels_[i].data[getInd(x,y)])*levels_[i].data[getInd(x,y)].z_(0)/levels_[i].data[getInd(x,y)].model_(0,0)*(1<<i)/kinect_params_.f+4*0.03f);
        pt.back = levels_[i+2].data[getInd2(x/4,y/4)].v_max_-
            model.model(levels_[i+2].data[getInd2(x/4,y/4)].model_(1)/levels_[i+2].data[getInd2(x/4,y/4)].model_(0,0),
                        levels_[i+2].data[getInd2(x/4,y/4)].model_(3)/levels_[i+2].data[getInd2(x/4,y/4)].model_(0,0))
                        > (model.get_max_gradient(levels_[i+2].data[getInd2(x/4,y/4)])*levels_[i+2].data[getInd2(x/4,y/4)].z_(0)/levels_[i+2].data[getInd2(x/4,y/4)].model_(0,0)*(1<<i)/kinect_params_.f+4*0.03f);
        //TODO: improve this stupid thing (but it was easy :) )
#endif
        outs.push_back(pt);
      }

      outline(ch_, levels_[i].w,levels_[i].h,outs,i, poly, model, mark);
//      if(poly.segments_.size()<1)
//        ROS_WARN("segment empty");

        if(filter_>0.f) {
          float area = poly.area();

          //ROS_INFO("%f %f",poly.weight_*poly.model_.param.v_max_*poly.model_.param.v_max_/area,filter_);

          if(poly.weight_*poly.model_.param.v_max_*poly.model_.param.v_max_/area>filter_)
            polygons_.push_back(poly);
        }
        else
          polygons_.push_back(poly);

      return;
    }

    SubStructure::VISITED_LIST<SubStructure::SVALUE> list_lower;
    for(int j=0; j<list.size; j++) {
      x= list.vals[j].v%levels_[i].w;
      y= list.vals[j].v/levels_[i].w;

      hops=9;

      if(filterOccupied(i,x,y,mark))
        continue;

      bool above = y>0 &&                    filterOccupied(i,x,y-1,mark);
      bool below = y+1<(int)levels_[i].h &&  filterOccupied(i,x,y+1,mark);
      bool left  = x>0 &&                    filterOccupied(i,x-1,y,mark);
      bool right = x+1<(int)levels_[i].w &&  filterOccupied(i,x+1,y,mark);

      if(above&&below&&left&&right)
        continue;

      if(above) {
        list_lower.add(SubStructure::SVALUE(getInd1(2*x,2*y),hops/2));
        list_lower.add(SubStructure::SVALUE(getInd1(2*x+1,2*y),hops/2));
      }
      if(below) {
        list_lower.add(SubStructure::SVALUE(getInd1(2*x,2*y+1),hops/2));
        list_lower.add(SubStructure::SVALUE(getInd1(2*x+1,2*y+1),hops/2));
      }
      if(left && !above) list_lower.add(SubStructure::SVALUE(getInd1(2*x,2*y),hops/2));
      if(left && !below) list_lower.add(SubStructure::SVALUE(getInd1(2*x,2*y+1),hops/2));
      if(right && !above) list_lower.add(SubStructure::SVALUE(getInd1(2*x+1,2*y),hops/2));
      if(right && !below) list_lower.add(SubStructure::SVALUE(getInd1(2*x+1,2*y+1),hops/2));
    }

    grow(list_lower, model, i-1, mark, false);
  }

  template <typename Point, typename PointLabel>
  int Segmentation_QuadRegression<Point,PointLabel>::getPos(int *ch, const int xx, const int yy, const int w, const int h) {
    int p=0;
    const int i=0;
    for(int x=-1; x<=1; x++) {
      for(int y=-1; y<=1; y++) {
        if( xx+x>=0 && yy+y>=0 && xx+x<w && yy+y<h &&
            (x||y) && ch[getInd(xx+x,yy+y)]>0)
        {
          p |= (1<<Contour2D::SplineMap[ (y+1)*3 + x+1]);
        }
      }

    }
    return p;
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::outline(int *ch, const int w, const int h, std::vector<SubStructure::SXY> &out, const int i, S_POLYGON &poly, const SubStructure::Model &model, const int mark)
  {
//    std::cout<<"OFF:\n"<<poly.param_.col(0)<<"\n";
//    std::cout<<"PLANE:\n"<<poly.proj2plane_<<"\n";
//    std::cout<<"P1:\n"<<poly.param_.col(1)<<"\n";
//    std::cout<<"P2:\n"<<poly.param_.col(2)<<"\n";

#ifdef STOP_TIME
    PrecisionStopWatch ssw;
    ssw.precisionStart();
#endif

    SubStructure::SXYcmp ttt;
    std::sort(out.begin(),out.end(), ttt);

    for(size_t j=0; j<out.size(); j++) {
      ch[ getInd(out[j].x,out[j].y) ]=(int)j+1;
    }

#if DEBUG_LEVEL>200
    {  char buf[128];
    int *bs = new int[w*h];
    memset(bs,0,w*h*4);
    for(size_t j=0; j<out.size(); j++) {
      bs[ getInd(out[j].x,out[j].y) ]=-(out[j].back+1);
    }
    sprintf(buf,"/tmp/poly%d.ppm",polygons_.size());
    QQPF_Debug::ppm(buf,w,h,bs);
    delete [] bs;
    }
#endif

    if(outline_check_size_<out.size()) {
      delete [] outline_check_;
      outline_check_ = new bool[out.size()];
      outline_check_size_=out.size();
    }
    memset(outline_check_,false,out.size());

    int n=-1;
    while(n+1<(int)out.size()) {
      ++n;
      if(outline_check_[n])
        continue;

      poly.segments_.push_back(std::vector<Eigen::Vector3f>());
#ifdef USE_BOOST_POLYGONS_
      poly.segments2d_.push_back(std::vector<BoostPoint>());
#elif defined(BACK_CHECK_REPEAT)
      poly.segments2d_.push_back(std::vector<Eigen::Vector2i>());
#endif

      int x=out[n].x;
      int y=out[n].y;
      int bf=8;
      int v=0;
      int back=0;
      int start_x=x, start_y=y;

      addPoint(i,x,y,mark, poly,model);
      int num=0,numb=0;

      while(1) {

        if(x<0 || y<0 || x>=w || y>=h || ch[ getInd(x,y) ]<1) {
          break;
        }

        back+=out[ch[ getInd(x,y) ]-1].back?1:0;
        ++numb;
        outline_check_[ch[ getInd(x,y) ]-1]=true;
        ch[ getInd(x,y) ]=-2;
        int p=getPos(ch,x,y,w,h);

        if(p==0|| (!Contour2D::g_Splines[bf][p].x&&!Contour2D::g_Splines[bf][p].y) )
        {
          break;
        }

        v+=v+Contour2D::g_Splines[bf][p].v;
        x+=Contour2D::g_Splines[bf][p].x;
        y+=Contour2D::g_Splines[bf][p].y;
        bf=Contour2D::g_Splines[bf][p].bf;
        ++num;

        if(std::abs(v)>3) {
          v=0;
          Eigen::Vector2f tv;
          tv(0)=x;tv(1)=y;
          addPoint(i,x,y,mark, poly,model,back/(float)numb);
          numb=back=0;
        }
      }
      if(poly.segments_.back().size()>0) poly.segments_.back()[0](2)=back/(float)numb;

      if(poly.segments_.back().size()<3 || (std::abs(x-start_x)+std::abs(y-start_y))>10 ) {
        poly.segments_.erase(poly.segments_.end()-1);

#if defined(USE_BOOST_POLYGONS_) || defined(BACK_CHECK_REPEAT)
        poly.segments2d_.erase(poly.segments2d_.end()-1);
#endif
      }


    }

//    for(size_t j=0; j<poly.segments_.size(); j++)
//      for(size_t k=0; k<poly.segments_[j].size(); k++)
//      {
//        std::cout<<"s\n"<<poly.segments_[j][k].head<2>()<<"\n";
//        std::cout<<"p\n"<<poly.project2world(poly.segments_[j][k].head<2>())<<"\n";
//      }

    for(size_t j=0; j<out.size(); j++) {
      ch[ getInd(out[j].x,out[j].y) ]=0;
    }

#ifdef STOP_TIME
    execution_time_polyextraction_ += ssw.precisionStop();
#endif

  }

  template <typename Point, typename PointLabel>
  bool Segmentation_QuadRegression<Point,PointLabel>::compute() {
    polygons_.clear();
    buildTree(*input_);
    calc();
    return true;
  }

  template <typename Point, typename PointLabel>
  boost::shared_ptr<const pcl::PointCloud<PointLabel> > Segmentation_QuadRegression<Point,PointLabel>::compute_labeled_pc()
  {
    typename pcl::PointCloud<PointLabel>::Ptr out(new pcl::PointCloud<PointLabel>);

    ROS_ASSERT(levels_.size()>1);

    out->resize(levels_[0].w*levels_[0].h);
    out->width = levels_[0].w;
    out->height= levels_[0].h;

    for(size_t x=0; x<levels_[0].w; x++)
    {
      for(size_t y=0; y<levels_[0].h; y++)
      {
        //position
        const int i=0;
        (*out)(x,y).x = levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0);
        (*out)(x,y).y = levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0);
        (*out)(x,y).z = levels_[0].data[getInd(x,y)].z_(0)/levels_[0].data[getInd(x,y)].model_(0,0);

        //color/label
        int mark = isOccupied(0,x,y);
        SetLabeledPoint<PointLabel>( (*out)(x,y), mark);
      }
    }

    return out;
  }

  template <typename Point, typename PointLabel>
  boost::shared_ptr<const pcl::PointCloud<PointLabel> > Segmentation_QuadRegression<Point,PointLabel>::compute_reconstructed_pc()
  {
    typename pcl::PointCloud<PointLabel>::Ptr out(new pcl::PointCloud<PointLabel>);

    ROS_ASSERT(levels_.size()>0);

    out->resize(levels_[0].w*levels_[0].h);
    out->width = levels_[0].w;
    out->height= levels_[0].h;

    for(size_t x=0; x<levels_[0].w; x++)
    {
      for(size_t y=0; y<levels_[0].h; y++)
      {
        //position
        const int i=0;
        (*out)(x,y).x = levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0);
        (*out)(x,y).y = levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0);

        //color/label
        int mark = isOccupied(0,x,y);
        if(mark>=0)
          (*out)(x,y).z = polygons_[mark].model_.model(
              levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0),
              levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0)
          );
        else
          (*out)(x,y).x = (*out)(x,y).y = (*out)(x,y).z = 0;
        SetLabeledPoint<PointLabel>( (*out)(x,y), mark);

        if(mark>0 && mark<(int)polygons_.size())
        {
          const float z = levels_[0].data[getInd(x,y)].z_(0)/levels_[0].data[getInd(x,y)].model_(0,0);

          Eigen::Vector3f p;
          p(0) =
              levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0);
          p(1) =
              levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0);
          p(2) = z;
          //const float d = std::min(std::abs(z - z_model), (polygons_[i].project2world(polygons_[i].nextPoint(p))-p).norm());
          //(*out)(x,y).r = (*out)(x,y).g = (*out)(x,y).b = d/0.1f * 255;
        }

      }
    }

    return out;
  }

  template <typename Point, typename PointLabel>
  void Segmentation_QuadRegression<Point,PointLabel>::compute_accuracy(float &mean, float &var, size_t &used, size_t &mem, size_t &points, float &avg_dist)
  {
    typename pcl::PointCloud<PointLabel>::Ptr out(new pcl::PointCloud<PointLabel>);

    pcl::KdTreeFLANN<Point> kdtree;

    std::vector<int> pointIdxNKNSearch(1);
    std::vector<float> pointNKNSquaredDistance(1);

    kdtree.setInputCloud (input_);

    ROS_ASSERT(levels_.size()>0);

    out->resize(levels_[0].w*levels_[0].h);
    out->width = levels_[0].w;
    out->height= levels_[0].h;

    RunningStat rstat;
    points = 0;
    avg_dist = 0;

    for(size_t x=0; x<levels_[0].w; x++)
    {
      for(size_t y=0; y<levels_[0].h; y++)
      {
        //position
        const int i=0;

        //color/label
        int mark = isOccupied(0,x,y);

        if(mark>0 && mark<(int)polygons_.size())
        {
          const float z_model = polygons_[mark].model_.model(
              levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0),
              levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0)
          );
          const float z = levels_[0].data[getInd(x,y)].z_(0)/levels_[0].data[getInd(x,y)].model_(0,0);

          Eigen::Vector3f p;
          p(0) =
              levels_[0].data[getInd(x,y)].model_(0,1)/levels_[0].data[getInd(x,y)].model_(0,0);
          p(1) =
              levels_[0].data[getInd(x,y)].model_(0,3)/levels_[0].data[getInd(x,y)].model_(0,0);
          p(2) = z;

          Point ps;
          ps.x = p(0);
          ps.y = p(1);
          ps.z = z_model;
          kdtree.nearestKSearch(ps, 1, pointIdxNKNSearch, pointNKNSquaredDistance);
          const float d = std::min(pointNKNSquaredDistance[0], std::min(std::abs(z - z_model), (polygons_[i].project2world(polygons_[i].nextPoint(p))-p).norm()));

          if(pcl_isfinite(d) && pcl_isfinite(z))
          {
            rstat.Push(d);
            avg_dist += z;
          }
        }

        if(levels_[0].data[getInd(x,y)].z_(0)/levels_[0].data[getInd(x,y)].model_(0,0)>0 && pcl_isfinite(levels_[0].data[getInd(x,y)].z_(0)/levels_[0].data[getInd(x,y)].model_(0,0)))
          points++;

      }
    }

    //points = levels_[0].w*levels_[0].h;
    used = rstat.NumDataValues();
    mem = 0;
    for(size_t i=0; i<polygons_.size(); i++)
      mem+=4*6 + polygons_[i].segments_.size()*2*4;

    mean = rstat.Mean();
    var = rstat.Variance();
    avg_dist /= points;
  }

  template <typename Point, typename PointLabel>
  Segmentation_QuadRegression<Point,PointLabel>::operator cob_3d_mapping_msgs::ShapeArray() const {
    cob_3d_mapping_msgs::ShapeArray sa;

    cob_3d_mapping_msgs::Shape s;
    if(only_planes_)
      s.type = cob_3d_mapping_msgs::Shape::POLYGON;
    else
      s.type = cob_3d_mapping_msgs::Shape::CURVED;

    sa.header.frame_id = s.header.frame_id = "/test";

    for(size_t i=0; i<polygons_.size(); i++) {
      if(polygons_[i].segments_.size()<1) continue;

      Eigen::Vector3f mi, ma;

      s.params.clear();
      s.centroid.x=(polygons_[i].param_.col(0)(0));
      s.centroid.y=(polygons_[i].param_.col(0)(1));
      s.centroid.z=(polygons_[i].param_.col(0)(2));

      /*s.params.push_back(polygons_[i].param_.col(1)(0));
      s.params.push_back(polygons_[i].param_.col(1)(1));

      s.params.push_back(polygons_[i].param_.col(2)(0));
      s.params.push_back(polygons_[i].param_.col(2)(1));
      s.params.push_back(polygons_[i].param_.col(2)(2));

      s.params.push_back(polygons_[i].proj2plane_.col(0)(0));
      s.params.push_back(polygons_[i].proj2plane_.col(0)(1));
      s.params.push_back(polygons_[i].proj2plane_.col(0)(2));

      s.params.push_back(polygons_[i].proj2plane_.col(1)(0));
      s.params.push_back(polygons_[i].proj2plane_.col(1)(1));
      s.params.push_back(polygons_[i].proj2plane_.col(1)(2));*/


      if(only_planes_) {
        ROS_ASSERT( std::abs(polygons_[i].model_.p(2))<0.001f &&
                    std::abs(polygons_[i].model_.p(4))<0.001f &&
                    std::abs(polygons_[i].model_.p(5))<0.001f );

        Eigen::Vector3f n = polygons_[i].model_.getNormal(0,0), v;
        v.fill(0);
        v(2) = polygons_[i].model_.p(0);
        for(int i=0; i<3; i++) s.params.push_back(n(i));
        s.params.push_back(n.dot(v));
      }
      else {
        for(int k=0; k<6; k++)
          s.params.push_back(polygons_[i].model_.p(k));
      }

      s.weight = polygons_[i].weight_;

      s.color.r=polygons_[i].color_[0];
      s.color.g=polygons_[i].color_[1];
      s.color.b=polygons_[i].color_[2];
      s.color.a=1.f;
      //s.color.a=std::min(1000.f,polygons_[i].weight_)/1000.f;

      s.points.clear();
      float backs=0;
      for(size_t j=0; j<polygons_[i].segments_.size(); j++) {
        pcl::PointCloud<pcl::PointXYZ> pc;
        pcl::PointXYZ pt;

        for(size_t k=0; k<polygons_[i].segments_[j].size(); k++) {
          pt.x=polygons_[i].segments_[j][k](0);
          pt.y=polygons_[i].segments_[j][k](1);
          pt.z=polygons_[i].segments_[j][k](2);
          if(j==0) {
            backs+=polygons_[i].segments_[j][k](2);
            if(k==0)
              mi = ma = polygons_[i].project2world( polygons_[i].segments_[j][k].head<2>() );
            else
            {
              Eigen::Vector3f t = polygons_[i].project2world( polygons_[i].segments_[j][k].head<2>() );
              mi(0) = std::min(t(0),mi(0));
              mi(1) = std::min(t(1),mi(1));
              mi(2) = std::min(t(2),mi(2));
              ma(0) = std::max(t(0),ma(0));
              ma(1) = std::max(t(1),ma(1));
              ma(2) = std::max(t(2),ma(2));
            }
          }
          if(pcl_isfinite(pt.x) && pcl_isfinite(pt.y)) {
            pc.push_back(pt);
          }
        }

        sensor_msgs::PointCloud2 pc2;
        pcl::toROSMsg(pc,pc2);

        s.points.push_back(pc2);
        s.holes.push_back(j>0);
      }

      //ROS_INFO("density %f",(mi-ma).squaredNorm()/(polygons_[i].weight_*polygons_[i].weight_));
      //if( (mi-ma).squaredNorm()/(polygons_[i].weight_*polygons_[i].weight_)<0.00002f)
      sa.shapes.push_back(s);
    }

    return sa;
  }


  template <typename Point, typename PointLabel>
  Segmentation_QuadRegression<Point,PointLabel>::operator visualization_msgs::Marker() const {
    visualization_msgs::Marker m;
    m.type = visualization_msgs::Marker::LINE_LIST;
    m.id = 0;
    m.action = visualization_msgs::Marker::ADD;
    m.pose.position.x = 0;
    m.pose.position.y = 0;
    m.pose.position.z = 0;
    m.pose.orientation.x = 0.0;
    m.pose.orientation.y = 0.0;
    m.pose.orientation.z = 0.0;
    m.pose.orientation.w = 1.0;
    m.scale.x = 0.02;
    m.scale.y = 0.1;
    m.scale.z = 0.1;
    m.color.a = 1.0;
    m.color.r = 1.0;
    m.color.g = 1.0;
    m.color.b = 1.0;

    for(size_t i=0; i<polygons_.size(); i++) {

//      std::cerr<<"OFF:\n"<<polygons_[i].param_.col(0)<<"\n";
//      std::cerr<<"PLANE:\n"<<polygons_[i].proj2plane_<<"\n";
//      std::cerr<<"P1:\n"<<polygons_[i].param_.col(1)<<"\n";
//      std::cerr<<"P2:\n"<<polygons_[i].param_.col(2)<<"\n";

      if(polygons_[i].segments_.size()<1) continue;

      for(size_t j=0; j<polygons_[i].segments_.size(); j++) {

        for(size_t k=0; k<polygons_[i].segments_[j].size(); k++)
        {
          std_msgs::ColorRGBA c;
          geometry_msgs::Point p;
          Eigen::Vector3f v1,v2;

          c.r=polygons_[i].segments_[j][(k+1)%polygons_[i].segments_[j].size()](2);
          c.g=0;
          c.b=1-c.r;
          c.a=1;

          v1 = polygons_[i].project2world(polygons_[i].segments_[j][k].head<2>());
          v2 = polygons_[i].project2world(polygons_[i].segments_[j][(k+1)%polygons_[i].segments_[j].size()].head<2>());


//          std::cerr<<"s\n"<<polygons_[i].segments_[j][k].head<2>()<<"\n";
//          std::cerr<<"P\n"<<v1<<"\n";

          if(!pcl_isfinite(v1.sum()) || !pcl_isfinite(v2.sum()))
            continue;

          //          std::cout<<"O\n"<<v1<<"\n";

          p.x = v1(0);
          p.y = v1(1);
          p.z = v1(2);
          m.points.push_back(p);
          m.colors.push_back(c);

          p.x = v2(0);
          p.y = v2(1);
          p.z = v2(2);
          m.points.push_back(p);
          m.colors.push_back(c);
        }
      }
    }

    return m;
  }

  template <typename Point, typename PointLabel>
  bool Segmentation_QuadRegression<Point,PointLabel>::extractImages() {
#ifdef USE_COLOR
    const pcl::PointCloud<Point> &pc = (*input_);

    for(size_t i=0; i<polygons_.size(); i++) {
      polygons_[i].img_.reset(new sensor_msgs::Image);

      if(polygons_[i].segments2d_.size()<1 || polygons_[i].segments2d_[0].size()<3) continue;

      //2d rect
      int mix=input_->width, miy=input_->height, max=-1, may=-1;
      for(size_t j=0; j<polygons_[i].segments2d_[0].size(); j++) { //outer hull
        mix = std::min(mix, polygons_[i].segments2d_[0][j](0));
        max = std::max(max, polygons_[i].segments2d_[0][j](0));
        miy = std::min(miy, polygons_[i].segments2d_[0][j](1));
        may = std::max(may, polygons_[i].segments2d_[0][j](1));
      }
#ifndef DO_NOT_DOWNSAMPLE_
      mix*=2;
      max*=2;
      miy*=2;
      may*=2;
#endif

      int w=max-mix, h=may-miy;
      max-=w/4;
      mix+=w/4;
      may-=h/4;
      miy+=h/4;

      polygons_[i].img_->width  = max-mix+1;
      polygons_[i].img_->height = may-miy+1;
      polygons_[i].img_->encoding = "rgb8";
      polygons_[i].img_->step = polygons_[i].img_->width*3;
      polygons_[i].img_->is_bigendian = false;
      polygons_[i].img_->data.resize( polygons_[i].img_->step*polygons_[i].img_->height );

      polygons_[i].color_[0] = polygons_[i].color_[1] = polygons_[i].color_[2] = 0.f;
      for(int x=mix; x<=max; x++) {
        for(int y=miy; y<=may; y++) {
          polygons_[i].img_->data[(y-miy)*polygons_[i].img_->step + 3*(x-mix) + 0] = pc[getIndPC(x,y)].r;
          polygons_[i].img_->data[(y-miy)*polygons_[i].img_->step + 3*(x-mix) + 1] = pc[getIndPC(x,y)].g;
          polygons_[i].img_->data[(y-miy)*polygons_[i].img_->step + 3*(x-mix) + 2] = pc[getIndPC(x,y)].b;

          polygons_[i].color_[0] += pc[getIndPC(x,y)].r;
          polygons_[i].color_[1] += pc[getIndPC(x,y)].g;
          polygons_[i].color_[2] += pc[getIndPC(x,y)].b;
        }
      }

      polygons_[i].color_[0] /= 255.f*polygons_[i].img_->width*polygons_[i].img_->height;
      polygons_[i].color_[1] /= 255.f*polygons_[i].img_->width*polygons_[i].img_->height;
      polygons_[i].color_[2] /= 255.f*polygons_[i].img_->width*polygons_[i].img_->height;
    }
  return true;
#else
  return false;
#endif
  }
