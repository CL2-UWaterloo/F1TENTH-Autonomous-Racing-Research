//C++ library includes
#include <memory> //for smart pointers
//#include <chrono> //for time 
#include <math.h> //for isnan, isinf etc.
#include <string>
#include <cstdlib> //for abs value function
#include <vector> /// CHECK: might cause errors due to double header in linker
#include <sstream> //for handling csv extraction
//#include <cmath> //may need
#include <iostream> //both for input/output and file stream and handling
#include <fstream>
#include <algorithm> //for max function

//#define EIGEN_DONT_ALIGN_STATICALLY
//Eigen
#include <Eigen/Eigen>


//ROS related headers
#include "rclcpp/rclcpp.hpp"
//message header
#include "nav_msgs/msg/odometry.hpp"
#include "ackermann_msgs/msg/ackermann_drive_stamped.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "visualization_msgs/msg/marker.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
//tf2 headers
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <geometry_msgs/msg/transform_stamped.hpp>
//#include <tf2/LinearMath/Matrix3x3.h>
//#include <tf2/LinearMath/Quaternion.h>



//headers to code written by you


//other macros
#define NODE_NAME "pure_pursuit_node" 
#define _USE_MATH_DEFINES
#define LOOKAHEAD 1.00 // in meters
#define K_p 0.5
#define STEERING_LIMIT 25 //degrees

//using namespaces 
//used for bind (uncomment below)
using std::placeholders::_1;
//using namespace Eigen;
//using namespace std; --> may need
//using namespace std::chrono_literals;


class PurePursuit : public rclcpp::Node {

public:
    PurePursuit() : Node(NODE_NAME) {
        // initialise parameters
        this->declare_parameter("go", true);
        drive_flag = this->get_parameter("go").as_bool();
        //set parameter during run time with: ros2 param set <node_name> <parameter_name> <value>
        //set parameter during launch time with: -p :=

        //initialise subscriber sharedptr obj
        subscription_odom = this->create_subscription<nav_msgs::msg::Odometry>(odom_topic, 1000, std::bind(&PurePursuit::odom_callback, this, _1));

        //initialise publisher sharedptr obj
        publisher_drive = this->create_publisher<ackermann_msgs::msg::AckermannDriveStamped>(drive_topic, 1000);
        //vis_path_pub = this->create_publisher<visualization_msgs::msg::MarkerArray>(rviz_waypoints_topic, 1000);
        vis_point_pub = this->create_publisher<visualization_msgs::msg::Marker>(rviz_waypointselected_topic, 1000);

        //initialise tf2 shared pointers
        tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
        transform_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        RCLCPP_INFO (this->get_logger(), "this node has been launched");

        download_waypoints();

        //copy all data once to data structure initialised before

    }

    ~PurePursuit() {}

    //EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    private:
    //global static (to be shared by all objects) and dynamic variables (each instance gets its own copy -> managed on the stack)
    struct csvFileData{
        std::vector<double> X;
        std::vector<double> Y;
        //double x_worldRef, y_worldRef, x_carRef, y_carRef;
        int index;

        Eigen::Vector3d p1_world;
        Eigen::Vector3d p1_car;// p2_world, p2_car;// p_car;
    };

    Eigen::Matrix3d rotation_m;

    //topic names
    std::string odom_topic = "/ego_racecar/odom";
    std::string drive_topic = "/drive";
    std::string car_refFrame = "ego_racecar/base_link";
    std::string global_refFrame = "map"; //TODO: might have to add backslashes before
    //std::string rviz_waypoints_topic = "/waypoints";
    std::string rviz_waypointselected_topic = "/waypoints_selected";

    //struct initialisation
    bool drive_flag; //parameter to freeze car
    
    //file object
    std::fstream csvFile_waypoints; 

    //struct initialisation
    csvFileData waypoints;

    //declare subscriber sharedpointer obj
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subscription_odom;

    //declare publisher sharedpointer obj
    rclcpp::Publisher<ackermann_msgs::msg::AckermannDriveStamped>::SharedPtr publisher_drive; 
    //rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr vis_path_pub; 
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr vis_point_pub; 

