#include <iostream>
#include <chrono>
#include <eigen3/Eigen/Eigen>
#include <spirit/spirit.h>
#include <spirit/BikeParameters.h>
#include <math.h>

int main(int argc, char** argv){

    // Testing OSG Gui
    // create ground
    spPose gnd = spPose::Identity();
    gnd.translate(spTranslation(0,0,-0.525));

    std::shared_ptr<Objects> objs = std::make_shared<Objects>(spPhyEngineType::PHY_NONE);
    //std::shared_ptr<Objects> objs = std::make_shared<Objects>(spPhyEngineType::PHY_BULLET);
    spObjectHandle box_handle = objs->CreateBox(gnd, spBoxSize(25,25,1), 0, spColor(1,1,1));

    // create bike
    BikeParams params;
    spObjectHandle bike_handle = objs->CreateVehicle(params.bike_param);

    // mesh
    //osg::ref_ptr<osg::Node> meshnode = osgDB::readNodeFile( "lab_v2.ply" );
    //spObjectHandle mesh_handle = objs->CreateMesh(meshnode);

    // set gui and add objects
    Gui gui;
    gui.Create(spGuiType::GUI_OSG);
    gui.AddObject(objs->GetObject(box_handle));
    gui.AddObject(objs->GetObject(bike_handle));
    //gui.AddObject(objs->GetObject(mesh_handle));
    spBike& bike = ((spBike&)objs->GetObject(bike_handle));


    //spState state;
    //state.pose = spPose::Identity();
    //state.pose.translate(spTranslation(0,0,0));
    //bike.SetState(state);
    //std::shared_ptr<spState> state_ptr = std::make_shared<spState>(state);

    // /*
    // BVP
    spTrajectory traj(gui, objs);

    /*
    // Add waypoints
    spPose pose0(spPose::Identity());
    pose0.translate(spTranslation(-1,-1,0));
    Eigen::AngleAxisd rot0(0,Eigen::Vector3d::UnitZ());
    pose0.rotate(rot0);
    traj.AddWaypoint(pose0,0);
    traj.GetWaypoint(0).SetLinearVelocityDirection(spLinVel(0,1,0));

    spPose pose1(spPose::Identity());
    pose1.translate(spTranslation(3,3,0));
    double angle = SP_PI/10;
    Eigen::AngleAxisd rot1(0,Eigen::Vector3d::UnitZ());
    pose1.rotate(rot1);
    traj.AddWaypoint(pose1,4);
    traj.GetWaypoint(1).SetLinearVelocityDirection(spLinVel(1,0,0));
    */

    // put waypoints on a elliptical path
    double a = 1.5;
    double b = 1.5;
    int num_waypoints = 8;
    for(int ii=0; ii<num_waypoints; ii++) {
        // calculate ellipse radius from theta and then get x , y coordinates of ellipse from r and theta
        double theta = ii*(2*SP_PI)/num_waypoints;
        double r = (a*b)/sqrt(b*b*pow(cos(theta),2)+a*a*pow(sin(theta),2));
        double x = r*cos(theta);
        double y = r*sin(theta);
        // slope of the line is
        double angle = atan2(-(x*b*b),(y*a*a));
        spPose pose(spPose::Identity());
        pose.translate(spTranslation(x,y,0.07));
        Eigen::AngleAxisd rot(angle+SP_PI_HALF/*+0.6*/,Eigen::Vector3d::UnitZ());
        pose.rotate(rot);
        traj.AddWaypoint(pose,1);
        spRotVel rotvel(0,0,2);
        traj.GetWaypoint(ii).SetRotVel(rotvel);
        traj.GetWaypoint(ii).SetLinearVelocityDirection(spLinVel(0,1,0));
    }

    traj.IsLoop(true);

    // Solve local plan
    spLocalPlanner<BikeSimFunctorRK4> localplanner(params.bike_param, false, &gui);
    spBVPWeightVec weight_vec;
    weight_vec << 100, 100, 0, 0, 0, 10, 0.009, 0.009, 0.009, 0.01, 0.01, 0.01, 0.1;
    localplanner.SetCostWeight(weight_vec);

    for(int ii=0; ii<traj.GetNumWaypoints(); ii++){
        traj.SetTravelDuration(ii, 2);
        localplanner.SolveInitialPlan(traj, ii);
        localplanner.SolveLocalPlan(traj);
    }

    // set cars initial pose
    spState state;
    state.pose = traj.GetWaypoint(0).GetPose();
    state.pose.translate(spTranslation(0,0,0));
    bike.SetState(state);
    std::shared_ptr<spState> state_ptr = std::make_shared<spState>(state);

    // /*
    std::cout<<"Control are ->"<<traj.GetControls(0)<<std::endl;
    BikeSimFunctorRK4 mysim(params.bike_param,state);
    //std::shared_ptr<spStateSeries> series = std::make_shared<spStateSeries>();
    while(!gui.ShouldQuit()){
        mysim(0,1,0.1,traj.GetControls(0),0,-1,0,state_ptr);
        bike.SetState(mysim.GetState());
        gui.Iterate(objs);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double yaw = mysim.GetState().pose.rotation().eulerAngles(0,1,2)[2];
        double lin_vel = std::sqrt(std::pow(mysim.GetState().linvel[0],2) + std::pow(mysim.GetState().linvel[1],2) + std::pow(mysim.GetState().linvel[2],2));
        std::cout<<"Yaw: "<<yaw<<" linear velocity: "<<lin_vel<<std::endl;
    } // */

     /*
    // MPC reference trackinglin_vel
    float horizon = 5;
    spMPC<BikeSimFunctorRK4> mpc(params.bike_param,horizon);

    spCtrlPts2ord_2dof controls;
    controls.col(0) = Eigen::Vector2d(0,10);
    controls.col(1) = Eigen::Vector2d(0,10);
    controls.col(2) = Eigen::Vector2d(0,10);

    BikeSimFunctorRK4 mysim(params.bike_param, state);


    if(mpc.CalculateControls(traj, bike.GetState(), controls)){
      std::cout << "controls -> \n" << controls << std::endl;

      while(!gui.ShouldQuit()){
          std::shared_ptr<spStateSeries> series = std::make_shared<spStateSeries>();
          mysim(0,(int)(horizon/DISCRETIZATION_STEP_SIZE),DISCRETIZATION_STEP_SIZE,controls,0,-1,series,state_ptr);
          for(int ii=0; ii<(int)(horizon/DISCRETIZATION_STEP_SIZE); ii++){
              spState tstate(*((*series)[ii]));
              bike.SetState(tstate);
              gui.Iterate(objs);

              double yaw = mysim.GetState().pose.rotation().eulerAngles(0,1,2)[2];
              std::cout<<"Yaw ->"<<yaw<<" "<<"speed -> " << tstate.linvel.norm() << std::endl;
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
       } // */
    //}

     // */

    //spCurve controls_curve(2,2);
    //traj.SetTravelDuration(0,1);
    //localplanner.SolveInitialPlan(traj,0);
    //double final_cost = localplanner.SolveLocalPlan(traj,0);
    //std::cout << "final cost: " << final_cost << std::endl;
    //controls_curve.SetBezierControlPoints(traj.GetControls(0));
    //std::cout << "bezier control points:\n"<<controls_curve.GetBezierControlPoints() << std::endl;

     /*
    // MPC controller circle maneuver
    float horizon = 5;
    double radius = 1;  // radius of circle
    double vel = 1;
    spMPC<BikeSimFunctorRK4> mpc(params.bike_param,horizon);

    spCtrlPts2ord_2dof inputcmd_curve;
    inputcmd_curve.col(0) = Eigen::Vector2d(0,0);
    inputcmd_curve.col(1) = Eigen::Vector2d(0,0);
    inputcmd_curve.col(2) = Eigen::Vector2d(0,0);

    BikeSimFunctorRK4 mysim(params.bike_param,state);

    if(mpc.CircleManReg(state,inputcmd_curve,radius,vel)) {
      std::cout << "controls -> \n" << inputcmd_curve << std::endl;

      std::shared_ptr<spStateSeries> series = std::make_shared<spStateSeries>();
      mysim(0,(int)(horizon/DISCRETIZATION_STEP_SIZE),DISCRETIZATION_STEP_SIZE,inputcmd_curve,0,-1,series,state_ptr);
      while(1){
      for(int ii=0; ii<(int)(horizon/DISCRETIZATION_STEP_SIZE); ii++){
        spState state(*((*series)[ii]));
        bike.SetState(state);
        gui.Iterate(objs);
        double yaw = mysim.GetState().pose.rotation().eulerAngles(0,1,2)[2];
        std::cout<<"Yaw ->"<<yaw<<" "<<"speed -> " << state.linvel.norm() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      std::cout << "done " << std::endl;
        gui.Iterate(objs);
      }
    } // */


     /*
    while(!gui.ShouldQuit())
    {

        BikeSimFunctorRK4 mysim(params.bike_param,state);
        mysim(0,1,0.1,inputcmd_curve,0,0,nullptr,state_ptr);
        bike.SetState(mysim.GetState());
        gui.Iterate(objs);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double yaw = mysim.GetState().pose.rotation().eulerAngles(0,1,2)[2];
        double lin_vel = std::sqrt(std::pow(mysim.GetState().linvel[0],2) + std::pow(mysim.GetState().linvel[1],2) + std::pow(mysim.GetState().linvel[2],2));
        std::cout<<"Yaw: "<<yaw<<" linear velocity: "<<lin_vel<<std::endl;

    } // */
    return 0;
}


