import socket

from turbojpeg import TurboJPEG
# pip install PyTurboJPEG

import cv2
import struct
import numpy as np

import time

import threading
from threading import Thread

from inference.online_detector import OnlineDetector
# from inference_augmentation_controller import Inferencer_Controller
# from inferencer_model import Inferencer_Model

IP = 'localhost'
# IP = '0.0.0.0'
PORT = 9999
BUFFER_MODE = ['REPEAT', 'WAIT'][1]
SYSTEM = 'local'

DEBUG = False

jpeg = TurboJPEG()


class InferenceServer(Thread):
    def __init__(self, client_socket, address, inferencer, debug=True, num_images=6, result_type=0, enable_external_view=False,
                 path=""):
        super(InferenceServer, self).__init__()
        self.socket = client_socket
        self.address = address
        self.debug = debug
        print('accepted connection from {}:{}'.format(address[0], address[1]))
        #
        # self.model = Inferencer_Model(num_images, inferencer_type)
        # self.model.debug = debug
        # self.model.result_type = result_type
        # self.model.enable_external_view = enable_external_view
        #
        # self.controller = Inferencer_Controller(self.model)

        # Run detector
        # self.video_input = src
        self.output_path = path
        self.inferencer = inferencer

        self.running = True

    def run_server(self):
        print("run server")
        try:
            while self.running:
                print("wait for request")
                request = self.recv_one_message()
                print("received: ", request)
                if request is None:
                    self.running = False
                    break
                continueThread = self.handle_request(request)

                if not continueThread:
                    break
            # self.controller.stop()
            print('thread ended')
        except Exception as e:
            print(e)

    def send_one_message(self, data):
        length = len(data)
        self.socket.sendall(struct.pack('i', length))
        self.socket.sendall(data)

    def recv_one_message(self):
        length = self.reclength()
        msg = self.recvall(length)
        # return msg.decode()
        return msg

    def recvall(self, count):
        buf = b''
        while count:
            newbuf = self.socket.recv(count)
            if not newbuf:
                return None
            buf += newbuf
            count -= len(newbuf)
        return buf

    def reclength(self):
        newbuf = self.socket.recv(4)
        length, = struct.unpack('i', newbuf)
        # print(length)
        return length

    def handle_request(self, request_msg):
        try:
            request_string = request_msg.decode()
        except Exception as e:
            request_string = 'unknown'

        if request_string == '':
            self.running = False

        elif request_string == 'STARTUP':
            # self.controller.prepare_model()
            # self.send_one_message(self.output_path)
            print('model loaded successfully')
            # self.send_one_message(b"Connected")
            self.send_one_message(self.output_path.encode('utf-8'))
            print("Unity connected")

        elif request_string == 'SHUTDOWN':
            self.send_one_message(b'SHUTDOWN')
            self.socket.shutdown(0)
            self.socket.close()
            print('shutdown')
            self.running = False

            return False

        elif request_string == 'MARK_PRIMARY_REGION':
            # Start tracking primary region
            print("MARK PRIMARY REGION")
            self.inferencer.mark_primary_region()
            # Send back the directory where visualizations are going to be saved
            self.send_one_message(b'Received_MARK_PRIMARY_REGION')

        elif request_string == 'START_TRACKING':
            self.inferencer.start_tracking()
            self.send_one_message(b'Start tracking')

        elif request_string == 'FINISH_TRACKING':
            self.inferencer.finish_tracking()
            self.send_one_message(b'Finish tracking')

        elif request_string == 'START_REPLAY':
            print("START REPLAY")
            obj_set = self.inferencer.start_replay()
            s = ','.join(obj_set)
            self.send_one_message(s.encode('utf-8'))

        elif request_string == 'TESTRECEIVE':
            print("Receive test requested")
            image = cv2.imread('testorig.jpg')
            self.send_image(image)

            # img_array = jpeg.encode(image)
            # self.send_one_message(img_array)

        elif request_string == 'RECEIVE':
            # print("RECEIVE request")
            if self.inferencer is None:
                # print("Start inferencer")
                # self.inferencer = OnlineDetector(self.video_input)
                # Thread(target=self.inferencer.run, args=()).start()
                # # Send empty byte array when not showing the replay
                # self.send_one_message(bytearray())
                pass
            else:
                if self.inferencer.is_replaying:
                    print("Replay")
                    # Send the stored motion histories one by one from a queue
                    if len(self.inferencer.intersection_img_list) > 0:
                        self.send_image(self.inferencer.intersection_img_list.pop(0))
                    else:
                        self.inferencer.is_replaying = False
                        # Send empty byte array when not showing the replay
                        self.send_one_message(bytearray())

                else:
                    # Send empty byte array when not showing the replay
                    self.send_one_message(bytearray())

            # # cv2.imshow('frame', combined_maps)
            # # cv2.waitKey(1)
            #
            # # print('receive started')
            #
            # if self.model.result_type != 4:
            #     if not self.model.new_result_computed:
            #         self.send_one_message(b"no new data available")
            #         return True
            #
            # result = self.controller.get_current_result()
            # self.model.new_result_computed = False
            #
            # img_array = jpeg.encode(result)
            # self.send_one_message(img_array)
            #
            # # combined_maps_bytes = combined_maps.tobytes()
            # # self.send_one_message(combined_maps_bytes)

        elif request_string == 'TEST':
            self.send_one_message(b"test return")

        elif request_string == 'RECEIVEPREDICTION':

            # if not self.model.new_result_computed:
            #     self.send_one_message(b"no new data available")
            #     return True
            #
            # separator = ','
            # send_value = str.encode(separator.join(self.controller.augmentation_inference_model.emds))
            # self.model.new_result_computed = False
            #
            # self.send_one_message(send_value)
            return True

        else:
            image = request_msg
            np_arr = np.fromstring(image, np.uint8)
            # img_np = jpeg.decode(np_arr)
            # self.controller.handle_image_received(img_np)

            # cv2.imshow('frame', img_np)
            # cv2.waitKey(1)
        return True

    def send_image(self, image):
        # encode image as np array
        # img_array = cv2.imencode('.jpg', image)[1]
        # self.send_one_message(img_array.tobytes())
        img_array = jpeg.encode(image)
        self.send_one_message(img_array)


def start_server(output_path, detector):
    # Start TCP Server
    bind_ip = IP
    bind_port = PORT

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((bind_ip, bind_port))
    server.listen(1)
    print('Listening on {}:{}'.format(bind_ip, bind_port))

    # add loop if more clients should be able to connect
    # for the headless Unity connection, we kill the instance every time Unity disconnects and start a new one.

    # while True:
    # Accept messages from TCP clients
    client_sock, address = server.accept()
    print(f"Server accepts: {client_sock}, {address}")
    client_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    global ir
    ir = InferenceServer(client_sock, address, detector, path=output_path)
    ir.start()
    return ir


if __name__ == '__main__':
    ir = start_server()