    //declare tf shared pointers
    std::shared_ptr<tf2_ros::TransformListener> transform_listener_{nullptr};
    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;

    //private functions
    double to_radians(double degrees) {
        double radians;
        return radians = degrees*M_PI/180;
    }

    double to_degrees(double radians) {
        double degrees;
        return degrees = radians*180/M_PI;
    }
    
    double p2pdist(double &x1, double &x2, double &y1, double &y2) {
        double dist = sqrt(pow((x2-x1),2)+pow((y2-y1),2));
        return dist;
    }

    //TODO: filter for nans and stuff that leads to partially or fully invalid way-points
    void download_waypoints () { //put all data in vectors
        //TODO: move file to the right repo and load
        //TODO: write error handling for vector mismatch (try-catch block). Resize arrays
        csvFile_waypoints.open("/sim_ws/src/pure_pursuit/src/waypoints_odom.csv", std::ios::in);

        RCLCPP_INFO (this->get_logger(), "%s", (csvFile_waypoints.is_open() ? "fileOpened" : "fileNOTopened"));
        
        //std::vector<std::string> row;
        std::string line, word, temp;

        while (!csvFile_waypoints.eof()) { //TODO: use eof to find end of file (if this doesn't work)
            
            //row.clear(); //empty row extraction string vector

            std::getline(csvFile_waypoints, line, '\n');
            //RCLCPP_INFO (this->get_logger(), "%s", line);


            std::stringstream s(line); //TODO: continue implementation

            int j = 0;
            while (getline(s, word, ',')) {

                RCLCPP_INFO (this->get_logger(), "%s", (word.empty() ? "wordempty" : "wordNOTempty"));
                //RCLCPP_INFO (this->get_logger(), "%f", std::stod(word));
                if (!word.empty()) {

                    //row.push_back(word);

                    if (j == 0) {
                        double x = std::stod(word);
                        waypoints.X.push_back(x);
                        RCLCPP_INFO (this->get_logger(), "%f... Xpoint", waypoints.X.back());
                    }
                    
                    if (j == 1) {
                        waypoints.Y.push_back(std::stod(word));
                        RCLCPP_INFO (this->get_logger(), "%f... Ypoint", waypoints.Y.back());
                    }
                }

                j++;
            }
        }

        csvFile_waypoints.close();
    }

    void visualize_points(Eigen::Vector3d &point) {
        //auto marker_array = visualization_msgs::msg::MarkerArray();
        auto marker = visualization_msgs::msg::Marker();
        marker.header.frame_id = "map";
        marker.header.stamp = rclcpp::Clock().now();
        marker.type = visualization_msgs::msg::Marker::SPHERE;
        marker.action = visualization_msgs::msg::Marker::ADD;
        marker.scale.x = 0.15;
        marker.scale.y = 0.15;
        marker.scale.z = 0.15;
        marker.color.a = 1.0; 
        marker.color.r = 1.0;

        marker.pose.position.x = point(1);
        marker.pose.position.y = point(2);
        marker.id = 1;

        vis_point_pub->publish(marker);

    }

    void get_waypoint(double x_car_world, double y_car_world) {//TODO: might have to add conditions here to get the different 

        double longest_distance = 0;
        for (unsigned int i = 0; i <= waypoints.X.size(); i++) {
            if (p2pdist(waypoints.X[i], x_car_world, waypoints.Y[i], y_car_world) < LOOKAHEAD && p2pdist(waypoints.X[i], x_car_world, waypoints.Y[i], y_car_world) > longest_distance) {
                longest_distance = p2pdist(waypoints.X[i], x_car_world, waypoints.Y[i], y_car_world);
                //waypoints.x_worldRef = X[i];
                //waypoints.y_worldRef = Y[i];
                waypoints.index = i;
            }
        }

        //TODO: run condition/loop to get closest index outside bubble for interpolation
    }

