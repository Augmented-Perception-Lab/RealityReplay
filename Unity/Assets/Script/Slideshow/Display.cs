using UnityEngine;
using System.IO;
//using System.Drawing;
using System.Collections;
using Microsoft.MixedReality.Toolkit.UI;
using Assets.Script.Remote;
using Assets.Script.Util;
using UnityEngine.UI;
using System.Collections.Generic;
using System.Linq;
using Microsoft.MixedReality.Toolkit.Utilities;


namespace Assets.Script.Controller
{
    public class Display : MonoBehaviour
    {//https://docs.unity3d.com/ScriptReference/Material.SetTexture.html
        public string ServerPath = @"..\";
        public string ScriptPath = @"networking\";
        public string ServerFileName = @"inference_server.py";
        public string ExamplePath = @"..\output\live_1663625656";


        public Interactable objCheckbox;

        public string imagePrefix = "";
    
        public string file = "../output/good_test";

        private string lastRunPath = "../output/lastRun.txt";

        public bool EnableDebugRemote = false;
        public int Port = 9999;
        //private Process _serverProcess;

        [HideInInspector]
        public int imageAmount = 0056;
        [HideInInspector]
        private bool isActivated = false;

        private int currentSlide = 0;
        public float playRate = 0.1f;
        Renderer renderer;

        private Client _client;

        public string visualization = "visualization";
        private Text currVis;
        private PinchSlider slider;
        private byte[] bytes;
        private Texture2D frame;
        private RenderTexture frameTex;

        private Dictionary<string, byte[]> vis_map;
        private Dictionary<string, Texture2D> combined_vis_map;

        public List<string> objList;
        public List<string> selObjList;

        private bool _started = false;

        //Pads numbers to fit the image numbering standard
        string padNumbers(int i, int length)
        {
            string number = i.ToString();
            int numberToPad = length - number.Length;
            string result = "";
            for (int x = 0; x < numberToPad; x++)
            {
                result += "0";
            }
            result += number;
            //print(result);
            return result;
        }


        public void UpdateVisualizations()
        {
            string fileExt = ".png";
            string vis_path;

            if (selObjList.Count > 0)
            {
                vis_path = file + "/" + visualization + "_" + selObjList[0];
                string[] files = Directory.GetFiles(vis_path, "*", SearchOption.TopDirectoryOnly);

                if (selObjList.Count > 1)
                {

                    for (int i = 0; i < files.Length; i++)
                    {
                        frame = new Texture2D(2, 2);
                        frame.hideFlags = HideFlags.HideAndDontSave;
                        vis_path = file + "/" + visualization + "_" + selObjList[0] + @"\" + imagePrefix + padNumbers(i, 4) + fileExt;
                        try
                        {
                            bytes = vis_map[vis_path];
                            frame.LoadImage(bytes);
                        }
                        catch
                        {
                            Debug.Log("file not found: " + vis_path);
                        }

                        for (var j = 1; j < selObjList.Count; j++)
                        {
                            vis_path = file + "/" + visualization + "_" + selObjList[j] + "/" + imagePrefix + padNumbers(j, 4) + fileExt;
                            try
                            {
                                bytes = vis_map[vis_path];
                            }
                            catch
                            {
                                Debug.Log("file not found: " + vis_path);
                            }
                            Texture2D objTex = new Texture2D(2, 2);
                            objTex.LoadImage(bytes);

                            int ix = 0;
                            Color[] objPixels = objTex.GetPixels();
                            for (int y = 0; y < frame.height; y++)
                            {
                                for (int x = 0; x < frame.width; x++)
                                {
                                    Color c = frame.GetPixel(x, y);
                                    frame.SetPixel(x, y, Color.Lerp(c, objPixels[ix], objPixels[ix].a));
                                    ix++;
                                }
                            }
                            frame.Apply();
                        }
                        selObjList = selObjList.OrderBy(q => q).ToList();
                        var combined_vis_map_key = i.ToString() + "_" + string.Join(",", selObjList.ToArray());
                        Debug.Log("saving key: " + combined_vis_map_key);
                        combined_vis_map[combined_vis_map_key] = frame;
                    }
                }

            }
        }

