import math
import os
import time
import numpy as np
import cv2
import torch
import sys
import copy

from scipy.ndimage.filters import gaussian_filter
from vidgear.gears.stabilizer import Stabilizer
from threading import Thread

from inference.marker_tracking import Marker
from inference.model import TASED_v2
import matplotlib

from detectron2.config import get_cfg
from detectron2.utils.logger import setup_logger

sys.path.insert(0, 'Detic/third_party/CenterNet2/projects/CenterNet2/')
from centernet.config import add_centernet_config
from Detic.detic.config import add_detic_config
from Detic.detic.predictor import VisualizationDemo

import inference.Equirec2Perspec as E2P
from inference.EquirecRotate import EquirectRotate

# Parameters
MHI_DURATION = 10
MOVING_THRESHOLD = 32
RESIZE_FRAME = 1
RESIZE_H = 2
RESIZE_W = 4
SALIENCY_THRESHOLD = 30
FOV = 60   # Varjo FOV: 115
SMOOTHING_RADIUS = 5
MOTION_LINE_WINDOW = 10
MOTION_HISTORY_WINDOW = 10
# REPLAY_WINDOW = 5
LINE_THICKNESS = 2
PRIMARY_THRESH = (5, 5)

# Static objects
static_objects = ['desk', 'sofa', 'dining_table', 'kitchen_table', 'coffee_table', 'crossbar']

allowlist = "jar,apple,banana,cup,flower_pot,basket,hat"
lightgray = (220, 220, 220, 255)
# linecolor = (237, 121, 140, 255)

setup_logger(name="fvcore")
logger = setup_logger()

# params for ShiTomasi corner detection
feature_params = dict( maxCorners = 100,
                       qualityLevel = 0.3,
                       minDistance = 7,
                       blockSize = 7 )
# Parameters for lucas kanade optical flow
lk_params = dict( winSize  = (30, 30),
                  maxLevel = 2,
                  criteria = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10, 0.03))

