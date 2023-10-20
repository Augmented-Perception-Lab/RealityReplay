using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System;
using TMPro;
using UnityEngine.XR;
using System.Timers;
using System.Text;
using System.IO;

public class SecondaryTaskController : MonoBehaviour
{
    public string filePath = "C:/Users/hyunsung/2022-Missed-Events/results/";
    private static GameObject letterCanvasObj;
    private static GameObject letterObj;
    private static TMP_Text letter;

    private static Timer timer;
    
    // to measure response time
    private static DateTime startTime;

    private static bool shouldUpdate;

    private static StringBuilder csv;

    // Start is called before the first frame update
    void Start()
    {
        letterCanvasObj = GameObject.FindWithTag("LetterCanvas");

        letterObj = GameObject.FindWithTag("Letter");
        letter = letterObj.GetComponent<TMP_Text>();

        shouldUpdate = false;
        SetTimer();

        // CSV to save results
        csv = new StringBuilder();
        filePath += DateTime.Now.ToString("yyyy-MM-dd-H-mm-ss");
        filePath += ".csv";
    }

    // Update is called once per frame
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.LeftArrow))
        {
            if (timer.Enabled)
            {
                timer.Stop();
            }
            timer.Start();
            Debug.Log("left arrow pressed");

            WriteToCsv("left", true);

            // Update letter and position
            shouldUpdate = true;
        }

        if (Input.GetKeyDown(KeyCode.RightArrow))
        {
            if (timer.Enabled)
            {
                timer.Stop();
            }
            timer.Start();
            Debug.Log("right arrow pressed");

            WriteToCsv("right", true);

            // Update letter and position
            shouldUpdate = true;
        }

        if (Input.GetKeyDown(KeyCode.Escape))
        {
            timer.Stop();
            File.WriteAllText(filePath, csv.ToString());
        }

        if (shouldUpdate)
        {
            UpdateLetter();
            UpdatePosition();
            shouldUpdate = false;
        }
    }

    private void OnDestroy()
    {
        timer.Stop();
        File.WriteAllText(filePath, csv.ToString());
    }

    private static void WriteToCsv(string lr, bool pressed)
    {
        DateTime now = DateTime.Now;
        TimeSpan ts = now - startTime;
        var newLine = string.Format("{0},{1},{2},{3}", letter.text, lr, pressed, ts.Milliseconds);
        csv.AppendLine(newLine);
    }

    private static char DrawLetter()
    {
        int randomLetter = 75;  // K
        float prob = GetRandomFloat(0.0, 1.0);
        // Target-Distractor ratio = 1:7
        if (prob > 0.125)
        {
            while (randomLetter.Equals(75))
            {
                System.Random rnd = new System.Random();
                randomLetter = rnd.Next(65, 91);

            };
        }

        // To record the response time
        startTime = DateTime.Now;

        return Convert.ToChar(randomLetter);
    }

    private static float GetRandomFloat(double minimum, double maximum)
    {
        System.Random rnd = new System.Random();
        return (float)(rnd.NextDouble() * (maximum - minimum) + minimum);
    }

    public static void UpdateLetter() 
    {
        letter.text = DrawLetter().ToString();
    }

    private static void UpdatePosition() 
    {
        double minimum = -0.9;
        double maximum = 0.9;

        float randX = GetRandomFloat(minimum, maximum);
        float randY = GetRandomFloat(minimum, maximum);

        letterCanvasObj.transform.localPosition = new Vector3(randX, randY, 0.0F);
    }

    private static void SetTimer()
    {
        // Create a timer with a two second interval.
        timer = new Timer(2000);

        // Hook up the Elapsed event for the timer.
        timer.Elapsed += OnTimedEvent;
        timer.AutoReset = true;
        timer.Enabled = false;
    }

    private static void OnTimedEvent(System.Object source, ElapsedEventArgs e)
    {
        Debug.Log(String.Format("The Elapsed event was raised at {0:HH:mm:ss.fff}", e.SignalTime));

        int newInterval = (new System.Random()).Next(1000, 2000);
        timer.Interval = newInterval;

        WriteToCsv("none", false);

        // Update letter and position
        shouldUpdate = true;
    }
}
