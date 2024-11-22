#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2_ros/transform_broadcaster.h"
#include "tf2/LinearMath/Quaternion.h"

class OdometryPublisher : public rclcpp::Node
{
public:
    OdometryPublisher() : Node("non_holonomic")
    {
        RCLCPP_INFO(this->get_logger(), "Node Started");
        // Initialize last_time_ to current ROS 2 time
        last_time_ = this->now();
        // Publisher for odometry messages
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/odom/holonomic", 10);
        // Read robot pose from sensor
        _read_speed = this->create_subscription<geometry_msgs::msg::Twist>("speed/non_holonomic", 10, std::bind(&OdometryPublisher::callback_non_holonomic, this, std::placeholders::_1));
        // Transform broadcaster
        tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);
        // Timer for periodic publishing
        timer_ = this->create_wall_timer(std::chrono::milliseconds(20), std::bind(&OdometryPublisher::publish, this));
    }

private:
    void callback_non_holonomic(const geometry_msgs::msg::Twist::SharedPtr msg)
    {
        // Extract x, y, and theta
        linear = msg->linear.x;
        angular = msg->angular.z;
        // RCLCPP_INFO(this->get_logger(), "R1 - x: %.2f, y: %.2f, theta: %.2f", current_x, current_y, theta);
    }
    void publish()
    {
        // Get current time
        auto current_time = this->now();
        // Compute time difference
        double dt = (current_time - last_time_).seconds();
        // Compute Localization
        dxy = linear * dt;
        dth = (angular * dt)/wheel_base;
        // 
        dx = cos(dth) * dxy;
        dy = sin(dth) * dxy;
        theta += dth;
        // 
        pos_x += dx * cos(theta) - dy* sin(theta);
        pos_y += dx * sin(theta) - dy * cos(theta);

        // Publish the transform
        geometry_msgs::msg::TransformStamped odom_trans;
        odom_trans.header.stamp = current_time;
        odom_trans.header.frame_id = "odom";
        odom_trans.child_frame_id = "robot";
        odom_trans.transform.translation.x = pos_x;
        odom_trans.transform.translation.y = pos_y;
        odom_trans.transform.translation.z = 0.0;
        tf2::Quaternion quat;
        quat.setRPY(0, 0, theta);
        odom_trans.transform.rotation.x = quat.x();
        odom_trans.transform.rotation.y = quat.y();
        odom_trans.transform.rotation.z = quat.z();
        odom_trans.transform.rotation.w = quat.w();
        tf_broadcaster_->sendTransform(odom_trans);

        // Publish the odometry message
        nav_msgs::msg::Odometry odom;
        odom.header.stamp = current_time;
        odom.header.frame_id = "odom";
        odom.child_frame_id = "robot";
        odom.pose.pose.position.x = pos_x;
        odom.pose.pose.position.y = pos_y;
        odom.pose.pose.position.z = 0.0;
        odom.pose.pose.orientation = odom_trans.transform.rotation;
        odom.twist.twist.linear.x = vx_;
        odom.twist.twist.linear.y = vy_;
        odom.twist.twist.angular.z = angular;
        odom_pub_->publish(odom);
        RCLCPP_WARN(this->get_logger(), "x = %.2f, y = %.2f", pos_x, pos_y);
    }

    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr _read_speed;
    std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Time last_time_;

    // Globle Variable
    double linear, angular;
    double vx_, vy_, vtheta_;
    double dxy, dth, dx, dy;
    double pos_x,pos_y,theta;
    double wheel_base = 0.3;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<OdometryPublisher>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}