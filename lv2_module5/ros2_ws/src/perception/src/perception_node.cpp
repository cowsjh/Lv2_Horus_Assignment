// perception_node: D435 color 영상을 받아 /target 을 발행하는 ROS2 노드
//
// 구독
//   <image_topic>         sensor_msgs/msg/Image   D435 color, rgb8
//                         (기본 /camera/camera/color/image_raw)
//
// 발행
//   /target               geometry_msgs/msg/PointStamped
//                         x = ex, y = ey, z = 면적비 (0 = 미검출), header = 원본 영상 header
//   /target/debug_image   sensor_msgs/msg/Image (bgr8)   오버레이
//                         publish_debug_image 가 true 일 때만
//   /target/debug_mask    sensor_msgs/msg/Image (mono8)  open/close 까지 끝난 최종 마스크
//                         publish_debug_mask 가 true 일 때만
//
// 발행 규칙 (발제 문제 2)
//   - 영상을 처리할 때마다 한 번 발행, 타이머로 이전 값을 다시 보내지 않음
//   - 미검출은 (0, 0, 0) 으로 발행
//   - 손상 프레임·계산값 NaN/inf 는 발행 안 함 -> 제어 쪽 입력 타임아웃 동작
//
// 파라미터
//   config_path               perception.yaml 경로, 비우면 패키지에 설치된 기본 설정
//                             (install/perception/share/perception/config/perception.yaml)
//                             ~ 로 시작하면 홈 폴더 기준으로 바꿔 읽음
//   image_topic               구독할 color 토픽
//   publish_debug_image       오버레이 영상 발행 여부 (처리 FPS 측정 시에는 false)
//   publish_debug_mask        마스크 영상 발행 여부 (처리 FPS 측정 시에는 false)
//   debug_image_every_n       디버그 영상(오버레이·마스크)을 n 프레임마다 한 번 발행
//   stats_period_sec          처리 FPS·검출 수 로그 주기 (0 이면 끔)
//
// 실행 예
//   ros2 run perception perception_node                              (기본 설정)
//   ros2 run perception perception_node --ros-args -p config_path:=~/tuning.yaml
//   bag 재처리: ros2 run perception perception_node --ros-args -r /target:=/target_replay

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <ament_index_cpp/get_package_prefix.hpp>  // PackageNotFoundError
#include <ament_index_cpp/get_package_share_path.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "perception/detector.hpp"
#include "perception/overlay.hpp"