    void quat_to_rot(double q0, double q1, double q2, double q3) { //w,x,y,z -> q0,q1,q2,q3
    //TODO: use Eigen toRotationMatrix() function
        double r00 = (double)(2.0 * (q0 * q0 + q1 * q1) - 1.0);
        double r01 = (double)(2.0 * (q1 * q2 - q0 * q3));
        double r02 = (double)(2.0 * (q1 * q3 + q0 * q2));
     
        double r10 = (double)(2.0 * (q1 * q2 + q0 * q3));
        double r11 = (double)(2.0 * (q0 * q0 + q2 * q2) - 1.0);
        double r12 = (double)(2.0 * (q2 * q3 - q0 * q1));
     
        double r20 = (double)(2.0 * (q1 * q3 - q0 * q2));
        double r21 = (double)(2.0 * (q2 * q3 + q0 * q1));
        double r22 = (double)(2.0 * (q0 * q0 + q3 * q3) - 1.0);

        rotation_m << r00, r01, r02, r10, r11, r12, r20, r21, r22; //fill rotation matrix
        RCLCPP_INFO (this->get_logger(), "rotation_m < %f, %f, %f>", rotation_m(0,0), rotation_m(0,1), rotation_m(0,2));
        RCLCPP_INFO (this->get_logger(), ".......... < %f, %f, %f>", rotation_m(1,0), rotation_m(1,1), rotation_m(1,2));
        RCLCPP_INFO (this->get_logger(), ".......... < %f, %f, %f>", rotation_m(2,0), rotation_m(2,1), rotation_m(2,2));

    }//TODO: check 

    void transformandinterp_waypoint() { //pass old waypoint here
        //STEPS: extract transform. transform index and next one. interpolate -->how?

        //calculate alpha. apply to scale equation

        //use tf2 to extract transformation between map and ego_racecar/base_link reference frames
        //apply transformation between points

        //extract transform message --> do all codework

        //apply translation vector and rotation matrix --> how to extract rotation matrix from quaternions?
        
        //initialise vectors
        waypoints.p1_world << waypoints.X[waypoints.index], waypoints.Y[waypoints.index], 0.0;
        //waypoints.p2_world << waypoints.X[waypoints.index+1], waypoints.Y[waypoints.index+1], 0.0;

        //rviz publish way point
        visualize_points(waypoints.p1_world);
        RCLCPP_INFO (this->get_logger(), "check");
        //look up transformation at that instant from tf_buffer_
        geometry_msgs::msg::TransformStamped transformStamped;
        
        try {
            transformStamped = tf_buffer_->lookupTransform(car_refFrame, global_refFrame,tf2::TimePointZero);
        } 
        catch (tf2::TransformException & ex) {
            RCLCPP_INFO(this->get_logger(), "Could not transform. Error: %s", ex.what());
        }

        //transform points (rotate first and then translate)
        RCLCPP_INFO (this->get_logger(), "translation_v(from transform_stamped) <%f, %f, %f>", transformStamped.transform.translation.x, transformStamped.transform.translation.y, transformStamped.transform.translation.z);
        Eigen::Vector3d translation_v(transformStamped.transform.translation.x, transformStamped.transform.translation.y, transformStamped.transform.translation.z);
        //Eigen::Vector3d translation_v;
        //translation_v << transformStamped.transform.translation.x, transformStamped.transform.translation.y, transformStamped.transform.translation.z;
        //RCLCPP_INFO (this->get_logger(), "Vector3d made successfully");
        RCLCPP_INFO (this->get_logger(), "translation_v < %f, %f, %f>", translation_v(0), translation_v(1), translation_v(2));
        //Eigen::Quaternion
        quat_to_rot(transformStamped.transform.rotation.w, transformStamped.transform.rotation.x, transformStamped.transform.rotation.y, transformStamped.transform.rotation.z);

        waypoints.p1_car = (rotation_m*waypoints.p1_world) + translation_v;
        //waypoints.p1_car = waypoints.p1_world + translation_v;
        //waypoints.p2_car = (rotation_m*waypoints.p2_world) + translation_v;
        RCLCPP_INFO (this->get_logger(), "waypoint: world frame: <%f, %f>", waypoints.p1_world(0), waypoints.p1_world(1));
        RCLCPP_INFO (this->get_logger(), "waypoint: car frame: <%f, %f>", waypoints.p1_car(0), waypoints.p1_car(1));
        //point transformations
        //interpolate by calculating alpha 

        //TODO: implement interpolation

    }

