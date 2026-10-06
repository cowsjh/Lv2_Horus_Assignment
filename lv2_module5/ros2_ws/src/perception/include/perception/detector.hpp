// 파란색 단일 목표 검출 (ROS 의존 없음)
//
// D435 color 프레임 한 장을 받아 /target 에 넣을 (x, y, z) 값을 만듦
//
// 처리 순서
//   1. 프레임 검사   : 처리할 수 없는 프레임이면 INVALID
//   2. 리사이즈      : resize_width 가 있으면 비율을 유지해 줄임
//   3. HSV 마스크    : 블러 -> HSV 변환 -> 파란색 범위만 남김
//   4. 마스크 정리   : open(점 잡음 제거) -> close(구멍 메우기)
//   5. 후보 측정     : 컨투어마다 면적과 bbox(중심) 계산
//   6. 후보 필터     : 너무 작은 후보 제외
//   7. 목표 선택     : 남은 후보 중 면적이 가장 큰 것
//   8. 결과 계산     : 정규화 오차와 면적비
//
// 목표 중심 (cx, cy) = bbox 중심 (bbox_style: axis | rotated)
// 정규화 오차        ex = (cx - W/2) / (W/2),  ey = (cy - H/2) / (H/2)   오른쪽·아래쪽이 +
// 면적비             area_ratio = contour_area / (W * H)
//
// /target (x, y, z) 판정 규칙
//   x, y, z 중 NaN/inf 가 있음   -> INVALID    (사용 금지, 발행하지 않음)
//   z <= 0                       -> NO_TARGET  (정상 프레임에서 미검출, (0, 0, 0) 발행)
//   |x| > 1, |y| > 1, z > 1      -> INVALID    (정규화 범위 밖)
//   그 외                        -> DETECTED
//   x = y = 0 이어도 z > 0 이면 화면 중앙의 검출, 미검출 여부는 z 로만 판단

#ifndef PERCEPTION__DETECTOR_HPP_
#define PERCEPTION__DETECTOR_HPP_

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

namespace perception
{

// ---------------------------------------------------------------------------
// 1. 설정 (lv2_module5/config/perception.yaml 의 detector 섹션)
// ---------------------------------------------------------------------------

// OpenCV HSV 범위 (H: 0~179, S·V: 0~255)
struct HsvRange
{
  std::array<int, 3> lower;
  std::array<int, 3> upper;
};

// 목표 중심을 구하는 bbox 형식
//   Axis    : cv::boundingRect, 화면 축에 맞춘 사각형
//   Rotated : cv::minAreaRect, 목표 방향에 맞춰 회전한 사각형 (기울어진 블록)
enum class BboxStyle { Axis, Rotated };

// 마스크 정리 반복 횟수 (설정 파일에서 바꾸지 않는 고정값)
//   open 1회  : 점 잡음 제거
//   close 2회 : 블록 안쪽 구멍·틈 메우기
constexpr int kOpenIterations = 1;
constexpr int kCloseIterations = 2;

struct DetectorConfig
{
  // 목표 색 HSV 범위, 여러 개면 OR 로 합침
  std::vector<HsvRange> hsv_ranges;

  // 후보 필터: 이 면적(px)보다 작으면 잡음으로 제외
  double min_area_px = 300.0;

  // 마스크 전처리: 가우시안 블러 커널(0 이면 끔, 홀수)과 모폴로지 커널 크기
  int blur_ksize = 5;
  int morph_kernel = 5;

  BboxStyle bbox_style = BboxStyle::Rotated;

  // 처리 전 리사이즈 폭(px), 0 이면 입력 해상도 그대로, 높이는 비율 유지
  int resize_width = 0;

  // 값이 잘못되면 std::invalid_argument
  void validate() const;
};

// perception.yaml 읽기, `detector` 섹션이 있으면 그 안의 값 사용
// 모르는 키나 잘못된 값이 있으면 std::invalid_argument
DetectorConfig load_config(const std::string & path);
DetectorConfig load_config_from_string(const std::string & yaml_text);

// "axis" | "rotated" (yaml 에 쓰는 이름과 같음)
std::string to_string(BboxStyle style);

// 설정 전체를 한 줄로 요약, 노드 시작 로그에서 실제 적용값 확인용
// 예: "HSV [98,120,40]~[130,255,255], blur 5, morph 5 (open 1, close 2), min_area 300px,
//      bbox rotated, resize 원본"
std::string describe(const DetectorConfig & config);

// ---------------------------------------------------------------------------
// 2. 결과와 /target (x, y, z) 판정
// ---------------------------------------------------------------------------

enum class TargetStatus { Detected, NoTarget, Invalid };

// "detected" | "no_target" | "invalid"
std::string to_string(TargetStatus status);

// (x, y, z) 값만 보고 검출·미검출·무효 판정, 제어 쪽에서도 같은 규칙 사용
TargetStatus classify_target(double x, double y, double z);

// 제어에 써도 되는 값이면 true, z=0 미검출과 NaN 은 모두 false
bool is_valid_target(double x, double y, double z);

struct TargetXyz
{
  double x;
  double y;
  double z;
};

// 한 프레임의 인지 결과
struct TargetResult
{
  TargetStatus status = TargetStatus::Invalid;
  int frame_width = 0;              // 실제로 처리한 프레임 크기 (W, H)
  int frame_height = 0;
  // bbox 중심 픽셀 좌표, 미검출·무효이면 NaN
  double cx = std::numeric_limits<double>::quiet_NaN();
  double cy = std::numeric_limits<double>::quiet_NaN();
  double ex = 0.0;                  // 정규화 오차 (-1 ~ 1)
  double ey = 0.0;
  double area_px = 0.0;             // 컨투어 면적(px)
  double area_ratio = 0.0;          // 면적비
  std::vector<cv::Point> contour;   // 선택된 컨투어, 검출일 때만 값 있음
  std::vector<cv::Point2f> bbox;    // bbox 꼭짓점 4개, 검출일 때만 값 있음
  std::string reason;               // 미검출·무효 사유

