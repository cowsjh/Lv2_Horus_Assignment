#include <rclcpp/rclcpp.hpp>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>


class ControlNode : public rclcpp::Node
{
public:

  ControlNode()
  : Node("control_node")
  {
    /*
     * 현재 모터 위치
     *
     * 실제 OpenCR/Dynamixel에서 읽은 값을
     * 이 변수에 넣으면 된다.
     *
     * 단위: rad
     */
    current_yaw_ = 0.0;
    current_pitch_ = 0.0;


    /*
     * 모터의 실제 허용 범위.
     *
     * controller는 계산하지 않는다.
     *
     * center_node에서 이미 계산된 delta를
     * 받아서 실제 모터에 전달하는 역할만 한다.
     *
     * 단위: rad
     */
    yaw_min_ =
      this->declare_parameter<double>(
        "yaw_min",
        -1.0
      );

    yaw_max_ =
      this->declare_parameter<double>(
        "yaw_max",
        1.0
      );

    pitch_min_ =
      this->declare_parameter<double>(
        "pitch_min",
        -1.0
      );

    pitch_max_ =
      this->declare_parameter<double>(
        "pitch_max",
        1.0
      );


    /*
     * center_node가 보내는 명령
     *
     * data[0] = yaw delta
     * data[1] = pitch delta
     *
     * 단위: rad
     */
    command_sub_ =
      this->create_subscription<
        std_msgs::msg::Float64MultiArray
      >(
        "/motor/command",
        10,
        std::bind(
          &ControlNode::commandCallback,
          this,
          std::placeholders::_1
        )
      );


    /*
     * 현재 모터 상태
     *
     * x = yaw
     * y = pitch
     *
     * center_node가 이 값을 받아서
     * 다음 이동량을 계산한다.
     */
    state_pub_ =
      this->create_publisher<
        geometry_msgs::msg::PointStamped
      >(
        "/motor/state",
        10
      );


    RCLCPP_INFO(
      this->get_logger(),
      "Control node started."
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Yaw range   : %.4f ~ %.4f rad",
      yaw_min_,
      yaw_max_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Pitch range : %.4f ~ %.4f rad",
      pitch_min_,
      pitch_max_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Waiting for /motor/command"
    );


    /*
     * 시작하자마자 현재 상태를 한 번 발행.
     */
    publishState();
  }


private:

  /*
   * 현재 모터 위치
   */
  double current_yaw_;
  double current_pitch_;


  /*
   * 모터 허용 범위
   */
  double yaw_min_;
  double yaw_max_;

  double pitch_min_;
  double pitch_max_;


  /*
   * ROS
   */
  rclcpp::Subscription<
    std_msgs::msg::Float64MultiArray
  >::SharedPtr command_sub_;

  rclcpp::Publisher<
    geometry_msgs::msg::PointStamped
  >::SharedPtr state_pub_;


  /*
   * center_node에서 명령 수신
   */
  void commandCallback(
    const std_msgs::msg::Float64MultiArray::SharedPtr msg
  )
  {
    /*
     * 잘못된 command 방지
     */
    if (msg->data.size() < 2)
    {
      RCLCPP_WARN(
        this->get_logger(),
        "/motor/command requires 2 values: "
        "[yaw_delta, pitch_delta]"
      );

      return;
    }


    /*
     * center_node가 계산한 상대 이동량.
     *
     * 여기서는 계산하지 않는다.
     */
    const double yaw_delta =
      msg->data[0];

    const double pitch_delta =
      msg->data[1];


    /*
     * NaN / Inf 방지
     */
    if (
      !std::isfinite(yaw_delta) ||
      !std::isfinite(pitch_delta)
    )
    {
      RCLCPP_WARN(
        this->get_logger(),
        "Invalid motor command: NaN or Inf"
      );

      return;
    }


    /*
     * center_node에서 이미 계산한
     * 상대 이동량을 현재 위치에 적용한다.
     */
    double target_yaw =
      current_yaw_ + yaw_delta;

    double target_pitch =
      current_pitch_ + pitch_delta;


    /*
     * 최종 모터 범위 보호.
     *
     * 이것은 제어 계산이 아니라
     * 하드웨어 안전 보호이다.
     */
    target_yaw =
      std::clamp(
        target_yaw,
        yaw_min_,
        yaw_max_
      );

    target_pitch =
      std::clamp(
        target_pitch,
        pitch_min_,
        pitch_max_
      );


    /*
     * 실제 모터 명령 부분.
     *
     * 현재는 테스트를 위해
     * 내부 상태만 갱신한다.
     *
     * 나중에 이 부분에
     * OpenCR/Dynamixel 명령을 넣으면 된다.
     */
    current_yaw_ =
      target_yaw;

    current_pitch_ =
      target_pitch;


    /*
     * 현재 모터 상태를 center_node로 전달.
     */
    publishState();


    RCLCPP_INFO(
      this->get_logger(),
      "Command received | "
      "delta yaw=%.4f pitch=%.4f | "
      "state yaw=%.4f pitch=%.4f",

      yaw_delta,
      pitch_delta,

      current_yaw_,
      current_pitch_
    );
  }


  /*
   * /motor/state 발행
   *
   * x = current yaw
   * y = current pitch
   */
  void publishState()
  {
    geometry_msgs::msg::PointStamped state;

    state.header.stamp =
      this->get_clock()->now();

    state.header.frame_id =
      "motor";

    state.point.x =
      current_yaw_;

    state.point.y =
      current_pitch_;

    state.point.z =
      0.0;

    state_pub_->publish(state);
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
    std::make_shared<ControlNode>()
  );

  rclcpp::shutdown();

  return 0;
}