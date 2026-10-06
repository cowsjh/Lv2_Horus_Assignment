// 저장 이미지·영상 일괄 검출 (ROS 의존 없음)
//
// 결과는 발제 저장소 구조에 맞춰 저장
//   이미지 : <results>/images/<scene>_<이름>_original.png  입력 원본 (리사이즈 전)
//            <results>/images/<scene>_<이름>_mask.png      정리된 마스크
//            <results>/images/<scene>_<이름>_overlay.png   검출 결과 오버레이
//            <이름> = 이미지 파일 이름(확장자 제외), 영상 프레임은 <영상 이름>_f<프레임 번호 6자리>
//            읽을 수 없는 입력은 이미지 없이 CSV 에 invalid 로만 기록
//   CSV    : <results>/logs/perception_<scene>.csv
//
// 용도: 정상·대상 없음·가림 3장면 결과물(문제 1), 평가 프레임 검출률 판정(평가 8)

#ifndef PERCEPTION__BATCH_HPP_
#define PERCEPTION__BATCH_HPP_

#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "perception/detector.hpp"

namespace perception
{

// 일괄 검출에 넣을 프레임 한 장
struct InputFrame
{
  std::filesystem::path file;       // 이미지 또는 영상 파일
  std::optional<int> frame_index;   // 영상의 프레임 번호, 이미지면 값 없음
  cv::Mat image;                    // 읽기 실패면 빈 Mat (검출기가 INVALID 로 기록)

  // 결과 파일 이름에 쓸 이름, 영상 프레임은 "<영상 이름>_f000123"
  std::string name() const;
};

bool is_image_file(const std::filesystem::path & path);
bool is_video_file(const std::filesystem::path & path);

// 입력이 폴더면 안의 이미지·영상 파일을 이름순으로, 파일이면 그 파일 그대로 반환
std::vector<std::filesystem::path> find_input_files(const std::vector<std::string> & inputs);

// 이미지는 한 장씩, 영상은 every_n 프레임마다(0, n, 2n, ...) handle_frame 호출
// 열 수 없는 영상은 warnings 에 알리고 건너뜀
void for_each_input_frame(
  const std::vector<std::string> & inputs, int every_n,
  const std::function<void(const InputFrame &)> & handle_frame, std::ostream & warnings);

// 프레임마다 검출하고 원본·마스크·오버레이·CSV 저장, CSV 경로 반환
// progress 에는 프레임마다 결과 한 줄씩 출력
std::filesystem::path run_batch(
  const TargetDetector & detector, const std::vector<std::string> & inputs,
  const std::filesystem::path & results_dir, const std::string & scene, int every_n,
  std::ostream & progress);

}  // namespace perception

#endif  // PERCEPTION__BATCH_HPP_
