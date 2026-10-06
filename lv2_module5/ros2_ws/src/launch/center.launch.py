from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():

    center_node = Node(
        package='center',
        executable='center_node',
        name='center_node',
        output='screen',
    )

    control_node = Node(
        package='control',
        executable='control_node',
        name='control_node',
        output='screen',
    )

    return LaunchDescription([
        center_node,
        control_node,
    ])