# From https://www.pyimagesearch.com/2015/12/21/increasing-webcam-fps-with-python-and-opencv/
class VideoStream:
    def __init__(self, src):
        matplotlib.use('TkAgg')
        self.src = src
        self.stream = cv2.VideoCapture(src)
        self.stream.set(cv2.CAP_PROP_FRAME_WIDTH, 1920 // RESIZE_FRAME)
        self.stream.set(cv2.CAP_PROP_FRAME_HEIGHT, 960 // RESIZE_FRAME)
        self.stream.set(cv2.CAP_PROP_FPS, 60)
        (self.grabbed, self.frame) = self.stream.read()
        print("size: ", self.stream.get(cv2.CAP_PROP_FRAME_WIDTH), self.stream.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.mh = None

        self.stopped = False

    def start(self):
        Thread(target=self.update, args=()).start()
        return self

    def update(self):
        while True:
            if self.stopped:
                return

            (self.grabbed, self.frame) = self.stream.read()

    def read(self):
        return self.frame

    def read_stream(self):
        (self.grabbed, self.frame) = self.stream.read()

    def YUVtoRGB(self, byteArray, shape=[1280, 720]):
        e = shape[0] * shape[1]
        Y = byteArray[0:e]
        Y = np.reshape(Y, (shape[1], shape[0]))

        s = e
        V = byteArray[s::2]
        V = np.repeat(V, 2, 0)
        V = np.reshape(V, (shape[1] / 2, shape[0]))
        V = np.repeat(V, 2, 0)

        U = byteArray[s + 1::2]
        U = np.repeat(U, 2, 0)
        U = np.reshape(U, (shape[1] / 2, shape[0]))
        U = np.repeat(U, 2, 0)

        RGBMatrix = (np.dstack([Y, U, V])).astype(np.uint8)
        RGBMatrix = cv2.cvtColor(RGBMatrix, cv2.COLOR_YUV2BGR_UYVY, 3)

    def stop(self):
        self.stopped = True

    def release(self):
        self.stream.release()

def setup_cfg():
    cfg = get_cfg()
    add_centernet_config(cfg)
    add_detic_config(cfg)
    cfg.merge_from_file("Detic/configs/Detic_LCOCOI21k_CLIP_SwinB_896b32_4x_ft4x_max-size.yaml")
    cfg.merge_from_list(['MODEL.WEIGHTS', 'Detic/models/Detic_LCOCOI21k_CLIP_SwinB_896b32_4x_ft4x_max-size.pth'])
    # Set score_threshold for builtin models
    cfg.MODEL.RETINANET.SCORE_THRESH_TEST = 0.5
    cfg.MODEL.ROI_HEADS.SCORE_THRESH_TEST = 0.5
    cfg.MODEL.PANOPTIC_FPN.COMBINE.INSTANCES_CONFIDENCE_THRESH = 0.5
    cfg.MODEL.ROI_BOX_HEAD.ZEROSHOT_WEIGHT_PATH = 'rand'  # load later
    # if not args.pred_all_class:
    #     cfg.MODEL.ROI_HEADS.ONE_CLASS_PER_PROPOSAL = True
    cfg.freeze()
    return cfg

def movingAverage(curve, radius):
    window_size = 2 * radius + 1
    # Define the filter
    f = np.ones(window_size) / window_size
    # Add padding to the boundaries
    curve_pad = np.lib.pad(curve, (radius, radius), 'edge')
    # Apply convolution
    curve_smoothed = np.convolve(curve_pad, f, mode='same')
    # Remove padding
    curve_smoothed = curve_smoothed[radius:-radius]
    # Return smoothed curve
    return curve_smoothed

def smooth(trajectory):
    smoothed_trajectory = np.copy(trajectory)
    # Filter the x, y and angle curves
    for i in range(3):
        smoothed_trajectory[:, i] = movingAverage(trajectory[:, i], radius=SMOOTHING_RADIUS)

    return smoothed_trajectory


def get_centroid(binary_mask):
    M = cv2.moments(binary_mask)
    if M["m00"] > 0:
        # Calculate the centroid of each mask
        cX = int(M["m10"] / M["m00"])
        cY = int(M["m01"] / M["m00"])
        return (cX, cY)
    return None


class OnlineDetector:
    def __init__(self, mode, video_input, output_path):
        self.mode = mode
        self.video_input = video_input
        # Motion history output paths
        self.motion_history_output = output_path + '/motion_history'
        self.motion_history_demo = output_path + '/motion_history_demo'
        # Motion line output paths
        self.motion_line_output = output_path + '/motion_line'
        self.motion_line_demo = output_path + '/motion_line_demo'
        self.motion_line_traj = output_path + '/motion_line_traj'
        # Replay output paths
        self.replay_demo = output_path + '/replay_demo'
        self.replay_output = output_path + '/replay'
        self.gray_replay_output = output_path + '/gray_replay'
        self.obj_mask_output = output_path + '/obj_mask'
        self.primary_region_path = output_path + '/primary_region'
        self.visualization_output = output_path + '/visualization'
        self.er_path = output_path + '/original_er'
        self.saliency_map_path = output_path + '/saliency_map'
        self.varjo_frame = output_path + '/varjo_frame.jpg'     # could be .bmp

        # Create output directories if they don't exist
        output_paths = [self.motion_history_output, self.motion_history_demo,
                        self.motion_line_output, self.motion_line_demo, self.motion_line_traj,
                        self.replay_output, self.gray_replay_output, self.replay_demo,
                        self.obj_mask_output, self.primary_region_path, self.saliency_map_path,
                        self.er_path, self.visualization_output]
        if not os.path.exists(output_path):
            os.mkdir(output_path)
        for path in output_paths:
            if not os.path.exists(path):
                os.mkdir(path)

        self.len_temporal = 32
        self.file_weight = '../inference/weights/TASED_updated.pt'

        self.current_saliency_map = None
        self.images_sal = []
        self.is_calc_saliency = False
        self.or_calc_count = 0
        self.or_pool_size = 1
        self.obj_mask = None
        self.obj_masked_img = None
        self.obj_bbox = None
        self.obj_set = set()
        self.centroids = {}
        self.masks = {}
        self.class_names = {}
        self.scores = {}
        self.motion_history = {}
        self.motion_history_dict = {}   # {'object_label': {frame_idx: [[(50,50,4)],[]]}
        self.motion_history_box = {}    # {'object_label': (x1, y1, x2, y2)}
        self.motion_replay_dict = {}    # {'object_label': {frame_idx: [[(50,50,4)],[]]}
        self.motion_gray_replay_dict = {}
        self.mh_prev_x, self.mh_prev_y = {}, {}
        self.mh_prev_w, self.mh_prev_h = {}, {}
        self.mh_dx, self.mh_dy = 0, 0
        self.mh_replay = []
        self.motion_lines = []     # store [[((new_x, new_y), (old_x, old_y)) for i] for frame]
        self.all_motion_lines = {}
        self.motion_line_dict = {}
        self.motion_line_color = {}  # store {label: color}
        self.motion_line_color = {
            # BGR
            'apple': (48, 64, 141),
            'jar': (68, 102, 155),
            'suitcase': (41, 46, 51),
            'basket': (112, 163, 203),
            'flower_pot': (31, 120, 56),
            'hat': (63, 79, 94),
            'cup': (216, 197, 183),
            'banana': (118, 206, 248),
            'crock_pot': (200, 200, 200),
            'bottle': (230, 220, 213),
            'person': (50, 10, 10),
            'watch': (189, 151, 70),
            'ball': (255,255,255),
            'figurine': (255,255,255),
            'book': (0,0,0),
            'knife': (0,0,0),
            'orange_(fruit)': (0,149,255),
            'keychain': (0, 215, 255),
        }

        self.timestamp = 0
        self.mh = None
        self.p0 = None
        self.er_frame = None
        self.marker = Marker()

        self.frame_w, self.frame_h = 640, 480
        self.view_size = (self.frame_h // RESIZE_H, self.frame_w // RESIZE_W)  # h, w
        self.center_view = (self.frame_h // RESIZE_H, self.frame_w // RESIZE_W)  # y, x
        self.replay_region = np.zeros(self.view_size)

        self.is_tracking = False
        self.primary_done = False
        self.intersection_img_list = []
        self.primary_history = []

        self.images_or = []
        self.obj_rec_history = []
        self.obj_rec_count = 0
        self.obj_rec_threads = []
        self.pending_jobs = []

        self.motion_line_list = []
        self.marker_position = (400 * 4 // RESIZE_FRAME, 240 * 4 // RESIZE_FRAME)
        self.primary_marked = False
        self.primary_offset = (0, 0)
        self.is_replaying = False
        self.frame_count = 0

        self.is_varjo_frame = False
        self.varjo_image = None

        self.sal_thread = None
        self.detic_thread = None

        # Detic
        cfg = setup_cfg()
        self.detic = VisualizationDemo(cfg
                                       , vocab="custom",
                                       custom_vocab=allowlist)

        self.marker_hidden = False
        self.marker_tl, self.marker_br = None, None


        self.model = TASED_v2()

        # load the weight file of TASED-Net and copy the parameters
        if os.path.isfile(self.file_weight):
            print('loading TASED-Net weight file')
            weight_dict = torch.load(self.file_weight)
            model_dict = self.model.state_dict()
            for name, param in weight_dict.items():
                if 'module' in name:
                    name = '.'.join(name.split('.')[1:])
                if name in model_dict:
                    if param.size() == model_dict[name].size():
                        model_dict[name].copy_(param)
                    else:
                        print(' size? ' + name, param.size(), model_dict[name].size())
                else:
                    print(' name? ' + name)

            print(' loaded')
        else:
            print('weight file?')
            
        self.model = self.model.cuda()
        torch.backends.cudnn.benchmark = False
        # run the eval function on the saliency model
        self.model.eval()


    def transform(self, snippet):
        ''' stack & noralization '''
        snippet = np.concatenate(snippet, axis=-1)
        snippet = torch.from_numpy(snippet).permute(2, 0, 1).contiguous().float()
        snippet = snippet.mul_(2.).sub_(255).div(255)

        return snippet.view(1, -1, 3, snippet.size(1), snippet.size(2)).permute(0, 2, 1, 3, 4)

    def process_sal_async(self):
        if not self.is_calc_saliency:
            images_resized = []

            for frame in self.images_sal:
                img = cv2.resize(frame, (384, 224))
                images_resized.append(img)

            self.is_calc_saliency = True
            Thread(target=self.process_sal, args=([images_resized])).start()

    def process_sal(self, images_resized):
        clip = self.transform(images_resized)
        self.current_saliency_map = self.get_saliency_map(clip)
        frame_idx = self.obj_rec_count

        # layer = np.broadcast_to(self.current_saliency_map[..., np.newaxis], clip.shape)
        # layer = np.asarray(layer, clip.dtype)
        # frame_weighted = cv2.addWeighted(clip, 1, layer, .5, 0)
        cv2.imwrite(self.saliency_map_path + f'/{frame_idx:04d}.jpg', self.current_saliency_map)
        # cv2.imwrite(self.saliency_map_path + '/{}.jpg'.format(frame_idx), frame_weighted)

        self.is_calc_saliency = False
        self.images_sal = []

    def process_detic_async(self):
        print("## Call object recognition")
        Thread(target=self.process_detic, args=()).start()

    def process_detic(self):
        # Process the first image in image stack instead of the current frame

        while True:
            if len(self.images_or) > 1:
                image = self.images_or.pop(0)
                if self.is_varjo_frame:
                    image = None
                    while image is None:
                        image = cv2.imread(
                            "../../VarjoCameraRecorder/VarjoCameraRecorder/bin/frames/left.bmp")
                    image_h, image_w, _ = image.shape
                    crop_w = int(image_w * 0.14)
                    crop_h = int(image_h * 0.23)
                    image = image[crop_h:image_h - crop_h, crop_w:image_w - crop_w]
                    cv2.imwrite(
                        "../../VarjoCameraRecorder/VarjoCameraRecorder/bin/frames/cropped_left.bmp",
                        image)

                    start_time = time.time()
                    predictions, visualized_output = self.detic.run_on_image(image)
                    output_image, orig_masks, orig_labels, orig_boxes = visualized_output
                    confidences = [int(label.split(" ")[1].strip('%')) for label in orig_labels]
                    orig_labels = [label.split(" ")[0] for label in orig_labels]

                    logger.info(
                        "{}: {} in {:.2f}s".format(
                            "LAST",
                            "detected {} instances".format(len(predictions["instances"]))
                            if "instances" in predictions
                            else "finished",
                            time.time() - start_time,
                        )
                    )
                    self.curr_obj_pos = list(zip(orig_masks, orig_labels, orig_boxes))
                    denylist = []

                    label_set = set(orig_labels)
                    for label in label_set:
                        indices = [i for i, x in enumerate(orig_labels) if x == label]
                        if len(indices) > 1:
                            max_confidence = 0
                            max_index = 0
                            for i in indices:
                                if confidences[i] >= max_confidence:
                                    max_confidence = confidences[i]
                                    max_index = i

                            indices.remove(max_index)
                            for i in indices:
                                denylist.append(i)

                    if self.curr_obj_pos is not None:
                        # Delete objects in denylist
                        self.curr_obj_pos = np.delete(self.curr_obj_pos, denylist, 0)

                    # self.apply_visualization(image, curr_obj_pos)
                    print("OBJECT RECOGNITION DONE")
                    break
                frame_idx = self.obj_rec_count
                self.obj_rec_count += 1
                print("Start evaluation: ", self.or_calc_count)

                start_time = time.time()
                predictions, visualized_output = self.detic.run_on_image(image)
                output_image, orig_masks, orig_labels, orig_boxes = visualized_output
                logger.info(
                    "{}: {} in {:.2f}s".format(
                        frame_idx,
                        "detected {} instances".format(len(predictions["instances"]))
                        if "instances" in predictions
                        else "finished",
                        time.time() - start_time,
                    )
                )
                confidences = [int(label.split(" ")[1].strip('%')) for label in orig_labels]
                orig_labels = [label.split(" ")[0] for label in orig_labels]
                print("orig labels: ", orig_labels)

                # Multithreading: Run or_calc_count number of object recognition jobs at a time
                self.or_calc_count -= 1

                # Semantic rejection
                # masks = orig_masks.copy()
                # orig_masks = [mask[orig_boxes.tensor[i][0]:ori] for i, mask in enumerate(orig_masks)]
                # print("orig box[0]: ", orig_boxes.tensor[0][0])
                # Boxes(tensor (Tensor[float]): a Nx4 matrix.  Each row is (x1, y1, x2, y2))

                masks = list(zip(orig_masks, orig_labels, orig_boxes))
                print("orig_masks: ", orig_masks)
                print("Masks: ", masks)

                denylist = []

                label_set = set(orig_labels)

                self.obj_set.update(label_set)

                for label in label_set:
                    indices = [i for i, x in enumerate(orig_labels) if x == label]
                    max_confidence = 0
                    max_index = 0
                    for i in indices:
                        if confidences[i] >= max_confidence:
                            max_confidence = confidences[i]
                            max_index = i
                    indices.remove(max_index)
                    for i in indices:
                        denylist.append(i)

                for i, label in enumerate(orig_labels):
                    # class_name = label.split()[0]
                    # # confidence = label.split()[1]
                    # labels.append(class_name)
                    if label in static_objects:
                        denylist.append(i)

                # Process output masks
                if masks is not None:
                    # Delete objects in denylist
                    masks = np.delete(masks, denylist, 0)

                    # Delete masks outside salient region
                    
                    denylist = []
                    if self.current_saliency_map is not None:
                        salient_region = np.argwhere(self.current_saliency_map > SALIENCY_THRESHOLD)
                        salient_region = np.delete(salient_region, -1, 1)
                        salient_region = [tuple(i) for i in salient_region]

                        for i, (mask, label, box) in enumerate(masks):
                            binary_mask = mask.mask
                            binary_mask_coords = np.argwhere(binary_mask > 0)
                            binary_mask_coords = [tuple(c) for c in binary_mask_coords]
                            intersections = set(binary_mask_coords).intersection(set(salient_region))

                            if len(intersections) <= 10:
                                denylist.append(i)

                        masks = np.delete(masks, denylist, 0)
                    

                    # Store masks for the frame
                    self.masks[frame_idx] = masks

                    # Thread(target=self.process_viz, args=([self.obj_rec_count-1])).start()
                    viz_start_time = time.time()
                    self.process_viz(self.obj_rec_count-1)
                    print(f"viz processing time: {time.time() - viz_start_time}s")

                print(f"time: {time.time() - start_time}s")

    def process_viz(self, frame_idx):
        # Compute motion line and motion history
        # when the object recognition of previous frame finishes
        # frame_idx = self.frame_count
        print("process VIZ: ", frame_idx)
        if frame_idx > 0:
            if os.path.exists(self.obj_mask_output + f'/{frame_idx - 1:04d}.jpg'):
                self.save_motion_line(frame_idx - 1, frame_idx)
                self.save_motion_history(frame_idx - 1, frame_idx)
            else:
                print("PATH NOT EXIST: ", self.obj_mask_output + f'/{frame_idx - 1:04d}.jpg')
        else:
            # For the first frame
            print("FIRST FRAME")
            h, w = self.view_size
            image = cv2.imread(self.primary_region_path + f'/{frame_idx:04d}.jpg')
            # masks, labels = self.masks[frame_idx]
            mask_labels = self.masks[frame_idx]
            binary_mask = np.full((h, w), 0, dtype=np.uint8)
            for mask, label, box in mask_labels:
                binary_mask = cv2.add(binary_mask, mask.mask)

                # Test writing the patch into the frame
                masked = cv2.bitwise_and(image, image, mask=binary_mask)
                masked = np.dstack((masked, np.zeros((h, w), dtype=np.uint8)))

                frame_cp = masked.copy()

                colors = masked[:, :, 0]
                alpha_channel = 255 - colors
                alpha_channel[alpha_channel == 255] = 0

                frame_cp[:, :, -1] = alpha_channel[:]
                masked = frame_cp

                mask_patch = masked[int(box[1]):int(box[3]), int(box[0]):int(box[2])]
                if label not in self.motion_history_dict.keys():
                    self.motion_history_dict[label] = {}
                    self.motion_replay_dict[label] = {}
                    self.motion_gray_replay_dict[label] = {}

                # Store motion history patch and (dx, dy) from initial position
                # For the first frame, (dx, dy) = (0, 0)
                self.motion_history_dict[label][frame_idx] = (mask_patch, (int(box[1]), int(box[0])))
                self.motion_replay_dict[label][frame_idx] = (mask_patch, (int(box[1]), int(box[0])))

                # gray_mask = np.zeros((h, w, 4), dtype=np.uint8)
                alpha_channel = binary_mask.copy() * 255
                alpha_mask = binary_mask.copy() * 255
                alpha_mask[alpha_mask == 255] = 0
                alpha_mask = np.broadcast_to(alpha_mask[..., np.newaxis],
                                             (h, w, 4))
                mask_copy = alpha_mask.copy()
                mask_copy[:, :, -1] = alpha_channel
                gray_mask = mask_copy

                self.motion_gray_replay_dict[label][frame_idx] = gray_mask
                gray_mask_patch = cv2.cvtColor(mask_patch, cv2.COLOR_BGR2GRAY)
                #
                # gray_mask_patch = np.broadcast_to(gray_mask_patch[..., np.newaxis],
                #                                   (mask_patch.shape[0], mask_patch.shape[1], 4))
                # self.motion_gray_replay_dict[label][frame_idx] = gray_mask_patch

                self.mh_prev_x[label], self.mh_prev_y[label] = int(box[0]), int(box[1])
                self.mh_prev_w[label], self.mh_prev_h[label] = int(box[2] - box[0]), int(box[3] - box[1])
                # cv2.imwrite(self.obj_mask_output + f'/mask_patch_{frame_idx:04d}_{label}.png', mask_patch)
            masked = cv2.bitwise_and(image, image, mask=binary_mask)
            cv2.imwrite(self.obj_mask_output + f'/{frame_idx:04d}.jpg', masked)
            # cv2.imwrite(self.replay_output + f'/{frame_idx:04d}.png', masked)

            self.motion_history[frame_idx] = np.zeros((h, w), np.float32)

    def get_saliency_map(self, clip):
        # Run saliency detection
        with torch.no_grad():
            smap = self.model(clip.cuda()).cpu().data[0]

        smap = (smap.numpy() * 255.).astype(int) / 255.
        smap = gaussian_filter(smap, sigma=7)
        grayscale = (smap / np.max(smap) * 255.).astype(np.uint8)

        color = np.zeros((224, 384, 3), dtype=np.uint8)
        color[:, :, 2] = grayscale
        resized_color = cv2.resize(color, (self.view_size[1], self.view_size[0]))

        return resized_color

    def save_motion_line(self, prev_idx, curr_idx):
        color = np.random.randint(0, 255, (100, 4))
        color[-1] = 255

        prev_frame = cv2.imread(self.primary_region_path + f'/{prev_idx:04d}.jpg')
        curr_frame = cv2.imread(self.primary_region_path + f'/{curr_idx:04d}.jpg')

        """Centroid-based motion line"""

        # Connect centroids of the same object label
        # prev_centroids = [(get_centroid(binary_mask.mask), label)
        #                   for binary_mask, label, box in self.masks[prev_idx]]
        curr_centroids = [(get_centroid(binary_mask.mask), label)
                          for binary_mask, label, box in self.masks[curr_idx]]

        # motion_lines = []
        if prev_idx == 0:
            self.motion_line_dict[prev_idx] = {}
            obj_motion_lines = {}
        else:
            obj_motion_lines = self.motion_line_dict[prev_idx].copy()

        for curr_v, label in curr_centroids:
            # motion_lines.append(curr_v)
            if label not in obj_motion_lines.keys():
                obj_motion_lines[label] = []
            # obj_motion_lines[label].append((curr_v, curr_frame[curr_v[0]][curr_v[1]]))
            obj_motion_lines[label].append(curr_v)

            
            # If line color is not predefined, get the centroid color
            if label not in self.motion_line_color.keys():
                centroid_color = curr_frame[curr_v[0]][curr_v[1]]
                self.motion_line_color[label] = centroid_color 
            

            if label not in self.all_motion_lines.keys():
                self.all_motion_lines[label] = []
            self.all_motion_lines[label].append(curr_v)

            # if len(obj_motion_lines[label]) > MOTION_LINE_WINDOW:
            #     obj_motion_lines[label].pop(0)
        self.motion_line_dict[curr_idx] = copy.deepcopy(obj_motion_lines)

        print(f"motion line dict at {curr_idx}: {self.motion_line_dict[curr_idx]}")
        transparent_img = np.zeros((curr_frame.shape[0], curr_frame.shape[1], 4), dtype=np.uint8)
        motion_line_img = np.zeros_like(transparent_img)

        for k, v in self.all_motion_lines.items():
            prev_centroid = None
            for centroid in v:
                curr_frame = cv2.circle(curr_frame, centroid, LINE_THICKNESS, lightgray, -1)
                if prev_centroid is not None:
                    curr_frame = cv2.line(curr_frame, prev_centroid, centroid, lightgray, LINE_THICKNESS)
                prev_centroid = centroid

        for label, v in obj_motion_lines.items():
            prev_centroid = None
            for centroid in v:
                linecolor = self.motion_line_color[label]
                linecolor = (int(linecolor[0]), int(linecolor[1]), int(linecolor[2]), 255)
                motion_line_img = cv2.circle(motion_line_img, centroid, LINE_THICKNESS, linecolor, -1)
                if prev_centroid is not None:
                    # cent_distance = math.sqrt((prev_centroid[0] - centroid[0])**2 + (prev_centroid[1] - centroid[1])**2)
                    # if cent_distance > 20:
                    motion_line_img = cv2.line(motion_line_img, prev_centroid, centroid, linecolor, LINE_THICKNESS)

                prev_centroid = centroid



    def save_motion_history(self, prev_idx, curr_idx):
        prev_frame = cv2.imread(self.primary_region_path + f'/{prev_idx:04d}.jpg')
        curr_frame = cv2.imread(self.primary_region_path + f'/{curr_idx:04d}.jpg')

        h, w, _ = prev_frame.shape

        curr_mask = np.full((h, w), 0, dtype=np.uint8)
        mask_per_obj = {}
        alpha_mask_per_obj = {}
        frame_masks = None
        # no_window_mask_per_obj = {}
        for i in range(MOTION_HISTORY_WINDOW):
            if curr_idx - (MOTION_HISTORY_WINDOW - i) < 0:
                frame_masks = self.masks[curr_idx]
            else:
                frame_masks = self.masks[curr_idx - (MOTION_HISTORY_WINDOW - i)]
            for mask, label, box in frame_masks:
                if label not in mask_per_obj.keys():
                    mask_per_obj[label] = np.full((h, w), 0, dtype=np.uint8)
                    alpha_mask_per_obj[label] = np.zeros((h, w, 4), dtype=np.uint8)
                    # no_window_mask_per_obj[label] = np.full((h, w), 0, dtype=np.uint8)
                curr_mask = cv2.add(curr_mask, mask.mask)

                mask_per_obj[label] = cv2.add(mask_per_obj[label], mask.mask)

                alpha_channel = mask.mask.copy() * 255
                alpha_mask = mask.mask.copy() * 255
                alpha_mask[alpha_mask == 255] = 0
                alpha_mask = np.broadcast_to(alpha_mask[..., np.newaxis],
                                             (h, w, 4))
                mask_copy = alpha_mask.copy()
                mask_copy[:, :, -1] = alpha_channel
                alpha_mask = mask_copy

                alpha_mask_per_obj[label] = cv2.addWeighted(alpha_mask_per_obj[label], 0.8, alpha_mask, 0.9, 0)

        curr_frame_masks = self.masks[curr_idx]

        prev_masked = cv2.bitwise_and(prev_frame, prev_frame, mask=curr_mask)
        curr_masked = cv2.bitwise_and(curr_frame, curr_frame, mask=curr_mask)
        for mask, label, box in frame_masks:
            if label not in self.motion_gray_replay_dict.keys():
                self.motion_replay_dict[label] = {}
                self.motion_gray_replay_dict[label] = {}
            if curr_idx not in self.motion_gray_replay_dict[label].keys():
                obj_masked = cv2.bitwise_and(prev_frame, prev_frame, mask=mask.mask)
                obj_masked = np.dstack((obj_masked, np.zeros((h, w), dtype=np.uint8)))
                gray_mask = cv2.cvtColor(obj_masked, cv2.COLOR_BGR2GRAY)

                alpha_channel = gray_mask.copy()
                alpha_channel[alpha_channel > 0] = 255
                obj_masked[:, :, -1] = alpha_channel

                self.motion_replay_dict[label][curr_idx] = (obj_masked[int(box[1]):int(box[3]), int(box[0]):int(box[2])],
                                                            (int(box[1]), int(box[0])))
                self.motion_gray_replay_dict[label][curr_idx] = alpha_mask_per_obj[label]

        cv2.imwrite(self.obj_mask_output + f'/{prev_idx:04d}.jpg', prev_masked)
        cv2.imwrite(self.obj_mask_output + f'/{curr_idx:04d}.jpg', curr_masked)

        # Motion history processing

        # Pixel-level difference within mask
        # 1. Get pixel-level motion history
        frame_diff = cv2.absdiff(prev_frame, curr_frame)
        gray_diff = cv2.cvtColor(frame_diff, cv2.COLOR_BGR2GRAY)
        ret, fgmask = cv2.threshold(gray_diff, MOVING_THRESHOLD, 1, cv2.THRESH_BINARY)
        prev_mh = self.motion_history[prev_idx]

        # update motion history
        timestamp = curr_idx
        cv2.motempl.updateMotionHistory(fgmask, prev_mh, timestamp, MHI_DURATION)

        self.motion_history[curr_idx] = prev_mh

        curr_masked = cv2.cvtColor(curr_masked, cv2.COLOR_BGR2GRAY)
        self.replay_region = self.replay_region + np.where(curr_masked > 0, 1, 0)
        replay_region_idx = np.argwhere(self.replay_region > 0)
        transparent_img = np.zeros((prev_mh.shape[0], prev_mh.shape[1], 4), dtype=np.uint8)
        ph_frame = np.zeros_like(transparent_img)
        if len(replay_region_idx) > 0:
            idx_T = replay_region_idx.T
            frame_a = cv2.cvtColor(curr_frame, cv2.COLOR_RGB2RGBA)
            ph_frame[idx_T[0], idx_T[1]] = frame_a[idx_T[0], idx_T[1]]

        # Mask motion history with combined object mask
        prev_mh = cv2.bitwise_and(prev_mh, prev_mh, mask=curr_masked)

        # Gradients in motion history based on timestamp
        self.mh = 255 - np.uint8(
            np.clip((prev_mh - (timestamp - MHI_DURATION)) / MHI_DURATION, 0, 1) * 255)

        frame_mh = np.broadcast_to(self.mh[..., np.newaxis], (prev_mh.shape[0], prev_mh.shape[1], 4))

        # Add alpha channel to motion history image
        frame_cp = frame_mh.copy()

        colors = frame_mh[:, :, 0]
        alpha_channel = colors.copy()
        alpha_channel[alpha_channel == 255] = 0

        frame_cp[:, :, -1] = alpha_channel[:]
        frame_mh = frame_cp

        # Mask motion history with each object mask

        for _, label, box in frame_masks:
            if label not in self.motion_history_dict.keys():
                self.motion_history_dict[label] = {}
                # self.motion_replay_dict[label] = {}
            # Store motion history patch and (dx, dy) from initial position
            if label not in self.motion_history_box.keys():
                self.motion_history_box[label] = {0: [int(box[0])], 1: [int(box[1])],
                                                  2: [int(box[2])], 3: [int(box[3])]}
            else:
                for i in range(4):
                    self.motion_history_box[label][i].append(int(box[i]))
                    if len(self.motion_history_box[label][i]) > MOTION_HISTORY_WINDOW:
                        self.motion_history_box[label][i].pop(0)

            mh_x1 = min(self.motion_history_box[label][0])
            mh_y1 = min(self.motion_history_box[label][1])
            mh_x2 = max(self.motion_history_box[label][2])
            mh_y2 = max(self.motion_history_box[label][3])
            self.motion_history_dict[label][curr_idx] = (frame_mh[mh_y1:mh_y2, mh_x1:mh_x2],
                                                         (mh_y1, mh_x1))
            self.motion_gray_replay_dict[label][curr_idx] = self.motion_gray_replay_dict[label][curr_idx][mh_y1:mh_y2,
                                                       mh_x1:mh_x2]
            self.mh_prev_x[label], self.mh_prev_y[label] = int(box[0]), int(box[1])

            self.mh_prev_w[label], self.mh_prev_h[label] = int(box[2]) - int(box[0]), int(box[3]) - int(box[1])

        # Transparent image to combine with motion replay and history
        transparent_img = np.zeros((prev_mh.shape[0], prev_mh.shape[1], 4), dtype=np.uint8)

        # Save motion history
        frame_weighted = cv2.addWeighted(transparent_img, 0, frame_mh, 0.5, 0)
       

    def apply_visualization(self):
        curr_obj_pos = self.curr_obj_pos    # self.curr_obj_pos here has the objects in Varjo image
        image = self.varjo_image
        image_h, image_w, _ = image.shape
        crop_w = int(image_w * 0.14)
        crop_h = int(image_h * 0.23)
        image = image[crop_h:image_h - crop_h, crop_w:image_w - crop_w]
        # Create a transparent image to draw motion lines, histories, and replays on
        transparent_img = np.zeros((image.shape[0], image.shape[1], 4), dtype=np.uint8)

        # Re-position and re-scale motion history
        for frame_idx in range(self.obj_rec_count):
            print(f"frame idx: {frame_idx}")
            frame_mask = transparent_img.copy()
            replay_mask = transparent_img.copy()
            gray_replay_mask = transparent_img.copy()
            motion_line_mask = transparent_img.copy()

            for mask, label, box in curr_obj_pos:
                # Save visualizations for each object
                per_obj_mask = transparent_img.copy()

                Vx1, Vy1, Vx2, Vy2 = int(box[0]), int(box[1]), int(box[2]), int(box[3])
                if label not in self.motion_history_dict.keys():
                    continue

                # Scale motion history according to the current object size
                x_scale_ratio = (Vx2 - Vx1) / self.mh_prev_w[label]
                y_scale_ratio = (Vy2 - Vy1) / self.mh_prev_h[label]
                print(f"X SCALE RATIO: {x_scale_ratio}")
                print(f"Y SCALE RATIO: {y_scale_ratio}")
                if frame_idx in self.motion_history_dict[label].keys():
                    motion_history, (Pm_y, Pm_x) = self.motion_history_dict[label][frame_idx]
                elif frame_idx == 0 or frame_idx-1 not in self.motion_history_dict[label].keys():
                    print(f"NO MOTION HISTORY for {label} at {frame_idx}")
                    continue
                else:
                    motion_history, (Pm_y, Pm_x) = self.motion_history_dict[label][frame_idx-1]
                    self.motion_history_dict[label][frame_idx] = self.motion_history_dict[label][frame_idx-1]

                new_width = int(motion_history.shape[1] * x_scale_ratio)
                new_height = int(motion_history.shape[0] * y_scale_ratio)
                resized = cv2.resize(motion_history, (new_width, new_height))

                # Adjusted motion history position
                mask_position = int(Vy1 - (self.mh_prev_y[label] - Pm_y) * y_scale_ratio),\
                                int(Vx1 - (self.mh_prev_x[label] - Pm_x) * x_scale_ratio)

                # Clip the mask if the mask overflows out of the mask frame,
                print("Resized Motion History Shape: ", resized.shape)
                print("frame mask shape: ", frame_mask.shape)
                if mask_position[1] + resized.shape[1] > frame_mask.shape[1]:
                    print(f"width clipped: {new_width}->{frame_mask.shape[1] - mask_position[1]}")
                    new_width = frame_mask.shape[1] - mask_position[1]
                if mask_position[0] + resized.shape[0] > frame_mask.shape[0]:
                    print(f"height clipped: {new_height}->{frame_mask.shape[0] - mask_position[0]}")
                    new_height = frame_mask.shape[0] - mask_position[0]

                layer = transparent_img.copy()
                y_diff, x_diff = 0, 0
                if mask_position[0] < 0:
                    y_diff = -mask_position[0]
                if mask_position[1] < 0:
                    x_diff = -mask_position[1]

                layer[mask_position[0]+y_diff:mask_position[0]+new_height,
                      mask_position[1]+x_diff:mask_position[1]+new_width] \
                    = resized[y_diff:new_height, x_diff:new_width]
                frame_mask = cv2.addWeighted(frame_mask, 1, layer, 1, 0)

                # Motion Line
                line_idx = max(frame_idx - 2, 1)
                if label not in self.motion_line_dict[line_idx].keys():
                    if frame_idx == 0:
                        continue
                    if label in self.motion_line_dict[line_idx - 1].keys():
                        self.motion_line_dict[line_idx][label] = self.motion_line_dict[line_idx - 1][label]
                    else:
                        continue
                centroids = np.array(self.motion_line_dict[line_idx][label]).copy()
                new_centroids = (centroids - (self.mh_prev_x[label], self.mh_prev_y[label])) * \
                                (x_scale_ratio, y_scale_ratio) + (Vx1, Vy1)

                prev_centroid = None
                for i, centroid in enumerate(new_centroids):
                    linecolor = self.motion_line_color[label]
                    linecolor = (int(linecolor[0]), int(linecolor[1]), int(linecolor[2]), 255)
                    centroid = (int(centroid[0]), int(centroid[1]))
                    if i == len(new_centroids) - 1:
                        start_point = np.array(centroid) - LINE_THICKNESS * 2
                        end_point = np.array(centroid) + LINE_THICKNESS * 2
                        motion_line_mask = cv2.rectangle(motion_line_mask, start_point, end_point, linecolor,
                                                         LINE_THICKNESS * 2)
                        per_obj_mask = cv2.rectangle(per_obj_mask, start_point, end_point, linecolor,
                                                         LINE_THICKNESS * 2)
                    else:
                        motion_line_mask = cv2.circle(motion_line_mask, centroid, LINE_THICKNESS, linecolor, -1)
                        per_obj_mask = cv2.circle(per_obj_mask, centroid, LINE_THICKNESS, linecolor, -1)
                    if prev_centroid is not None:
                        motion_line_mask = cv2.line(motion_line_mask, prev_centroid, centroid, linecolor,
                                                    LINE_THICKNESS)
                        per_obj_mask = cv2.line(per_obj_mask, prev_centroid, centroid, linecolor,
                                                    LINE_THICKNESS)
                    prev_centroid = centroid


                # Adjust Motion History
                if frame_idx in self.motion_gray_replay_dict[label].keys():
                    gray_replay = self.motion_gray_replay_dict[label][frame_idx]
                elif frame_idx == 0 or frame_idx - 1 not in self.motion_gray_replay_dict[label].keys():
                    print(f"NO MOTION REPLAY for {label} at {frame_idx - 1}")
                    continue
                else:
                    gray_replay = self.motion_gray_replay_dict[label][frame_idx - 1]
                    self.motion_gray_replay_dict[label][frame_idx] = self.motion_gray_replay_dict[label][
                        frame_idx - 1]
                gray_resized = cv2.resize(gray_replay, (new_width, new_height))
                layer = transparent_img.copy()
                layer[mask_position[0] + y_diff:mask_position[0] + new_height,
                mask_position[1] + x_diff:mask_position[1] + new_width] \
                    = gray_resized[y_diff:new_height, x_diff:new_width]
                gray_replay_mask = cv2.addWeighted(gray_replay_mask, 1, layer, 1, 0)
                per_obj_mask = cv2.addWeighted(per_obj_mask, 1.0, layer, 0.8, 0)
                print(f"gray replay mask position[1], mask position[0]: {mask_position[1]}, {mask_position[0]}")
                print(f"gray replay new height, new width: {new_height}, {new_width}")

                # Adjust motion replay
                if frame_idx in self.motion_replay_dict[label].keys():
                    motion_replay, (Pm_y, Pm_x) = self.motion_replay_dict[label][frame_idx]
                elif frame_idx == 0 or frame_idx - 1 not in self.motion_replay_dict[label].keys():
                    print(f"NO MOTION REPLAY for {label} at {frame_idx}")
                    continue
                else:
                    motion_replay, (Pm_y, Pm_x) = self.motion_replay_dict[label][frame_idx-1]
                    self.motion_replay_dict[label][frame_idx] = self.motion_replay_dict[label][frame_idx-1]
                new_width = int(motion_replay.shape[1] * x_scale_ratio)
                new_height = int(motion_replay.shape[0] * y_scale_ratio)
                resized = cv2.resize(motion_replay, (new_width, new_height))

                replay_mask_position = int(Vy1 - (self.mh_prev_y[label] - Pm_y) * y_scale_ratio), \
                                       int(Vx1 - (self.mh_prev_x[label] - Pm_x) * x_scale_ratio)
                if replay_mask_position[1] + resized.shape[1] > frame_mask.shape[1]:
                    print(f"replay width clipped: {new_width}->{frame_mask.shape[1] - replay_mask_position[1]}")
                    new_width = frame_mask.shape[1] - replay_mask_position[1]
                if replay_mask_position[0] + resized.shape[0] > frame_mask.shape[0]:
                    print(f"replay height clipped: {new_height}->{frame_mask.shape[0] - replay_mask_position[0]}")
                    new_height = frame_mask.shape[0] - replay_mask_position[0]

                layer = transparent_img.copy()
                print("replay mask position [1], replay mask position [0]: ", replay_mask_position[1], replay_mask_position[0])
                print("replay x_diff, y_diff: ", x_diff, y_diff)
                print("new width, new_height: ", new_width, new_height)

                y_diff, x_diff = 0, 0
                if replay_mask_position[0] < 0:
                    y_diff = -replay_mask_position[0]
                if replay_mask_position[1] < 0:
                    x_diff = -replay_mask_position[1]

                layer[replay_mask_position[0] + y_diff:replay_mask_position[0] + new_height,
                      replay_mask_position[1] + x_diff:replay_mask_position[1] + new_width] \
                    = resized[y_diff:new_height, x_diff:new_width]
                replay_mask = cv2.addWeighted(replay_mask, 1, layer, 1, 0)

                per_obj_mask = cv2.add(per_obj_mask, layer)

                # Save the combined visualization per object
                if not os.path.exists(self.visualization_output + f'_{label}'):
                    os.mkdir(self.visualization_output + f'_{label}')
                cv2.imwrite(self.visualization_output + f'_{label}/{frame_idx:04d}.png', per_obj_mask)

            # Save adjusted visualizations
            cv2.imwrite(self.motion_history_output + f'/{frame_idx:04d}.png', frame_mask)
            frame_weighted = cv2.addWeighted(transparent_img, 0, replay_mask, 1, 0)
            cv2.imwrite(self.replay_output + f'/{frame_idx:04d}.png', frame_weighted)
            frame_weighted = cv2.addWeighted(transparent_img, 0, gray_replay_mask, 0.5, 0)
            cv2.imwrite(self.gray_replay_output + f'/{frame_idx:04d}.png', frame_weighted)
            cv2.imwrite(self.motion_line_output + f'/{frame_idx:04d}.png', motion_line_mask)


            # Combine everything
            frame_weighted = cv2.addWeighted(transparent_img, 0, gray_replay_mask, 0.8, 0)
            frame_weighted = cv2.addWeighted(frame_weighted, 1.0, motion_line_mask, 1.0, 0)
            frame_combined = cv2.add(frame_weighted, replay_mask)
            cv2.imwrite(self.visualization_output + f'/{frame_idx:04d}.png', frame_combined)

    def mark_primary_region(self):
        # Extract the primary region from 360-degree frame
        equ = E2P.Equirectangular(self.er_frame)
        marker_position = self.marker.get_position(self.er_frame)

        if marker_position is not None:
            marker_x, marker_y = marker_position

            # Find the position of the center in spherical coordinates
            c_theta, c_phi = equ.translate_xy_to_spherical(self.center_view[1], self.center_view[0])
            # c_theta -= 90
            c_phi += 180
            c_phi -= 20

            # Find the position of the marker in spherical coordinates
            m_theta, m_phi = equ.translate_xy_to_spherical(marker_x, marker_y)

            # In the first frame, find the offset between primary region and marker
            theta, phi = c_theta - m_theta, c_phi - m_phi

            self.primary_offset = (theta, phi)
            self.primary_marked = True
        else:
            print("NO MARKER FOUND")

    def start_tracking(self):
        self.is_tracking = True

    def finish_tracking(self):
        self.is_tracking = False

    def start_replay(self):
        self.primary_done = True
        return self.obj_set

    def run(self):
        print("Online detector run")
        if self.video_input == "":
            # webcam input
            cap = VideoStream(0)
        else:
            # File Input
            cap = VideoStream(self.video_input)

        cap.read_stream()

        orig_shape = cap.frame.shape
        frame = cv2.resize(cap.frame, (orig_shape[1] // RESIZE_FRAME, orig_shape[0] // RESIZE_FRAME))
        self.frame_h, self.frame_w = frame.shape[:2]
        self.er_frame = frame

        # Center of the frame
        self.view_size = (self.frame_h // RESIZE_H, self.frame_w // RESIZE_W)  # h, w
        p_height, p_width = self.view_size
        self.center_view = (self.frame_h // RESIZE_H, self.frame_w // RESIZE_W)  # y, x

        self.replay_region = np.zeros(self.view_size)

        self.frame_count = 0
        prev_gray = None
        primary_region = None

        last_transform = np.identity(3)

        self.process_detic_async()
        while True:
            cap.read_stream()
            if cap.frame is not None:
                frame = cv2.resize(cap.frame, (orig_shape[1] // RESIZE_FRAME, orig_shape[0] // RESIZE_FRAME))
                self.er_frame = frame
            if self.primary_done:
                break
            if self.mode == "live_streaming" and not self.is_tracking:
                continue
            else:
                pass

            if cap.frame is not None:
                self.frame_count += 1
                cv2.imwrite(self.er_path + f'/{self.frame_count - 1:04d}.jpg', self.er_frame)
                if self.mode == "video_input" and self.frame_count == 1:
                    print("mark primary region")
                    self.mark_primary_region()
                # Extract the primary region from 360-degree frame
                equ = E2P.Equirectangular(frame)
                marker_position = self.marker.get_position(frame)
                if marker_position is not None:
                    marker_x, marker_y = marker_position
                    # Find the position of the marker in spherical coordinates
                    marker_theta, marker_phi = equ.translate_xy_to_spherical(marker_x, marker_y)

                    # if theta is not None and self.primary_marked:
                    if self.primary_marked:
                        # Find the primary region using marker position & offset
                        theta = self.primary_offset[0]
                        phi = self.primary_offset[1]

                        primary_region = equ.GetPerspective(FOV, marker_theta + theta,
                                                            marker_phi + phi, p_height, p_width)


                        # Motion stabilization
                        
                        if self.frame_count <= 1:
                            prev_gray = cv2.cvtColor(primary_region, cv2.COLOR_BGR2GRAY)
                            cv2.imwrite(self.primary_region_path + f'/{self.frame_count - 1:04d}.jpg', primary_region)
                            continue
    
                        # # Pre-define transformation-store array
                        # transforms = np.zeros((n_frames-1, 3), np.float32)
    
                        # Detect feature points in previous frame
                        prev_pts = cv2.goodFeaturesToTrack(prev_gray,
                                                           maxCorners=200,
                                                           qualityLevel=0.01,
                                                           minDistance=30,
                                                           blockSize=3)
    
                        # Convert current frame to grayscale
                        curr_gray = cv2.cvtColor(primary_region, cv2.COLOR_BGR2GRAY)
    
                        # Calculate optical flow (i.e. track feature points)
                        curr_pts, status, err = cv2.calcOpticalFlowPyrLK(prev_gray, curr_gray, prev_pts, None)

                        # Set curr_gray to prev_gray
                        prev_gray = curr_gray
    
                        # Sanity check
                        assert prev_pts.shape == curr_pts.shape
    
                        # Filter only valid points
                        idx = np.where(status==1)[0]
                        prev_pts = prev_pts[idx]
                        curr_pts = curr_pts[idx]
    
                        # Find transformation matrix
                        transform, _ = cv2.estimateAffine2D(prev_pts, curr_pts)

                        if transform is not None:
                            transform = np.append(transform, [[0, 0, 1]], axis=0)
                        if transform is None:
                            transform = last_transform

                        transform = transform.dot(last_transform)
                        last_transform = transform
                        inverse_transform = cv2.invertAffineTransform(transform[:2])
                        primary_region = cv2.warpAffine(primary_region, inverse_transform, (p_width, p_height))
                        
                        ### END STABILIZATION ###

                        # Save primary region
                        self.primary_history.append(primary_region)
                        cv2.imwrite(self.primary_region_path + f'/{self.frame_count - 1:04d}.jpg', primary_region)

                        # Append primary region image to object recognition queue
                        self.images_or.append(primary_region)
                else:
                    print("@@@@@@@ MARKER NOT FOUND @@@@@@@")
                    self.frame_count -= 1

            # Call Detic for self.or_pool_size(=1) frame at a time
            if len(self.images_or) > 1:
                self.or_calc_count += 1
            elif cap.frame is None:
                break

            if not self.is_calc_saliency and primary_region is not None:
                self.images_sal.append(primary_region)
                if len(self.images_sal) == self.len_temporal:
                    print("Run saliency model")
                    self.process_sal_async()

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        while self.varjo_image is None:
            self.varjo_image = cv2.imread("../../VarjoCameraRecorder/VarjoCameraRecorder/bin/frames/left.bmp")
        self.images_or.append(self.varjo_image)
        self.is_varjo_frame = True
        time.sleep(3)
        self.apply_visualization()

        print("APPLY VISUALIZATION DONE")

        # When everything done, release the capture
        cap.stop()
        cap.release()
        cv2.destroyAllWindows()
