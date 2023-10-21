# RealityReplay

GitHub repo for 

This repository contains the source code for our paper: 

[**RealityReplay: Detecting and Replaying Temporal Changes In Situ using Mixed Reality**](https://augmented-perception.org/publications/2023-realityreplay.html)<br>
Hyunsung Cho, Matthew Komar, David Lindlbauer<br>
IMWUT 2023

If you find this repository useful, please cite our paper:
```bibtex
@inproceedings {Cho23, 
 author = {Cho, Hyunsung and Komar, Matthew and Lindlbauer, David}, 
 title = {RealityReplay: Detecting and Replaying Temporal Changes In Situ using Mixed Reality}, 
 year = {2023}, 
 publisher = {Association for Computing Machinery}, 
 address = {New York, NY, USA}, 
 url = {https://doi.org/10.1145/3610888}, 
 doi = {10.1145/3610888}, 
 keywords = { Mixed Reality, Augmented Reality, Computational Interaction}, 
 location = {Cancun, Mexico}, 
 series = {IMWUT '23} 
}
```

## Hardware Requirements
- Ricoh Theta V (connected to the computer in live streaming mode)
- Varjo XR-3 headset (connected to the computer with Varjo Base and SteamVR installed) 
- Windows 10 computer

## Installation
Clone this repository. 

Go to `Python` directory.
```bash
cd Python
```

Create a conda environment. 

```bash
conda create --name realityreplay python=3.8 -y
conda activate realityreplay
conda install pytorch torchvision torchaudio cudatoolkit=11.1 -c pytorch-lts -c nvidia
```

Install Detic according to the Detic installation instructions [here](https://github.com/facebookresearch/Detic).

```bash
git clone git@github.com:facebookresearch/detectron2.git
cd detectron2
pip install -e .

cd ..
git clone https://github.com/facebookresearch/Detic.git --recurse-submodules
cd Detic
pip install -r requirements.txt
```


Install all required Python packages.
```bash
pip install -r requirements.txt
```

## Run
First, run the Python server. Inside `Python` directory,
```bash
python run.py
```

Open `Unity` in Unity Editor (editor version 2020.3.14f1) and click the Play button. 

Open `VarjoCameraRecorder/VarjoCameraRecorder/bin/MRCameraRecorder.exe` to grab the Varjo view frames.s

Use the Unity GUI buttons (e.g., Connect, Mark Primary Region, Start Tracking, Finish Tracking) in the Unity Game view for interaction.