        //reads slide from frame
        void setSlide(int i, string visualization)
        {

            string fileExt;
            string vis_path;
            if (visualization.Equals("primary_region"))
            {
                fileExt = ".jpg";
            }
            else
            {
                fileExt = ".png";
            }

            try
            {
                Destroy(frameTex);
                Destroy(frame);
            }
            catch
            {
                Debug.Log("ignore first frame for destroy");
            }


            frame = new Texture2D(2, 2);
            frame.hideFlags = HideFlags.HideAndDontSave;

            // Get texture

            if (visualization.Equals("primary_region"))
            {
                vis_path = file + "/" + visualization + @"\" + imagePrefix + padNumbers(i, 4) + fileExt;

                try
                {
                    bytes = vis_map[vis_path];
                    print(vis_path);
                    print("Set Slide " + i);
                }
                catch
                {
                    print("file not found: " + vis_path);
                    bytes = File.ReadAllBytes("../output/transparent.png");
                }
                frame.LoadImage(bytes);
            }
            else if (visualization.Equals("visualization"))
            {
                vis_path = file + "/" + visualization + @"\" + imagePrefix + padNumbers(i, 4) + fileExt;
                try
                {
                    bytes = vis_map[vis_path];
                    print(vis_path);
                    print("Set Slide " + i);
                }
                catch
                {
                    print("file not found: " + vis_path);
                    bytes = File.ReadAllBytes("../output/transparent.png");
                }
                frame.LoadImage(bytes);
            }
            else
            {

                if (selObjList.Count > 0)
                {
                    vis_path = file + "/visualization_" + selObjList[0] + @"\" + imagePrefix + padNumbers(i, 4) + fileExt;
                    try
                    {
                        //bitmap = new Bitmap(vis_path);
                        //graphics = System.Drawing.Graphics.FromImage(bitmap);
                        bytes = vis_map[vis_path];
                        frame.LoadImage(bytes);
                    }
                    catch
                    {
                        Debug.Log("file not found: " + vis_path);
                    }
                }
                if (selObjList.Count > 1)
                {
                    //selObjList = selObjList.OrderBy(q => q).ToList();
                    //var combined_vis_map_key = i.ToString() + "_" + string.Join(",", selObjList.ToArray());
                    //Debug.Log("vis map key: " + combined_vis_map_key);
                    //frame = combined_vis_map[combined_vis_map_key];
                    for (var j = 1; j < selObjList.Count; j++)
                    {
                        vis_path = file + "/visualization_" + selObjList[j] + @"\" + imagePrefix + padNumbers(i, 4) + fileExt;

                        try
                        {
                            bytes = vis_map[vis_path];
                        }
                        catch
                        {
                            Debug.Log("file not found: " + vis_path);
                        }


                        Texture2D objTex = new Texture2D(2, 2);
                        objTex.LoadImage(bytes);

                        int ix = 0;
                        Color[] objPixels = objTex.GetPixels();
                        for (int y = 0; y < frame.height; y++)
                        {
                            for (int x = 0; x < frame.width; x++)
                            {
                                Color c = frame.GetPixel(x, y);
                                frame.SetPixel(x, y, Color.Lerp(c, objPixels[ix], objPixels[ix].a));
                                ix++;
                            }
                        }
                        frame.Apply();
                    }
                }
            }


            //Fetch the Renderer from the GameObject
            renderer = GetComponent<Renderer>();
            if (i >= imageAmount + 1 || i <= 0)
            {
                print("slider attempted to access an element out of bounds");
                return;
            }

            
            //print("width, height: " + frame.width + ", " + frame.height);

            // Set the Texture you assign in the Inspector as the main texture (Or Albedo)
            //renderer.material.SetTexture("_MainTex", frameTex);
            renderer.material.SetTexture("_MainTex", frame);
            //renderer.material.SetTextureScale("_MainTex", new Vector2(-1, 1));
            Resources.UnloadUnusedAssets();
            System.GC.Collect();
        }

        float map(float x, float in_min, float in_max, float out_min, float out_max)
        {
            return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
        }

        //is used with button  from MRTK
        public void activate()
        {
            if (!isActivated)
            {
                isActivated = true;
            }
            else
            {
                isActivated = false;

            }
        }
        private void setVisualization()
        {
            string file2 = file + "/" + visualization;
            try
            {
                imageAmount = Directory.GetFiles(file2, "*", SearchOption.TopDirectoryOnly).Length - 1;
            }
            catch
            {
                imageAmount = 0;
            }

            // Set initial value to the last frame
            slider.SliderValue = 1.0f;
            //setSlide(imageAmount, visualization);
        }

        // Callback for motion line button
        public void setMotionLine()
        {
            visualization = "motion_line";
            setVisualization();
            setVisualizationText("Motion Line");
        }

