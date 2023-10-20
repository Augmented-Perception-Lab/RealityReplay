import numpy as np


def getRotMatrix(rotation):
    """
    :param rotation: (yaw, pitch, roll) in degree
    :return: general rotational matrix
    refer this: https://en.wikipedia.org/wiki/Rotation_matrix#General_rotations
    """
    yaw, pitch, roll = (rotation / 180) * np.pi

    Rz = np.array([[np.cos(yaw), -np.sin(yaw), 0],
                   [np.sin(yaw), np.cos(yaw), 0],
                   [0, 0, 1]])
    Ry = np.array([[np.cos(pitch), 0, np.sin(pitch)],
                   [0, 1, 0],
                   [-np.sin(pitch), 0, np.cos(pitch)]])
    Rx = np.array([[1, 0, 0],
                   [0, np.cos(roll), -np.sin(roll)],
                   [0, np.sin(roll), np.cos(roll)]])

    return Rz @ Ry @ Rx


def Pixel2LatLon(equirect):
    # LatLon (H, W, (lat, lon))
    h, w = equirect.shape

    Lat = (0.5 - np.arange(0, h) / h) * np.pi
    Lon = (np.arange(0, w) / w - 0.5) * 2 * np.pi

    Lat = np.tile(Lat[:, np.newaxis], w)
    Lon = np.tile(Lon, (h, 1))

    return np.dstack((Lat, Lon))


def LatLon2Sphere(LatLon):
    Lat = LatLon[:, :, 0]
    Lon = LatLon[:, :, 1]
    x = np.cos(Lat) * np.cos(Lon)
    y = np.cos(Lat) * np.sin(Lon)
    z = np.sin(Lat)

    return np.dstack((x, y, z))


def Sphere2LatLon(xyz):
    Lat = np.pi / 2 - np.arccos(xyz[:, :, 2])
    Lon = np.arctan2(xyz[:, :, 1], xyz[:, :, 0])

    return np.dstack((Lat, Lon))


def LatLon2Pixel(LatLon):
    h, w, _ = LatLon.shape
    Lat = LatLon[:, :, 0]
    Lon = LatLon[:, :, 1]
    i = (h * (0.5 - Lat / np.pi)) % h
    j = (w * (0.5 + Lon / (2 * np.pi))) % w

    return np.dstack((i, j)).astype('int')


class EquirectRotate:
    """
    @:param height: height of image to rotate
    @:param width: widht of image to rotate
    @:param rotation: x, y, z degree to rotate
  """

    def __init__(self, height: int, width: int, rotation: tuple):
        assert height * 2 == width
        self.height = height
        self.width = width

        out_img = np.zeros((height, width))  # (H, W)

        # mapping equirect coordinate into LatLon coordinate system
        self.out_LonLat = Pixel2LatLon(out_img)  # (H, W, (lat, lon))

        # mapping LatLon coordinate into xyz(sphere) coordinate system
        self.out_xyz = LatLon2Sphere(self.out_LonLat)  # (H, W, (x, y, z))

        self.src_xyz = np.zeros_like(self.out_xyz)  # (H, W, (x, y, z))

        # make pair of xyz coordinate between src_xyz and out_xyz.
        # src_xyz @ R = out_xyz
        # src_xyz     = out_xyz @ R^t
        self.R = getRotMatrix(np.array(rotation))
        Rt = np.transpose(self.R)  # we should fill out the output image, so use R^t.
        for i in range(self.height):
            for j in range(self.width):
                self.src_xyz[i][j] = self.out_xyz[i][j] @ Rt

        # mapping xyz(sphere) coordinate into LatLon coordinate system
        self.src_LonLat = Sphere2LatLon(self.src_xyz)  # (H, W, 2)

        # mapping LatLon coordinate into equirect coordinate system
        self.src_Pixel = LatLon2Pixel(self.src_LonLat)  # (H, W, 2)

    def rotate(self, image):
        """
    :param image: (H, W, C)
    :return: rotated (H, W, C)
    """
        assert image.shape[:2] == (self.height, self.width)

        rotated_img = np.zeros_like(image)  # (H, W, C)
        for i in range(self.height):
            for j in range(self.width):
                pixel = self.src_Pixel[i][j]
                rotated_img[i][j] = image[pixel[0]][pixel[1]]
        return rotated_img

    def setInverse(self, isInverse=False):
        if not isInverse:
            return

        # re-generate coordinate pairing
        self.R = np.transpose(self.R)
        Rt = np.transpose(self.R)  # we should fill out the output image, so use R^t.
        for i in range(self.height):
            for j in range(self.width):
                self.src_xyz[i][j] = self.out_xyz[i][j] @ Rt

        # mapping xyz(sphere) coordinate into LatLon coordinate system
        self.src_LonLat = Sphere2LatLon(self.src_xyz)  # (H, W, 2)

        # mapping LatLon coordinate into equirect coordinate system
        self.src_Pixel = LatLon2Pixel(self.src_LonLat)  # (H, W, 2)


def pointRotate(h, w, index, rotation):
    """
  :param i, j: index of pixel in equirectangular
  :param rotation: (yaw, pitch, roll) in degree
  :return: rotated index of pixel
  """
    i, j = index
    assert (0 <= i < h) and (0 <= j < w)

    # convert pixel index to LatLon
    Lat = (0.5 - i / h) * np.pi
    Lon = (j / w - 0.5) * 2 * np.pi

    # convert LatLon to xyz
    x = np.cos(Lat) * np.cos(Lon)
    y = np.cos(Lat) * np.sin(Lon)
    z = np.sin(Lat)
    xyz = np.array([x, y, z])
    R = getRotMatrix(np.array(rotation))
    rotated_xyz = xyz @ R

    _Lat = np.pi / 2 - np.arccos(rotated_xyz[2])
    _Lon = np.arctan2(rotated_xyz[1], rotated_xyz[0])

    _i = h * (0.5 - _Lat / np.pi) % h
    _j = w * (0.5 + _Lon / (2 * np.pi)) % w

    return _i, _j
