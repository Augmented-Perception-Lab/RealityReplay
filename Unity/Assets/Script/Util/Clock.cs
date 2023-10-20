using System;
using TMPro;
using UnityEngine;

namespace Assets.Script.Util
{
    public class Clock : MonoBehaviour
    {
        public TextMeshPro TextObject;
        
        // Start is called before the first frame update
        void Start()
        {
            
        }

        //Adapted from https://subscription.packtpub.com/book/game_development/9781784391362/1/ch01lvl1sec11/displaying-a-digital-clock
        void Update()
        {
            var time = DateTime.Now;
            var hour = LeadingZero(time.Hour);
            var minute = LeadingZero(time.Minute);
            var second = LeadingZero(time.Second);
            var millisecond = LeadingZero(time.Millisecond, 3);

            TextObject.text = hour + ":" + minute + ":" + second + ":" + millisecond;
        }

        string LeadingZero(int n, int pad = 2)
        {
            return n.ToString().PadLeft(pad, '0');
        }
    }
}
