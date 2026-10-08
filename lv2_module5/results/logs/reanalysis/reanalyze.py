#!/usr/bin/env python3
"""Recompute tracking metrics from recorded bags (result re-analysis).

Reads /target and /tracking_status directly from bag files with rosbag2_py.
Nothing is played back or published, so no motor command is sent.

Definitions (same as report.md "P 추적"):
  detected  : /target with z > 0
  valid     : detected and the latest /tracking_status is TRACKING
  excluded  : target_frames - valid
  rmse_ex   : sqrt(mean(x^2)) over valid frames
  lost_transitions : times the state becomes LOST (initial LOST counts once);
                     /tracking_status is published periodically, so repeated
                     LOST messages are not counted again

Usage:
  python3 reanalyze.py <bag_dir> [<bag_dir> ...] > reanalysis.csv
"""
import math
import os
import sys

import rosbag2_py
from geometry_msgs.msg import PointStamped
from rclpy.serialization import deserialize_message
from std_msgs.msg import String


def state_of(text):
    # Accept both "TRACKING" and "LOST -> TRACKING" style messages.
    return text.split('->')[-1].strip().split()[0]


def analyze(bag_dir):
    reader = rosbag2_py.SequentialReader()
    reader.open(rosbag2_py.StorageOptions(uri=bag_dir, storage_id='mcap'),
                rosbag2_py.ConverterOptions('', ''))
    reader.set_filter(rosbag2_py.StorageFilter(topics=['/target', '/tracking_status']))

    state = None
    frames = detected = valid = lost = 0
    sq_sum = 0.0
    t_first = t_last = None
    while reader.has_next():
        topic, data, t = reader.read_next()
        if topic == '/tracking_status':
            new_state = state_of(deserialize_message(data, String).data)
            if new_state == 'LOST' and state != 'LOST':
                lost += 1
            state = new_state
            continue
        msg = deserialize_message(data, PointStamped)
        frames += 1
        t_first = t if t_first is None else t_first
        t_last = t
        if msg.point.z > 0:
            detected += 1
            if state == 'TRACKING':
                valid += 1
                sq_sum += msg.point.x ** 2

    span = (t_last - t_first) / 1e9 if frames else 0.0
    ratio = 100.0 * valid / frames if frames else 0.0
    rmse = math.sqrt(sq_sum / valid) if valid else float('nan')
    return [os.path.basename(os.path.normpath(bag_dir)), frames, f'{span:.2f}',
            detected, valid, frames - valid, f'{ratio:.1f}', f'{rmse:.4f}', lost]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    print('bag,target_frames,target_span_s,detected,valid,excluded,'
          'valid_tracking_ratio_pct,rmse_ex,lost_transitions')
    for bag_dir in sys.argv[1:]:
        print(','.join(str(v) for v in analyze(bag_dir)))


if __name__ == '__main__':
    main()
