import argparse
import threading
import time
import os

from networking.inference_server import start_server
from inference.online_detector import OnlineDetector


def main():
    run_mode = args.mode
    video_input = args.src
    output_path = "../output/"
    if not os.path.exists(output_path):
        os.mkdir(output_path)
    if run_mode == "video_input":
        video_input_tokens = video_input.split("\\")[-1].split(".")
        # For video input, output_path default is '../output'
        output_path = output_path + video_input_tokens[0]

    else:   # live_streaming
        # Naming convention for output path in live streaming mode
        output_path = output_path + f'live_{int(time.time())}'

    # Run detector
    print("Run online detector")
    detector = OnlineDetector(run_mode, video_input, output_path)

    if run_mode == "live_streaming":
        # Start Python server
        server = start_server(output_path, detector)
        server_job = threading.Thread(target=server.run_server, args=())
        server_job.start()

    print("run detector")
    detector.run()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", default="", help="Path to an input video file. Leave blank to use live stream.")
    parser.add_argument("--mode", default="live_streaming", help="live_streaming, video_input")
    args = parser.parse_args()
    main()
