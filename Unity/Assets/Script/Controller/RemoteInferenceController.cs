using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Threading;
using Assets.Script.Model;
using Assets.Script.Remote;
using Assets.Script.Util;
using LightBuzz.Jpeg;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Assets.Script.Controller
{
    public class RemoteInferenceController : MonoBehaviour
    {
        public AppModel AppModel;

        public bool AutoStartServer = false;
        public bool AutoConnectToServer = false;
        public bool EnableDebugRemote = false;
        public bool EnableDebug = false;
        public int Port = 9999;
        public int InferencerType = 0;

        /*
         * 0: Overlay
         * 1: Saliency map
         * 2: Saliency maps diff
         * 3: Combined (slow)
         */
        public int ResultType = 0;

        public string CondaEnvPath = @"C:\Users\hyunsung\Anaconda3\envs\missed-reality\";
        public string ServerFileName = @"inference_server.py";
        public string ServerPath = @"C:\Users\hyunsung\2022-Missed-Events\";
        public string ScriptPath = @"networking\";

        public CamTextureSource[] Inferencer;

        public Material ResponseMaterial;
        public Transform ResponseContainer;
        
        private bool _started;

        private Client _client;
        private Process _serverProcess;

        private Dictionary<CamTextureSource, Color32[]> _currentFrames;
        private Dictionary<CamTextureSource, bool> _framesReadyFlags;

        private bool _sendFrames;
        private bool _newDataToSendAvailable;
        private bool _dataThreadRunning;

        private bool _receiveFrames;
        private Thread _imageReceiveThread;

        private Texture2D _responseTexture2D;
        private int _numEnabledInferences;

        private bool _newSaliencyMapAvailable;
        private List<byte[]> _highlightFrames;      // TODO need more for multiple regions of interests
        private byte[] _currentSaliencyMap;
        private byte[] _currentSaliencyMapDecoded;

        private Color32[] _allFrames;
        private JpegEncoder _encoder;
        private JpegDecoder _decoder;

        void Start()
        {

        }

        // Update is called once per frame
        void Update()
        {
            if (!_started)
            {
                _started = true;

                //_numEnabledInferences = Inferencer.Count(inferencer => inferencer.enabled);
                _numEnabledInferences = 1;

                _allFrames = new Color32[_numEnabledInferences * AppModel.CamTextureSize.x * AppModel.CamTextureSize.y];
                _encoder = new JpegEncoder();
                _decoder = new JpegDecoder();

                var numResultType = ResultType < 3 ? 1 : 3;

                ResponseContainer.localScale = new Vector3(ResponseContainer.localScale.x * _numEnabledInferences, ResponseContainer.localScale.y, ResponseContainer.localScale.z * numResultType);
                _responseTexture2D = new Texture2D(AppModel.CamTextureSize.x * _numEnabledInferences, AppModel.CamTextureSize.y, TextureFormat.RGBA32, false);
                ResponseMaterial.mainTexture = _responseTexture2D;

                _currentFrames = new Dictionary<CamTextureSource, Color32[]>();
                _framesReadyFlags = new Dictionary<CamTextureSource, bool>();

                //foreach (var inferencer in Inferencer)
                //{
                //    _currentFrames.Add(inferencer, null);
                //    _framesReadyFlags.Add(inferencer, false);

                //    inferencer.NewFrameAvailable += HandleNewFrameAvailable;
                //}

                var dataThread = new Thread(SendFramesAsync);
                _dataThreadRunning = true;
                dataThread.Start();

                if (AutoStartServer)
                {
                    StartServer();
                }

                if (AutoConnectToServer)
                {
                    InitClient();
                }
            }

            if (_newSaliencyMapAvailable && EnableDebug)
            {
                _newSaliencyMapAvailable = false;

                //_responseTexture2D.LoadRawTextureData(_currentSaliencyMapDecoded);

                _responseTexture2D.LoadImage(_currentSaliencyMap);
                _responseTexture2D.Apply();
            }
        }

        void OnGUI()
        {

            if (GUILayout.Button("Start server"))
            {
                StartServer();
            }

            if (GUILayout.Button("Connect"))
            {
                InitClient();
            }

            if (GUILayout.Button("Disconnect"))
            {
                Disconnect();
            }

            if (GUILayout.Button("Receive image"))
            {
                ReceiveTestImage();
            }

            if (GUILayout.Button("Test"))
            {
                TestConnection();
            }

            if (!_sendFrames)
            {
                if (GUILayout.Button("Send Frames Start"))
                {
                    _sendFrames = true;
                    SendToAllInferencers(typeof(CamTextureSource).GetMethod("HandleStartSendFrame"));
                }
            }
            else
            {
                if (GUILayout.Button("Send Frames End"))
                {
                    _sendFrames = false;
                    SendToAllInferencers(typeof(CamTextureSource).GetMethod("HandleEndSendFrame"));
                }
            }

            if (!_receiveFrames)
            {
                if (GUILayout.Button("Receive Frames Start"))
                {
                    HandleStartReceiveFrame();
                    _receiveFrames = true;
                }
            }
            else
            {
                if (GUILayout.Button("Receive Frames End"))
                {
                    _receiveFrames = false;
                }
            }
        }

        public void StartServer()
        {
            var fileName = CondaEnvPath + (EnableDebugRemote ? "python.exe" : "pythonw.exe");
            var arguments = ServerPath + ScriptPath + ServerFileName +
                                    " --port " + Port +
                                    " --debug " + (EnableDebugRemote ? "True" : "False") +
                                    " --num_images " + _numEnabledInferences +
                                    " --result_type " + ResultType;

            _serverProcess = CommandLineUtil.ExecuteCommand(ServerPath, fileName, arguments);
        }

        public void InitClient()
        {
            if (_client != null)
            {
                Debug.LogWarning("Client already connected");
                return;
            }

            _client = new Client();
            _client.ConnectToTcpServer(Port);
            _client.Send("STARTUP");

            var response = _client?.ListenSync();
            Debug.Log(response);

            GameObject sceneCamObj = GameObject.Find("ScreenCamera");
            if (sceneCamObj != null)
            {
                // Print the real dimensions of scene viewport
                Debug.Log(sceneCamObj.GetComponent<Camera>().pixelRect);
            }
        }

        void SendToAllInferencers(MethodInfo methodInfo)
        {
            foreach (var inferencer in Inferencer)
            {
                if (inferencer != null && inferencer.Started)
                    methodInfo.Invoke(inferencer, null);
            }
        }

        private void HandleNewFrameAvailable(object sender, FrameReadyEventArgs e)
        {
            var currentInferencer = (CamTextureSource)sender;

            _currentFrames[currentInferencer] = currentInferencer.CurrentFrame;
            _framesReadyFlags[currentInferencer] = true;

            foreach (var inferencer in Inferencer)
            {
                if (inferencer.Started && !_framesReadyFlags[inferencer])
                    return;
            }

            _newDataToSendAvailable = true;

            foreach (var inferencer in Inferencer)
            {
                if (inferencer.Started)
                    _framesReadyFlags[inferencer] = false;
            }
        }

        private void SendFramesAsync(object obj)
        {
            while (_dataThreadRunning)
            {
                if (_newDataToSendAvailable)
                {
                    _newDataToSendAvailable = false;

                    SendFrames();
                }
            }

        }

        private void SendFrames()
        {
            if (_client == null)
            {
                Debug.LogWarning("Client not initialized. Cannot send image.");
                _sendFrames = false;
                return;
            }

            /*
             * reverse loop. python splits the data, which reverses the order.
             * the reverse loop avoid having to reverse the data in python
             */
            var runner = 0;
            for (var index = Inferencer.Length - 1; index >= 0; index--)
            {
                var inferencer = Inferencer[index];
                if (inferencer.Started)
                {
                    var frame = _currentFrames[inferencer];
                    frame.CopyTo(_allFrames, runner * frame.Length);
                    runner++;
                }
            }

            var allFramesBytes = TextureUtil.Color32ArrayToByteArray(_allFrames);

            //ms.Add(w.ElapsedMilliseconds);
            var encodedFrames = _encoder.Encode(allFramesBytes, AppModel.CamTextureSize.x, AppModel.CamTextureSize.y * _numEnabledInferences);
            SendFrames(encodedFrames);

            //SendFrames(allFramesBytes);


            //var texture2D = new Texture2D(384, 224 * _numEnabledInferences, TextureFormat.RGBA32, false);
            ////Set hide flags to avoid memory leak.
            //texture2D.hideFlags = HideFlags.HideAndDontSave;
            //texture2D.SetPixels32(allFrames);
            //texture2D.Apply();

            //var buffer = texture2D.EncodeToJPG();

            //var sendFramesThread = new Thread(() => SendFrames(encodedFrames));
            //sendFramesThread.Start();

            //Destroy to avoid memory leak.
            //Destroy(texture2D);
        }

        private void SendFrames(byte[] allBuffers)
        {
            if (_client == null)
            {
                Debug.LogWarning("Client not initialized. Cannot send image.");
                _sendFrames = false;
                return;
            }

            _sendFrames = _client.Send(allBuffers);

            if (!_sendFrames)
                SendToAllInferencers(typeof(CamTextureSource).GetMethod("HandleEndSendFrame"));
        }

        public void ReceiveTestImage()
        {
            _client.Send("TESTRECEIVE");
            var response = _client.ListenSyncRaw();

            _currentSaliencyMap = response;
            if (_decoder == null)
                _decoder = new JpegDecoder();
            _currentSaliencyMapDecoded = _decoder.Decode(_currentSaliencyMap);
            _newSaliencyMapAvailable = true;
        }

        public void TestConnection()
        {
            _client.Send("TEST");
            string response = _client.ListenSync();
            Debug.Log(response);
        }

        public void HandleStartReceiveFrame()
        {
            _receiveFrames = true;

            if (_imageReceiveThread == null || !_imageReceiveThread.IsAlive)
            {
                Debug.Log("Start receiving frames");
                _imageReceiveThread = new Thread(ReceiveFrames);
                _imageReceiveThread.Start();
            }


        }

        private void ReceiveFrames()
        {
            while (_receiveFrames)
            {
                Debug.Log("inside while");
                //if (_client == null)
                //    Debug.Log("client is null");
                //    return;

                Debug.Log("Send RECEIVE");
                _receiveFrames = _client.Send("RECEIVE");
                var response = _client.ListenSyncRaw();
                Debug.Log("receiving frames");
                Debug.Log(response.Length);
                if (response.Length < 100)
                    continue;

                _currentSaliencyMap = response;
                _currentSaliencyMapDecoded = _decoder.Decode(_currentSaliencyMap);
                // TODO Add to array
                _newSaliencyMapAvailable = true;
            }
        }

        public void Disconnect(int sleepBetweenShutdownMS = 0)
        {
            _sendFrames = false;
            SendToAllInferencers(typeof(CamTextureSource).GetMethod("HandleEndSendFrame"));
            Thread.Sleep(sleepBetweenShutdownMS);

            _receiveFrames = false;
            Thread.Sleep(sleepBetweenShutdownMS);

            if (_client != null)
            {
                _client.Send("SHUTDOWN");
                var response = _client?.ListenSync();
                Debug.Log(response);

                _client.Close();
                _client = null;
            }

            _serverProcess?.Close();
        }

        void OnApplicationQuit()
        {
            _dataThreadRunning = false;

            Disconnect(100);
        }
    }
}
