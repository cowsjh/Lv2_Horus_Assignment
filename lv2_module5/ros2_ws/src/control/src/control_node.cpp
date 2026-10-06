#include <rclcpp/rclcpp.hpp>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

class ControlNode : public rclcpp::Node
{
public:
  ControlNode()
  : Node("control_node")
  {
    port_ = this->declare_parameter<std::string>(
      "port",
      "/dev/opencr"
    );

    baudrate_ = this->declare_parameter<int>(
      "baudrate",
      115200
    );

    if (!openSerial())
    {
      RCLCPP_FATAL(
        this->get_logger(),
        "Failed to open serial port: %s",
        port_.c_str()
      );

      throw std::runtime_error("serial open failed");
    }

    command_sub_ =
      this->create_subscription<std_msgs::msg::Float64MultiArray>(
        "/motor/command",
        10,
        std::bind(
          &ControlNode::commandCallback,
          this,
          std::placeholders::_1
        )
      );

    state_pub_ =
      this->create_publisher<geometry_msgs::msg::PointStamped>(
        "/motor/state",
        10
      );

    serial_timer_ =
      this->create_wall_timer(
        std::chrono::milliseconds(10),
        std::bind(
          &ControlNode::pollSerial,
          this
        )
      );

    RCLCPP_INFO(
      this->get_logger(),
      "Control node started."
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Serial port: %s",
      port_.c_str()
    );

    RCLCPP_INFO(
      this->get_logger(),
      "Baudrate: %d",
      baudrate_
    );
  }

  ~ControlNode()
  {
    if (serial_fd_ >= 0)
    {
      close(serial_fd_);
      serial_fd_ = -1;
    }
  }

private:
  rclcpp::Subscription<
    std_msgs::msg::Float64MultiArray
  >::SharedPtr command_sub_;

  rclcpp::Publisher<
    geometry_msgs::msg::PointStamped
  >::SharedPtr state_pub_;

  rclcpp::TimerBase::SharedPtr serial_timer_;

  int serial_fd_ = -1;

  std::string port_;
  int baudrate_ = 115200;

  std::string rx_buffer_;

private:
  bool openSerial()
  {
    serial_fd_ = open(
      port_.c_str(),
      O_RDWR | O_NOCTTY | O_NONBLOCK
    );

    if (serial_fd_ < 0)
    {
      RCLCPP_ERROR(
        this->get_logger(),
        "open(%s) failed: errno=%d (%s)",
        port_.c_str(),
        errno,
        std::strerror(errno)
      );

      return false;
    }

    struct termios tty {};

    if (tcgetattr(serial_fd_, &tty) != 0)
    {
      RCLCPP_ERROR(
        this->get_logger(),
        "tcgetattr() failed: errno=%d (%s)",
        errno,
        std::strerror(errno)
      );

      close(serial_fd_);
      serial_fd_ = -1;
      return false;
    }

    cfmakeraw(&tty);

    speed_t speed;

    switch (baudrate_)
    {
      case 115200:
        speed = B115200;
        break;

      default:
        RCLCPP_ERROR(
          this->get_logger(),
          "Unsupported baudrate: %d",
          baudrate_
        );

        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= CLOCAL;
    tty.c_cflag |= CREAD;

    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0)
    {
      RCLCPP_ERROR(
        this->get_logger(),
        "tcsetattr() failed: errno=%d (%s)",
        errno,
        std::strerror(errno)
      );

      close(serial_fd_);
      serial_fd_ = -1;
      return false;
    }

    RCLCPP_INFO(
      this->get_logger(),
      "Serial connected."
    );

    return true;
  }

  void commandCallback(
    const std_msgs::msg::Float64MultiArray::SharedPtr msg
  )
  {
    if (msg->data.size() < 2)
    {
      RCLCPP_WARN(
        this->get_logger(),
        "Need [yaw,pitch]"
      );

      return;
    }

    const int yaw =
      static_cast<int>(msg->data[0]);

    const int pitch =
      static_cast<int>(msg->data[1]);

    char line[64];

    std::snprintf(
      line,
      sizeof(line),
      "G,%d,%d\n",
      yaw,
      pitch
    );

    if (serial_fd_ >= 0)
    {
      const ssize_t written =
        write(
          serial_fd_,
          line,
          std::strlen(line)
        );

      if (written < 0)
      {
        RCLCPP_WARN(
          this->get_logger(),
          "Serial write failed: errno=%d (%s)",
          errno,
          std::strerror(errno)
        );

        return;
      }
    }

    RCLCPP_INFO(
      this->get_logger(),
      "TX: %s",
      line
    );
  }

  void pollSerial()
  {
    if (serial_fd_ < 0)
    {
      return;
    }

    char buf[256];

    const int n =
      read(
        serial_fd_,
        buf,
        sizeof(buf) - 1
      );

    if (n < 0)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        RCLCPP_WARN(
          this->get_logger(),
          "Serial read failed: errno=%d (%s)",
          errno,
          std::strerror(errno)
        );
      }

      return;
    }

    if (n == 0)
    {
      return;
    }

    buf[n] = '\0';

    RCLCPP_INFO(
      this->get_logger(),
      "RAW RX [%d bytes]: %s",
      n,
      buf
    );

    rx_buffer_ += buf;

    size_t pos;

    while (
      (pos = rx_buffer_.find('\n'))
      != std::string::npos
    )
    {
      std::string line =
        rx_buffer_.substr(0, pos);

      rx_buffer_.erase(
        0,
        pos + 1
      );

      if (!line.empty() && line.back() == '\r')
      {
        line.pop_back();
      }

      parseStatus(line);
    }
  }

  void parseStatus(
    const std::string & line
  )
  {
    int yaw;
    int pitch;
    int flags;

    if (
      std::sscanf(
        line.c_str(),
        "S,%d,%d,%d",
        &yaw,
        &pitch,
        &flags
      ) != 3
    )
    {
      RCLCPP_WARN(
        this->get_logger(),
        "Invalid status: [%s]",
        line.c_str()
      );

      return;
    }

    geometry_msgs::msg::PointStamped msg;

    msg.header.stamp =
      this->get_clock()->now();

    msg.header.frame_id =
      "motor";

    msg.point.x =
      static_cast<double>(yaw);

    msg.point.y =
      static_cast<double>(pitch);

    msg.point.z =
      static_cast<double>(flags);

    state_pub_->publish(msg);

    RCLCPP_INFO(
      this->get_logger(),
      "RX: yaw=%d pitch=%d flags=%d",
      yaw,
      pitch,
      flags
    );
  }
};

int main(
  int argc,
  char ** argv
)
{
  rclcpp::init(argc, argv);

  rclcpp::spin(
    std::make_shared<ControlNode>()
  );

  rclcpp::shutdown();

  return 0;
}