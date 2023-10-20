
using UnityEditor;
using UnityEngine;
//using UnityEngine.VR;
using UnityEngine.XR;

namespace Assets.Script.Controller
{
    [CustomEditor(typeof(AppController))]
    public class AppControllerEditor : Editor
    {

        public override void OnInspectorGUI()
        {
            base.OnInspectorGUI();

            var controller = (AppController)target;

            if (GUILayout.Button("Confirm changes", GUILayout.Width(150)))
            {
                controller.ConfirmPropertiesChanged();
            }
        }
    }

    public class AppController : MonoBehaviour
    {
        public bool EnableVR = true;
        
        // Start is called before the first frame update
        void Start()
        {
            ConfirmPropertiesChanged();
        }

        // Update is called once per frame
        void Update()
        {

        }

        public void ConfirmPropertiesChanged()
        {
            XRSettings.enabled = EnableVR;
        }
    }
}
