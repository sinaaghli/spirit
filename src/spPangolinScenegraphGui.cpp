#include <spirit/Gui/spPangolinScenegraphGui.h>

spPangolinScenegraphGui::spPangolinScenegraphGui()
    : a_button_("ui.A_Button", false, false),
      a_double_("ui.A_Double", 3, 0, 5),
      an_int_("ui.An_Int", 2, 0, 5),
      a_double_log_("ui.Log_scale var", 3, 1, 1E4, true),
      a_checkbox_("ui.A_Checkbox", false, true),
      an_int_no_input_("ui.An_Int_No_Input", 2),
      save_window_("ui.Save_Window", false, false) {}

spPangolinScenegraphGui::~spPangolinScenegraphGui() {
  // remove globjects in row
  for (int ii = globjects_.size() - 1; ii >= 0; ii--) {
    glscenegraph_.RemoveChild(globjects_[ii]);
    delete (globjects_[ii]);
  }
}

void spPangolinScenegraphGui::InitGui() {
  if (!spGeneralTools::CheckFileExists(SPIRITGUI_PARAM_FILE)) {
    std::cerr << "Error: Missing Pangolin config file." << std::endl;
  } else {
    // Load configuration data
    pangolin::ParseVarsFile(SPIRITGUI_PARAM_FILE);
  }
  // Create OpenGL window in single line
  pangolin::CreateWindowAndBind(SPIRITGUI_WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT);

  SceneGraph::GLSceneGraph::ApplyPreferredGlSettings();
  glClearColor(0, 0, 0, 0);
  glewInit();

  // 3D Mouse handler requires depth testing to be enabled
  glEnable(GL_DEPTH_TEST);

  glrenderstate_ = pangolin::OpenGlRenderState(
      pangolin::ProjectionMatrix(WINDOW_WIDTH, WINDOW_HEIGHT, 420, 420,
                                 WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 0.1,
                                 1000),
      pangolin::ModelViewLookAt(2, 2, 2, 0, 0, 0, pangolin::AxisZ));

  handler_scenegraph_ = new SceneGraph::HandlerSceneGraph(
      glscenegraph_, glrenderstate_, pangolin::AxisZ, 0.01f);

  // Add named OpenGL viewport to window and provide 3D Handler
  pangoview_.SetBounds(0.0, 1.0, 0, 1.0, -(double)WINDOW_WIDTH / WINDOW_HEIGHT)
      .SetHandler(handler_scenegraph_)
      .SetDrawFunction(
          SceneGraph::ActivateDrawFunctor(glscenegraph_, glrenderstate_));

  // Create Globjects
  SceneGraph::GLGrid grid(10, 1, false);
  globjects_.push_back(new SceneGraph::GLGrid(grid));
  globjects_.push_back(new SceneGraph::GLLight(1000, 1000, 1000));
  globjects_.push_back(new SceneGraph::GLLight(0, 0, 10));
  globjects_.push_back(new SceneGraph::GLLight(0, 0, 0));

  // Add already created globjects to glscenegraph_
  for (int ii = 0; ii < globjects_.size(); ii++) {
    glscenegraph_.AddChild(globjects_[ii]);
  }

  pangolin::DisplayBase().AddDisplay(pangoview_);

  // Add named Panel and bind to variables beginning 'ui'
  // A Panel is just a View with a default layout and input handling
  pangolin::CreatePanel("ui")
      .SetBounds(0.0, 1.0, 0.0, pangolin::Attach::Pix(UI_PANEL_WIDTH));

  //////////////////////////////////////////////////
  /// Register Keyboard actions
  pangolin::RegisterKeyPressCallback(
      pangolin::PANGO_CTRL + 'b',
      pangolin::SetVarFunctor<double>("ui.A Double", 3.5));

  // Demonstration of how we can register a keyboard hook to trigger a method
  pangolin::RegisterKeyPressCallback(pangolin::PANGO_CTRL + 'r',
                                     this->KeyActionMethodSample);
}

bool spPangolinScenegraphGui::ShouldQuit() { return (pangolin::ShouldQuit()); }

void spPangolinScenegraphGui::Refresh() {
  // Clear entire screen
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Activate efficiently by object
  pangoview_.Activate(glrenderstate_);

  // Swap frames and Process Events
  pangolin::FinishFrame();
}