namespace perception
{

using geometry_msgs::msg::PointStamped;
using sensor_msgs::msg::Image;

constexpr char kDefaultImageTopic[] = "/camera/camera/color/image_raw";
constexpr int kWarnThrottleMs = 5000;

// 발제 기본 QoS: best-effort, depth 1
rclcpp::QoS target_qos()
{
  return rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
}

// 읽을 설정 파일과 그 출처
struct ConfigSource
{
  std::filesystem::path path;
  bool from_parameter;   // true: config_path 로 지정, false: 패키지 기본 설정
};

// "~" 또는 "~/..." 로 시작하는 경로를 홈 폴더 기준 경로로 변환
// ROS2 파라미터는 ~ 를 자동으로 바꿔 주지 않음
std::filesystem::path expand_home(const std::string & path)
{
  if (path != "~" && path.rfind("~/", 0) != 0) {
    return path;
  }
  const char * home = std::getenv("HOME");
  if (home == nullptr) {
    throw std::invalid_argument("HOME 환경 변수가 없어 ~ 경로를 바꿀 수 없습니다: " + path);
  }
  return std::filesystem::path(home) / path.substr(std::min<size_t>(path.size(), 2));
}

// 빌드할 때 함께 설치된 기본 설정 (CMakeLists.txt 의 install(FILES ...))
std::filesystem::path default_config_path()
{
  try {
    return ament_index_cpp::get_package_share_path("perception") / "config" / "perception.yaml";
  } catch (const ament_index_cpp::PackageNotFoundError &) {
    throw std::runtime_error(
      "기본 설정을 찾을 수 없습니다. ros2_ws 에서 source install/setup.bash 를 했는지 확인하거나 "
      "-p config_path:=<perception.yaml 경로> 로 지정하세요.");
  }
}

// 로그용 설정 경로, 링크(colcon build --symlink-install)면 원본 위치도 붙임
// 링크가 아니면 빌드 때 복사된 파일이라 yaml 을 고친 뒤 다시 빌드해야 반영됨
std::string describe_config_path(const std::filesystem::path & path)
{
  std::error_code error;
  if (!std::filesystem::is_symlink(path, error)) {
    return path.string();
  }
  return path.string() + " -> " + std::filesystem::canonical(path, error).string();
}

ConfigSource resolve_config_source(const std::string & config_path)
{
  if (config_path.empty()) {
    return {default_config_path(), false};
  }
  return {std::filesystem::absolute(expand_home(config_path)), true};
}

class PerceptionNode : public rclcpp::Node
{
public:
  PerceptionNode()
  : Node("perception_node"),
    config_source_(resolve_config_source(declare_parameter<std::string>("config_path", ""))),
    detector_(load_config(config_source_.path.string()))
  {
    const auto image_topic = declare_parameter<std::string>("image_topic", kDefaultImageTopic);
    const bool publish_debug_image = declare_parameter<bool>("publish_debug_image", false);
    const bool publish_debug_mask = declare_parameter<bool>("publish_debug_mask", false);
    debug_every_n_ = std::max<int64_t>(1, declare_parameter<int64_t>("debug_image_every_n", 1));
    const double stats_period_sec = declare_parameter<double>("stats_period_sec", 5.0);

    target_pub_ = create_publisher<PointStamped>("/target", target_qos());
    if (publish_debug_image) {
      overlay_pub_ = create_publisher<Image>("/target/debug_image", target_qos());
    }
    if (publish_debug_mask) {
      mask_pub_ = create_publisher<Image>("/target/debug_mask", target_qos());
    }

    // 카메라 드라이버가 reliable 로 발행해도 best-effort 구독은 호환됨
    image_sub_ = create_subscription<Image>(
      image_topic, rclcpp::SensorDataQoS(),
      [this](const Image::ConstSharedPtr msg) {on_image(msg);});

    stats_start_ = std::chrono::steady_clock::now();
    if (stats_period_sec > 0.0) {
      stats_timer_ = create_wall_timer(
        std::chrono::duration<double>(stats_period_sec), [this]() {log_stats();});
    }

    RCLCPP_INFO(
      get_logger(), "구독: %s -> 발행: /target%s%s", image_topic.c_str(),
      overlay_pub_ ? ", /target/debug_image" : "", mask_pub_ ? ", /target/debug_mask" : "");
    RCLCPP_INFO(
      get_logger(), "설정: %s (%s)", describe_config_path(config_source_.path).c_str(),
      config_source_.from_parameter ? "config_path 지정" : "기본값");
    RCLCPP_INFO(get_logger(), "검출 설정: %s", describe(detector_.config()).c_str());
  }

private:
  void on_image(const Image::ConstSharedPtr & msg)
  {
    ++frame_count_;

    cv_bridge::CvImageConstPtr bgr;
    try {
      // rgb8 등은 bgr8 로 변환, 이미 bgr8 이면 복사 없이 공유
      bgr = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::BGR8);
    } catch (const cv_bridge::Exception & error) {
      ++stats_.skipped;
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), kWarnThrottleMs,
        "이미지 변환 실패, 발행 안 함: %s", error.what());
      return;
    }

    const Detection detection = detector_.process_debug(bgr->image);

    publish_target(msg->header, detection.result);

    const bool debug_frame = detection.debug && frame_count_ % debug_every_n_ == 0;
    if (debug_frame && overlay_pub_) {
      publish_overlay(msg->header, detection);
    }
    if (debug_frame && mask_pub_) {
      publish_mask(msg->header, detection);
    }
  }

  void publish_target(const std_msgs::msg::Header & header, const TargetResult & result)
  {
    const auto xyz = result.to_xyz();
    if (!xyz) {  // INVALID: 발행 안 함
      ++stats_.skipped;
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), kWarnThrottleMs,
        "무효 프레임, 발행 안 함: %s", result.reason.c_str());
      return;
    }

    PointStamped target;
    target.header = header;  // 촬영 시각·frame_id 유지
    target.point.x = xyz->x;
    target.point.y = xyz->y;
    target.point.z = xyz->z;
    target_pub_->publish(target);

    ++stats_.processed;
    if (result.detected()) {
      ++stats_.detected;
    }
  }

  void publish_overlay(const std_msgs::msg::Header & header, const Detection & detection)
  {
    const cv::Mat overlay =
      draw_overlay(detection.debug->frame, detection.result, detection.debug->rejected);
    const cv_bridge::CvImage image(header, sensor_msgs::image_encodings::BGR8, overlay);
    overlay_pub_->publish(*image.toImageMsg());
  }

  void publish_mask(const std_msgs::msg::Header & header, const Detection & detection)
  {
    const cv_bridge::CvImage image(
      header, sensor_msgs::image_encodings::MONO8, detection.debug->mask);
    mask_pub_->publish(*image.toImageMsg());
  }

  void log_stats()
  {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - stats_start_).count();
    const double fps = elapsed > 0.0 ? stats_.processed / elapsed : 0.0;
    RCLCPP_INFO(
      get_logger(), "처리 %d프레임 (%.1f FPS), 검출 %d, 발행 안 함 %d",
      stats_.processed, fps, stats_.detected, stats_.skipped);
    stats_ = Stats{};
    stats_start_ = now;
  }

  struct Stats
  {
    int processed = 0;
    int detected = 0;
    int skipped = 0;
  };

  ConfigSource config_source_;   // detector_ 보다 먼저 초기화되어야 한다 (선언 순서)
  TargetDetector detector_;
  int64_t debug_every_n_ = 1;
  int64_t frame_count_ = 0;
  Stats stats_;
  std::chrono::steady_clock::time_point stats_start_;

  rclcpp::Publisher<PointStamped>::SharedPtr target_pub_;
  rclcpp::Publisher<Image>::SharedPtr overlay_pub_;
  rclcpp::Publisher<Image>::SharedPtr mask_pub_;
  rclcpp::Subscription<Image>::SharedPtr image_sub_;
  rclcpp::TimerBase::SharedPtr stats_timer_;
};

}  // namespace perception

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int exit_code = 0;
  try {
    rclcpp::spin(std::make_shared<perception::PerceptionNode>());
  } catch (const std::exception & error) {
    // 설정 파일 없음·설정 오류 등, 원인을 남기고 종료
    RCLCPP_FATAL(rclcpp::get_logger("perception_node"), "%s", error.what());
    exit_code = 1;
  }
  rclcpp::shutdown();
  return exit_code;
}
