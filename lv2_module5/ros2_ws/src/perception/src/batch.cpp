#include "perception/batch.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "perception/overlay.hpp"

namespace fs = std::filesystem;

namespace perception
{

namespace
{

const std::set<std::string> kImageSuffixes = {".png", ".jpg", ".jpeg", ".bmp"};
const std::set<std::string> kVideoSuffixes = {".mp4", ".avi", ".mov", ".mkv"};

const char * const kCsvHeader =
  "file,frame,status,reason,width,height,cx,cy,x,y,z,rejected,process_ms";

std::string lower_suffix(const fs::path & path)
{
  std::string suffix = path.extension().string();
  std::transform(
    suffix.begin(), suffix.end(), suffix.begin(),
    [](unsigned char c) {return std::tolower(c);});
  return suffix;
}

// printf 형식으로 숫자 출력, NaN 은 "nan"
std::string format_number(const char * format, double value)
{
  if (std::isnan(value)) {
    return "nan";
  }
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), format, value);
  return buffer;
}

// CSV 칸에 쉼표·따옴표가 있으면 따옴표로 감쌈
std::string csv_field(const std::string & text)
{
  if (text.find_first_of(",\"\n") == std::string::npos) {
    return text;
  }
  std::string quoted = "\"";
  for (char c : text) {
    quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
  }
  return quoted + "\"";
}

std::string csv_row(const InputFrame & item, const Detection & detection, double elapsed_ms)
{
  const TargetResult & result = detection.result;
  const auto xyz = result.to_xyz();

  std::string rejected;
  if (detection.debug) {
    for (const auto & candidate : detection.debug->rejected) {
      rejected += (rejected.empty() ? "" : ";") + candidate.reject;
    }
  }

  std::vector<std::string> fields = {
    csv_field(item.file.filename().string()),
    item.frame_index ? std::to_string(*item.frame_index) : "",
    to_string(result.status),
    csv_field(result.reason),
    std::to_string(result.frame_width),
    std::to_string(result.frame_height),
    format_number("%.2f", result.cx),
    format_number("%.2f", result.cy),
    xyz ? format_number("%.4f", xyz->x) : "",
    xyz ? format_number("%.4f", xyz->y) : "",
    xyz ? format_number("%.5f", xyz->z) : "",
    rejected,
    format_number("%.2f", elapsed_ms),
  };

  std::string row;
  for (size_t i = 0; i < fields.size(); ++i) {
    row += (i == 0 ? "" : ",") + fields[i];
  }
  return row;
}

std::string describe_xyz(const TargetResult & result)
{
  const auto xyz = result.to_xyz();
  if (!xyz) {
    return "(발행 안 함)";
  }
  char buffer[96];
  std::snprintf(buffer, sizeof(buffer), "(%.4f, %.4f, %.5f)", xyz->x, xyz->y, xyz->z);
  return buffer;
}

void read_video_frames(
  const fs::path & path, int every_n,
  const std::function<void(const InputFrame &)> & handle_frame, std::ostream & warnings)
{
  cv::VideoCapture capture(path.string());
  if (!capture.isOpened()) {
    warnings << path.filename().string() << ": 영상을 열 수 없어 건너뜀" << std::endl;
    return;
  }

  cv::Mat image;
  for (int frame_index = 0; capture.read(image); ++frame_index) {
    if (frame_index % every_n == 0) {
      handle_frame(InputFrame{path, frame_index, image});
    }
  }
}

}  // namespace

std::string InputFrame::name() const
{
  if (!frame_index) {
    return file.stem().string();
  }
  char suffix[16];
  std::snprintf(suffix, sizeof(suffix), "_f%06d", *frame_index);
  return file.stem().string() + suffix;
}

bool is_image_file(const fs::path & path)
{
  return kImageSuffixes.count(lower_suffix(path)) > 0;
}

bool is_video_file(const fs::path & path)
{
  return kVideoSuffixes.count(lower_suffix(path)) > 0;
}

std::vector<fs::path> find_input_files(const std::vector<std::string> & inputs)
{
  std::vector<fs::path> files;
  for (const auto & input : inputs) {
    const fs::path item(input);
    if (!fs::is_directory(item)) {
      files.push_back(item);
      continue;
    }
    std::vector<fs::path> folder_files;
    for (const auto & entry : fs::directory_iterator(item)) {
      if (entry.is_regular_file() && (is_image_file(entry.path()) || is_video_file(entry.path()))) {
        folder_files.push_back(entry.path());
      }
    }
    std::sort(folder_files.begin(), folder_files.end());
    files.insert(files.end(), folder_files.begin(), folder_files.end());
  }
  return files;
}

void for_each_input_frame(
  const std::vector<std::string> & inputs, int every_n,
  const std::function<void(const InputFrame &)> & handle_frame, std::ostream & warnings)
{
  if (every_n < 1) {
    throw std::invalid_argument("every_n 은 1 이상이어야 합니다.");
  }
  for (const auto & path : find_input_files(inputs)) {
    if (is_video_file(path)) {
      read_video_frames(path, every_n, handle_frame, warnings);
    } else {
      handle_frame(InputFrame{path, std::nullopt, cv::imread(path.string(), cv::IMREAD_COLOR)});
    }
  }
}

fs::path run_batch(
  const TargetDetector & detector, const std::vector<std::string> & inputs,
  const fs::path & results_dir, const std::string & scene, int every_n, std::ostream & progress)
{
  const fs::path image_dir = results_dir / "images";
  const fs::path log_dir = results_dir / "logs";
  fs::create_directories(image_dir);
  fs::create_directories(log_dir);
  const fs::path csv_path = log_dir / ("perception_" + scene + ".csv");

  std::ofstream csv(csv_path);
  if (!csv) {
    throw std::runtime_error("CSV 파일을 만들 수 없습니다: " + csv_path.string());
  }
  csv << kCsvHeader << "\n";

  const auto handle_frame = [&](const InputFrame & item) {
      const auto start = std::chrono::steady_clock::now();
      const Detection detection = detector.process_debug(item.image);
      const double elapsed_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

      csv << csv_row(item, detection, elapsed_ms) << "\n";

      if (detection.debug) {
        const std::string name = scene + "_" + item.name();
        const cv::Mat overlay =
          draw_overlay(detection.debug->frame, detection.result, detection.debug->rejected);
        cv::imwrite((image_dir / (name + "_original.png")).string(), item.image);
        cv::imwrite((image_dir / (name + "_mask.png")).string(), detection.debug->mask);
        cv::imwrite((image_dir / (name + "_overlay.png")).string(), overlay);
      }

      progress << item.name() << ": " << to_string(detection.result.status) << " "
               << describe_xyz(detection.result) << std::endl;
    };

  for_each_input_frame(inputs, every_n, handle_frame, progress);
  return csv_path;
}

}  // namespace perception