void spPangolinScenegraphGui::CheckKeyboardAction() {
  if (pangolin::Pushed(a_button_))
    std::cout << "You Pushed a button!" << std::endl;

  // Overloading of Var<T> operators allows us to treat them like
  // their wrapped types, eg:
  if (a_checkbox_) an_int_ = a_double_;

  an_int_no_input_ = an_int_;

  if (pangolin::Pushed(save_window_)) {
    pangolin::SaveWindowOnRender("window");
  }
}

void spPangolinScenegraphGui::KeyActionMethodSample() {
  std::cout
      << "KeyActionMethodSample() method of spPangolinScenegraph is called ..."
      << std::endl;
}

void spPangolinScenegraphGui::AddBox(spBox &box) {
  SceneGraph::GLBox glbox;
  glbox.SetPose(box.GetPose().matrix());
  glbox.SetScale(box.GetDimensions());
  glbox.SetCheckerboard(0);
  globjects_.push_back(new SceneGraph::GLBox(glbox));
  box.SetGuiIndex(globjects_.size()-1);
  glscenegraph_.AddChild(globjects_[globjects_.size()-1]);
}

void spPangolinScenegraphGui::AddWaypoint(spWaypoint& waypoint) {
  SceneGraph::GLWayPoint* glwaypoint = new SceneGraph::GLWayPoint();
  glwaypoint->SetPose(waypoint.GetPose().matrix());
  glwaypoint->SetCubeDim(UI_WAYPOINT_BOX_DIM);
  glwaypoint->SetColor(waypoint.GetColor()[0],waypoint.GetColor()[1],waypoint.GetColor()[2]);
  glwaypoint->SetVelocity(waypoint.GetLength());
  globjects_.push_back(glwaypoint);
  waypoint.SetGuiIndex(globjects_.size()-1);
  glscenegraph_.AddChild(globjects_[globjects_.size()-1]);
}

void spPangolinScenegraphGui::AddVehicle(spVehicle& vehicle)
{
  // draw vehicle with a box for chassis and four cylinders for wheels
  SceneGraph::GLBox glchassis;
  glchassis.SetPose(vehicle.GetPose().matrix());
  glchassis.SetScale(vehicle.GetChassisSize());
  globjects_.push_back(new SceneGraph::GLBox(glchassis));
  vehicle.SetGuiIndex(globjects_.size()-1);
  glscenegraph_.AddChild(globjects_[globjects_.size()-1]);

  for(int ii=0; ii<vehicle.GetNumberOfWheels(); ii++) {
    SceneGraph::GLCylinder glwheel;
    glwheel.Init(vehicle.GetWheel(ii)->GetRadius(),vehicle.GetWheel(ii)->GetRadius(),vehicle.GetWheel(ii)->GetWidth(),20,1);
    glwheel.SetPose(vehicle.GetWheel(ii)->GetPose().matrix());
    globjects_.push_back(new SceneGraph::GLCylinder(glwheel));
    vehicle.GetWheel(ii)->SetGuiIndex(globjects_.size()-1);
    glscenegraph_.AddChild(globjects_[globjects_.size()-1]);
  }
}

void spPangolinScenegraphGui::AddLineStrip(spLineStrip& linestrip) {
  SceneGraph::GLLineStrip* gllinestrip = new SceneGraph::GLLineStrip;
  gllinestrip->SetIgnoreDepth(true);
  gllinestrip->SetPose(linestrip.GetPose().matrix());
  gllinestrip->SetLineWidth(2);
  SceneGraph::Vector3dAlignedVec points = linestrip.GetLineStripPoints();
  gllinestrip->SetPointsFromTrajectory(points);
  gllinestrip->SetColor(linestrip.GetColor()[0],linestrip.GetColor()[1],linestrip.GetColor()[2],1);
  globjects_.push_back(std::move(gllinestrip));
  linestrip.SetGuiIndex(globjects_.size()-1);
  glscenegraph_.AddChild(globjects_[globjects_.size()-1]);
}


void spPangolinScenegraphGui::UpdateBoxGuiObject(spBox& spobj) {
  int gui_index = spobj.GetGuiIndex();
  globjects_[gui_index]->SetPose(spobj.GetPose().matrix());
  globjects_[gui_index]->SetScale(spobj.GetDimensions());
}

