// 검출 결과 오버레이 (ROS 의존 없음)
//   목표       : bbox + bbox 중심 + 영상 중심까지 선, 상단에 ex·ey·면적비
//   제외 후보  : 회색 컨투어 + 제외 사유
// perception_node 의 /target/debug_image 와 batch_detect 의 *_overlay.png 가 같은 그림 사용

#ifndef PERCEPTION__OVERLAY_HPP_
#define PERCEPTION__OVERLAY_HPP_

#include <vector>

#include <opencv2/core.hpp>

#include "perception/detector.hpp"

namespace perception
{

// 검출 결과를 그린 새 이미지 반환, 원본 frame 은 바꾸지 않음
cv::Mat draw_overlay(
  const cv::Mat & frame, const TargetResult & result, const std::vector<Candidate> & rejected = {});

}  // namespace perception

#endif  // PERCEPTION__OVERLAY_HPP_
