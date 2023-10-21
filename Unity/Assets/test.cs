using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace Microsoft.MixedReality.Toolkit.UI
{
    public class test : MonoBehaviour
    {
        // Start is called before the first frame update
        void Start()
        {
            bool supportsArticulatedHands = false;
            bool supportsMotionController = false;

            IMixedRealityCapabilityCheck capabilityCheck = CoreServices.InputSystem as IMixedRealityCapabilityCheck;
            if (capabilityCheck != null)
            {
                supportsArticulatedHands = capabilityCheck.CheckCapability(MixedRealityCapability.ArticulatedHand);
                supportsMotionController = capabilityCheck.CheckCapability(MixedRealityCapability.MotionController);

            }
            Debug.Log("TEST");
            Debug.Log(supportsArticulatedHands);
            Debug.Log(supportsMotionController);
          
        }

        // Update is called once per frame
        void Update()
        {

        }
    }

}