void spPangolinScenegraphGui::UpdateWaypointGuiObject(spWaypoint& spobj) {
  int gui_index = spobj.GetGuiIndex();
  SceneGraph::GLWayPoint* glwaypoint = (SceneGraph::GLWayPoint*)globjects_[gui_index];
  glwaypoint->SetPose(spobj.GetPose().matrix());
  glwaypoint->SetColor(spobj.GetColor()[0],spobj.GetColor()[1],spobj.GetColor()[2]);
  glwaypoint->SetVelocity(spobj.GetLength());
}

void spPangolinScenegraphGui::UpdateVehicleGuiObject(spVehicle& vehicle) {
  int chassis_index = vehicle.GetGuiIndex();
  globjects_[chassis_index]->SetPose(vehicle.GetPose().matrix());
  globjects_[chassis_index]->SetScale(vehicle.GetChassisSize());
  for(int ii=0; ii<vehicle.GetNumberOfWheels(); ii++) {
    int wheel_index = vehicle.GetWheel(ii)->GetGuiIndex();
    // apply gui coordinate difference with spirit coordinates
    spPose glwheelpose(vehicle.GetWheel(ii)->GetPose());
    Eigen::AngleAxisd ang(M_PI/2,Eigen::Vector3d::UnitY());
    glwheelpose.rotate(ang);
    glwheelpose.translate(spTranslation(0,0,-vehicle.GetWheel(ii)->GetWidth()/2));
    globjects_[wheel_index]->SetPose(glwheelpose.matrix());
  }
}

void spPangolinScenegraphGui::UpdateLineStripGuiObject(spLineStrip& spobj) {
  int gui_index = spobj.GetGuiIndex();
  SceneGraph::GLLineStrip* gllinestrip = (SceneGraph::GLLineStrip*) globjects_[gui_index];
  gllinestrip->SetPose(spobj.GetPose().matrix());
  gllinestrip->SetColor(spobj.GetColor()[0],spobj.GetColor()[1],spobj.GetColor()[2],1);
  SceneGraph::Vector3dAlignedVec points = spobj.GetLineStripPoints();
  gllinestrip->SetPointsFromTrajectory(points);
}

void spPangolinScenegraphGui::UpdateGuiObjectsFromSpirit(Objects& spobj) {
  // go through all spirit objects
  for(int ii=0; ii<spobj.GetNumOfObjects(); ii++) {
    //only update objects which had gui property changes
    if(spobj.GetObject(ii).HasChangedGui()) {
      // update the gui object
      switch (spobj.GetObject(ii).GetObjecType()) {
        case spObjectType::WHEEL:
        {
          SPERROREXIT("WHEEL object should not be created by itself.");
          break;
        }
        case spObjectType::WAYPOINT:
        {
          UpdateWaypointGuiObject((spWaypoint&)spobj.GetObject(ii));
          break;
        }
        case spObjectType::BOX:
        {
          UpdateBoxGuiObject((spBox&)spobj.GetObject(ii));
          break;
        }
        case spObjectType::VEHICLE:
        {
          UpdateVehicleGuiObject((spVehicle&)spobj.GetObject(ii));
          break;
        }
        case spObjectType::LINESTRIP:
        {
          UpdateLineStripGuiObject((spLineStrip&)spobj.GetObject(ii));
          break;
        }
        default:
        {
          SPERROREXIT("Unknown spirit object type.");
        }
      }
    }
  }
}

void spPangolinScenegraphGui::UpdateSpiritObjectsFromGui(Objects& spobjects) {
  for(int ii=0; ii<spobjects.GetNumOfObjects(); ii++) {
    //only update objects which are dynamic
    if(spobjects.GetObject(ii).IsGuiModifiable()) {
      switch (spobjects.GetObject(ii).GetObjecType()) {
        case spObjectType::BOX:
        {
          break;
        }
        case spObjectType::VEHICLE:
        {
          break;
        }
        case spObjectType::WHEEL:
        {
          break;
        }
        case spObjectType::WAYPOINT:
        {
          spWaypoint& spwaypoint = (spWaypoint&) spobjects.GetObject(ii);
          SceneGraph::GLWayPoint* glwaypoint = (SceneGraph::GLWayPoint*) globjects_[spwaypoint.GetGuiIndex()];
          spwaypoint.SetPose(spPose(glwaypoint->GetPose4x4_po()));
          spwaypoint.SetLength(glwaypoint->GetVelocity());
          break;
        }
        case spObjectType::LINESTRIP:
        {
          break;
        }
        default:
        {
          std::cerr << "Unknown spirit object type." << std::endl;
        }
      }
    }
  }

}
