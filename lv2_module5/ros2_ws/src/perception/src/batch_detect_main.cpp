// batch_detect: 저장 이미지·영상 일괄 검출 명령
//
// 사용 (lv2_module5/ 에서):
//   ros2 run perception batch_detect --config config/perception.yaml --results results
//       --scene normal [--every 5] <이미지·영상 파일 또는 폴더, 여러 개 가능>
//   (실제로는 한 줄로 입력)
//
// 입력: 이미지(.png .jpg .jpeg .bmp), 영상(.mp4 .avi .mov .mkv), 또는 이 파일들이 든 폴더
// --every n: 영상에서 n 프레임마다 한 장만 검출 (기본 1 = 전부), 이미지에는 적용 안 함

#include <iostream>
#include <string>
#include <vector>

#include "perception/batch.hpp"
#include "perception/detector.hpp"

namespace
{

const char * const kUsage =
  "사용법: batch_detect --config <perception.yaml> --results <results 폴더> --scene <장면 이름>\n"
  "                     [--every <n>] <이미지·영상 파일 또는 폴더>...\n"
  "  장면 이름 예: normal, none, covered, eval\n";

struct Arguments
{
  std::string config;
  std::string results;
  std::string scene;
  int every = 1;
  std::vector<std::string> inputs;
};

// 인자가 잘못되면 오류 문구를 담아 std::invalid_argument
Arguments parse_arguments(int argc, char ** argv)
{
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto next_value = [&]() -> std::string {
        if (i + 1 >= argc) {
          throw std::invalid_argument(arg + " 뒤에 값이 필요합니다.");
        }
        return argv[++i];
      };

    if (arg == "--config") {
      args.config = next_value();
    } else if (arg == "--results") {
      args.results = next_value();
    } else if (arg == "--scene") {
      args.scene = next_value();
    } else if (arg == "--every") {
      args.every = std::stoi(next_value());
    } else if (arg.rfind("--", 0) == 0) {
      throw std::invalid_argument("알 수 없는 옵션: " + arg);
    } else {
      args.inputs.push_back(arg);
    }
  }

  if (args.config.empty() || args.results.empty() || args.scene.empty() || args.inputs.empty()) {
    throw std::invalid_argument("--config, --results, --scene 과 입력 파일·폴더가 필요합니다.");
  }
  if (args.every < 1) {
    throw std::invalid_argument("--every 는 1 이상이어야 합니다.");
  }
  return args;
}

}  // namespace

int main(int argc, char ** argv)
{
  Arguments args;
  try {
    args = parse_arguments(argc, argv);
  } catch (const std::exception & error) {
    std::cerr << error.what() << "\n" << kUsage;
    return 2;
  }

  try {
    const perception::TargetDetector detector(perception::load_config(args.config));
    const auto csv_path =
      perception::run_batch(detector, args.inputs, args.results, args.scene, args.every, std::cout);
    std::cout << "CSV: " << csv_path.string() << std::endl;
  } catch (const std::exception & error) {
    std::cerr << "오류: " << error.what() << std::endl;
    return 1;
  }
  return 0;
}
