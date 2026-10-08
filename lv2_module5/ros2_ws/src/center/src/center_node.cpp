#include <rclcpp/rclcpp.hpp>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <std_msgs/msg/string.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>


// ---------------------------------------------------------------------------
// Tracking state machine (ROS 의존 없음)
//
// 상태 (팀 설계 "상태 전이 로직", 발제 문제 4)
//   IDLE     : 시작 후 목표를 아직 확인하지 못함
//   TRACKING : 신선한 목표 추적 중
//   LOST     : 추적 중 목표를 놓침 (사유 NO_TARGET 또는 TIMEOUT)
//
// 전이
//   IDLE     → TRACKING : 검출 연속 resume_frames 프레임
//   TRACKING → LOST     : 미검출 첫 프레임 (NO_TARGET)
//   TRACKING → LOST     : 마지막 /target 수신 후 input_timeout_sec 초과 (TIMEOUT)
//   LOST     → TRACKING : 검출 연속 resume_frames 프레임
//
// /target 을 받을 때마다 on_target(), 그 직후와 주기 타이머에서 update()
// 시각은 초 단위 (노드 시계 기준)
// ---------------------------------------------------------------------------

namespace
{

enum class TrackingState { Idle, Tracking, Lost };

// /tracking_status 값
const char * to_string(TrackingState state)
{
  switch (state) {
    case TrackingState::Idle:
      return "IDLE";
    case TrackingState::Tracking:
      return "TRACKING";
    case TrackingState::Lost:
      return "LOST";
  }
  return "UNKNOWN";
}

// /target (x, y, z) 가 검출인지: 유한값, z > 0, 정규화 범위 안 (인지 classify_target 과 같은 규칙)
bool is_detected(double x, double y, double z)
{
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    return false;
  }
  return z > 0.0 && z <= 1.0 && std::fabs(x) <= 1.0 && std::fabs(y) <= 1.0;
}

class TrackingStateMachine
{
public:
  TrackingStateMachine(double input_timeout_sec, int resume_frames)
  : input_timeout_sec_(input_timeout_sec), resume_frames_(resume_frames)
  {
  }

  // /target 수신 시 호출
  void on_target(double now_sec, bool detected)
  {
    has_input_ = true;
    last_input_sec_ = now_sec;
    has_new_frame_ = true;
    new_frame_detected_ = detected;
    consecutive_detect_ = detected ? consecutive_detect_ + 1 : 0;
  }

  // 상태 갱신, 상태가 바뀌었으면 true
  bool update(double now_sec)
  {
    const TrackingState before = state_;
    const bool timed_out = input_timed_out(now_sec);
    if (timed_out) {
      consecutive_detect_ = 0;  // 끊긴 동안의 검출 수는 무효
    }
    const bool target_confirmed = !timed_out && consecutive_detect_ >= resume_frames_;
    const bool new_no_target = has_new_frame_ && !new_frame_detected_;
    has_new_frame_ = false;

    switch (state_) {
      case TrackingState::Idle:
        if (target_confirmed) {
          change_state(TrackingState::Tracking, "TARGET_CONFIRMED");
        } else {
          reason_ = !has_input_ ? "NO_INPUT" : (timed_out ? "TIMEOUT" : "WAITING_TARGET");
        }
        break;

      case TrackingState::Tracking:
        if (timed_out) {
          change_state(TrackingState::Lost, "TIMEOUT");
        } else if (new_no_target) {
          change_state(TrackingState::Lost, "NO_TARGET");
        }
        break;

      case TrackingState::Lost:
        if (target_confirmed) {
          change_state(TrackingState::Tracking, "TARGET_CONFIRMED");
        } else if (timed_out) {
          reason_ = "TIMEOUT";
        } else if (new_no_target) {
          reason_ = "NO_TARGET";  // 입력은 다시 오지만 목표 없음
        }
        break;
    }
    return state_ != before;
  }

  TrackingState state() const {return state_;}

  // 현재 상태의 사유 (NO_INPUT, WAITING_TARGET, TARGET_CONFIRMED, NO_TARGET, TIMEOUT)
  const std::string & reason() const {return reason_;}

private:
  // 한 번이라도 받은 뒤 input_timeout_sec 동안 새 /target 이 없으면 true
  bool input_timed_out(double now_sec) const
  {
    return has_input_ && (now_sec - last_input_sec_) > input_timeout_sec_;
  }

  void change_state(TrackingState next, const std::string & reason)
  {
    state_ = next;
    reason_ = reason;
  }

  double input_timeout_sec_;
  int resume_frames_;

  TrackingState state_ = TrackingState::Idle;
  std::string reason_ = "NO_INPUT";

  bool has_input_ = false;          // /target 을 한 번이라도 받았는지
  double last_input_sec_ = 0.0;     // 마지막 /target 수신 시각
  bool has_new_frame_ = false;      // 마지막 update() 이후 새 /target 이 왔는지
  bool new_frame_detected_ = false;
  int consecutive_detect_ = 0;      // 연속 검출 수, 미검출·끊김이면 0
};

}  // namespace


class CenterNode : public rclcpp::Node
{
public:
  CenterNode()
  : Node("center_node"),
    state_machine_(0.5, 3)
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

    // P gain

    yaw_kp_ =
      this->declare_parameter<double>(
        "yaw_kp",
        100.0
      );    

    pitch_kp_ =
      this->declare_parameter<double>(
        "pitch_kp",
        100.0
      );
      
    // Maximum movement per command
    // Unit: tick

