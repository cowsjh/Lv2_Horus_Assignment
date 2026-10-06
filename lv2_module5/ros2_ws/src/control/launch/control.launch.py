from launch import LaunchDescription
from launch_ros.actions import Node
import os


def generate_launch_description():

    config_file = os.path.join(
        os.path.dirname(__file__),
        "../../../../config/control.yaml"
    )

    config_file = os.path.abspath(config_file)

    return LaunchDescription([
        Node(
            package='control',
            executable='control_node',
            name='control_node',
            output='screen',
            parameters=[config_file]
        )
    ])