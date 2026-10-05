#include "perception/detector.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include <opencv2/imgproc.hpp>
#include <yaml-cpp/yaml.h>

namespace perception
{

namespace
{

constexpr std::array<int, 3> kHsvMax = {179, 255, 255};  // OpenCV HSV 최댓값 (H, S, V)

// perception.yaml detector 섹션에 쓸 수 있는 키, 그 외 키는 오타로 보고 거부
const std::set<std::string> kKnownKeys = {
  "hsv_ranges", "min_area_px", "blur_ksize", "morph_kernel", "bbox_style", "resize_width",
};

std::array<int, 3> read_hsv_triplet(const YAML::Node & node, const std::string & name)
{
  if (!node || !node.IsSequence() || node.size() != 3) {
    throw std::invalid_argument("hsv_ranges 의 " + name + " 는 값 3개짜리 목록이어야 합니다.");
  }
  return {node[0].as<int>(), node[1].as<int>(), node[2].as<int>()};
}

BboxStyle parse_bbox_style(const std::string & text)
{
  if (text == "axis") {
    return BboxStyle::Axis;
  }
  if (text == "rotated") {
    return BboxStyle::Rotated;
  }
  throw std::invalid_argument("bbox_style 은 axis 또는 rotated 여야 합니다: " + text);
}

DetectorConfig config_from_yaml(const YAML::Node & root)
{
  const YAML::Node section = root["detector"] ? root["detector"] : root;
  if (!section.IsMap()) {
    throw std::invalid_argument("설정 파일이 비어 있거나 형식이 잘못됐습니다.");
  }

  for (const auto & item : section) {
    const auto key = item.first.as<std::string>();
    if (kKnownKeys.count(key) == 0) {
      throw std::invalid_argument("알 수 없는 설정 키: " + key);
    }
  }

  DetectorConfig config;
  const YAML::Node ranges = section["hsv_ranges"];
  if (ranges && !ranges.IsSequence()) {
    throw std::invalid_argument("hsv_ranges 는 목록이어야 합니다.");
  }
  for (const auto & range : ranges) {
    config.hsv_ranges.push_back(
      {read_hsv_triplet(range["lower"], "lower"), read_hsv_triplet(range["upper"], "upper")});
  }
  if (section["min_area_px"]) {
    config.min_area_px = section["min_area_px"].as<double>();
  }
  if (section["blur_ksize"]) {
    config.blur_ksize = section["blur_ksize"].as<int>();
  }
  if (section["morph_kernel"]) {
    config.morph_kernel = section["morph_kernel"].as<int>();
  }
  if (section["bbox_style"]) {
    config.bbox_style = parse_bbox_style(section["bbox_style"].as<std::string>());
  }
  if (section["resize_width"] && !section["resize_width"].IsNull()) {
    config.resize_width = section["resize_width"].as<int>();
  }

  config.validate();
  return config;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. 설정
// ---------------------------------------------------------------------------

void DetectorConfig::validate() const
{
  if (hsv_ranges.empty()) {
    throw std::invalid_argument("hsv_ranges 가 비어 있습니다.");
  }
  for (const auto & range : hsv_ranges) {
    for (size_t i = 0; i < 3; ++i) {
      const bool in_order =
        0 <= range.lower[i] && range.lower[i] <= range.upper[i] && range.upper[i] <= kHsvMax[i];
      if (!in_order) {
        throw std::invalid_argument("잘못된 HSV 범위입니다 (0 <= lower <= upper <= 179/255/255).");
      }
    }
  }
  if (min_area_px < 0.0) {
    throw std::invalid_argument("min_area_px 는 0 이상이어야 합니다.");
  }
  if (blur_ksize < 0 || (blur_ksize > 0 && blur_ksize % 2 == 0)) {
    throw std::invalid_argument("blur_ksize 는 0 또는 양의 홀수여야 합니다.");
  }
  if (morph_kernel < 1) {
    throw std::invalid_argument("morph_kernel 은 1 이상이어야 합니다.");
  }
  if (resize_width != 0 && resize_width < 16) {
    throw std::invalid_argument("resize_width 는 0(끔) 또는 16 이상이어야 합니다.");
  }
}

DetectorConfig load_config(const std::string & path)
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::Exception & error) {
    throw std::invalid_argument(
      "설정 파일을 읽을 수 없습니다: " + path + " (" + error.what() + ")");
  }
  return config_from_yaml(root);
}

DetectorConfig load_config_from_string(const std::string & yaml_text)
{
  return config_from_yaml(YAML::Load(yaml_text));
}

std::string to_string(BboxStyle style)
{
  return style == BboxStyle::Axis ? "axis" : "rotated";
}

std::string describe(const DetectorConfig & config)
{
  const auto triplet = [](const std::array<int, 3> & values) {
      return "[" + std::to_string(values[0]) + "," + std::to_string(values[1]) + "," +
             std::to_string(values[2]) + "]";
    };

  std::ostringstream text;
  text << "HSV ";
  for (size_t i = 0; i < config.hsv_ranges.size(); ++i) {
    const auto & range = config.hsv_ranges[i];
    text << (i == 0 ? "" : " | ") << triplet(range.lower) << "~" << triplet(range.upper);
  }
  text << ", blur " << config.blur_ksize
       << ", morph " << config.morph_kernel
       << " (open " << kOpenIterations << ", close " << kCloseIterations << ")"
       << ", min_area " << config.min_area_px << "px"
       << ", bbox " << to_string(config.bbox_style)
       << ", resize " << (config.resize_width > 0 ? std::to_string(config.resize_width) : "원본");
  return text.str();
}

// ---------------------------------------------------------------------------
// 2. 결과와 /target (x, y, z) 판정
// ---------------------------------------------------------------------------

std::string to_string(TargetStatus status)
{
  switch (status) {
    case TargetStatus::Detected:
      return "detected";
    case TargetStatus::NoTarget:
      return "no_target";
    case TargetStatus::Invalid:
      return "invalid";
  }
  return "invalid";
}

TargetStatus classify_target(double x, double y, double z)
{
  if (!(std::isfinite(x) && std::isfinite(y) && std::isfinite(z))) {
    return TargetStatus::Invalid;
  }
  if (z <= 0.0) {
    return TargetStatus::NoTarget;
  }
  if (std::abs(x) > 1.0 || std::abs(y) > 1.0 || z > 1.0) {
    return TargetStatus::Invalid;
  }
  return TargetStatus::Detected;
}

bool is_valid_target(double x, double y, double z)
{
  return classify_target(x, y, z) == TargetStatus::Detected;
}

std::optional<TargetXyz> TargetResult::to_xyz() const
{
  if (status == TargetStatus::Detected) {
    return TargetXyz{ex, ey, area_ratio};
  }
  if (status == TargetStatus::NoTarget) {
    return TargetXyz{0.0, 0.0, 0.0};
  }
  return std::nullopt;
}

TargetResult TargetResult::no_target(int width, int height, std::string reason)
{
  TargetResult result;
  result.status = TargetStatus::NoTarget;
  result.frame_width = width;
  result.frame_height = height;
  result.reason = std::move(reason);
  return result;
}

TargetResult TargetResult::invalid(int width, int height, std::string reason)
{
  TargetResult result;
  result.status = TargetStatus::Invalid;
  result.frame_width = width;
  result.frame_height = height;
  result.reason = std::move(reason);
  return result;
}

// ---------------------------------------------------------------------------
// 3. 검출
// ---------------------------------------------------------------------------

TargetDetector::TargetDetector(DetectorConfig config)
: config_(std::move(config))
{
  config_.validate();
}

TargetResult TargetDetector::process(const cv::Mat & bgr) const
{
  return process_debug(bgr).result;
}

Detection TargetDetector::process_debug(const cv::Mat & bgr) const
{
  // 1. 프레임 검사
  const std::string problem = validate_bgr(bgr);
  if (!problem.empty()) {
    return {TargetResult::invalid(0, 0, problem), std::nullopt};
  }

  // 2. 리사이즈
  const cv::Mat frame = resize_to_width(bgr, config_.resize_width);
  const int width = frame.cols;
  const int height = frame.rows;

  // 3~4. 마스크 생성과 정리
  cv::Mat mask = hsv_mask(frame, config_.hsv_ranges, config_.blur_ksize);
  mask = clean_mask(mask, config_.morph_kernel);

  // 5~6. 후보 측정과 필터
  auto [accepted, rejected] = find_candidates(mask);

  // 7. 목표 선택
  const auto target = select_target(accepted);
  DetectionDebug debug{frame, mask, std::move(accepted), std::move(rejected)};
  if (!target) {
    const std::string reason = rejection_summary(debug.rejected);
    return {TargetResult::no_target(width, height, reason), std::move(debug)};
  }

  // 8. 결과 계산
  return {build_result(*target, width, height), std::move(debug)};
}

std::pair<std::vector<Candidate>, std::vector<Candidate>>
TargetDetector::find_candidates(const cv::Mat & mask) const
{
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  std::vector<Candidate> accepted;
  std::vector<Candidate> rejected;
  for (const auto & contour : contours) {
    auto candidate = measure_contour(contour, config_.bbox_style);
    if (!candidate) {
      continue;
    }
    candidate->reject = reject_reason(*candidate, config_);
    if (candidate->reject.empty()) {
      accepted.push_back(std::move(*candidate));
    } else {
      rejected.push_back(std::move(*candidate));
    }
  }
  return {std::move(accepted), std::move(rejected)};
}

TargetResult TargetDetector::build_result(const Candidate & target, int width, int height) const
{
  const auto [ex, ey] = normalize_error(target.cx, target.cy, width, height);
  const double ratio = area_ratio(target.area_px, width, height);

  // 계산 결과에 NaN/inf 가 섞이면 미검출(0)로 바꾸지 않고 무효로 처리
  if (classify_target(ex, ey, ratio) != TargetStatus::Detected) {
    std::ostringstream reason;
    reason << "non-finite or out-of-range (" << ex << ", " << ey << ", " << ratio << ")";
    return TargetResult::invalid(width, height, reason.str());
  }

  TargetResult result;
  result.status = TargetStatus::Detected;
  result.frame_width = width;
  result.frame_height = height;
  result.cx = target.cx;
  result.cy = target.cy;
  result.ex = ex;
  result.ey = ey;
  result.area_px = target.area_px;
  result.area_ratio = ratio;
  result.contour = target.contour;
  result.bbox = target.bbox;
  return result;
}

std::string validate_bgr(const cv::Mat & image)
{
  if (image.empty()) {
    return "frame is empty";
  }
  if (image.dims != 2 || image.channels() != 3) {
    return "frame is not HxWx3 (channels=" + std::to_string(image.channels()) + ")";
  }
  if (image.depth() != CV_8U) {
    return "frame dtype is not uint8";
  }
  return "";
}

cv::Mat resize_to_width(const cv::Mat & bgr, int width)
{
  if (width <= 0 || bgr.cols == width) {
    return bgr;
  }
  const double scale = static_cast<double>(width) / bgr.cols;
  const int height = std::max(1, static_cast<int>(std::lround(bgr.rows * scale)));
  cv::Mat resized;
  cv::resize(bgr, resized, cv::Size(width, height), 0, 0, cv::INTER_AREA);
  return resized;
}

cv::Mat hsv_mask(const cv::Mat & bgr, const std::vector<HsvRange> & hsv_ranges, int blur_ksize)
{
  // 블러 결과는 새 Mat 에 담음
  // 입력과 같은 Mat 에 쓰면 호출한 쪽 이미지(원본·카메라 메시지)까지 블러됨
  // (cv::Mat 대입은 데이터를 복사하지 않고 공유)
  cv::Mat source;
  if (blur_ksize > 0) {
    cv::GaussianBlur(bgr, source, cv::Size(blur_ksize, blur_ksize), 0);
  } else {
    source = bgr;
  }
  cv::Mat hsv;
  cv::cvtColor(source, hsv, cv::COLOR_BGR2HSV);

  cv::Mat mask = cv::Mat::zeros(hsv.size(), CV_8UC1);
  cv::Mat range_mask;
  for (const auto & range : hsv_ranges) {
    cv::inRange(
      hsv,
      cv::Scalar(range.lower[0], range.lower[1], range.lower[2]),
      cv::Scalar(range.upper[0], range.upper[1], range.upper[2]),
      range_mask);
    mask |= range_mask;
  }
  return mask;
}

cv::Mat clean_mask(const cv::Mat & mask, int kernel_size)
{
  const cv::Mat kernel =
    cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size));
  cv::Mat cleaned;
  cv::morphologyEx(mask, cleaned, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), kOpenIterations);
  cv::morphologyEx(cleaned, cleaned, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), kCloseIterations);
  return cleaned;
}