    double p_controller () { //pass waypoint
        
        double radial_dist = waypoints.p1_car.norm();
        double angle = K_p*(2*(waypoints.p1_car(1))/(pow(radial_dist,2)));
        //double angle = 0.5 * radial_dist;

        return angle;
    }

    double get_velocity(double steering_angle) {
        double velocity;
        if (abs(steering_angle) > to_radians(0.0) && abs(steering_angle) < to_radians(10.0)) {
            velocity = 1.5;
        } 
        else if (abs(steering_angle) > to_radians(10.0) && abs(steering_angle) < to_radians(20.0)) {
            velocity = 1.0;
        } 
        else {
            velocity = 0.5;
        }
        return velocity;
    }

    void publish_message (double steering_angle) {
        auto drive_msgObj = ackermann_msgs::msg::AckermannDriveStamped();
        //drive_msgObj.drive.steering_angle = std::min(steering_angle, to_radians(STEERING_LIMIT)); //ensure steering angle is dynamically capable
        drive_msgObj.drive.steering_angle = steering_angle;

        RCLCPP_INFO (this->get_logger(), "driveflag: %s", (drive_flag ? "true" : "false"));

        if (drive_flag == false) {
            drive_msgObj.drive.speed = 0.0;
        }
        else {drive_msgObj.drive.speed = get_velocity(drive_msgObj.drive.steering_angle);}

        publisher_drive->publish(drive_msgObj);
        //RCLCPP_INFO (this->get_logger(), "%f ..... %f", drive_msgObj.drive.speed, to_degrees(drive_msgObj.drive.steering_angle));
    }

    void odom_callback(const nav_msgs::msg::Odometry::ConstSharedPtr odom_submsgObj) {
        
        double x_car_world = odom_submsgObj->pose.pose.position.x;
        double y_car_world = odom_submsgObj->pose.pose.position.y;
        // TODO: find the current waypoint to track using methods mentioned in lecture
        // interpolate between different way-points 
        get_waypoint(x_car_world, y_car_world);

        // TODO: transform goal point to vehicle frame of reference
        //use tf2 transform the goal point 
        transformandinterp_waypoint();

        // TODO: calculate curvature/steering angle
        //p controller
        double steering_angle = p_controller();

        // TODO: publish drive message, don't forget to limit the steering angle.
        //publish object and message: AckermannDriveStamped on drive topic 
        publish_message(steering_angle);
    }

};



int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node_ptr = std::make_shared<PurePursuit>(); // initialise node pointer
    rclcpp::spin(node_ptr);
    rclcpp::shutdown();
    return 0;
}

/*
    steps: - subscribe to odom message to get car's ground truth
           - use particle filter to get local 

    TODO: - run pure pursuit using waypoints generated from track
          - run slam_toolbox and localisation filter 
          - generate waypoints using spline (write python script) or remote controller 
          - run pure pursuit on the car --> need to subscribe to particle filter subscriber to get localisation

    Do lab 5, 7, 8 in Waterloo + 9 (project with Yash)

    TODO: use lejung's csv file

    Questions: 
    - how do we get the waypoints in the vehicles reference frame --> convert using tf2 or nav odom heading

    TODO: - add condition to make sure the car follows in the right direction --> convert all points to car's ref frame
                                                                              --> use Ct x transform and compare points
                                                                              --> 
          - add waypoint visualiser


    Failuremodes: - tf2
                  - point downloading (look eof)
                  - Eigen Vector2d instead of 3d
                  - Eigen compilation issues --> see Eigen hints

    Generally work on optimising for speed
    Look into using Eigen and building with eigen dependencies

    TODO: implement interpolation

    TODO: ways to improve pure pursuit: - interpolate/project waypoints
                                        - tune controller
                                        - generate better waypoints
*/