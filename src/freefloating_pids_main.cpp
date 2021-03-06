
#include <freefloating_gazebo/freefloating_pids_body.h>
#include <freefloating_gazebo/freefloating_pids_joint.h>

using std::cout;
using std::endl;

int main(int argc, char ** argv)
{
    ROS_INFO("Initializing PID Loop for AUV robot.");
    ROS_INFO("Position control of the robot is available at the setpoint in body/setpoint");
    // init ROS node
    ros::init(argc, argv, "freefloating_pid_control");
    ros::NodeHandle rosnode;
    ros::NodeHandle control_node(rosnode, "controllers");

    // wait for body or joint param
    bool control_body = false;
    bool control_joints = false;
    while(!control_body && !control_joints)
    {
        sleep(5);
        control_body = control_node.hasParam("/controllers/config/body");
        // no joints to control for this robot so ignore
        //control_joints = control_node.hasParam("/auv_robot/controllers/config/joints/name");
        ROS_INFO("Control_body = True");
    }

    ROS_INFO("PID Loop enabled for body detected");


    // -- Parse body data if needed ---------

    // body setpoint and state topics
    std::string body_setpoint_topic, body_state_topic, body_command_topic;
    std::vector<std::string> controlled_axes;

    body_command_topic = "/auv_robot/body_command";

    if(control_body)
    {
        control_body = true;
        control_node.param("/controllers/config/body/setpoint", body_setpoint_topic, std::string("body_setpoint"));
        control_node.param("/controllers/config/body/state", body_state_topic, std::string("body_state"));
        control_node.param("/controllers/config/body/command", body_command_topic, std::string("body_command"));
        ROS_INFO("Finished setting parameters");
        // controlled body axes
        control_node.getParam("/auv_robot/controllers/config/body/axes", controlled_axes);
    }

    // -- Parse joint data if needed ---------
    std::string joint_setpoint_topic, joint_state_topic, joint_command_topic;
    if(control_joints)
    {
        control_joints = true;
        // joint setpoint and state topics
        control_node.param("config/joints/setpoint", joint_setpoint_topic, std::string("joint_setpoint"));
        control_node.param("config/joints/state", joint_state_topic, std::string("joint_states"));
        control_node.param("config/joints/command", joint_command_topic, std::string("joint_command"));
        cout << "PID node, joint_state_topic: " << joint_state_topic << endl;
    }
    // -- end parsing parameter server

    // loop rate
    ros::Rate loop(100);
    ros::Duration dt(.01);

    ros::SubscribeOptions ops;

    // -- Init body ------------------
    // PID's class
    FreeFloatingBodyPids body_pid;
    ros::Subscriber body_setpoint_subscriber, body_state_subscriber;
    ros::Publisher body_command_publisher;
    if(control_body)
    {
        body_pid.Init(control_node, dt, controlled_axes);

        // setpoint
        body_setpoint_subscriber =
                rosnode.subscribe(body_setpoint_topic, 1, &FreeFloatingBodyPids::SetpointCallBack, &body_pid);
        // measure
        ROS_INFO("Subscribed to body_setpoint");
        body_state_subscriber =
                rosnode.subscribe(body_state_topic, 1, &FreeFloatingBodyPids::MeasureCallBack, &body_pid);
        // command
        ROS_INFO("Subscribed to measure");
        body_command_publisher =
                rosnode.advertise<geometry_msgs::Wrench>("/auv_robot/body_command", 1);
        ROS_INFO("Now publishing commands to body command at topic auv_robot/body_command");
    } 

    ROS_INFO("Body control has been initialized");

    // -- Init joints ------------------
    FreeFloatingJointPids joint_pid;
    // declare subscriber / publisher
    ros::Subscriber joint_setpoint_subscriber, joint_state_subscriber;
    ros::Publisher joint_command_publisher;

    if(control_joints)
    {
        // pid
        joint_pid.Init(control_node, dt);

        // setpoint
        joint_setpoint_subscriber = rosnode.subscribe(joint_setpoint_topic, 1, &FreeFloatingJointPids::SetpointCallBack, &joint_pid);

        // measure
        joint_state_subscriber = rosnode.subscribe(joint_state_topic, 1, &FreeFloatingJointPids::MeasureCallBack, &joint_pid);

        // command
        joint_command_publisher = rosnode.advertise<sensor_msgs::JointState>(joint_command_topic, 1);
    }

    std::vector<std::string> joint_names;
    if(control_joints)
        control_node.getParam("config/joints/name", joint_names);

    ROS_INFO("Init PID control for %s: %i body axes, %i joints", rosnode.getNamespace().c_str(), (int) controlled_axes.size(), (int) joint_names.size());


    while(ros::ok())
    {
        // update body and publish
        if(control_body)
        {
	   if(body_pid.UpdatePID())
           {
                ROS_INFO("Publishing body command, updating position");
                body_command_publisher.publish(body_pid.WrenchCommand());
           } else { ROS_INFO("Failed to updatePID"); }
       } else { ROS_INFO("Failed to control body"); }

        // update joints and publish
        if(control_joints)
            if(joint_pid.UpdatePID())
                joint_command_publisher.publish(joint_pid.EffortCommand());

        ros::spinOnce();
        loop.sleep();
    }
}
