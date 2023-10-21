using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using Varjo.XR;


public class CameraSetup : MonoBehaviour
{
    // Start is called before the first frame update
    void Start()
    {
        VarjoRendering.SetOpaque(false);
        VarjoMixedReality.StartRender();
        //VarjoMixedReality.LockCameraConfig();
        VarjoMixedReality.EnableDepthEstimation();
        //VarjoMixedReality.SetCameraPropertyMode();
    }
    // Update is called once per frame
    void Update()
    {
        
    }
}
