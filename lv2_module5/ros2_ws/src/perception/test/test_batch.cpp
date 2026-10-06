// 일괄 검출 시험: 이미지·영상 입력, 영상 프레임 간격, 결과 파일 이름·CSV

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "perception/batch.hpp"
#include "perception/detector.hpp"

namespace fs = std::filesystem;

namespace perception
{
namespace
{

constexpr int W = 640;
constexpr int H = 480;

cv::Mat make_frame(bool with_target)
{
  cv::Mat image(H, W, CV_8UC3, cv::Scalar(40, 40, 40));
  if (with_target) {
    cv::rectangle(
      image, cv::Point(W / 2 - 40, H / 2 - 40), cv::Point(W / 2 + 40, H / 2 + 40),
      cv::Scalar(255, 0, 0), cv::FILLED);
  }
  return image;
}

void write_video(const fs::path & path, const std::vector<cv::Mat> & frames)
{
  const int codec = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
  cv::VideoWriter writer(path.string(), codec, 30, cv::Size(W, H));
  ASSERT_TRUE(writer.isOpened());
  for (const auto & frame : frames) {
    writer.write(frame);
  }
}

// CSV 를 줄 단위·칸 단위로 읽기 (시험 데이터에는 따옴표 없음)
std::vector<std::vector<std::string>> read_csv(const fs::path & path)
{
  std::vector<std::vector<std::string>> rows;
  std::ifstream file(path);
  std::string line;
  std::getline(file, line);  // 머리줄
  while (std::getline(file, line)) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
      fields.push_back(field);
    }
    if (!line.empty() && line.back() == ',') {
      fields.emplace_back();
    }
    rows.push_back(fields);
  }
  return rows;
}

// CSV 앞쪽 열 순서: file, frame, status
constexpr size_t kFile = 0;
constexpr size_t kFrame = 1;
constexpr size_t kStatus = 2;

class BatchTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    const auto * unit_test = ::testing::UnitTest::GetInstance();
    dir_ = fs::temp_directory_path() /
      ("perception_batch_" + std::to_string(unit_test->random_seed()) + "_" +
      unit_test->current_test_info()->name());
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }

  void TearDown() override {fs::remove_all(dir_);}

  fs::path dir_;
  TargetDetector detector_{load_config(PERCEPTION_CONFIG_PATH)};
  std::ostringstream progress_;
};

TEST_F(BatchTest, ReadsEveryNthVideoFrame)
{
  write_video(
    dir_ / "clip.avi",
    {make_frame(true), make_frame(false), make_frame(true), make_frame(false), make_frame(true)});

  const fs::path csv =
    run_batch(detector_, {(dir_ / "clip.avi").string()}, dir_ / "results", "video", 2, progress_);

  const auto rows = read_csv(csv);
  ASSERT_EQ(rows.size(), 3u);
  const std::vector<std::string> expected_frames = {"0", "2", "4"};
  for (size_t i = 0; i < rows.size(); ++i) {
    EXPECT_EQ(rows[i][kFile], "clip.avi");
    EXPECT_EQ(rows[i][kFrame], expected_frames[i]);
    EXPECT_EQ(rows[i][kStatus], "detected");
  }
  const fs::path images = dir_ / "results/images";
  for (const std::string kind : {"original", "mask", "overlay"}) {
    EXPECT_TRUE(fs::exists(images / ("video_clip_f000002_" + kind + ".png"))) << kind;
  }
  EXPECT_FALSE(fs::exists(images / "video_clip_f000001_original.png"));  // 건너뛴 프레임
}

TEST_F(BatchTest, OriginalIsSavedUnchanged)
{
  cv::Mat image = make_frame(true);
  cv::Mat noise(image.size(), image.type());
  cv::randu(noise, 0, 40);
  image += noise;
  cv::imwrite((dir_ / "shot.png").string(), image);

  run_batch(detector_, {(dir_ / "shot.png").string()}, dir_ / "results", "orig", 1, progress_);

  const cv::Mat saved = cv::imread((dir_ / "results/images/orig_shot_original.png").string());
  ASSERT_FALSE(saved.empty());
  EXPECT_EQ(cv::norm(saved, image, cv::NORM_INF), 0.0);
}

TEST_F(BatchTest, FolderMixesImagesAndVideos)
{
  cv::imwrite((dir_ / "a.png").string(), make_frame(false));
  write_video(dir_ / "b.avi", {make_frame(false), make_frame(false)});

  const auto rows =
    read_csv(run_batch(detector_, {dir_.string()}, dir_ / "results", "mixed", 1, progress_));

  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0][kFile], "a.png");
  EXPECT_EQ(rows[0][kFrame], "");
  EXPECT_EQ(rows[1][kFile], "b.avi");
  EXPECT_EQ(rows[1][kFrame], "0");
  EXPECT_EQ(rows[2][kFrame], "1");
  for (const auto & row : rows) {
    EXPECT_EQ(row[kStatus], "no_target");
  }
}

TEST_F(BatchTest, UnreadableImageIsRecordedAsInvalid)
{
  std::ofstream(dir_ / "broken.png") << "not an image";

  const fs::path csv =
    run_batch(detector_, {(dir_ / "broken.png").string()}, dir_ / "results", "bad", 1, progress_);
  const auto rows = read_csv(csv);

  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0][kStatus], "invalid");
  EXPECT_TRUE(fs::is_empty(dir_ / "results/images"));   // 읽을 수 없는 입력은 이미지 없음
}

TEST(InputFrame, NameUsesFrameIndexForVideo)
{
  EXPECT_EQ((InputFrame{"dir/shot.png", std::nullopt, {}}).name(), "shot");
  EXPECT_EQ((InputFrame{"dir/clip.mp4", 123, {}}).name(), "clip_f000123");
}

}  // namespace
}  // namespace perception