BoundingBox bounding_box(const std::vector<cv::Point> & contour, BboxStyle style)
{
  if (style == BboxStyle::Axis) {
    const cv::Rect rect = cv::boundingRect(contour);
    // width·height 는 픽셀 개수라서 마지막 픽셀 좌표는 x+w-1 (minAreaRect 와 같은 좌표 기준)
    const float left = static_cast<float>(rect.x);
    const float top = static_cast<float>(rect.y);
    const float right = static_cast<float>(rect.x + rect.width - 1);
    const float bottom = static_cast<float>(rect.y + rect.height - 1);
    return {
      (left + right) / 2.0,
      (top + bottom) / 2.0,
      {{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
  }

  const cv::RotatedRect rect = cv::minAreaRect(contour);
  std::vector<cv::Point2f> corners(4);
  rect.points(corners.data());
  return {rect.center.x, rect.center.y, corners};
}

std::optional<Candidate> measure_contour(const std::vector<cv::Point> & contour, BboxStyle style)
{
  const double area = cv::contourArea(contour);
  if (area <= 0.0) {
    return std::nullopt;
  }
  BoundingBox box = bounding_box(contour, style);

  Candidate candidate;
  candidate.contour = contour;
  candidate.area_px = area;
  candidate.cx = box.cx;
  candidate.cy = box.cy;
  candidate.bbox = std::move(box.corners);
  return candidate;
}

std::string reject_reason(const Candidate & candidate, const DetectorConfig & config)
{
  if (candidate.area_px < config.min_area_px) {
    return "too_small";
  }
  return "";
}

std::optional<Candidate> select_target(const std::vector<Candidate> & candidates)
{
  if (candidates.empty()) {
    return std::nullopt;
  }
  return *std::max_element(
    candidates.begin(), candidates.end(),
    [](const Candidate & a, const Candidate & b) {return a.area_px < b.area_px;});
}

std::pair<double, double> normalize_error(double cx, double cy, int width, int height)
{
  const double half_width = width / 2.0;
  const double half_height = height / 2.0;
  return {(cx - half_width) / half_width, (cy - half_height) / half_height};
}

double area_ratio(double area_px, int width, int height)
{
  return area_px / (static_cast<double>(width) * height);
}

std::string rejection_summary(const std::vector<Candidate> & rejected)
{
  if (rejected.empty()) {
    return "no candidate";
  }
  std::map<std::string, int> counts;  // 사유 이름순 정렬
  for (const auto & candidate : rejected) {
    ++counts[candidate.reject];
  }
  std::ostringstream summary;
  summary << "rejected ";
  bool first = true;
  for (const auto & [reason, count] : counts) {
    summary << (first ? "" : ", ") << reason << "=" << count;
    first = false;
  }
  return summary.str();
}

}  // namespace perception