    max_delta_tick_ =
      this->declare_parameter<double>(
        "max_delta_tick_",
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

    // Tracking state (IDLE / TRACKING / LOST)
    //
    // input_timeout_sec : /target 무수신 → LOST(TIMEOUT)
    // resume_frames     : 연속 검출 프레임 수 → TRACKING
    // state_check_period_sec : 무수신 감지 주기

    const double input_timeout_sec =
      this->declare_parameter<double>(
        "input_timeout_sec",
        0.5
      );

    const int resume_frames =
      this->declare_parameter<int>(
        "resume_frames",
        3
      );

    const double state_check_period_sec =
      this->declare_parameter<double>(
        "state_check_period_sec",
        0.05
      );

    state_machine_ =
      TrackingStateMachine(input_timeout_sec, resume_frames);

    // /tracking_status : IDLE / TRACKING / LOST

    status_pub_ =
      this->create_publisher<std_msgs::msg::String>(
        "/tracking_status",
        rclcpp::QoS(rclcpp::KeepLast(10)).reliable()
      );

    state_timer_ =
      this->create_wall_timer(
        std::chrono::duration<double>(state_check_period_sec),
        std::bind(
          &CenterNode::stateTimerCallback,
          this
        )
      );

    RCLCPP_INFO(
      this->get_logger(),
      "Center node started."
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Tracking    : timeout %.2f s, resume %d frames",
      input_timeout_sec,
      resume_frames
    );

    publishStatus(this->now());
    last_control_time_ = this->now();

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
      "Kp          : yaw=%.3f pitch=%.3f",
      yaw_kp_,
      pitch_kp_
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

  double yaw_kp_;
  double pitch_kp_;

  double max_delta_tick_;
  double deadband_;

  double last_yaw_delta =  0;
  double last_pitch_delta = 0;

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

  // Tracking state

  TrackingStateMachine state_machine_;

  rclcpp::Publisher<
    std_msgs::msg::String
  >::SharedPtr status_pub_;

  rclcpp::TimerBase::SharedPtr state_timer_;

  rclcpp::Time last_status_pub_;
   
  // control dt 계산용
  rclcpp::Time last_control_time_;

  // 상태 갱신, 바뀌면 로그와 /tracking_status 발행

  void updateState(
    const rclcpp::Time & now
  )
  {
    const TrackingState before =
      state_machine_.state();
    const std::string reason_before =
      state_machine_.reason();

    if (!state_machine_.update(now.seconds()))
    {
      // 상태는 그대로지만 인지 입력이 끊긴 경우 한 번만 기록 (예: LOST 중 인지 중단)
      if (state_machine_.reason() == "TIMEOUT" && reason_before != "TIMEOUT")
      {
        RCLCPP_INFO(
          this->get_logger(),
          "STATE: %s (TIMEOUT)",
          to_string(state_machine_.state())
        );
      }
      return;
    }

    RCLCPP_INFO(
      this->get_logger(),
      "STATE: %s -> %s (%s)",
      to_string(before),
      to_string(state_machine_.state()),
      state_machine_.reason().c_str()
    );

    publishStatus(now);
  }


  void publishStatus(
    const rclcpp::Time & now
  )
  {
    std_msgs::msg::String status;

    status.data =
      to_string(state_machine_.state());

    status_pub_->publish(status);

    last_status_pub_ = now;
  }


  // 무수신(TIMEOUT) 감지, 상태는 바뀔 때 + 1초마다 발행

  void stateTimerCallback()
  {
    const rclcpp::Time now =
      this->now();

    updateState(now);

    if ((now - last_status_pub_).seconds() >= 1.0)
    {
      publishStatus(now);
    }
  }


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


    // Tracking state
    //
    // TRACKING 일 때만 아래 명령 계산 진행
    // IDLE / LOST 이면 명령을 보내지 않음

    const rclcpp::Time now =
      this->now();

    state_machine_.on_target(
      now.seconds(),
      is_detected(ex, ey, area_ratio)
    );

    updateState(now);

    if (state_machine_.state() != TrackingState::Tracking)
    {
      return;
    }


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
    double error_x = std::clamp(ex, -1.0, 1.0);
    double error_y = std::clamp(ey, -1.0, 1.0);


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

    if (error_x == 0.0 && error_y == 0.0)
    {
      RCLCPP_INFO_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000, "Target is inside deadband.");

      return;
    }

    // const double dt = std::clamp((now - last_control_time_).seconds(), 0.0, 1.0);
    const double dt = 1;    
    // last_control_time_ = now;
  
    // speed × dt = position increment
    double yaw_delta   = last_yaw_delta + -yaw_kp_ * error_x * dt;
    double pitch_delta = last_pitch_delta + -pitch_kp_ * error_y * dt;

    // 이전 goal 변수에 저장
    // last_yaw_delta = yaw_delta;
    // last_pitch_delta = pitch_delta;

    // Limit movement per command
    yaw_delta   = std::clamp(yaw_delta, -max_delta_tick_, max_delta_tick_);
    pitch_delta = std::clamp(pitch_delta, -max_delta_tick_, max_delta_tick_);


    // Calculate absolute target position
    const double target_yaw = std::clamp(current_yaw_ + yaw_delta, yaw_min_, yaw_max_);
    const double target_pitch = std::clamp(current_pitch_ + pitch_delta, pitch_min_, pitch_max_);

    // Publish absolute motor position

    std_msgs::msg::Float64MultiArray command;

    command.data.resize(2);

    command.data[0] = target_yaw;
    command.data[1] = target_pitch;

    command_pub_->publish(command);

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


int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CenterNode>());

  rclcpp::shutdown();

  return 0;
}