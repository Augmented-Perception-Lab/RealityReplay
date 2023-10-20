using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using Assets.Script.Model;
using Assets.Script.Util;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;
using Debug = UnityEngine.Debug;
using Random = UnityEngine.Random;

namespace Assets.Script.Remote
{
    public class CamTextureSource : MonoBehaviour
    {
        public AppModel AppModel;

        public event EventHandler<FrameReadyEventArgs> NewFrameAvailable;

        public Color32[] CurrentFrame { get; set; }

        public bool Started { get; set; }

        private bool _sendFrames;

        /*
         * Using AsyncGPU readback to send images to python faster without sacrificing framerate. This might introduce a slight delay.
         * Note that the HDR setting in the camera must be TURNED OFF to avoid a false aspect ratio when getting the buffer.
         */
        private readonly Queue<AsyncGPUReadbackRequest> _requests = new Queue<AsyncGPUReadbackRequest>();

        void Start()
        {
        }

        void Update()
        {
            if (!Started)
            {
                Started = true;
                CurrentFrame = new Color32[AppModel.CamTextureSize.x * AppModel.CamTextureSize.y];
            }

            if (_sendFrames)
            {
                while (_requests.Count > 0)
                {
                    var req = _requests.Peek();

                    if (req.hasError)
                    {
                        Debug.Log("GPU readback error detected.");
                        _requests.Dequeue();
                    }
                    else if (req.done)
                    {
                        var buffer = req.GetData<Color32>();

                        if (Time.frameCount % AppModel.FrameRateFactor == 0)
                        {
                            //CurrentFrame = new Color32[buffer.Length];

                            //CurrentFrame = buffer.ToArray();
                            //NewFrameAvailable?.Invoke(this, new FrameReadyEventArgs() { });

                            
                            CopyToCurrentFrame(buffer);

                            NewFrameAvailable?.Invoke(this, new FrameReadyEventArgs() { });

                            //var thread = new Thread(() =>
                            //{
                            //    CopyToCurrentFrame(buffer);
                            //    NewFrameAvailable?.Invoke(this, new FrameReadyEventArgs() { });
                            //});
                            //thread.Start();

                            //var t = new Thread(InvokeNewFrameAsync);
                            //t.Start();

                            //Color32[] toArray = new Color32[buffer.Length];
                            //buffer.CopyTo(toArray);

                            //NewFrameAvailable?.Invoke(this, new FrameReadyEventArgs() { });
                        }

                        _requests.Dequeue();
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        private void CopyToCurrentFrame(NativeArray<Color32> buffer)
        {
            buffer.CopyTo(CurrentFrame);
        }

        void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (_sendFrames)
            {
                if (_requests.Count < 8)
                    _requests.Enqueue(AsyncGPUReadback.Request(source));
                else
                    Debug.Log("Too many requests.");

                Graphics.Blit(source, destination);
            }
        }

        //Method is called by name through reflection
        public void HandleStartSendFrame()
        {
            _sendFrames = true;
        }

        //Method is called by name through reflection
        public void HandleEndSendFrame()
        {
            _sendFrames = false;
        }
    }
}
