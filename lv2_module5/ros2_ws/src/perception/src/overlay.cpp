#include "perception/overlay.hpp"

#include <cmath>
#include <cstdio>
#include <string>

#include <opencv2/imgproc.hpp>

namespace perception
{

namespace
{

// BGR 색
const cv::Scalar kGreen(0, 200, 0);
const cv::Scalar kOrange(0, 200, 255);
const cv::Scalar kRed(0, 0, 255);
const cv::Scalar kGray(160, 160, 160);
const cv::Scalar kWhite(255, 255, 255);
const cv::Scalar kBlack(0, 0, 0);

constexpr int kFont = cv::FONT_HERSHEY_SIMPLEX;

cv::Scalar status_color(TargetStatus status)
{
  switch (status) {
    case TargetStatus::Detected:
      return kGreen;
    case TargetStatus::NoTarget:
      return kOrange;
    case TargetStatus::Invalid:
      return kRed;
  }
  return kRed;
}

// 검은 배경 박스 위에 글자 쓰기
void draw_label(
  cv::Mat & canvas, const std::string & text, cv::Point origin, const cv::Scalar & color,
  double scale = 0.6)
{
  int baseline = 0;
  const cv::Size size = cv::getTextSize(text, kFont, scale, 1, &baseline);
  cv::rectangle(
    canvas,
    cv::Point(origin.x - 4, origin.y - size.height - 6),
    cv::Point(origin.x + size.width + 4, origin.y + baseline + 2),
    kBlack, cv::FILLED);
  cv::putText(canvas, text, origin, kFont, scale, color, 1, cv::LINE_AA);
}

cv::Point to_pixel(double x, double y)
{
  return {static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))};
}

void draw_target(
  cv::Mat & canvas, const TargetResult & result, cv::Point image_center, const cv::Scalar & color)
{
  if (!result.bbox.empty()) {
    std::vector<cv::Point> corners;
    for (const auto & corner : result.bbox) {
      corners.push_back(to_pixel(corner.x, corner.y));
    }
    cv::polylines(canvas, corners, true, color, 2);
  }

  const cv::Point target_center = to_pixel(result.cx, result.cy);
  cv::circle(canvas, target_center, 6, color, cv::FILLED);
  cv::line(canvas, image_center, target_center, color, 1);
}

}  // namespace

cv::Mat draw_overlay(
  const cv::Mat & frame, const TargetResult & result, const std::vector<Candidate> & rejected)
{
  cv::Mat canvas = frame.clone();
  const cv::Point image_center(canvas.cols / 2, canvas.rows / 2);

  for (const auto & candidate : rejected) {
    cv::drawContours(canvas, std::vector<std::vector<cv::Point>>{candidate.contour}, -1, kGray, 1);
    draw_label(canvas, candidate.reject, to_pixel(candidate.cx, candidate.cy), kGray, 0.45);
  }

  cv::drawMarker(canvas, image_center, kWhite, cv::MARKER_CROSS, 20, 2);

  const cv::Scalar color = status_color(result.status);
  std::string text;
  if (result.detected()) {
    draw_target(canvas, result, image_center, color);
    char buffer[96];
    std::snprintf(
      buffer, sizeof(buffer), "ex=%+.3f ey=%+.3f area=%.4f",
      result.ex, result.ey, result.area_ratio);
    text = buffer;
  } else {
    text = to_string(result.status) + ": " + result.reason;
  }

  draw_label(canvas, text, cv::Point(10, 24), color);
  return canvas;
}

}  // namespace perception
