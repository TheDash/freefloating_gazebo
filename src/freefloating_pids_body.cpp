#include <freefloating_gazebo/freefloating_pids_body.h>

using std::cout;
using std::endl;
using std::string;


void FreeFloatingBodyPids::Init(const ros::NodeHandle &_node, ros::Duration&_dt, const std::vector<std::string>&_controlled_axes)
{

    ROS_INFO("initializing freefloatingbodypid object");

    pid_node_ = _node;
    // init dt from rate
    dt_ = _dt;

    // deal with controlled axes
    const unsigned int n = _controlled_axes.size();
    position_pids_.resize(n);
    velocity_pids_.resize(n);

    std::string axes[] = {"x", "y", "z", "roll", "pitch", "yaw"};


    // get whether or not we use dynamic reconfigure
    bool use_dynamic_reconfig;
    _node.param("config/body/dynamic_reconfigure", use_dynamic_reconfig, false);

    for(unsigned int i=0;i<n;++i)
    {
        unsigned int j;
        for(j=0;j<6;++j)
        {
            if(_controlled_axes[i] == axes[j])
                break;
        }
        // here we have the controlled axe
        switch(j)
        {
        case 0:
            position_pids_[i].error_ptr = &(pose_lin_error_.x());
            velocity_pids_[i].error_ptr = &(velocity_lin_error_.x());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.force.x);
            break;
        case 1:
            position_pids_[i].error_ptr = &(pose_lin_error_.y());
            velocity_pids_[i].error_ptr = &(velocity_lin_error_.y());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.force.y);
            break;
        case 2:
            position_pids_[i].error_ptr = &(pose_lin_error_.z());
            velocity_pids_[i].error_ptr = &(velocity_lin_error_.z());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.force.z);
            break;
        case 3:
            position_pids_[i].error_ptr = &(pose_ang_error_.x());
            velocity_pids_[i].error_ptr = &(velocity_ang_error_.x());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.torque.x);
            break;
        case 4:
            position_pids_[i].error_ptr = &(pose_ang_error_.y());
            velocity_pids_[i].error_ptr = &(velocity_ang_error_.y());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.torque.y);
            break;
        case 5:
            position_pids_[i].error_ptr = &(pose_ang_error_.z());
            velocity_pids_[i].error_ptr = &(velocity_ang_error_.z());
            position_pids_[i].command_ptr = velocity_pids_[i].command_ptr = &(wrench_command_.torque.z);
            break;
        }

        ROS_INFO("Initializing position and velocity PIDs");
        InitPID(position_pids_[i].pid, ros::NodeHandle(_node, axes[j] + "/position"), use_dynamic_reconfig);
        InitPID(velocity_pids_[i].pid, ros::NodeHandle(_node, axes[j] + "/velocity"), use_dynamic_reconfig);

        //        BuildPIDCouple(ros::NodeHandle(_node, axes[j]), pose_error_ptr, vel_error_ptr, command_pose_ptr, command_vel_ptr);
    }

    InitSwitchServices("body");
    ROS_INFO("initialized switch services");
}

