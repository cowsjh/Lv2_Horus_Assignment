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
    /*
     * 모터 실제 허용 범위.
     *
     * 반드시 실제 장비에 맞는 값으로 실행 파라미터에서 설정할 것.
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
     * 한 번에 허용할 최대 상대 이동량.
     *
     * 이것도 rad 단위.
     *
     * 실제 장비 안전값에 맞게 조정할 것.
     */
    max_delta_rad_ =
      this->declare_parameter<double>(
        "max_delta_rad",
        0.05
      );


    /*
     * 중앙 근처에서 떨림 방지.
     *
     * 예:
     * ex = 0.01 정도면 무시.
     */
    deadband_ =
      this->declare_parameter<double>(
        "deadband",
        0.03
      );


    /*
     * /target
     *
     * x = ex
     * y = ey
     * z = area_ratio
     */
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

    /*
     * /motor/state
     *
     * x = current yaw
     * y = current pitch
     */
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


    /*
     * /motor/command
     *
     * data[0] = yaw delta
     * data[1] = pitch delta
     */
    command_pub_ =
      this->create_publisher<std_msgs::msg::Float64MultiArray>
      (
        "/motor/command",
        10
      );
      
    RCLCPP_INFO(
      this->get_logger(),
      "Center node started."
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
      "Max delta   : %.4f rad",
      max_delta_rad_
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Deadband    : %.4f",
      deadband_
    );
  }


private:

  /*
   * 현재 Dynamixel 위치
   */
  double current_yaw_ = 0.0;
  double current_pitch_ = 0.0;

  bool motor_state_received_ = false;


  /*
   * 파라미터
   */
  double yaw_min_;
  double yaw_max_;

  double pitch_min_;
  double pitch_max_;

  double max_delta_rad_;
  double deadband_;


  /*
   * ROS
   */
  rclcpp::Subscription<
    geometry_msgs::msg::PointStamped
  >::SharedPtr target_sub_;

  rclcpp::Subscription<
    geometry_msgs::msg::PointStamped
  >::SharedPtr motor_state_sub_;

  rclcpp::Publisher<
    std_msgs::msg::Float64MultiArray
  >::SharedPtr command_pub_;


  /*
   * control_node가 OpenCR에서 받은
   * 현재 모터 상태
   */
  void motorStateCallback(
    const geometry_msgs::msg::PointStamped::SharedPtr msg
  )
  {
    current_yaw_ =
      msg->point.x;

    current_pitch_ =
      msg->point.y;

    motor_state_received_ = true;
  }


  /*
   * perception_node의 /target
   */
  void targetCallback(
    const geometry_msgs::msg::PointStamped::SharedPtr msg
  )
  {
    /*
     * 아직 현재 모터 위치를 모르면
     * 움직이지 않는다.
     */
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


    /*
     * perception 결과
     */
    double ex =
      msg->point.x;

    double ey =
      msg->point.y;

    double area_ratio =
      msg->point.z;


    /*
     * NO_TARGET
     *
     * perception 코드에서:
     *
     * NO_TARGET -> (0, 0, 0)
     *
     * 따라서 z == 0 이면
     * 명령을 보내지 않는다.
     */
    if (area_ratio <= 0.0)
    {
      return;
    }


    /*
     * 혹시 모를 범위 초과 방지
     */
    ex =
      std::clamp(
        ex,
        -1.0,
        1.0
      );

    ey =
      std::clamp(
        ey,
        -1.0,
        1.0
      );


    /*
     * 중앙 deadband
     */
    if (std::fabs(ex) < deadband_)
    {
      ex = 0.0;
    }

    if (std::fabs(ey) < deadband_)
    {
      ey = 0.0;
    }


    /*
     * 둘 다 중앙이면 움직일 필요 없음
     */
    if (
      ex == 0.0 &&
      ey == 0.0
    )
    {
      return;
    }


    /*
     * 현재 위치에서 가능한 이동량 계산
     *
     * 예:
     *
     * current yaw = 1.5
     * yaw max     = 1.8
     *
     * 오른쪽으로 움직일 수 있는 양:
     *
     * 1.8 - 1.5 = 0.3 rad
     *
     *
     * current yaw = 1.5
     * yaw min     = 0.5
     *
     * 왼쪽으로 움직일 수 있는 양:
     *
     * 0.5 - 1.5 = -1.0 rad
     */
    double yaw_negative_range =
      yaw_min_ - current_yaw_;

    double yaw_positive_range =
      yaw_max_ - current_yaw_;

    double pitch_negative_range =
      pitch_min_ - current_pitch_;

    double pitch_positive_range =
      pitch_max_ - current_pitch_;


    /*
     * 정규화 오차(-1 ~ +1)를
     * 현재 위치에서 가능한 상대각으로 변환
     *
     * ex > 0:
     *   현재 위치 -> yaw_max 방향
     *
     * ex < 0:
     *   현재 위치 -> yaw_min 방향
     */
    double yaw_delta = 0.0;

    if (ex > 0.0)
    {
      yaw_delta =
        ex *
        yaw_positive_range;
    }
    else if (ex < 0.0)
    {
      /*
       * yaw_negative_range 자체가 음수이므로
       * -ex를 곱해 음수 delta를 만든다.
       */
      yaw_delta =
        (-ex) *
        yaw_negative_range;
    }


    /*
     * pitch도 같은 방식
     */
    double pitch_delta = 0.0;

    if (ey > 0.0)
    {
      pitch_delta =
        ey *
        pitch_positive_range;
    }
    else if (ey < 0.0)
    {
      pitch_delta =
        (-ey) *
        pitch_negative_range;
    }


    /*
     * 한 번에 너무 크게 움직이지 않도록 제한
     */
    yaw_delta =
      std::clamp(
        yaw_delta,
        -max_delta_rad_,
        max_delta_rad_
      );

    pitch_delta =
      std::clamp(
        pitch_delta,
        -max_delta_rad_,
        max_delta_rad_
      );


    /*
     * 제한 적용 후 예상 위치
     */
    double predicted_yaw =
      current_yaw_ +
      yaw_delta;

    double predicted_pitch =
      current_pitch_ +
      pitch_delta;


    /*
     * 최종 절대 범위를 한 번 더 확인
     */
    predicted_yaw =
      std::clamp(
        predicted_yaw,
        yaw_min_,
        yaw_max_
      );

    predicted_pitch =
      std::clamp(
        predicted_pitch,
        pitch_min_,
        pitch_max_
      );


    /*
     * 우리가 control_node에 보내는 것은
     * 절대각이 아니라 상대 이동량이므로
     *
     * clamp된 목표 - 현재 위치
     *
     * 로 다시 계산한다.
     */
    yaw_delta =
      predicted_yaw -
      current_yaw_;

    pitch_delta =
      predicted_pitch -
      current_pitch_;


    /*
     * /motor/command 발행
     */
    std_msgs::msg::Float64MultiArray command;

    command.data.resize(2);

    command.data[0] =
      yaw_delta;

    command.data[1] =
      pitch_delta;

    command_pub_->publish(
      command
    );


    RCLCPP_INFO_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      1000,

      "Target ex=%.3f ey=%.3f area=%.4f | "
      "Motor yaw=%.4f pitch=%.4f | "
      "Delta yaw=%.4f pitch=%.4f | "
      "Next yaw=%.4f pitch=%.4f",

      ex,
      ey,
      area_ratio,

      current_yaw_,
      current_pitch_,

      yaw_delta,
      pitch_delta,

      predicted_yaw,
      predicted_pitch
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