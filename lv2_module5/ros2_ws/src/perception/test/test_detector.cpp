// 검출 라이브러리 시험: 검출·오차 부호·면적·목표 선택·bbox·/target 판정·설정 검증

#include <cmath>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>
#include <opencv2/imgproc.hpp>

#include "perception/detector.hpp"
#include "perception/overlay.hpp"

namespace perception
{
namespace
{

constexpr int W = 640;
constexpr int H = 480;
const cv::Scalar kBlue(255, 0, 0);     // BGR, Hue 120
const cv::Scalar kNavy(90, 20, 20);    // 어두운 파랑
const cv::Scalar kRed(0, 0, 255);
const cv::Scalar kBackground(40, 40, 40);
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Square
{
  int cx;
  int cy;
  int half;
  cv::Scalar color;
};

cv::Mat make_frame(const std::vector<Square> & squares = {})
{
  cv::Mat image(H, W, CV_8UC3, kBackground);
  for (const auto & s : squares) {
    cv::rectangle(
      image, cv::Point(s.cx - s.half, s.cy - s.half), cv::Point(s.cx + s.half, s.cy + s.half),
      s.color, cv::FILLED);
  }
  return image;
}

cv::Mat tilted_card(float cx, float cy, float w, float h, float angle)
{
  cv::Mat image(H, W, CV_8UC3, kBackground);
  cv::Point2f corners[4];
  cv::RotatedRect({cx, cy}, {w, h}, angle).points(corners);
  std::vector<cv::Point> polygon;
  for (const auto & c : corners) {
    polygon.emplace_back(static_cast<int>(std::lround(c.x)), static_cast<int>(std::lround(c.y)));
  }
  cv::fillPoly(image, std::vector<std::vector<cv::Point>>{polygon}, kBlue);
  return image;
}

DetectorConfig base_config()
{
  return load_config(PERCEPTION_CONFIG_PATH);
}

TargetDetector make_detector(BboxStyle style = BboxStyle::Rotated)
{
  DetectorConfig config = base_config();
  config.bbox_style = style;
  return TargetDetector(config);
}

// ---------- 기본 검출 ----------

TEST(Detector, YamlIsBlueOnly)
{
  const DetectorConfig config = base_config();
  ASSERT_EQ(config.hsv_ranges.size(), 1u);
  EXPECT_LE(config.hsv_ranges[0].lower[0], 120);
  EXPECT_GE(config.hsv_ranges[0].upper[0], 120);
}

TEST(Detector, CenteredTargetHasZeroError)
{
  const TargetResult result = make_detector().process(make_frame({{W / 2, H / 2, 30, kBlue}}));
  ASSERT_EQ(result.status, TargetStatus::Detected);
  const auto xyz = result.to_xyz();
  ASSERT_TRUE(xyz.has_value());
  EXPECT_LT(std::abs(xyz->x), 0.01);
  EXPECT_LT(std::abs(xyz->y), 0.01);
  EXPECT_GT(xyz->z, 0.0);
  EXPECT_EQ(classify_target(xyz->x, xyz->y, xyz->z), TargetStatus::Detected);
}

TEST(Detector, DarkBlueDetected)
{
  EXPECT_TRUE(make_detector().process(make_frame({{W / 2, H / 2, 30, kNavy}})).detected());
}

TEST(Detector, HorizontalErrorSign)
{
  for (const auto & [cx, sign] : std::vector<std::pair<int, double>>{{560, 1.0}, {80, -1.0}}) {
    const TargetResult result = make_detector().process(make_frame({{cx, H / 2, 30, kBlue}}));
    ASSERT_TRUE(result.detected());
    EXPECT_EQ(std::copysign(1.0, result.ex), sign);
    EXPECT_NEAR(result.ex, (cx - W / 2.0) / (W / 2.0), 0.01);
  }
}

TEST(Detector, VerticalErrorDownIsPositive)
{
  EXPECT_GT(make_detector().process(make_frame({{W / 2, 400, 30, kBlue}})).ey, 0.0);
}

TEST(Detector, AreaRatio)
{
  const TargetResult result = make_detector().process(make_frame({{W / 2, H / 2, 30, kBlue}}));
  const double expected = 60.0 * 60.0 / (W * H);
  EXPECT_NEAR(result.area_ratio, expected, expected * 0.1);
}

TEST(Detector, RedIsNotDetected)
{
  const TargetResult result = make_detector().process(make_frame({{W / 2, H / 2, 30, kRed}}));
  EXPECT_EQ(result.status, TargetStatus::NoTarget);
  const auto xyz = result.to_xyz();
  ASSERT_TRUE(xyz.has_value());
  EXPECT_EQ(xyz->x, 0.0);
  EXPECT_EQ(xyz->y, 0.0);
  EXPECT_EQ(xyz->z, 0.0);
  EXPECT_TRUE(std::isnan(result.cx));
  EXPECT_TRUE(std::isnan(result.cy));
}

TEST(Detector, SmallBlobIgnored)
{
  const TargetResult result = make_detector().process(make_frame({{W / 2, H / 2, 5, kBlue}}));
  EXPECT_EQ(result.status, TargetStatus::NoTarget);
  EXPECT_NE(result.reason.find("too_small"), std::string::npos);
}

TEST(Detector, FullFrameBlueIsDetected)
{
  // too_large 필터가 없어서 화면 전체가 파랗다면 그대로 목표로 봄
  cv::Mat image(H, W, CV_8UC3, kBlue);
  EXPECT_TRUE(make_detector().process(image).detected());
}

TEST(Detector, LargestCandidateSelected)
{
  const cv::Mat image = make_frame({{100, 100, 15, kBlue}, {500, 300, 40, kBlue}});
  const TargetResult result = make_detector().process(image);
  EXPECT_NEAR(result.cx, 500, 2);
  EXPECT_NEAR(result.cy, 300, 2);
}

TEST(Detector, LargestWinsEvenFarFromCenter)
{
  // 중앙에 가까운 작은 후보보다 가장자리의 큰 후보를 고름
  const cv::Mat image = make_frame({{330, 250, 15, kBlue}, {580, 60, 40, kBlue}});
  const TargetResult result = make_detector().process(image);
  EXPECT_NEAR(result.cx, 580, 2);
  EXPECT_NEAR(result.cy, 60, 2);
}

TEST(Detector, NoMemoryBetweenFrames)
{
  const TargetDetector detector = make_detector();
  EXPECT_TRUE(detector.process(make_frame({{560, H / 2, 30, kBlue}})).detected());
  const auto xyz = detector.process(make_frame()).to_xyz();  // 이전 좌표 재사용 없음
  ASSERT_TRUE(xyz.has_value());
  EXPECT_EQ(xyz->z, 0.0);
}

TEST(Detector, ResizeKeepsNormalizedError)
{
  DetectorConfig config = base_config();
  config.resize_width = 320;
  const TargetResult result = TargetDetector(config).process(make_frame({{480, 360, 40, kBlue}}));
  ASSERT_TRUE(result.detected());
  EXPECT_EQ(result.frame_width, 320);
  EXPECT_EQ(result.frame_height, 240);
  EXPECT_NEAR(result.ex, 0.5, 0.02);
  EXPECT_NEAR(result.ey, 0.5, 0.02);
}

TEST(Detector, InputImageIsNotModified)
{
  // 블러 등 중간 처리가 호출한 쪽 이미지(원본 저장·카메라 메시지)를 바꾸면 안 됨
  cv::Mat image = make_frame({{W / 2, H / 2, 30, kBlue}});
  cv::Mat noise(image.size(), image.type());
  cv::randu(noise, 0, 40);
  image += noise;   // 블러로 값이 바뀌도록 잡음 추가
  const cv::Mat before = image.clone();

  const Detection detection = make_detector().process_debug(image);

  EXPECT_EQ(cv::norm(image, before, cv::NORM_INF), 0.0);
  ASSERT_TRUE(detection.debug.has_value());
  EXPECT_EQ(cv::norm(detection.debug->frame, before, cv::NORM_INF), 0.0);   // 오버레이 바탕 = 원본
}

// ---------- bbox ----------

TEST(Bbox, CenterOnTiltedCard)
{
  for (const auto style : {BboxStyle::Axis, BboxStyle::Rotated}) {
    for (const float angle : {0.0f, 25.0f, 45.0f, 70.0f}) {
      const cv::Mat image = tilted_card(450, 300, 120, 70, angle);
      const TargetResult result = make_detector(style).process(image);
      ASSERT_TRUE(result.detected()) << "angle=" << angle;
      EXPECT_NEAR(result.cx, 450, 1.5) << "angle=" << angle;
      EXPECT_NEAR(result.cy, 300, 1.5) << "angle=" << angle;
      EXPECT_NEAR(result.ex, (450 - W / 2.0) / (W / 2.0), 0.01);
      EXPECT_NEAR(result.ey, (300 - H / 2.0) / (H / 2.0), 0.01);
      EXPECT_EQ(result.bbox.size(), 4u);
    }
  }
}

TEST(Bbox, AxisIsUprightAndEnclosesTiltedCard)
{
  const cv::Mat image = tilted_card(320, 240, 120, 70, 30);
  const TargetResult result = make_detector(BboxStyle::Axis).process(image);
  ASSERT_EQ(result.bbox.size(), 4u);
  std::vector<float> xs;
  std::vector<float> ys;
  for (const auto & p : result.bbox) {
    xs.push_back(p.x);
    ys.push_back(p.y);
  }
  std::sort(xs.begin(), xs.end());
  std::sort(ys.begin(), ys.end());
  EXPECT_EQ(xs[0], xs[1]);   // 화면 축에 맞춘 사각형
  EXPECT_EQ(xs[2], xs[3]);
  EXPECT_EQ(ys[0], ys[1]);
  EXPECT_EQ(ys[2], ys[3]);
  EXPECT_GT(xs[3] - xs[0], 120);   // 기울어진 카드보다 큰 박스
}

TEST(Bbox, RotatedFollowsCardShape)
{
  const cv::Mat image = tilted_card(320, 240, 120, 70, 30);
  const TargetResult result = make_detector(BboxStyle::Rotated).process(image);
  ASSERT_EQ(result.bbox.size(), 4u);
  std::vector<double> sides;
  for (size_t i = 0; i < 4; ++i) {
    sides.push_back(cv::norm(result.bbox[i] - result.bbox[(i + 1) % 4]));
  }
  std::sort(sides.begin(), sides.end());
  EXPECT_NEAR(sides.front(), 70, 3);
  EXPECT_NEAR(sides.back(), 120, 3);
}

TEST(Bbox, AbsentWhenNotDetected)
{
  EXPECT_TRUE(make_detector().process(make_frame()).bbox.empty());
}

// ---------- 무효 입력 ----------

TEST(Detector, InvalidFramesAreNotPublished)
{
  const std::vector<cv::Mat> bad_frames = {
    cv::Mat(),
    cv::Mat(H, W, CV_8UC1, cv::Scalar(0)),
    cv::Mat(H, W, CV_32FC3, cv::Scalar(0, 0, 0)),
  };
  for (const auto & image : bad_frames) {
    const Detection detection = make_detector().process_debug(image);
    EXPECT_EQ(detection.result.status, TargetStatus::Invalid);
    EXPECT_FALSE(detection.result.to_xyz().has_value());
    EXPECT_FALSE(detection.debug.has_value());
  }
}

TEST(Overlay, RunsForEveryStatus)
{
  const TargetDetector detector = make_detector();
  const std::vector<cv::Mat> images = {
    make_frame({{W / 2, H / 2, 30, kBlue}}),   // 검출
    make_frame({{W / 2, H / 2, 5, kBlue}}),    // 작은 후보만 있음 (미검출)
    make_frame(),                              // 목표 없음
  };
  for (const cv::Mat & image : images) {
    const Detection detection = detector.process_debug(image);
    ASSERT_TRUE(detection.debug.has_value());
    const cv::Mat overlay =
      draw_overlay(detection.debug->frame, detection.result, detection.debug->rejected);
    EXPECT_EQ(overlay.size(), image.size());
    EXPECT_EQ(overlay.type(), image.type());
  }
}

TEST(Error, NormalizeBounds)
{
  EXPECT_EQ(normalize_error(0, 0, W, H), std::make_pair(-1.0, -1.0));
  EXPECT_EQ(normalize_error(W, H, W, H), std::make_pair(1.0, 1.0));
}

// ---------- /target (x, y, z) 판정 ----------

TEST(ClassifyTarget, Rules)
{
  const std::vector<std::tuple<double, double, double, TargetStatus>> cases = {
    {0.2, -0.1, 0.05, TargetStatus::Detected},
    {0.0, 0.0, 0.05, TargetStatus::Detected},    // 중앙 목표: x=y=0 이어도 z>0 이면 검출
    {0.0, 0.0, 0.0, TargetStatus::NoTarget},
    {0.4, 0.2, 0.0, TargetStatus::NoTarget},     // z=0 이면 x·y 값과 무관하게 미검출
    {0.0, 0.0, -0.1, TargetStatus::NoTarget},
    {kNan, 0.0, 0.05, TargetStatus::Invalid},
    {0.0, kNan, 0.05, TargetStatus::Invalid},
    {0.0, 0.0, kNan, TargetStatus::Invalid},
    {kNan, kNan, kNan, TargetStatus::Invalid},
    {kNan, 0.0, 0.0, TargetStatus::Invalid},     // NaN 이 섞이면 z=0 보다 무효가 우선
    {kInf, 0.0, 0.05, TargetStatus::Invalid},
    {1.5, 0.0, 0.05, TargetStatus::Invalid},
    {0.0, 0.0, 1.5, TargetStatus::Invalid},
  };
  for (const auto & [x, y, z, expected] : cases) {
    EXPECT_EQ(classify_target(x, y, z), expected) << x << ", " << y << ", " << z;
    EXPECT_EQ(is_valid_target(x, y, z), expected == TargetStatus::Detected);
  }
}

TEST(ClassifyTarget, ToXyzByStatus)
{
  TargetResult detected;
  detected.status = TargetStatus::Detected;
  detected.ex = 0.25;
  detected.ey = 0.0;
  detected.area_ratio = 0.01;
  const auto xyz = detected.to_xyz();
  ASSERT_TRUE(xyz.has_value());
  EXPECT_EQ(xyz->x, 0.25);
  EXPECT_EQ(xyz->z, 0.01);

  const auto none = TargetResult::no_target(640, 480).to_xyz();
  ASSERT_TRUE(none.has_value());
  EXPECT_EQ(none->z, 0.0);

  EXPECT_FALSE(TargetResult::invalid(0, 0, "x").to_xyz().has_value());
}

// ---------- 설정 검증 ----------

const char * const kGoodRanges = "hsv_ranges: [{lower: [100, 150, 70], upper: [130, 255, 255]}]\n";

TEST(Config, ReadsDetectorSectionAndNullResize)
{
  const DetectorConfig config = load_config_from_string(
    "detector:\n"
    "  hsv_ranges: [{lower: [100, 150, 70], upper: [130, 255, 255]}]\n"
    "  bbox_style: axis\n"
    "  resize_width: null\n");
  EXPECT_EQ(config.bbox_style, BboxStyle::Axis);
  EXPECT_EQ(config.resize_width, 0);
  EXPECT_EQ(config.hsv_ranges[0].lower[1], 150);
}

TEST(Config, BadValuesRejected)
{
  const std::vector<std::string> bad_configs = {
    "hsv_ranges: []\n",
    "hsv_ranges: [{lower: [0, 0, 0], upper: [200, 255, 255]}]\n",
    "hsv_ranges: [{lower: [20, 0, 0], upper: [10, 255, 255]}]\n",
    "hsv_ranges: [{lower: [0, 0], upper: [10, 255, 255]}]\n",
    std::string(kGoodRanges) + "blur_ksize: 4\n",
    std::string(kGoodRanges) + "bbox_style: quad\n",
    std::string(kGoodRanges) + "resize_width: 8\n",
    std::string(kGoodRanges) + "typo_key: 1\n",
    std::string(kGoodRanges) + "max_area_ratio: 0.6\n",    // 제거한 설정
    std::string(kGoodRanges) + "selection: largest\n",     // 제거한 설정 (고정: 가장 큰 후보)
    std::string(kGoodRanges) + "open_iterations: 1\n",     // 제거한 설정 (고정: 1회)
    std::string(kGoodRanges) + "close_iterations: 2\n",    // 제거한 설정 (고정: 2회)
  };
  for (const auto & text : bad_configs) {
    EXPECT_THROW(load_config_from_string(text), std::invalid_argument) << text;
  }
}

TEST(Config, DescribeShowsAppliedValues)
{
  const DetectorConfig config = load_config_from_string(
    "hsv_ranges: [{lower: [98, 120, 40], upper: [130, 255, 255]}]\n"
    "min_area_px: 300\n"
    "bbox_style: rotated\n"
    "resize_width: null\n");
  EXPECT_EQ(
    describe(config),
    "HSV [98,120,40]~[130,255,255], blur 5, morph 5 (open 1, close 2), min_area 300px, "
    "bbox rotated, resize 원본");

  DetectorConfig resized = config;
  resized.resize_width = 320;
  resized.hsv_ranges.push_back({{0, 0, 0}, {10, 255, 255}});
  EXPECT_NE(describe(resized).find("| [0,0,0]~[10,255,255]"), std::string::npos);
  EXPECT_NE(describe(resized).find("resize 320"), std::string::npos);
}

TEST(Config, MissingFileRejected)
{
  EXPECT_THROW(load_config("/nonexistent/perception.yaml"), std::invalid_argument);
}

}  // namespace
}  // namespace perception
