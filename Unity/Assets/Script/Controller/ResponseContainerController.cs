using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using Microsoft.MixedReality.Toolkit.UI;


public class ResponseContainerController : MonoBehaviour
{
    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        
    }

    public void OnSliderUpdated(SliderEventData eventData)
    {
        float sliderValue = eventData.NewValue;
        // change the frame index accordingly
        // TODO keep in mind that in needs to scale to multiple regions
    }
}