        // Callback for motion history button
        public void setMotionHistory()
        {
            visualization = "gray_replay";
            setVisualization();
            setVisualizationText("Motion History");
        }


        // Callback for motion replay button
        public void setMotionReplay()
        {
            visualization = "replay";
            setVisualization();
            setVisualizationText("Motion Replay");
        }

        // Callback for scene playback button
        public void setScenePlayback()
        {
            visualization = "primary_region";
            setVisualization();
            setVisualizationText("Scene Playback");
        }

        // Callback for combined vis button
        public void setCombinedVis()
        {
            visualization = "visualization";
            setVisualization();
            setVisualizationText("Combined");
        }

        public void setObjectVis()
        {
            visualization = "visualization_object";
        }

        private void setVisualizationText(string txt)
        {
            currVis.text = txt;
        }

        // Callback for PinchSlider
        public void showSlide(SliderEventData eventData) {
            string filePath = "";
            if (visualization.Equals("visualization_object"))
            {
                filePath = file + "/visualization";
            }
            else 
            {
                filePath = file + "/" + visualization;
            }
            try
            {
                imageAmount = Directory.GetFiles(filePath, "*", SearchOption.TopDirectoryOnly).Length - 1;
            }
            catch
            {
                imageAmount = 0;
            }
            int output = (int)map(eventData.NewValue, 0, 1, 0, imageAmount);
            setSlide(output, visualization);
        }

        //loops through all the slides from startSlide at a given playrate (in seconds). 
        public IEnumerator playSlides(float playRate, int startSlide = 1)
        {
            for (int i = startSlide; i < imageAmount; i++)
            {
                yield return new WaitForSeconds(playRate);
                setSlide(i, "replay");
            }
            //StartCoroutine(playSlides(playRate));
            yield return null;
        }
        
        void Start()
        {
        }

        void Update()
        {
            if(!_started)
            {
                _started = true;
                DoStart();
            }
        }

        // Start is called before the first frame update
        void DoStart()
        {
            vis_map = new Dictionary<string, byte[]>();
            combined_vis_map = new Dictionary<string, Texture2D>();
            //string file2 = file + '/' + visualization;
            ////figure out how many images in directory.
            //imageAmount = Directory.GetFiles(file2, "*", SearchOption.TopDirectoryOnly).Length;
            byte[] bytes = File.ReadAllBytes("Assets/Resources/transparent.png");

            renderer = GetComponent<Renderer>();
            Texture2D frame = new Texture2D(2, 2);
            frame.LoadImage(bytes);

            // Set the Texture you assign in the Inspector as the main texture (Or Albedo)
            renderer.material.SetTexture("_MainTex", frame);

            GameObject visTextObj = GameObject.FindWithTag("VisualizationText");
            currVis = visTextObj.GetComponent<Text>();
            currVis.text = "Motion Line";

            GameObject sliderObj = GameObject.FindWithTag("Slider");
            slider = sliderObj.GetComponent<PinchSlider>();
            slider.SliderValue = 1.0f;
        }

        //public void StartServer()
        //{
        //    var fileName = CondaEnvPath + (EnableDebugRemote ? "python.exe" : "pythonw.exe");
        //    var arguments = ServerPath + ScriptPath + ServerFileName +
        //                            " --port " + Port +
        //                            " --debug " + (EnableDebugRemote ? "True" : "False");

        //    _serverProcess = CommandLineUtil.ExecuteCommand(ServerPath, fileName, arguments);
        //}

        public void InitClient()
        {
            Debug.Log("Init client is called");
            if (_client != null)
            {
                Debug.LogWarning("Client already connected");
                return;
            }

            _client = new Client();
            _client.ConnectToTcpServer(Port);
            if (_client == null)
            {
                Debug.Log("It's already null but somehow works");
            }
            
            _client.Send("STARTUP");
            Debug.Log("Sent STARTUP");
            var response = _client?.ListenSync();
            Debug.Log(response);
            // Save output path
            file = response;
        }

        public void Disconnect(int sleepBetweenShutdownMS = 0)
        {
            _client.Send("SHUTDOWN");
            var response = _client?.ListenSync();
            Debug.Log(response);

            _client.Close();
            _client = null;
        }