bool FreeFloatingBodyPids::UpdatePID()
{
    if(setpoint_received_)
    {
        ROS_INFO("Setpoint Received.. updating position");
        if(control_type_ == POSITION_CONTROL)
        {
            ROS_INFO("Updating for position control");
            Eigen::Matrix3d world_to_body = pose_ang_measure_inv_.toRotationMatrix();
            // express pose error in the body frame
            pose_lin_error_ = world_to_body * (pose_lin_setpoint_ - pose_lin_measure_);
            // quaternion error in the body frame

            Eigen::Quaterniond q(pose_ang_setpoint_ * pose_ang_measure_inv_);
            const double sin_theta_over_2 = sqrt(q.x()*q.x() + q.y()*q.y() + q.z()*q.z());
            if(sin_theta_over_2 == 0)
                pose_ang_error_ = Eigen::Vector3d(0,0,0);
            else
            {
                Eigen::Vector3d u(q.x(),q.y(),q.z());
                u *= 1./sin_theta_over_2;
                pose_ang_error_ = 2*atan2(sin_theta_over_2, q.w()) * world_to_body * u;
            }

            //cout << "Pose lin error in WF: " << (pose_lin_setpoint_ - pose_lin_measure_).transpose() << endl;
            //cout << "Pose ang error in WF: " << (world_to_body.inverse() * pose_ang_error_).transpose() << endl;

          /*          const double sin_theta_over_2 = sqrt(q.x()*q.x() + q.y()*q.y() + q.z()*q.z());
            if(sin_theta_over_2 == 0)
                pose_ang_error_ = Eigen::Vector3d(0,0,0);
            else
            {
                Eigen::Vector3d u(q.x(),q.y(),q.z());
                u /= sin_theta_over_2;
                pose_ang_error_ = 2*atan2(sin_theta_over_2, q.w()) * world_to_body * u;
            }*/
            //
            //            pose_ang_error_ =  pose_ang_measure_inv_ * (pose_ang_setpoint_ * pose_ang_measure_inv_);//.GetAsEuler());
            // writes the velocity setpoint
            UpdatePositionPID();
        }
        else
        {
            // velocity error is already in the body frame
            velocity_lin_error_ =  velocity_lin_setpoint_ - velocity_lin_measure_;
            velocity_ang_error_ =  velocity_ang_setpoint_ - velocity_ang_measure_;

            ROS_INFO("Updating velocity PID");
            // writes the wrench command
            UpdateVelocityPID();

        }

    }

    return setpoint_received_;
}

void FreeFloatingBodyPids::SetpointCallBack(const freefloating_gazebo::BodySetpointConstPtr &_msg)
{
    setpoint_received_ = true;
    ROS_INFO("setpoint received");
  //  if(_msg->reference_frame == "velocity")
    {
    //    control_type_ = VELOCITY_CONTROL;
        // velocity setpoint is already in the body frame
        velocity_lin_setpoint_ = Eigen::Vector3d(_msg->twist.linear.x, _msg->twist.linear.y, _msg->twist.linear.z);
        velocity_ang_setpoint_ = Eigen::Vector3d(_msg->twist.angular.x, _msg->twist.angular.y, _msg->twist.angular.z);
    }
 //   else
    {
    //    control_type_ = POSITION_CONTROL;
        pose_lin_setpoint_ = Eigen::Vector3d(_msg->pose.position.x, _msg->pose.position.y, _msg->pose.position.z);
        pose_ang_setpoint_ = Eigen::Quaterniond(_msg->pose.orientation.w, _msg->pose.orientation.x, _msg->pose.orientation.y, _msg->pose.orientation.z);
    }
}



void FreeFloatingBodyPids::MeasureCallBack(const nav_msgs::OdometryConstPtr &_msg)
{
    // positions are expressed in the world frame, rotation is inversed
    pose_lin_measure_ = Eigen::Vector3d(_msg->pose.pose.position.x, _msg->pose.pose.position.y, _msg->pose.pose.position.z);
    pose_ang_measure_inv_ = Eigen::Quaterniond(_msg->pose.pose.orientation.w, _msg->pose.pose.orientation.x, _msg->pose.pose.orientation.y, _msg->pose.pose.orientation.z).inverse();

    // change velocities from world to body frame
    velocity_lin_measure_ = pose_ang_measure_inv_.toRotationMatrix()*Eigen::Vector3d(_msg->twist.twist.linear.x, _msg->twist.twist.linear.y, _msg->twist.twist.linear.z);
    velocity_ang_measure_ = pose_ang_measure_inv_.toRotationMatrix()*Eigen::Vector3d(_msg->twist.twist.angular.x, _msg->twist.twist.angular.y, _msg->twist.twist.angular.z);

}