  bool detected() const {return status == TargetStatus::Detected;}

  // /target 에 넣을 (x, y, z)
  //   DETECTED  -> (ex, ey, area_ratio)
  //   NO_TARGET -> (0, 0, 0)
  //   INVALID   -> 값 없음 (발행하지 않음, 이전 값 재사용도 금지)
  std::optional<TargetXyz> to_xyz() const;

  static TargetResult no_target(int width, int height, std::string reason = "");
  static TargetResult invalid(int width = 0, int height = 0, std::string reason = "");
};

// ---------------------------------------------------------------------------
// 3. 검출
// ---------------------------------------------------------------------------

// 마스크에서 찾은 컨투어 하나
struct Candidate
{
  std::vector<cv::Point> contour;
  double area_px = 0.0;
  double cx = 0.0;                  // bbox 중심
  double cy = 0.0;
  std::vector<cv::Point2f> bbox;    // 꼭짓점 4개
  std::string reject;               // 필터에서 제외된 사유, 통과하면 빈 문자열
};

// 디버그·결과 저장용 중간 결과
struct DetectionDebug
{
  cv::Mat frame;                    // 실제로 처리한(리사이즈된) BGR 프레임
  cv::Mat mask;                     // 정리된 이진 마스크
  std::vector<Candidate> accepted;
  std::vector<Candidate> rejected;
};

struct Detection
{
  TargetResult result;
  std::optional<DetectionDebug> debug;  // 무효 프레임이면 값 없음
};

// 프레임 한 장을 받아 TargetResult 반환, 이전 프레임 결과는 기억하지 않음
class TargetDetector
{
public:
  explicit TargetDetector(DetectorConfig config);

  TargetResult process(const cv::Mat & bgr) const;
  Detection process_debug(const cv::Mat & bgr) const;

  const DetectorConfig & config() const {return config_;}

private:
  std::pair<std::vector<Candidate>, std::vector<Candidate>> find_candidates(
    const cv::Mat & mask) const;
  TargetResult build_result(const Candidate & target, int width, int height) const;

  DetectorConfig config_;
};

// 단계별 함수 (테스트·도구에서도 사용)

// 처리할 수 없는 프레임이면 이유, 정상이면 빈 문자열
std::string validate_bgr(const cv::Mat & image);

// 폭을 width 로 맞추고 높이는 비율 유지, width 가 0 이면 그대로
cv::Mat resize_to_width(const cv::Mat & bgr, int width);

// HSV 범위 안의 픽셀만 255 인 이진 마스크
cv::Mat hsv_mask(const cv::Mat & bgr, const std::vector<HsvRange> & hsv_ranges, int blur_ksize);

// open(kOpenIterations 회)으로 점 잡음 제거, close(kCloseIterations 회)로 구멍 메움
cv::Mat clean_mask(const cv::Mat & mask, int kernel_size);

struct BoundingBox
{
  double cx;
  double cy;
  std::vector<cv::Point2f> corners;
};

// 컨투어의 bbox 와 그 중심
BoundingBox bounding_box(const std::vector<cv::Point> & contour, BboxStyle style);

// 면적과 bbox 계산, 면적이 0 인 컨투어는 값 없음
std::optional<Candidate> measure_contour(const std::vector<cv::Point> & contour, BboxStyle style);

// 후보를 제외할 사유, 통과하면 빈 문자열
std::string reject_reason(const Candidate & candidate, const DetectorConfig & config);

// 남은 후보 중 면적이 가장 큰 것, 후보가 없으면 값 없음
std::optional<Candidate> select_target(const std::vector<Candidate> & candidates);

// 영상 중심 대비 오차를 -1 ~ 1 로 정규화, 오른쪽·아래쪽이 +
std::pair<double, double> normalize_error(double cx, double cy, int width, int height);

double area_ratio(double area_px, int width, int height);

// 미검출 사유 문자열 (예: "rejected too_small=2")
std::string rejection_summary(const std::vector<Candidate> & rejected);

}  // namespace perception

#endif  // PERCEPTION__DETECTOR_HPP_