        public void MarkPrimaryRegion()
        {
            Debug.Log("MARK PRIMARY REGION");

            // Fix the slide position by detaching it from the parent camera object
            var slide = GameObject.FindWithTag("Slide").transform;
            slide.parent = null;
            var slideGlobalPosition = slide.position;
            Debug.Log("slide global position: ");
            Debug.Log(slideGlobalPosition);
            string[] lines = { file, slideGlobalPosition.ToString() };
            File.WriteAllLines(lastRunPath, lines);

            using (StreamWriter sw = File.AppendText(file + "/metadata.txt"))
            {
                sw.WriteLine(slideGlobalPosition.ToString());
            }
            

            // Fix the secondary task position by detaching it from the parent camera object
            var secondaryTask = GameObject.FindWithTag("SecondaryTask").transform;
            secondaryTask.parent = null;
            // TODO: add secondarytask position to lastRunPath as well?

            // Set the slide's rotation w.r.t. the world (after detaching from the camera)
            slide.transform.eulerAngles = new Vector3(185, 0, 0);

            if (_client == null)
            {
                Debug.Log("Sorry it's null but you can try");
            }
            _client.Send("MARK_PRIMARY_REGION");
            var response = _client?.ListenSync();
            Debug.Log(response);
        }

        public void StartTracking()
        {
            Debug.Log("START TRACKING");
            _client.Send("START_TRACKING");
            var response = _client?.ListenSync();
            Debug.Log(response);
        }

        public void FinishTracking()
        {
            Debug.Log("FINISH TRACKING");
            _client.Send("FINISH_TRACKING");
            var response = _client?.ListenSync();
            Debug.Log(response);
        }

        public void StartReplay()
        {
            Debug.Log("START REPLAY");
            _client.Send("START_REPLAY");
            var response = _client?.ListenSync();
            using (StreamWriter sw = File.AppendText(file + "/metadata.txt"))
            {
                sw.WriteLine(response);
            }
            objList = response.Split(',').ToList<string>();
            Debug.Log(response);
        }



        public void LoadVisualizations()
        {
            Debug.Log("LOAD_VISUALIZATION");
            //string[] visualizations = { "motion_line", "gray_replay", "replay", "primary_region", "visualization" };
            string[] visualizations = {"visualization", "primary_region" };
            string vis_path;
            string fileExt = ".png";



            for (int i = 0; i < visualizations.Length; i++)
            {
                vis_path = file + "/" + visualizations[i];
                if (visualizations[i].Equals("primary_region"))
                {
                    fileExt = ".jpg";
                }
                else
                {
                    Debug.Log("visualizations: " + visualizations[i]);
                    fileExt = ".png";
                }
                string[] files = Directory.GetFiles(vis_path, "*", SearchOption.TopDirectoryOnly);

                for (int j = 0; j < files.Length; j++)
                {
                    bytes = File.ReadAllBytes(vis_path + "/" + imagePrefix + padNumbers(j, 4) + fileExt);
                    vis_map.Add(files[j], bytes);
                    Debug.Log("Added to vis map: " + files[j]);
                }
            }

            fileExt = ".png";

            for (int i = 0; i < objList.Count; i++)
            {
                vis_path = file + "/visualization_" + objList[i];
                try
                {
                    string[] files = Directory.GetFiles(vis_path, "*", SearchOption.TopDirectoryOnly);
                    for (int j = 0; j < files.Length; j++)
                    {
                        try
                        {
                            bytes = File.ReadAllBytes(vis_path + "/" + imagePrefix + padNumbers(j, 4) + fileExt);
                            vis_map.Add(files[j], bytes);
                            Debug.Log("Added to vis map: " + files[j]);
                        }
                        catch
                        {
                            Debug.Log("Object didn't appear yet");
                        }
                    }
                }
                catch
                {
                    Debug.Log("Object not caught in the last frame");
                }
            }

            GameObject objectBoard = GameObject.FindWithTag("ObjectBoard");
            GridObjectCollection objectBoardCollection = objectBoard.GetComponent<GridObjectCollection>();
            objectBoard.transform.rotation = Quaternion.identity;


            // Object toggle button
            for (var i = 0; i < objList.Count; i++)
            {
                Interactable newCheckbox = Instantiate(objCheckbox, new Vector3(0, 0, 0), Quaternion.identity);
                newCheckbox.transform.parent = objectBoard.transform;
                newCheckbox.transform.rotation = Quaternion.identity;
                var onToggleReceiver = newCheckbox.AddReceiver<InteractableOnToggleReceiver>();
                // On Select, append the selected item to the selected object array
                string objCopy = objList[i];
                onToggleReceiver.OnSelect.AddListener(() =>
                {
                    Debug.Log("OnSelect: " + objCopy);
                    selObjList.Add(objCopy);
                    //UpdateVisualizations();
                });
                // On Deselect, remove the selected item from the selected object array
                onToggleReceiver.OnDeselect.AddListener(() =>
                {
                    Debug.Log("OnDeselect: " + objCopy);
                    selObjList.Remove(objCopy);
                    //UpdateVisualizations();
                });

                ButtonConfigHelper allButtonConfigHelper = newCheckbox.GetComponent<ButtonConfigHelper>();
                allButtonConfigHelper.MainLabelText = objList[i];
            }

            // Check all button
            Interactable allCheckbox = Instantiate(objCheckbox, new Vector3(0, 0, 0), Quaternion.identity);
            allCheckbox.transform.parent = objectBoard.transform;
            allCheckbox.transform.rotation = Quaternion.identity;
            var allOnToggleReceiver = allCheckbox.AddReceiver<InteractableOnToggleReceiver>();
            allOnToggleReceiver.OnSelect.AddListener(() =>
            {
                setCombinedVis();
            });
            allOnToggleReceiver.OnDeselect.AddListener(() =>
            {
                setObjectVis();
            });

            ButtonConfigHelper buttonConfigHelper = allCheckbox.GetComponent<ButtonConfigHelper>();
            buttonConfigHelper.MainLabelText = "Show All";

            objectBoardCollection.UpdateCollection();

            
        }


