from os.path import exists
import numpy as np
import cv2
import cv2.aruco as aruco


class Marker:
    def __init__(self):
        self.aruco_dict = aruco.Dictionary_get(aruco.DICT_6X6_250 )
        self.aruco_params = aruco.DetectorParameters_create()
        # self.marker = self.generate_marker()

    def generate_marker(self):
        # Select type of aruco marker (size)

        # Create an image from the marker
        # second param is ID number
        # last param is total image size
        img = aruco.drawMarker(self.aruco_dict, 1, 200)

        marker_path = "../test_marker.jpg"
        if exists(marker_path):
            cv2.imwrite(marker_path, img)
        return img

    def get_position(self, frame):
        (corners, ids, rejected) = aruco.detectMarkers(frame, self.aruco_dict, parameters=self.aruco_params)
        if len(corners) > 0:
            ids = ids.flatten()

            for (markerCorner, markerID) in zip(corners, ids):
                # (x, y) coordinates of marker corners:
                # top left, top right, bottom right, bottom left
                corners = markerCorner.reshape((4, 2))
                (top_left, _, bottom_right, _) = corners

                # convert each of the (x, y)-coordinate pairs to integers
                # top_left = (int(top_left[0]), int(top_left[1]))
                center = (int(top_left[0] + (bottom_right[0]-top_left[0])/2),
                          int(top_left[1] + (bottom_right[1]-top_left[1])/2))
                # draw the top left (x,y) coordinates of the marker
                cv2.circle(frame, center, 4, (0, 0, 255), -1)

                return center
        else:
            return None

    def get_corners(self, frame):
        (corners, ids, rejected) = aruco.detectMarkers(frame, self.aruco_dict, parameters=self.aruco_params)
        if len(corners) > 0:
            ids = ids.flatten()

            for (markerCorner, markerID) in zip(corners, ids):
                # (x, y) coordinates of marker corners:
                # top left, top right, bottom right, bottom left
                corners = markerCorner.reshape((4, 2))
                return corners
        else:
            return None

