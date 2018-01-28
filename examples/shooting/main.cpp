#include <HAL/Gamepad.pb.h>
#include <HAL/Gamepad/GamepadDevice.h>
#include <HAL/Car.pb.h>
#include <HAL/Car/CarDevice.h>
#include <thread>
//#include <signal.h>
#include <spirit/spirit.h>
#include <HAL/Posys/PosysDevice.h>
#include <atomic>


hal::CarCommandMsg commandMSG;
double gamepad_steering = 0;
double gamepad_throttle = 0;
spPose posys_;
bool flag_auto = false;

void Posys_Handler(hal::PoseMsg& PoseData) {
    posys_ = spPose::Identity();
    posys_.translate(spTranslation(PoseData.pose().data(0),PoseData.pose().data(1),0.06/*PoseData.pose().data(2)*/));
    spRotation rot(PoseData.pose().data(6),PoseData.pose().data(3),PoseData.pose().data(4),PoseData.pose().data(5));
    Eigen::AngleAxisd tracker_rot(-SP_PI_HALF,Eigen::Vector3d::UnitZ());
    posys_.rotate(rot);
    posys_.rotate(tracker_rot);
}

void GamepadCallback(hal::GamepadMsg& _msg) {
  gamepad_steering = -_msg.axes().data(0);
  gamepad_throttle = _msg.axes().data(4)*30;
  if(_msg.buttons().data(5)){
      flag_auto = true;
  } else {
      flag_auto = false;
  }
}

void CarSensorCallback(hal::CarStateMsg msg) {
//    std::cout << "-> "
//              << msg.swing_angle_fl() << ", "
//              << msg.swing_angle_fr() << ", "
//              << msg.swing_angle_rl() << ", "
//              << msg.swing_angle_rr() << ", "
//              << msg.wheel_speed_fl() << ", "
//              << msg.wheel_speed_fr() << ", "
//              << msg.wheel_speed_fl() << ", "
//              << msg.wheel_speed_rl() << ", "
//              << msg.whflag_autoeel_speed_rr() << ", "
//              << -msg.steer_angle() << ", "
//              << std::endl;
}

int main(int argc, char** argv) {
  // connect to a gamepad
  hal::Gamepad gamepad("gamepad:/");
//  gamepad.RegisterGamepadDataCallback(&GamepadCallback);

  // Connect to NinjaV3Car
//  hal::Car ninja_car("ninja_v3:[baud=115200,dev=/dev/ttyUSB0]//");

  //ninja_car.RegisterCarStateDataCallback(&CarSensorCallback);

  // initialize command packet
  commandMSG.set_steering_angle(0);
  commandMSG.set_throttle_percent(0);
  //////////////////////////////
//  hal::Posys vicon("vicon://tracker:[ninja]");
//  vicon.RegisterPosysDataCallback(&Posys_Handler);
  /////////////////////////////
  spSettings settings_obj;
  settings_obj.SetGuiType(spGuiType::GUI_PANGOSCENEGRAPH);
  settings_obj.SetPhysicsEngineType(spPhyEngineType::PHY_BULLET);
  spirit spworld(settings_obj);

  std::vector<spObjectHandle> cars;
#define num_cars 27
  double disc_step_size = 0.01;
  double simulation_length = 1;

  spworld.car_param.pose.translate(spTranslation(1.5,0,0));
  spObjectHandle car_handle = spworld.objects_.CreateVehicle(spworld.car_param);
  spworld.gui_.AddObject(spworld.objects_.GetObject(car_handle));
  spAWSDCar& estimation_car = (spAWSDCar&) spworld.objects_.GetObject(car_handle);

  // create a flat ground
  spPose gnd_pose_ = spPose::Identity();
  gnd_pose_.translate(spTranslation(0,0,-0.5));
  spObjectHandle gnd_handle = spworld.objects_.CreateBox(gnd_pose_,spBoxSize(50,50,1),0,spColor(0,1,0));
  spworld.gui_.AddObject(spworld.objects_.GetObject(gnd_handle));

  // set friction coefficent of ground
  ((spBox&)spworld.objects_.GetObject(gnd_handle)).SetFriction(1);

  /////////////////////////////////

  std::vector<std::shared_ptr<spStateSeries>> sim_traj;
  spState current_state = estimation_car.GetState();
  std::shared_ptr<spState> state_ptr = std::make_shared<spState>(current_state);
  std::vector<std::shared_ptr<CarSimFunctor>> sims;
  std::vector<std::shared_ptr<double>> costs;

  // create search pattern
  std::vector<spCtrlPts2ord_2dof> cntrl_vars_vec(num_cars);
  for(int jj=0; jj<=2; jj++) {
    for(int ii=0; ii<=8; ii++) {
      cntrl_vars_vec[(9*jj)+ii].col(0) = Eigen::Vector2d(spworld.car_param.steering_servo_lower_limit+ii*(spworld.car_param.steering_servo_upper_limit-spworld.car_param.steering_servo_lower_limit)/8,10+jj*5);
      cntrl_vars_vec[(9*jj)+ii].col(1) = Eigen::Vector2d(spworld.car_param.steering_servo_lower_limit+ii*(spworld.car_param.steering_servo_upper_limit-spworld.car_param.steering_servo_lower_limit)/8,10+jj*5);
      cntrl_vars_vec[(9*jj)+ii].col(2) = Eigen::Vector2d(spworld.car_param.steering_servo_lower_limit+ii*(spworld.car_param.steering_servo_upper_limit-spworld.car_param.steering_servo_lower_limit)/8,10+jj*5);
    }
  }

  // create cars.
  for(int ii = 0; ii < num_cars; ii++) {
    sims.push_back(std::make_shared<CarSimFunctor>(spworld.car_param,current_state));
    costs.push_back(std::make_shared<double>(0));
  }

  while(1){
    spTimestamp t0 = spGeneralTools::Tick();
    sim_traj.clear();
    costs.clear();
    for(int ii = 0; ii < num_cars; ii++) {
      costs.push_back(std::make_shared<double>(0));
    }

    // Update current state
    *state_ptr = estimation_car.GetState();

    // Run simulations
    for(int ii = 0; ii < num_cars; ii++) {
      sim_traj.push_back(std::make_shared<spStateSeries>());
      sims[ii]->RunInThread(ii,(int)(simulation_length/disc_step_size), disc_step_size, cntrl_vars_vec[ii], 0, -1, sim_traj[ii], state_ptr,costs[ii]);
    }
    for(int ii = 0; ii < num_cars; ii++) {
      sims[ii]->WaitForThreadJoin();
    }

    // Pick lowest cost constrol signal and apply to the vehicle
    int lowest_cost_index = 0;
    double lowest_cost = *(costs[lowest_cost_index]);
    for(int ii=1; ii<num_cars; ii++) {
      if(*(costs[ii])<lowest_cost) {
        lowest_cost = *(costs[ii]);
        lowest_cost_index = ii;
      }
    }

    // calc the processing time
    double time = spGeneralTools::Tock_ms(t0);
//    std::cout << "time is " << time << std::endl;

    std::cout << "cost " << lowest_cost << " , " << lowest_cost_index << std::endl;
    estimation_car.SetFrontSteeringAngle((cntrl_vars_vec[lowest_cost_index]).col(0)[0]);
    estimation_car.SetEngineMaxVel((cntrl_vars_vec[lowest_cost_index]).col(0)[1]);
    spworld.objects_.StepPhySimulation(0.1);
    spworld.gui_.Iterate(spworld.objects_);
//      std::this_thread::sleep_for(std::chrono::milliseconds(100));

  }
  return 0;
}