        //public void ObjectCheckboxHandler(string obj)
        //{
        //    // Show the object's visualizations on checked
        //    selObjArray.Append(obj);
        //}

        public static Vector3 StringToVector3(string sVector)
        {
            // Remove the parentheses
            if (sVector.StartsWith("(") && sVector.EndsWith(")"))
            {
                sVector = sVector.Substring(1, sVector.Length - 2);
            }

            // split the items
            string[] sArray = sVector.Split(',');

            // store as a Vector3
            Vector3 result = new Vector3(
                float.Parse(sArray[0]),
                float.Parse(sArray[1]),
                float.Parse(sArray[2]));

            return result;
        }

        public void RestoreLastRun()
        {
            string[] lines = File.ReadAllLines(lastRunPath);
            file = lines[0];
            LoadVisualizations();
            var slideGlobalPosition = StringToVector3(lines[1]);
            var slide = GameObject.FindWithTag("Slide").transform;
            slide.parent = null;
            slide.position = slideGlobalPosition;
        }

        public void RestoreVisualization()
        {
            file = ExamplePath;
            string[] lines = File.ReadAllLines(file + "/metadata.txt");
            objList = lines[1].Split(',').ToList<string>();
            LoadVisualizations();
            var slideGlobalPosition = StringToVector3(lines[0]);
            var slide = GameObject.FindWithTag("Slide").transform;
            slide.parent = null;
            slide.position = slideGlobalPosition;
        }

        public void SwitchToNoAbstraction()
        {
            setScenePlayback();
            var slide = GameObject.FindWithTag("Slide").transform;

            Vector3 scaleChange = new Vector3(-0.9f, -0.6f, 0.0f);
            Vector3 positionChange = new Vector3(-0.9f, 0.5f, 0.0f);
            slide.localScale += scaleChange;
            slide.position += positionChange;
        }



        void OnGUI()
        {
            //if (GUILayout.Button("Start server"))
            //{
            //    InitClient();
            //}

            if (GUILayout.Button("Connect"))
            {
                Debug.Log("Connect pressed");
                InitClient();
            }


            if (GUILayout.Button("Mark primary region"))
            {
                MarkPrimaryRegion();
            }

            if (GUILayout.Button("Start tracking"))
            {
                StartTracking();
            }

            if (GUILayout.Button("Finish tracking"))
            {
                FinishTracking();
            }

            if (GUILayout.Button("Start replay"))
            {
                StartReplay();
            }

            if (GUILayout.Button("Load visualization"))
            {
                LoadVisualizations();
            }

            if (GUILayout.Button("Disconnect"))
            {
                Disconnect();
            }

            if (GUILayout.Button("Restore last run"))
            {
                RestoreLastRun();
            }

            if (GUILayout.Button("Restore example"))
            {
                RestoreVisualization();
            }

            if (GUILayout.Button("No abstraction"))
            {
                SwitchToNoAbstraction();
            }
            
        }
        
    }
}