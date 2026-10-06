#include <rclcpp/rclcpp.hpp>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>

#include <algorithm>
#include <cmath>
#include <functional>


class CenterNode : public rclcpp::Node
{
public:
  CenterNode()
  : Node("center_node")
  {
    // Dynamixel position range
    // Unit: tick (0 ~ 4095)

    yaw_min_ =
      this->declare_parameter<double>(
        "yaw_min",
        0.0
      );

    yaw_max_ =
      this->declare_parameter<double>(
        "yaw_max",
        4095.0
      );

    pitch_min_ =
      this->declare_parameter<double>(
        "pitch_min",
        0.0
      );

    pitch_max_ =
      this->declare_parameter<double>(
        "pitch_max",
        4095.0
      );

    // Maximum movement per command
    // Unit: tick

    max_delta_tick_ =
      this->declare_parameter<double>(
        "max_delta_tick",
        100.0
      );

    // Deadband for target error
    // ex / ey range: -1.0 ~ +1.0

    deadband_ =
      this->declare_parameter<double>(
        "deadband",
        0.03
      );

    // /target
    //
    // point.x = ex
    // point.y = ey
    // point.z = area_ratio

    target_sub_ =
      this->create_subscription<
        geometry_msgs::msg::PointStamped
      >(
        "/target",
        rclcpp::QoS(rclcpp::KeepLast(1)).best_effort(),
        std::bind(
          &CenterNode::targetCallback,
          this,
          std::placeholders::_1
        )
      );

    // /motor/state
    //
    // point.x = current yaw tick
    // point.y = current pitch tick
    // point.z = flags

    motor_state_sub_ =
      this->create_subscription<
        geometry_msgs::msg::PointStamped
      >(
        "/motor/state",
        10,
        std::bind(
          &CenterNode::motorStateCallback,
          this,
          std::placeholders::_1
        )
      );

    // /motor/command
    //
    // data[0] = absolute yaw tick
    // data[1] = absolute pitch tick

    command_pub_ =
      this->create_publisher<
        std_msgs::msg::Float64MultiArray
      >(
        "/motor/command",
        10
      );

    RCLCPP_INFO(
      this->get_logger(),
      "Center node started."
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Yaw range   : %.1f ~ %.1f tick",
      yaw_min_,
      yaw_max_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Pitch range : %.1f ~ %.1f tick",
      pitch_min_,
      pitch_max_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Max delta   : %.1f tick",
      max_delta_tick_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Deadband    : %.3f",
      deadband_
    );
  }


private:

  // Current Dynamixel position
  // Unit: tick

  double current_yaw_ = 2048.0;
  double current_pitch_ = 2048.0;

  bool motor_state_received_ = false;

  // Parameters

  double yaw_min_;
  double yaw_max_;

  double pitch_min_;
  double pitch_max_;

  double max_delta_tick_;
  double deadband_;

  // ROS

  rclcpp::Subscription<
    geometry_msgs::msg::PointStamped
  >::SharedPtr target_sub_;

  rclcpp::Subscription<
    geometry_msgs::msg::PointStamped
  >::SharedPtr motor_state_sub_;

  rclcpp::Publisher<
    std_msgs::msg::Float64MultiArray
  >::SharedPtr command_pub_;


  // /motor/state callback

  void motorStateCallback(
    const geometry_msgs::msg::PointStamped::SharedPtr msg
  )
  {
    current_yaw_ =
      msg->point.x;

    current_pitch_ =
      msg->point.y;

    motor_state_received_ = true;

    RCLCPP_INFO_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      2000,

      "Motor state: yaw=%.1f pitch=%.1f flags=%.1f",

      current_yaw_,
      current_pitch_,
      msg->point.z
    );
  }


  // /target callback

  void targetCallback(
    const geometry_msgs::msg::PointStamped::SharedPtr msg
  )
  {
    const double ex =
      msg->point.x;

    const double ey =
      msg->point.y;

    const double area_ratio =
      msg->point.z;


    // Target input debug

    RCLCPP_INFO_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      1000,

      "TARGET: ex=%.3f ey=%.3f area=%.4f",

      ex,
      ey,
      area_ratio
    );


    // Wait until motor state is received

    if (!motor_state_received_)
    {
      RCLCPP_WARN_THROTTLE(
        this->get_logger(),
        *this->get_clock(),
        2000,

        "Waiting for /motor/state"
      );

      return;
    }


    // No target

    if (area_ratio <= 0.0)
    {
      RCLCPP_INFO_THROTTLE(
        this->get_logger(),
        *this->get_clock(),
        2000,

        "No target."
      );

      return;
    }


    // Clamp perception error

    double error_x =
      std::clamp(
        ex,
        -1.0,
        1.0
      );

    double error_y =
      std::clamp(
        ey,
        -1.0,
        1.0
      );


    // Deadband

    if (std::fabs(error_x) < deadband_)
    {
      error_x = 0.0;
    }

    if (std::fabs(error_y) < deadband_)
    {
      error_y = 0.0;
    }


    // Target is already centered

    if (
      error_x == 0.0 &&
      error_y == 0.0
    )
    {
      RCLCPP_INFO_THROTTLE(
        this->get_logger(),
        *this->get_clock(),
        2000,

        "Target is inside deadband."
      );

      return;
    }


    // Calculate yaw delta

    double yaw_delta = 0.0;

    if (error_x > 0.0)
    {
      const double available =
        yaw_max_ - current_yaw_;

      yaw_delta =
        error_x * available;
    }
    else if (error_x < 0.0)
    {
      const double available =
        current_yaw_ - yaw_min_;

      yaw_delta =
        error_x * available;
    }


    // Calculate pitch delta

    double pitch_delta = 0.0;

    if (error_y > 0.0)
    {
      const double available =
        pitch_max_ - current_pitch_;

      pitch_delta =
        error_y * available;
    }
    else if (error_y < 0.0)
    {
      const double available =
        current_pitch_ - pitch_min_;

      pitch_delta =
        error_y * available;
    }


    // Limit movement per command

    yaw_delta =
      std::clamp(
        yaw_delta,
        -max_delta_tick_,
        max_delta_tick_
      );

    pitch_delta =
      std::clamp(
        pitch_delta,
        -max_delta_tick_,
        max_delta_tick_
      );


    // Calculate absolute target position

    const double target_yaw =
      std::clamp(
        current_yaw_ + yaw_delta,
        yaw_min_,
        yaw_max_
      );

    const double target_pitch =
      std::clamp(
        current_pitch_ + pitch_delta,
        pitch_min_,
        pitch_max_
      );


    // Publish absolute motor position

    std_msgs::msg::Float64MultiArray command;

    command.data.resize(2);

    command.data[0] =
      target_yaw;

    command.data[1] =
      target_pitch;

    command_pub_->publish(
      command
    );


    // Command debug

    RCLCPP_INFO(
      this->get_logger(),

      "COMMAND: "
      "ex=%.3f ey=%.3f area=%.4f | "
      "current=(%.1f, %.1f) | "
      "delta=(%.1f, %.1f) | "
      "target=(%.1f, %.1f)",

      error_x,
      error_y,
      area_ratio,

      current_yaw_,
      current_pitch_,

      yaw_delta,
      pitch_delta,

      target_yaw,
      target_pitch
    );
  }
};


int main(
  int argc,
  char ** argv
)
{
  rclcpp::init(
    argc,
    argv
  );

  rclcpp::spin(
    std::make_shared<CenterNode>()
  );

  rclcpp::shutdown();

  return 0;
}