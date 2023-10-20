using System;
using Unity.Collections;
using UnityEngine;

namespace Assets.Script.Remote
{
    public class FrameReadyEventArgs : EventArgs
    {
        public Color32[] Frame { get; set; }
    }
}