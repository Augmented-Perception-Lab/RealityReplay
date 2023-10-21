## Python-Unity Networking Interface
The core Python-Unity networking interface is written in`RemoteInferenceController.cs`.

### How networking is set up
The code executes a Python command to start the Python server in `public void StartServer()`.
The program initializes a client that connects to the Python server and establishes a TCP connection in `public void InitClient()`.
`StartServer()` and `InitClient()` are automatically called on start, but you may also initialize the server by running the Python command yourself in a console, and initialize the client by clicking the GUI `Connect` button when you play the Unity scene.

### Updating UI based on data communication
`void Update()` is called once per frame and updates the textures.
`_responseTexture2D` is the texture for displaying the image received from Python 
(`_currentSaliencyMap` for JPG images or `_currentSaliencyMapDecoded`) for decoded images.

### Test: Receiving an image from Python
Play the Unity scene and click `Receive image` button. 